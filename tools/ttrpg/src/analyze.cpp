#include <ttrpg/analyze.h>

#include <ttrpg/conventions.h>

#include <functional>
#include <stdexcept>

namespace {

const TFieldDecl* FindField(const TClassDecl& c, const std::string& name) {
    for (const auto& f : c.Fields) {
        if (f.Name == name) {
            return &f;
        }
    }
    return nullptr;
}

bool IsArithmetic(expr::EBinaryOp op) {
    using O = expr::EBinaryOp;
    return op == O::Add || op == O::Sub || op == O::Mul || op == O::Div;
}

std::string BinOpToCpp(expr::EBinaryOp op) {
    switch (op) {
        case expr::EBinaryOp::Add: return "+";
        case expr::EBinaryOp::Sub: return "-";
        case expr::EBinaryOp::Mul: return "*";
        case expr::EBinaryOp::Div: return "/";
        default:
            throw std::runtime_error(
                "comparison/logical operators are not supported in computed defaults");
    }
}

std::string InferAndValidate(const expr::TExprNode& e,
                             const TClassDecl& c,
                             const std::unordered_map<std::string, const TClassDecl*>& classes,
                             const std::unordered_map<std::string, std::size_t>& index,
                             std::vector<std::size_t>& deps) {
    using K = expr::ENodeKind;
    const std::string where = " (in class '" + c.Name + "')";
    switch (e.Kind) {
        case K::IntLiteral:
            return "int";
        case K::Var: {
            if (e.HasDollar) {
                throw std::runtime_error("'$' variables are not supported in computed defaults" + where);
            }
            auto it = index.find(e.Text);
            if (it == index.end()) {
                throw std::runtime_error("unknown field '" + e.Text + "' referenced in initializer" + where);
            }
            const TFieldDecl& field = c.Fields[it->second];
            if (field.Derived) {
                throw std::runtime_error("cannot reference derived field '" + e.Text +
                                         "' by bare name in an initializer (it has no storage)" + where);
            }
            deps.push_back(it->second);
            return field.TypeName;
        }
        case K::Member: {
            std::string baseType = InferAndValidate(*e.Lhs, c, classes, index, deps);
            auto cls = classes.find(baseType);
            if (cls == classes.end()) {
                throw std::runtime_error("'" + baseType + "' is not a class; cannot access member '" +
                                         e.Text + "'" + where);
            }
            const TFieldDecl* member = FindField(*cls->second, e.Text);
            if (member == nullptr) {
                throw std::runtime_error("class '" + baseType + "' has no field '" + e.Text + "'" + where);
            }
            return member->TypeName;
        }
        case K::Binary: {
            if (!IsArithmetic(e.BinOp)) {
                throw std::runtime_error("comparison/logical operators are not supported in computed defaults" + where);
            }
            std::string lt = InferAndValidate(*e.Lhs, c, classes, index, deps);
            std::string rt = InferAndValidate(*e.Rhs, c, classes, index, deps);
            if (!IsBuiltinInt(lt) || !IsBuiltinInt(rt)) {
                throw std::runtime_error("arithmetic operands must be int" + where);
            }
            return "int";
        }
        case K::Call:
            throw std::runtime_error("function calls are not supported in computed defaults" + where);
        case K::Unary:
            throw std::runtime_error("unary operators are not supported in computed defaults" + where);
    }
    throw std::runtime_error("unsupported node in computed initializer" + where);
}

}

bool InitIsComputed(const expr::TExprNode& e, const std::unordered_set<std::string>& fieldNames) {
    using K = expr::ENodeKind;
    switch (e.Kind) {
        case K::IntLiteral:
            return false;
        case K::Var:
            return fieldNames.count(e.Text) != 0;
        case K::Member:
        case K::Call:
        case K::Unary:
        case K::Binary:
            return true;
    }
    return false;
}

std::string InitExprToCpp(const expr::TExprNode& e, const std::string& selfPrefix) {
    using K = expr::ENodeKind;
    switch (e.Kind) {
        case K::IntLiteral:
            return e.Text;
        case K::Var:
            return selfPrefix + e.Text + "_";
        case K::Member:
            return InitExprToCpp(*e.Lhs, selfPrefix) + "." + e.Text + "()";
        case K::Binary:
            return "(" + InitExprToCpp(*e.Lhs, selfPrefix) + " " + BinOpToCpp(e.BinOp) + " " +
                   InitExprToCpp(*e.Rhs, selfPrefix) + ")";
        case K::Call:
        case K::Unary:
            break;
    }
    throw std::runtime_error("unsupported node in computed initializer lowering");
}

std::vector<std::size_t> FieldInitOrder(
    const TClassDecl& c,
    const std::unordered_map<std::string, const TClassDecl*>& classes) {
    std::unordered_set<std::string> field_names;
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < c.Fields.size(); ++i) {
        field_names.insert(c.Fields[i].Name);
        index[c.Fields[i].Name] = i;
    }

    std::vector<std::vector<std::size_t>> deps(c.Fields.size());
    std::vector<bool> derived(c.Fields.size(), false);
    for (std::size_t i = 0; i < c.Fields.size(); ++i) {
        const TFieldDecl& f = c.Fields[i];
        if (f.Derived) {
            derived[i] = true;
            std::vector<std::size_t> ignored;
            std::string t = InferAndValidate(*f.Init, c, classes, index, ignored);
            if (t != f.TypeName) {
                throw std::runtime_error(
                    "derived field '" + f.Name + "' in class '" + c.Name + "' is declared '" +
                    f.TypeName + "' but its initializer expression has type '" + t + "'");
            }
            continue;
        }
        if (!f.Init || !InitIsComputed(*f.Init, field_names)) {
            continue;
        }
        if (!IsBuiltinInt(f.TypeName) && !IsBuiltinBoundedQuantity(f.TypeName)) {
            throw std::runtime_error(
                "computed field '" + f.Name + "' in class '" + c.Name +
                "' must be int or BoundedQuantity");
        }
        std::string t = InferAndValidate(*f.Init, c, classes, index, deps[i]);
        if (!IsBuiltinInt(t)) {
            throw std::runtime_error(
                "computed field '" + f.Name + "' in class '" + c.Name +
                "' initializer must evaluate to int");
        }
    }

    enum class EMark { White, Gray, Black };
    std::vector<EMark> mark(c.Fields.size(), EMark::White);
    for (std::size_t i = 0; i < c.Fields.size(); ++i) {
        if (derived[i]) {
            mark[i] = EMark::Black;
        }
    }
    std::vector<std::size_t> order;
    order.reserve(c.Fields.size());

    std::function<void(std::size_t)> visit = [&](std::size_t i) {
        if (mark[i] == EMark::Black) {
            return;
        }
        if (mark[i] == EMark::Gray) {
            throw std::runtime_error(
                "cyclic field initializer dependency in class '" + c.Name +
                "' involving '" + c.Fields[i].Name + "'");
        }
        mark[i] = EMark::Gray;
        for (std::size_t d : deps[i]) {
            visit(d);
        }
        mark[i] = EMark::Black;
        order.push_back(i);
    };
    for (std::size_t i = 0; i < c.Fields.size(); ++i) {
        visit(i);
    }
    return order;
}
