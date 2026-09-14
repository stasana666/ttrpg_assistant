#include <ttrpg/program.h>

#include <ttrpg/conventions.h>
#include <ttrpg/abilities.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {

using TStatement = TClassDecl::TStatement;
using TMethod = TClassDecl::TMethod;
using TClasses = std::unordered_map<std::string, const TClassDecl*>;
using TEnums = std::unordered_map<std::string, const TEnumDecl*>;

TEnums EnumTable(const TLoadedSchemas& loaded) {
    TEnums result;
    for (const auto& [stem, module] : loaded.ByStem) {
        for (const auto& enumeration : module.Enums) result.emplace(enumeration.Name, &enumeration);
    }
    for (const auto& enumeration : loaded.Primary.Enums) result[enumeration.Name] = &enumeration;
    return result;
}

TClasses ClassTable(const TLoadedSchemas& loaded) {
    TClasses result;
    for (const auto& [stem, module] : loaded.ByStem) {
        for (const auto& c : module.Classes) result.emplace(c.Name, &c);
    }
    for (const auto& c : loaded.Primary.Classes) result[c.Name] = &c;
    return result;
}

const TFieldDecl* Field(const TClassDecl& c, const std::string& name) {
    for (const auto& field : c.Fields) if (field.Name == name) return &field;
    return nullptr;
}

bool NewExpression(const expr::TExprNode& value) {
    using K = expr::ENodeKind;
    if (value.Kind == K::ContextVar || value.Kind == K::ObjectInit || value.Kind == K::Call ||
        value.Kind == K::Unary || value.Kind == K::Index || value.Kind == K::StringLiteral ||
        value.Kind == K::ListLiteral) return true;
    if (value.Kind == K::Binary && value.BinOp != expr::EBinaryOp::Add && value.BinOp != expr::EBinaryOp::Sub &&
        value.BinOp != expr::EBinaryOp::Mul && value.BinOp != expr::EBinaryOp::Div) return true;
    if (value.Lhs && NewExpression(*value.Lhs)) return true;
    if (value.Rhs && NewExpression(*value.Rhs)) return true;
    for (const auto& arg : value.Args) if (NewExpression(arg)) return true;
    return false;
}

std::string Operator(expr::EBinaryOp op) {
    using O = expr::EBinaryOp;
    switch (op) {
        case O::Add: return "+";
        case O::Sub: return "-";
        case O::Mul: return "*";
        case O::Div: return "/";
        case O::And: return "&&";
        case O::Or: return "||";
        case O::Eq: return "==";
        case O::Ne: return "!=";
        case O::Lt: return "<";
        case O::Le: return "<=";
        case O::Gt: return ">";
        case O::Ge: return ">=";
    }
    throw std::runtime_error("unknown binary operator");
}

struct TValue {
    std::string Type{};
    std::string Code{};
    bool Effect = false;
    bool Persistent = false;
    std::string Guard{};
    std::string Storage{};
    std::string OwnerGuard{};
    const TFieldDecl* Field = nullptr;
    EContainer Container = EContainer::None;
    std::string KeyType{};
    std::string ElementType{};
    std::string Binding{};
    std::string LocalCode{};
};

class TCompiler {
public:
    TCompiler(const TClassDecl& self, const TLoadedSchemas& loaded)
        : self_(self), loaded_(loaded), classes_(ClassTable(loaded)), enums_(EnumTable(loaded)) {
        classes_[self.Name] = &self;
    }

    [[noreturn]] void Fail(const std::string& message) const {
        throw std::runtime_error(message + " (in " + self_.Name +
                                 (method_ ? "." + method_->Name : "") + ")");
    }

    void CheckType(const std::string& type, bool allowVoid = false) const {
        if (IsBuiltinPrimitive(type) || IsBuiltinBoundedQuantity(type) || IsBuiltinResource(type) || IsBuiltinFeature(type) ||
            loaded_.SymbolTable.contains(type) || classes_.contains(type) ||
            (allowVoid && type == "void")) return;
        Fail("unknown type '" + type + "'");
    }

    void Match(const TValue& value, const std::string& expected) const {
        if (value.Type != expected) Fail("expected '" + expected + "', got '" + value.Type + "'");
    }

    TValue Read(const expr::TExprNode& expression, const std::string& expected = "", bool effects = false) {
        using K = expr::ENodeKind;
        const auto& e = expression;
        switch (e.Kind) {
            case K::IntLiteral: {
                std::size_t consumed = 0;
                long long value = 0;
                try {
                    value = std::stoll(e.Text, &consumed, 10);
                    if (consumed != e.Text.size() || value < -2147483648LL || value > 2147483647LL) {
                        Fail("integer literal outside int range");
                    }
                } catch (const std::logic_error&) { Fail("integer literal outside int range"); }
                return {.Type = "int", .Code = std::to_string(value)};
            }
            case K::StringLiteral: return {.Type = "string", .Code = "std::string(" + CppStringLiteral(e.Text) + ")"};
            case K::ContextVar:
                if (e.Text != "self") Fail("unknown context variable '@" + e.Text + "'");
                if (action_) {
                    return {.Type = self_.Name, .Code = "action_self.Read()", .Persistent = true,
                        .Guard = "action_self.Guard()", .Binding = "action_self"};
                }
                return {.Type = self_.Name, .Code = "std::as_const(self)", .Persistent = true,
                        .Guard = "TGuarded<" + self_.Name + ">(self)", .Storage = "self"};
            case K::Var: {
                if (e.HasDollar) Fail("use '@' for rule context variables");
                if (e.Text == "true" || e.Text == "false") return {.Type = "bool", .Code = e.Text};
                if (e.Text == "min_int" || e.Text == "max_int") {
                    return {.Type = "int", .Code = DefaultExprToCpp(e.Text, "int")};
                }
                const auto local = locals_.find(e.Text);
                if (local != locals_.end()) return local->second;
                if (Field(self_, e.Text)) {
                    expr::TExprNode base;
                    base.Kind = K::ContextVar;
                    base.Text = "self";
                    expr::TExprNode member;
                    member.Kind = K::Member;
                    member.Text = e.Text;
                    member.Lhs = std::make_shared<expr::TExprNode>(base);
                    return Read(member, expected);
                }
                if (expected == "Feature") {
                    const auto definition = ResolveAbility(e.Text, loaded_);
                    if (definition.Kind != EAbilityDeclarationKind::Feature) Fail("Feature values require a feature definition");
                    return {.Type = "Feature", .Code = "TFeatureSelection(" + CppStringLiteral(definition.OwnerStem + "." + e.Text) + ")"};
                }
                const TEnumDecl* literal_type = nullptr;
                for (const auto& [name, enumeration] : enums_) {
                    if ((!expected.empty() && name != expected) ||
                        std::find(enumeration->Values.begin(), enumeration->Values.end(), e.Text) == enumeration->Values.end()) {
                        continue;
                    }
                    if (literal_type) Fail("ambiguous enum value '" + e.Text + "'; a typed context is required");
                    literal_type = enumeration;
                }
                if (literal_type) return {.Type = literal_type->Name, .Code = literal_type->Name + "::" + e.Text};
                Fail("unknown name '" + e.Text + "'");
            }
            case K::Member: {
                auto owner = Read(*e.Lhs);
                auto cls = classes_.find(owner.Type);
                if (cls == classes_.end()) Fail("type '" + owner.Type + "' has no fields");
                const auto* field = Field(*cls->second, e.Text);
                if (!field) Fail("unknown field '" + owner.Type + "." + e.Text + "'");
                const bool direct = owner.Storage == "self";
                if (direct) dependencies_.insert(e.Text);
                TValue result;
                result.Type = CppMemberType(*field, loaded_.SymbolTable);
                if (field->Container == EContainer::None) result.Type = field->TypeName;
                const std::string receiver = owner.Persistent ? owner.Code :
                    "static_cast<const " + owner.Type + "&>(" + owner.Code + ")";
                result.Code = "(" + receiver + ")." + e.Text + "()";
                result.Persistent = owner.Persistent;
                result.Field = field;
                result.Container = field->Container;
                result.KeyType = field->Container == EContainer::Map ? field->TypeName : "";
                result.ElementType = field->Container == EContainer::Map ? field->ValueTypeName : field->TypeName;
                if (!field->Derived && direct) {
                    result.Storage = "self." + e.Text + "_";
                    result.Guard = "self." + e.Text + "()";
                } else if (!field->Derived && !owner.Guard.empty()) {
                    result.Guard = "ctx.Project(" + owner.Guard + ", [](auto& value) { return value." + e.Text + "(); })";
                }
                result.OwnerGuard = owner.Guard;
                return result;
            }
            case K::Binary: {
                using O = expr::EBinaryOp;
                TValue left;
                TValue right;
                if ((e.BinOp == O::Eq || e.BinOp == O::Ne) && UnboundName(*e.Lhs) && !UnboundName(*e.Rhs)) {
                    right = Read(*e.Rhs);
                    left = Read(*e.Lhs, right.Type);
                } else {
                    left = Read(*e.Lhs);
                    right = Read(*e.Rhs, left.Type);
                }
                std::string type = "int";
                if (e.BinOp == O::And || e.BinOp == O::Or) {
                    Match(left, "bool"); Match(right, "bool"); type = "bool";
                } else if (e.BinOp == O::Eq || e.BinOp == O::Ne) {
                    Match(right, left.Type);
                    if (!left.Binding.empty() && !right.Binding.empty()) {
                        left.Code += ".Id()";
                        right.Code += ".Id()";
                    } else if (!IsBuiltinPrimitive(left.Type) && !enums_.contains(left.Type)) {
                        Fail("equality requires primitive values or values of the same enum type");
                    }
                    type = "bool";
                } else {
                    Match(left, "int"); Match(right, "int");
                    if (e.BinOp != O::Add && e.BinOp != O::Sub && e.BinOp != O::Mul && e.BinOp != O::Div) type = "bool";
                }
                std::string code;
                if (type == "int") {
                    const std::string function = e.BinOp == O::Add ? "Add" : e.BinOp == O::Sub ? "Subtract" : e.BinOp == O::Mul ? "Multiply" : "Divide";
                    code = "TRuleInteger::" + function + "(" + left.Code + ", " + right.Code + ")";
                } else {
                    code = "(" + left.Code + " " + Operator(e.BinOp) + " " + right.Code + ")";
                }
                return {.Type = type, .Code = code};
            }
            case K::Unary: {
                auto operand = Read(*e.Lhs);
                const auto type = e.UnOp == expr::EUnaryOp::Not ? "bool" : "int";
                Match(operand, type);
                return {.Type = type, .Code = e.UnOp == expr::EUnaryOp::Not ? "(!" + operand.Code + ")" : "TRuleInteger::Negate(" + operand.Code + ")"};
            }
            case K::Call: return Call(e, effects);
            case K::ObjectInit: {
                const auto type = e.Text.empty() ? expected : e.Text;
                const auto cls = classes_.find(type);
                if (cls == classes_.end()) Fail("unknown constructed class '" + type + "'");
                if (cls->second->GameEntity && method_ != nullptr) {
                    Fail("creating entities inside methods requires transactional entity allocation, which is not supported yet");
                }
                std::unordered_map<std::string, std::string> values;
                for (const auto& initializer : e.Fields) {
                    const auto* field = Field(*cls->second, initializer.Name);
                    if (!field) Fail("unknown initializer field '" + type + "." + initializer.Name + "'");
                    if (field->Derived) Fail("cannot initialize derived field '" + initializer.Name + "'");
                    if (values.contains(initializer.Name)) Fail("duplicate initializer field '" + initializer.Name + "'");
                    auto value = ReadInitializer(initializer.Value, *field);
                    values.emplace(initializer.Name, value.Code);
                }
                std::string code = type + "::Create({";
                bool first = true;
                for (const auto& field : cls->second->Fields) {
                    auto it = values.find(field.Name);
                    if (it == values.end()) continue;
                    if (!first) code += ", ";
                    first = false;
                    code += "." + field.Name + " = " + it->second;
                }
                return {.Type = type, .Code = code + "})"};
            }
            case K::Index: Fail("indexing rule collections requires stable element addressing and is not implemented");
            case K::ListLiteral: Fail("list literals require a collection field initializer");
        }
        Fail("unsupported expression");
    }

    TValue ReadInitializer(const expr::TExprNode& expression, const TFieldDecl& field) {
        if (field.Container == EContainer::None) {
            auto value = Read(expression, field.TypeName);
            if ((field.TypeName == "BoundedQuantity" || field.TypeName == "Resource") && value.Type == "int") {
                value.Type = field.TypeName;
                value.Code = CppTypeFor(field.TypeName) + "(" + value.Code + ")";
            }
            Match(value, field.TypeName);
            return value;
        }
        if (field.Container != EContainer::Collection || expression.Kind != expr::ENodeKind::ListLiteral) {
            Fail("container initialization requires a list literal for a collection field");
        }
        const std::string type = CppMemberType(field, loaded_.SymbolTable);
        std::string code = "([&] { " + type + " values; ";
        for (const auto& element : expression.Args) {
            auto value = Read(element, field.TypeName);
            Match(value, field.TypeName);
            code += "values.Add(" + value.Code + "); ";
        }
        code += "return values; }())";
        return {.Type = type, .Code = code};
    }

    TValue Call(const expr::TExprNode& call, bool effects) {
        if (!call.Lhs) {
            if (call.Text == "roll") {
                if (!action_ || !effects) Fail("roll is allowed only as an effectful action expression");
                if (call.Args.size() != 2) Fail("roll expects a count and a die size");
                auto count = Read(call.Args[0]);
                auto size = Read(call.Args[1]);
                Match(count, "int");
                Match(size, "int");
                return {.Type = "int", .Code = "RollActionDice(Context, " + count.Code + ", " + size.Code + ")", .Effect = true};
            }
            if (call.Text != "min" && call.Text != "max") Fail("unknown function '" + call.Text + "'");
            if (call.Args.size() != 2) Fail("function '" + call.Text + "' expects two arguments");
            auto left = Read(call.Args[0]);
            auto right = Read(call.Args[1]);
            Match(left, "int"); Match(right, "int");
            return {.Type = "int", .Code = "std::" + call.Text + "(" + left.Code + ", " + right.Code + ")"};
        }
        if (call.Lhs->Kind != expr::ENodeKind::Member) Fail("only named methods can be called");
        const auto& member = *call.Lhs;
        auto owner = Read(*member.Lhs);
        if (owner.Container == EContainer::Map) {
            if (member.Text != "get") Fail("map supports only the pure 'get' lookup");
            if (call.Args.size() != 2) Fail("map.get expects a key and a fallback value");
            auto key = Read(call.Args[0], owner.KeyType);
            auto fallback = Read(call.Args[1], owner.ElementType);
            Match(key, owner.KeyType);
            Match(fallback, owner.ElementType);
            const auto value_type = CppTypeFor(owner.ElementType);
            const std::string code = "([](const auto& rule_map, " + CppTypeFor(owner.KeyType) +
                " rule_key, " + value_type + " rule_fallback) -> " + value_type +
                " { const auto rule_entry = rule_map.find(rule_key); if (rule_entry == rule_map.end()) { return rule_fallback; } "
                "return rule_entry->second; }(" + owner.Code + ", " + key.Code + ", " + fallback.Code + "))";
            return {.Type = owner.ElementType, .Code = code};
        }
        const auto cls = classes_.find(owner.Type);
        if (cls == classes_.end()) Fail("method receiver must be a class");
        const TMethod* method = nullptr;
        for (const auto& candidate : cls->second->Methods) if (candidate.Name == member.Text) method = &candidate;
        if (!method) Fail("unknown method '" + owner.Type + "." + member.Text + "'");
        if (method->Parameters.size() != call.Args.size()) Fail("wrong argument count for '" + member.Text + "'");
        if (method->Query && owner.Storage == "self") {
            std::unordered_set<std::string> visiting;
            QueryDependencies(*method, visiting);
        }
        if (!method->Query && !effects) Fail("mutating method '" + member.Text + "' is not allowed in a pure expression");
        if (!method->Query && owner.Guard.empty()) Fail("mutating methods require a stable persistent receiver");
        std::string args;
        for (std::size_t i = 0; i < call.Args.size(); ++i) {
            auto value = Read(call.Args[i], method->Parameters[i].TypeName);
            Match(value, method->Parameters[i].TypeName);
            if (i) args += ", ";
            args += value.Code;
        }
        std::string code;
        if (method->Query) {
            code = "(" + owner.Code + ")." + method->Name + "(" + args + ")";
        } else {
            code = "ctx.Invoke(" + owner.Guard + ", &" + owner.Type + "::" + method->Name;
            if (!args.empty()) code += ", " + args;
            code += ")";
        }
        return {.Type = method->ReturnType, .Code = code, .Effect = !method->Query};
    }

    TCompiledClass Compile() {
        ValidateNames();
        TCompiledClass result;
        std::unordered_map<std::string, std::size_t> indices;
        for (std::size_t i = 0; i < self_.Fields.size(); ++i) indices[self_.Fields[i].Name] = i;
        std::vector<std::vector<std::size_t>> dependencies(self_.Fields.size());
        for (std::size_t i = 0; i < self_.Fields.size(); ++i) {
            const auto& field = self_.Fields[i];
            TCompiledField compiled{field, CppMemberType(field, loaded_.SymbolTable), ""};
            CheckType(field.TypeName);
            if (field.Container != EContainer::None) {
                const auto type = loaded_.SymbolTable.find(field.TypeName);
                const bool declared = type != loaded_.SymbolTable.end();
                if (field.Container == EContainer::Set && (!declared ||
                    (type->second.Kind != ETypeKind::Enum && type->second.Kind != ETypeKind::Variant))) {
                    Fail("set elements must be declared enums or variants");
                }
                if (field.Container == EContainer::Collection && !IsBuiltinFeature(field.TypeName) &&
                    (!declared || type->second.Kind != ETypeKind::Class)) {
                    Fail("collection elements must be declared classes or Feature");
                }
                if (field.Container == EContainer::Map) {
                    if (!declared || type->second.Kind != ETypeKind::Enum) Fail("map keys must be declared enums");
                    CheckType(field.ValueTypeName);
                    if (IsBuiltinFeature(field.ValueTypeName)) Fail("Feature supports scalar and collection fields only");
                }
            }
            const auto nested = classes_.find(field.Container == EContainer::Map ? field.ValueTypeName : field.TypeName);
            if (nested != classes_.end() && nested->second->GameEntity) {
                Fail("entity-valued fields are not supported yet: '" + field.Name + "'");
            }
            if (field.Init) {
                dependencies_.clear();
                auto value = ReadInitializer(*field.Init, field);
                compiled.Initializer = value.Code;
                for (const auto& dependency : dependencies_) dependencies[i].push_back(indices.at(dependency));
                std::sort(dependencies[i].begin(), dependencies[i].end());
            }
            result.Fields.push_back(std::move(compiled));
        }
        std::vector<int> marks(self_.Fields.size(), 0);
        std::function<void(std::size_t)> visit = [&](std::size_t i) {
            if (marks[i] == 2) return;
            if (marks[i] == 1) Fail("cyclic field dependency at '" + self_.Fields[i].Name + "'");
            marks[i] = 1;
            for (auto dependency : dependencies[i]) visit(dependency);
            marks[i] = 2;
            if (!self_.Fields[i].Derived) result.InitOrder.push_back(i);
        };
        for (std::size_t i = 0; i < self_.Fields.size(); ++i) visit(i);
        for (const auto& method : self_.Methods) {
            method_ = &method;
            locals_.clear();
            CheckType(method.ReturnType, true);
            for (const auto& parameter : method.Parameters) {
                CheckType(parameter.TypeName);
                if (!IsBuiltinPrimitive(parameter.TypeName) && !enums_.contains(parameter.TypeName)) {
                    Fail("method parameters require primitive or enum value types");
                }
                if (!locals_.emplace(parameter.Name, TValue{.Type = parameter.TypeName, .Code = "arg_" + parameter.Name}).second) {
                    Fail("duplicate parameter '" + parameter.Name + "'");
                }
            }
            TCompiledMethod compiled{method, {}};
            auto& body = compiled.Body;
            body.push_back("[[maybe_unused]] " + std::string(method.Query ? "const " : "") + "auto& self = *this;");
            if (!method.Query) body.push_back("auto transaction = ctx.Atomic();");
            const bool returns = Statements(method.Body, body, "");
            if (!returns) {
                if (method.ReturnType != "void") Fail("method must return a value on every path");
                if (!method.Query) body.push_back("transaction.Commit();");
            }
            result.Methods.push_back(std::move(compiled));
        }
        method_ = nullptr;
        result.Grants = CompileGrants(self_, loaded_);
        return result;
    }

    TCompiledAction CompileActionBody(const TAbilityDecl& declaration) {
        if (!self_.GameEntity) Fail("compiled action owner must be a GameEntity");
        action_ = true;
        TMethod body_method;
        body_method.Name = declaration.Name;
        body_method.ReturnType = "void";
        method_ = &body_method;
        TCompiledAction result;
        std::size_t first = 0;
        while (first < declaration.Body.size() && declaration.Body[first].Kind == TStatement::EKind::Cost) {
            result.Costs.push_back(ActionCost(declaration.Body[first++]));
        }
        result.Body.push_back("[[maybe_unused]] auto action_self = ActionSelf<" + self_.Name + ">(Context);");
        result.Body.push_back("[[maybe_unused]] auto ctx = Context->transformator->ExecutionContext();");
        ActionStatements(declaration.Body, first, result.Body, "");
        method_ = nullptr;
        return result;
    }

private:
    void ValidateActionStep(std::vector<std::string>& output, const std::string& indent) const {
        output.push_back(indent + "ValidateActionActor(Context);");
        output.push_back(indent + "action_self.View().Validate();");
        std::set<std::string> bindings;
        for (const auto& [name, value] : locals_) {
            if (!value.Binding.empty()) bindings.insert(value.Binding);
        }
        for (const auto& binding : bindings) output.push_back(indent + binding + ".View().Validate();");
    }

    TValue EntityBinding(const std::string& type, const std::string& name) const {
        return {.Type = type, .Code = name + ".Read()", .Persistent = true,
            .Guard = name + ".Guard()", .Binding = name, .LocalCode = name};
    }

    std::int64_t ConstantInteger(const expr::TExprNode& value) const {
        using K = expr::ENodeKind;
        using O = expr::EBinaryOp;
        std::int64_t result = 0;
        if (value.Kind == K::IntLiteral) {
            try { result = std::stoll(value.Text); }
            catch (const std::logic_error&) { Fail("cost integer is outside int range"); }
        } else if (value.Kind == K::Var && !value.HasDollar &&
                   (value.Text == "min_int" || value.Text == "max_int")) {
            result = value.Text == "min_int" ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
        } else if (value.Kind == K::Unary && value.UnOp == expr::EUnaryOp::Negate) {
            result = -ConstantInteger(*value.Lhs);
        } else if (value.Kind == K::Binary) {
            const auto left = ConstantInteger(*value.Lhs);
            const auto right = ConstantInteger(*value.Rhs);
            switch (value.BinOp) {
                case O::Add: result = left + right; break;
                case O::Sub: result = left - right; break;
                case O::Mul: result = left * right; break;
                case O::Div:
                    if (right == 0) Fail("division by zero in resource cost");
                    result = left / right;
                    break;
                default: Fail("cost must be a constant integer expression");
            }
        } else if (value.Kind == K::Call && !value.Lhs && value.Args.size() == 2 &&
                   (value.Text == "min" || value.Text == "max")) {
            const auto left = ConstantInteger(value.Args[0]);
            const auto right = ConstantInteger(value.Args[1]);
            result = value.Text == "min" ? std::min(left, right) : std::max(left, right);
        } else {
            Fail("cost must be a constant integer expression");
        }
        if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max()) {
            Fail("cost integer is outside int range");
        }
        return result;
    }

    TCompiledActionCost ActionCost(const TStatement& statement) {
        auto amount = Read(*statement.Value);
        Match(amount, "int");
        const auto count = ConstantInteger(*statement.Value);
        if (count < 0) Fail("resource cost cannot be negative");
        std::vector<std::string> path;
        std::function<void(const expr::TExprNode&)> read_path = [&](const expr::TExprNode& value) {
            if (value.Kind == expr::ENodeKind::ContextVar && value.Text == "self") return;
            if (value.Kind != expr::ENodeKind::Member || !value.Lhs) Fail("cost requires a stored field path rooted at @self");
            read_path(*value.Lhs);
            path.push_back(value.Text);
        };
        read_path(*statement.Target);
        if (path.empty()) Fail("cost requires a Resource field");
        TCompiledActionCost result;
        result.Key = self_.Name;
        result.Count = static_cast<int>(count);
        result.Read = "BindActionEntity<" + self_.Name + ">(player).Read()";
        result.Write = "BindActionEntity<" + self_.Name + ">(player).Guard()";
        const TClassDecl* owner = &self_;
        for (std::size_t i = 0; i < path.size(); ++i) {
            const auto* field = Field(*owner, path[i]);
            if (!field) Fail("unknown cost field '" + path[i] + "'");
            if (field->Derived || field->Container != EContainer::None) Fail("cost requires stored scalar fields");
            result.Key += "." + PascalToSnake(path[i]);
            result.Read += "." + path[i] + "()";
            result.Write = "ProjectActionField(" + result.Write + ", [](auto& value) { return value." + path[i] + "(); })";
            if (i + 1 == path.size()) {
                if (!IsBuiltinResource(field->TypeName)) Fail("cost requires a Resource field");
            } else {
                const auto nested = classes_.find(field->TypeName);
                if (nested == classes_.end()) Fail("cost path must traverse stored class fields");
                owner = nested->second;
            }
        }
        return result;
    }

    void ActionStatements(const std::vector<TStatement>& body, std::size_t first,
                          std::vector<std::string>& output, const std::string& indent) {
        for (std::size_t i = first; i < body.size(); ++i) {
            const auto& statement = body[i];
            if (statement.Kind != TStatement::EKind::Choose) {
                if (Statements({statement}, output, indent)) {
                    if (i + 1 != body.size()) Fail("unreachable statement after return");
                    return;
                }
                continue;
            }
            ValidateActionStep(output, indent);
            const auto type = classes_.find(statement.TypeName);
            if (type == classes_.end() || !type->second->GameEntity) Fail("choose requires a GameEntity type");
            if (locals_.contains(statement.Name)) Fail("duplicate local '" + statement.Name + "'");
            std::map<std::string, std::string> captured;
            for (const auto& [name, value] : locals_) captured.emplace(name, value.LocalCode);
            std::string captures = "Context, action_self";
            for (const auto& [name, code] : captured) captures += ", captured_" + name + " = " + code;
            const auto binding_type = "TGeneratedEntityBinding<" + statement.TypeName + ">";
            const auto binding_name = "bound_" + statement.Name;
            locals_.emplace(statement.Name, EntityBinding(statement.TypeName, binding_name));
            std::string predicate = "true";
            if (statement.Value) {
                auto value = Read(*statement.Value);
                Match(value, "bool");
                predicate = value.Code;
            }
            output.push_back(indent + "ChooseActionEntity<" + statement.TypeName + ">(Context, ActionEntities<" + statement.TypeName + ">(Context),");
            output.push_back(indent + "    [" + captures + "]([[maybe_unused]] const " + binding_type + "& " + binding_name + ") {");
            output.push_back(indent + "        (void)Context;");
            output.push_back(indent + "        (void)action_self;");
            for (const auto& [name, code] : captured) output.push_back(indent + "        [[maybe_unused]] auto " + code + " = captured_" + name + ";");
            ValidateActionStep(output, indent + "        ");
            output.push_back(indent + "        return " + predicate + ";");
            output.push_back(indent + "    },");
            output.push_back(indent + "    [" + captures + "]([[maybe_unused]] " + binding_type + " " + binding_name + ") {");
            output.push_back(indent + "        [[maybe_unused]] auto ctx = Context->transformator->ExecutionContext();");
            output.push_back(indent + "        (void)action_self;");
            for (const auto& [name, code] : captured) output.push_back(indent + "        [[maybe_unused]] auto " + code + " = captured_" + name + ";");
            ValidateActionStep(output, indent + "        ");
            ActionStatements(body, i + 1, output, indent + "        ");
            output.push_back(indent + "    });");
            return;
        }
    }

    bool UnboundName(const expr::TExprNode& value) const {
        return value.Kind == expr::ENodeKind::Var && !value.HasDollar &&
            value.Text != "true" && value.Text != "false" && value.Text != "min_int" && value.Text != "max_int" &&
            !locals_.contains(value.Text) && !Field(self_, value.Text);
    }

    void QueryDependencies(const TMethod& method, std::unordered_set<std::string>& visiting) {
        if (!visiting.insert(method.Name).second) Fail("recursive query '" + method.Name + "'");
        std::unordered_set<std::string> names;
        for (const auto& parameter : method.Parameters) names.insert(parameter.Name);
        std::function<void(const expr::TExprNode&, const std::unordered_set<std::string>&)> expression;
        expression = [&](const expr::TExprNode& value, const std::unordered_set<std::string>& scope) {
            using K = expr::ENodeKind;
            if (value.Kind == K::Var && !value.HasDollar && !scope.contains(value.Text) && Field(self_, value.Text)) {
                dependencies_.insert(value.Text);
            }
            if (value.Kind == K::Member && value.Lhs && value.Lhs->Kind == K::ContextVar &&
                value.Lhs->Text == "self" && Field(self_, value.Text)) {
                dependencies_.insert(value.Text);
            }
            if (value.Kind == K::Call && value.Lhs && value.Lhs->Kind == K::Member &&
                value.Lhs->Lhs && value.Lhs->Lhs->Kind == K::ContextVar && value.Lhs->Lhs->Text == "self") {
                for (const auto& nested : self_.Methods) {
                    if (nested.Name == value.Lhs->Text && nested.Query) QueryDependencies(nested, visiting);
                }
            }
            if (value.Lhs) expression(*value.Lhs, scope);
            if (value.Rhs) expression(*value.Rhs, scope);
            for (const auto& argument : value.Args) expression(argument, scope);
            for (const auto& field : value.Fields) expression(field.Value, scope);
        };
        std::function<void(const std::vector<TStatement>&, std::unordered_set<std::string>)> statements;
        statements = [&](const std::vector<TStatement>& body, std::unordered_set<std::string> scope) {
            for (const auto& statement : body) {
                if (statement.Value) expression(*statement.Value, scope);
                if (statement.Target) expression(*statement.Target, scope);
                if (statement.Kind == TStatement::EKind::Let) scope.insert(statement.Name);
                statements(statement.Body, scope);
                statements(statement.ElseBody, scope);
            }
        };
        statements(method.Body, std::move(names));
        visiting.erase(method.Name);
    }

    void ValidateNames() {
        std::unordered_set<std::string> names;
        const std::unordered_set<std::string> keywords{
            "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break",
            "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept",
            "const", "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await", "co_return", "co_yield",
            "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern",
            "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
            "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast",
            "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch",
            "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
            "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
        };
        if (keywords.contains(self_.Name)) Fail("reserved class name '" + self_.Name + "'");
        const std::unordered_set<std::string> reserved{"Create", "FromJson", "Restore", "ToJson", "GetAst", "RegisterDslProperties", "TInitialValues", "TUninitialized", "VisitGrantedAbilities"};
        for (const auto& field : self_.Fields) {
            if (field.Name == self_.Name || field.Name.ends_with('_') || keywords.contains(field.Name) || reserved.contains(field.Name) || (self_.GameEntity && field.Name == "Id")) Fail("reserved field name '" + field.Name + "'");
            if (!names.insert(field.Name).second) Fail("duplicate member '" + field.Name + "'");
        }
        for (const auto& method : self_.Methods) {
            if (method.Name == self_.Name || method.Name.ends_with('_') || keywords.contains(method.Name) || reserved.contains(method.Name)) Fail("reserved method name '" + method.Name + "'");
            if (!names.insert(method.Name).second) Fail("duplicate member '" + method.Name + "'");
        }
    }

    bool Statements(const std::vector<TStatement>& statements, std::vector<std::string>& output, const std::string& indent) {
        using K = TStatement::EKind;
        bool returns = false;
        for (const auto& statement : statements) {
            if (returns) Fail("unreachable statement after return");
            if (action_) ValidateActionStep(output, indent);
            auto line = [&](const std::string& value) { output.push_back(indent + value); };
            const bool effects = !method_->Query;
            switch (statement.Kind) {
                case K::Let: {
                    if (locals_.contains(statement.Name)) Fail("duplicate local '" + statement.Name + "'");
                    auto value = Read(*statement.Value, "", effects);
                    if (value.Type == "void") Fail("cannot bind void result");
                    if (action_ && !value.Binding.empty()) {
                        line("[[maybe_unused]] auto local_" + statement.Name + " = " + value.Binding + ";");
                        locals_[statement.Name] = EntityBinding(value.Type, "local_" + statement.Name);
                        break;
                    }
                    line("[[maybe_unused]] auto local_" + statement.Name + " = " + value.Code + ";");
                    locals_[statement.Name] = {.Type = value.Type, .Code = "local_" + statement.Name,
                        .Container = value.Container, .KeyType = value.KeyType, .ElementType = value.ElementType,
                        .LocalCode = "local_" + statement.Name};
                    break;
                }
                case K::Require: {
                    auto condition = Read(*statement.Value);
                    Match(condition, "bool");
                    line("if (!(" + condition.Code + ")) throw std::runtime_error(" +
                         CppStringLiteral("require failed in " + self_.Name + "." + method_->Name + " at line " + std::to_string(statement.Location.line)) + ");");
                    break;
                }
                case K::Assign:
                case K::AddAssign:
                case K::SubAssign: {
                    auto target = Read(*statement.Target);
                    auto value = Read(*statement.Value, target.Type, effects);
                    Match(value, target.Type);
                    if (statement.Kind != K::Assign) {
                        Match(target, "int");
                        value.Code = "TRuleInteger::" + std::string(statement.Kind == K::AddAssign ? "Add" : "Subtract") + "(" + target.Code + ", " + value.Code + ")";
                    }
                    if (target.Persistent) {
                        if (method_->Query) Fail("query cannot modify persistent fields");
                        if (!target.Field || target.Field->Derived) Fail("cannot assign to a derived value");
                        if (target.Guard.empty() || target.OwnerGuard.empty()) Fail("field write requires a stable owner");
                        if (target.Field->Container != EContainer::None || classes_.contains(target.Type)) {
                            Fail("replacing objects or collections requires stable field paths");
                        }
                        if (!target.Storage.empty()) {
                            line("ctx.Assign(" + target.Storage + ", " + value.Code + ", this, " + CppStringLiteral(PascalToSnake(target.Field->Name)) + ");");
                        } else {
                            line("ctx.AssignGuarded(" + target.Guard + ", " + value.Code + ", " + target.OwnerGuard + ", " + CppStringLiteral(PascalToSnake(target.Field->Name)) + ");");
                        }
                    } else {
                        if (statement.Target->Kind != expr::ENodeKind::Var || !locals_.contains(statement.Target->Text) ||
                            target.Code.rfind("local_", 0) != 0) Fail("assignment target must be a local or a stored field");
                        line(target.Code + " = " + value.Code + ";");
                    }
                    break;
                }
                case K::Call: {
                    if (statement.Value->Kind != expr::ENodeKind::Call) Fail("expected method call statement");
                    auto value = Read(*statement.Value, "", effects);
                    line("(void)" + value.Code + ";");
                    break;
                }
                case K::Return: {
                    if (statement.Value) {
                        auto value = Read(*statement.Value, method_->ReturnType, effects);
                        Match(value, method_->ReturnType);
                        if (value.Type == "void") {
                            line(value.Code + ";");
                            if (!method_->Query && !action_) line("transaction.Commit();");
                            line("return;");
                        } else {
                            line("auto result = " + value.Code + ";");
                            if (!method_->Query && !action_) line("transaction.Commit();");
                            line("return result;");
                        }
                    } else {
                        if (method_->ReturnType != "void") Fail("return value required");
                        if (!method_->Query && !action_) line("transaction.Commit();");
                        line("return;");
                    }
                    returns = true;
                    break;
                }
                case K::If: {
                    auto condition = Read(*statement.Value);
                    Match(condition, "bool");
                    auto saved = locals_;
                    line("if (" + condition.Code + ") {");
                    const bool yes = Statements(statement.Body, output, indent + "    ");
                    locals_ = saved;
                    line("} else {");
                    const bool no = Statements(statement.ElseBody, output, indent + "    ");
                    locals_ = std::move(saved);
                    line("}");
                    returns = yes && no;
                    break;
                }
                case K::Choose: Fail("choose is supported only at the top level of an action body");
                case K::Cost: Fail("cost declarations must precede all statements in an action body");
            }
        }
        return returns;
    }

    const TClassDecl& self_;
    const TLoadedSchemas& loaded_;
    TClasses classes_;
    TEnums enums_;
    std::unordered_map<std::string, TValue> locals_;
    std::unordered_set<std::string> dependencies_;
    const TMethod* method_ = nullptr;
    bool action_ = false;
};

}

bool UsesRuleSyntax(const TClassDecl& declaration, const TLoadedSchemas& loaded) {
    const auto classes = ClassTable(loaded);
    std::unordered_set<std::string> visiting;
    std::function<bool(const TClassDecl&)> inspect = [&](const TClassDecl& c) {
        if (!visiting.insert(c.Name).second) return false;
        if (c.GameEntity || !c.Methods.empty() || !c.Grants.empty()) return true;
        for (const auto& field : c.Fields) {
            if (IsBuiltinFeature(field.TypeName)) return true;
            if (field.Init && NewExpression(*field.Init)) return true;
            const auto nested = classes.find(field.TypeName);
            if (nested != classes.end() && inspect(*nested->second)) return true;
        }
        return false;
    };
    return inspect(declaration);
}

TCompiledClass CompileClass(const TClassDecl& declaration, const TLoadedSchemas& loaded) {
    return TCompiler(declaration, loaded).Compile();
}

std::string CompileInstance(const TInstanceDecl& declaration, const TLoadedSchemas& loaded) {
    const auto classes = ClassTable(loaded);
    const auto type = classes.find(declaration.TypeName);
    if (type == classes.end()) throw std::runtime_error("unknown instance type '" + declaration.TypeName + "'");
    TClassDecl empty;
    empty.Name = "instance " + declaration.Name;
    TCompiler compiler(empty, loaded);
    auto value = compiler.Read(declaration.Value, declaration.TypeName);
    compiler.Match(value, declaration.TypeName);
    return value.Code;
}

TCompiledAction CompileAction(const TAbilityDecl& declaration, const TLoadedSchemas& loaded) {
    if (declaration.Kind != EAbilityDeclarationKind::Action || declaration.OwnerType.empty()) {
        throw std::runtime_error("compiled action requires an owner type");
    }
    const auto classes = ClassTable(loaded);
    const auto owner = classes.find(declaration.OwnerType);
    if (owner == classes.end()) throw std::runtime_error("unknown action owner type '" + declaration.OwnerType + "'");
    return TCompiler(*owner->second, loaded).CompileActionBody(declaration);
}
