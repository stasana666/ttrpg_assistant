#include <ttrpg/program.h>

#include <ttrpg/conventions.h>
#include <ttrpg/abilities.h>
#include <ttrpg/events.h>

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
using TVariants = std::unordered_map<std::string, const TVariantDecl*>;
using TInstances = std::unordered_map<std::string, const TInstanceDecl*>;

TInstances InstanceTable(const TLoadedSchemas& loaded) {
    TInstances result;
    auto add = [&](const TSchemaModule& module) {
        for (const auto& instance : module.Instances) {
            if (!result.emplace(instance.Name, &instance).second) throw std::runtime_error("duplicate named value '" + instance.Name + "'");
            if (loaded.SymbolTable.contains("Create" + instance.Name)) throw std::runtime_error("named value factory conflicts with type 'Create" + instance.Name + "'");
        }
    };
    for (const auto& [stem, module] : loaded.ByStem) if (stem != loaded.PrimaryStem) add(module);
    add(loaded.Primary);
    return result;
}

bool ContainsNamedValue(const expr::TExprNode& expression, const TInstances& instances) {
    if (expression.Kind == expr::ENodeKind::Var && !expression.HasDollar && instances.contains(expression.Text)) return true;
    if (expression.Lhs && ContainsNamedValue(*expression.Lhs, instances)) return true;
    if (expression.Rhs && ContainsNamedValue(*expression.Rhs, instances)) return true;
    for (const auto& argument : expression.Args) if (ContainsNamedValue(argument, instances)) return true;
    for (const auto& field : expression.Fields) if (ContainsNamedValue(field.Value, instances)) return true;
    return false;
}

TVariants VariantTable(const TLoadedSchemas& loaded) {
    TVariants result;
    for (const auto& [stem, module] : loaded.ByStem) {
        for (const auto& variant : module.Variants) result[variant.Name] = &variant;
    }
    for (const auto& variant : loaded.Primary.Variants) result[variant.Name] = &variant;
    return result;
}

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

enum class ESequenceKind { None, Contribution, Entity, Value };

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
    std::string SequenceElement{};
    std::string SourceBinding{};
    std::string VariantAlternative{};
    ESequenceKind SequenceKind = ESequenceKind::None;
};

class TCompiler {
public:
    TCompiler(const TClassDecl& self, const TLoadedSchemas& loaded,
              std::shared_ptr<std::vector<std::string>> construction = std::make_shared<std::vector<std::string>>())
        : self_(self), loaded_(loaded), classes_(ClassTable(loaded)), enums_(EnumTable(loaded)),
          variants_(VariantTable(loaded)), events_(EventSymbols(loaded)), instances_(InstanceTable(loaded)),
          construction_(std::move(construction)) {
        classes_[self.Name] = &self;
        for (const auto& [name, variant] : variants_) {
            for (const auto& alternative : variant->Alternatives) {
                payloads_.emplace(PayloadStructName(name, alternative.Name), &alternative);
            }
        }
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
                if (action_ || hook_ || preflight_) {
                    return EntityBinding(self_.Name, "action_self", action_);
                }
                return {.Type = self_.Name, .Code = "std::as_const(self)", .Persistent = true,
                        .Guard = "TGuarded<" + CppRuleType(self_.Name) + ">(self)", .Storage = "self"};
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
                if (const auto variant = variants_.find(expected); variant != variants_.end()) {
                    return ReadVariant(e, *variant->second, Alternative(*variant->second, e.Text));
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
                if (literal_type) return {.Type = literal_type->Name, .Code = CppRuleType(literal_type->Name) + "::" + e.Text};
                if (const auto instance = instances_.find(e.Text); instance != instances_.end()) {
                    return ReadNamedValue(*instance->second);
                }
                Fail("unknown name '" + e.Text + "'");
            }
            case K::Member: {
                auto owner = Read(*e.Lhs);
                if (IsBuiltinPosition(owner.Type)) {
                    if (e.Text != "X" && e.Text != "Y") Fail("Position has only X and Y coordinates");
                    return {.Type = "int", .Code = "(" + owner.Code + ")." + (e.Text == "X" ? "x" : "y")};
                }
                if (const auto payload = payloads_.find(owner.Type); payload != payloads_.end()) {
                    const TFieldDecl* field = nullptr;
                    for (const auto& candidate : payload->second->Fields) if (candidate.Name == e.Text) field = &candidate;
                    if (!field) Fail("unknown variant payload field '" + e.Text + "'");
                    return {.Type = field->Container == EContainer::None ? field->TypeName : CppMemberType(*field, loaded_.SymbolTable),
                        .Code = "(" + owner.Code + ")." + e.Text, .Field = field,
                        .Container = field->Container, .KeyType = field->Container == EContainer::Map ? field->TypeName : "",
                        .ElementType = field->Container == EContainer::Map ? field->ValueTypeName : field->TypeName};
                }
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
                    "static_cast<const " + CppRuleType(owner.Type) + "&>(" + owner.Code + ")";
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
                    } else if (!IsBuiltinPrimitive(left.Type) && !IsBuiltinPosition(left.Type) && !enums_.contains(left.Type)) {
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
                if (IsBuiltinPosition(e.Text.empty() ? expected : e.Text)) {
                    RequireWorldScope();
                    std::map<std::string, std::string> coordinates;
                    for (const auto& field : e.Fields) {
                        if (field.Name != "X" && field.Name != "Y") Fail("Position has only X and Y coordinates");
                        const auto value = Read(field.Value, "int");
                        Match(value, "int");
                        if (!coordinates.emplace(field.Name, value.Code).second) Fail("duplicate Position coordinate");
                    }
                    if (coordinates.size() != 2) Fail("Position requires both X and Y coordinates");
                    return {.Type = "Position", .Code = "TPosition{" + coordinates.at("X") + ", " + coordinates.at("Y") + "}"};
                }
                if (const auto payload = payloads_.find(expected); payload != payloads_.end()) {
                    if (!e.Text.empty() && e.Text != payload->second->Name) Fail("fallback must construct the requested variant payload");
                    return {.Type = expected, .Code = ReadPayload(e, *payload->second, expected)};
                }
                if (const auto variant = variants_.find(expected); variant != variants_.end()) {
                    return ReadVariant(e, *variant->second, Alternative(*variant->second, e.Text));
                }
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
                ValidateConstructionDefaults(*cls->second, values);
                std::string code = CppRuleType(type) + "::Create({";
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
            case K::ListLiteral: {
                if (!expected.starts_with("list<") || !expected.ends_with('>')) Fail("list literals require a collection field initializer or typed choice source");
                const auto element = expected.substr(5, expected.size() - 6);
                const auto cls = classes_.find(element);
                const bool entity = cls != classes_.end() && cls->second->GameEntity;
                if (!IsBuiltinPrimitive(element) && !IsBuiltinPosition(element) && !enums_.contains(element) && !entity) {
                    Fail("choice lists require primitive, enum, Position, or GameEntity values");
                }
                std::string code = "std::vector<" + CppRuleType(entity ? "TGeneratedEntityBinding<" + element + ">" : CppTypeFor(element)) + ">{";
                for (std::size_t i = 0; i < e.Args.size(); ++i) {
                    const auto value = Read(e.Args[i], element);
                    Match(value, element);
                    if (i) code += ", ";
                    if (entity && value.Binding.empty()) Fail("entity choice values require checked bindings");
                    code += entity ? value.Binding : value.Code;
                }
                return {.Type = expected, .Code = code + "}", .SequenceElement = element,
                    .SequenceKind = entity ? ESequenceKind::Entity : ESequenceKind::Value};
            }
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
        const std::string type = CppMemberType(field, loaded_.SymbolTable);
        std::string code = "([&] { " + CppRuleType(type) + " values; ";
        if (field.Container == EContainer::Map) {
            if (expression.Kind != expr::ENodeKind::ObjectInit || !expression.Text.empty()) Fail("map initialization requires a contextual { Key = Value; } literal");
            std::unordered_set<std::string> keys;
            TFieldDecl valueField = field;
            valueField.Container = EContainer::None;
            valueField.TypeName = field.ValueTypeName;
            valueField.ValueTypeName.clear();
            for (const auto& element : expression.Fields) {
                if (!keys.insert(element.Name).second) Fail("duplicate map key '" + element.Name + "'");
                const auto enumeration = enums_.find(field.TypeName);
                if (enumeration == enums_.end() || std::find(enumeration->second->Values.begin(), enumeration->second->Values.end(), element.Name) == enumeration->second->Values.end()) {
                    Fail("unknown map key '" + element.Name + "' for enum '" + field.TypeName + "'");
                }
                auto value = ReadInitializer(element.Value, valueField);
                code += "values.emplace(" + CppRuleType(field.TypeName) + "::" + element.Name + ", " + value.Code + "); ";
            }
            return {.Type = type, .Code = code + "return values; }())", .Container = EContainer::Map,
                .KeyType = field.TypeName, .ElementType = field.ValueTypeName};
        }
        if (expression.Kind != expr::ENodeKind::ListLiteral) Fail("container initialization requires a list literal");
        std::unordered_set<std::string> alternatives;
        std::size_t elementIndex = 0;
        for (const auto& element : expression.Args) {
            auto value = Read(element, field.TypeName);
            Match(value, field.TypeName);
            if (field.Container == EContainer::Set && variants_.contains(field.TypeName)) {
                if (!value.VariantAlternative.empty() && !alternatives.insert(value.VariantAlternative).second) Fail("duplicate variant alternative '" + value.VariantAlternative + "'");
                const auto local = "element_" + std::to_string(elementIndex++);
                code += "auto " + local + " = " + value.Code + "; if (values.Has(" + local +
                    ".Kind())) { throw std::invalid_argument(" + CppStringLiteral("duplicate variant alternative in " + field.Name) +
                    "); } values.Set(std::move(" + local + ")); ";
            } else {
                code += std::string(field.Container == EContainer::Collection ? "values.Add(" : "values.insert(") + value.Code + "); ";
            }
        }
        code += "return values; }())";
        return {.Type = type, .Code = code, .Container = field.Container, .ElementType = field.TypeName};
    }

    TValue Call(const expr::TExprNode& call, bool effects) {
        if (!call.Lhs) {
            if (call.Text == "position" || call.Text == "adjacent_positions" ||
                call.Text == "contains_position" || call.Text == "can_occupy" || call.Text == "move") {
                RequireWorldScope();
                const bool moving = call.Text == "move";
                const auto count = call.Args.size();
                if (count != (call.Text == "can_occupy" || moving ? 2u : 1u) && !(moving && count == 3)) {
                    Fail("wrong argument count for position operation '" + call.Text + "'");
                }
                if (call.Text == "adjacent_positions" || call.Text == "contains_position") {
                    const auto from = Read(call.Args.front(), "Position");
                    Match(from, "Position");
                    if (call.Text == "contains_position") return {.Type = "bool", .Code = "world.Contains(" + from.Code + ")"};
                    return {.Type = "list<Position>", .Code = "world.AdjacentPositions(" + from.Code + ")",
                        .SequenceElement = "Position", .SequenceKind = ESequenceKind::Value};
                }
                const auto entity = Read(call.Args.front());
                if (entity.Binding.empty()) Fail("position operations require checked GameEntity bindings");
                const auto view = entity.Binding + ".View()";
                if (call.Text == "position") return {.Type = "Position", .Code = "world.Position(" + view + ")"};
                const auto destination = Read(call.Args.back(), "Position");
                Match(destination, "Position");
                if (!moving) return {.Type = "bool", .Code = "world.CanOccupy(" + view + ", " + destination.Code + ")"};
                if (!action_ || !effects) Fail("move is allowed only as an effectful action expression");
                if (entity.Guard.empty()) Fail("move requires a mutable selected entity or action actor");
                std::string origin = "world.Position(" + view + ")";
                if (count == 3) {
                    const auto from = Read(call.Args[1], "Position");
                    Match(from, "Position");
                    origin = from.Code;
                }
                return {.Type = "void", .Code = "MoveActionEntity(Context, " + view + ", " + origin + ", " + destination.Code + ")", .Effect = true};
            }
            if (call.Text == "entities") {
                RequireWorldScope();
                if (call.Args.size() != 1 || call.Args.front().Kind != expr::ENodeKind::Var || call.Args.front().HasDollar) {
                    Fail("entities expects one GameEntity type name");
                }
                const auto& type = call.Args.front().Text;
                const auto found = classes_.find(type);
                if (found == classes_.end() || !found->second->GameEntity) Fail("entities expects a declared GameEntity type");
                return {.Type = "list<" + type + ">", .Code = "world.Entities<" + CppRuleType(type) + ">()",
                    .SequenceElement = type, .SequenceKind = ESequenceKind::Entity};
            }
            static const std::unordered_map<std::string, std::string> worldCalls{
                {"grid_distance", "GridDistance"}, {"same_team", "SameTeam"},
                {"has_line_of_effect", "HasLineOfEffect"}, {"position_x", "PositionX"}, {"position_y", "PositionY"}
            };
            if (const auto function = worldCalls.find(call.Text); function != worldCalls.end()) {
                RequireWorldScope();
                const bool coordinate = call.Text == "position_x" || call.Text == "position_y";
                if (call.Args.size() != (coordinate ? 1u : 2u)) Fail("wrong argument count for world query '" + call.Text + "'");
                std::string code = "world." + function->second + "(";
                for (std::size_t i = 0; i < call.Args.size(); ++i) {
                    const auto value = Read(call.Args[i]);
                    const auto type = classes_.find(value.Type);
                    if (value.Binding.empty() || type == classes_.end() || !type->second->GameEntity) {
                        Fail("world queries require checked GameEntity bindings");
                    }
                    if (i) code += ", ";
                    code += value.Binding + ".View()";
                }
                return {.Type = coordinate || call.Text == "grid_distance" ? "int" : "bool", .Code = code + ")"};
            }
            if (const auto event = events_.find(call.Text); event != events_.end()) {
                if (hook_) Fail("event dispatch is not allowed inside a feature hook");
                if (!action_) Fail("event dispatch currently requires an action context");
                const auto& declaration = *event->second.Declaration;
                if (call.Args.size() != declaration.Parameters.size() + 1) Fail("wrong argument count for event '" + call.Text + "'");
                auto owner = Read(call.Args[0], declaration.OwnerType);
                Match(owner, declaration.OwnerType);
                if (owner.Binding.empty()) Fail("event owner requires a checked entity binding");
                const auto tag = CppRuleType(EventTagName(declaration));
                std::string code = "DispatchFeatureEvent<" + tag + ">(Context, " + owner.Binding + ", " + tag + "::TArguments{";
                for (std::size_t i = 0; i < declaration.Parameters.size(); ++i) {
                    const auto& parameter = declaration.Parameters[i];
                    auto value = Read(call.Args[i + 1], parameter.TypeName);
                    Match(value, parameter.TypeName);
                    if (i != 0) code += ", ";
                    const auto cls = classes_.find(parameter.TypeName);
                    if (cls != classes_.end() && cls->second->GameEntity) {
                        if (value.Binding.empty()) Fail("entity event parameters require checked bindings");
                        code += value.Binding;
                    } else {
                        code += value.Code;
                    }
                }
                return {.Type = "list<" + declaration.ReturnType + ">", .Code = code + "})",
                    .SequenceElement = declaration.ReturnType, .SequenceKind = ESequenceKind::Contribution};
            }
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
        if (owner.Container == EContainer::None && IsBuiltinResource(owner.Type)) {
            if (member.Text == "Count" && call.Args.empty()) return {.Type = "int", .Code = "(" + owner.Code + ").Count()"};
            if ((member.Text != "Add" && member.Text != "Spend" && member.Text != "SetCount") || call.Args.size() != 1) {
                Fail("Resource supports Count(), Add(count), Spend(count), and SetCount(count)");
            }
            if (!effects) Fail("resource mutation is not allowed in a pure expression");
            if (!owner.Persistent || owner.Guard.empty() || owner.OwnerGuard.empty() || !owner.Field) {
                Fail("resource mutation requires a stable persistent field");
            }
            const auto count = Read(call.Args.front(), "int");
            Match(count, "int");
            const auto operation = member.Text == "SetCount" ? "SetResourceCount" : member.Text + "Resource";
            return {.Type = "void", .Code = "ctx." + operation + "(" + owner.Guard + ", " + count.Code +
                ", " + owner.OwnerGuard + ", " + CppStringLiteral(PascalToSnake(owner.Field->Name)) + ")", .Effect = true};
        }
        if (owner.Container == EContainer::Set) {
            if (const auto variant = variants_.find(owner.ElementType); variant != variants_.end()) {
                if (member.Text == "set" || member.Text == "erase") {
                    if (call.Args.size() != 1) Fail("variant set mutation expects one argument");
                    if (!effects) Fail("variant set mutation is not allowed in a pure expression");
                    if (!owner.Persistent || owner.Guard.empty() || owner.OwnerGuard.empty() || !owner.Field) {
                        Fail("variant set mutation requires a stable persistent field");
                    }
                    std::string value;
                    if (member.Text == "set") {
                        const auto replacement = Read(call.Args.front(), owner.ElementType);
                        Match(replacement, owner.ElementType);
                        value = replacement.Code;
                    } else {
                        const auto& key = call.Args.front();
                        if (key.Kind != expr::ENodeKind::Var || key.HasDollar) Fail("variant erase requires an alternative name");
                        value = CppRuleType(VariantKindEnum(variant->first)) + "::" + Alternative(*variant->second, key.Text).Name;
                    }
                    return {.Type = "void", .Code = std::string("ctx.") + (member.Text == "set" ? "SetVariant" : "EraseVariant") +
                        "(" + owner.Guard + ", " + value + ", " + owner.OwnerGuard + ", " +
                        CppStringLiteral(PascalToSnake(owner.Field->Name)) + ")", .Effect = true};
                }
                if (member.Text != "has" && member.Text != "get") Fail("variant sets support only 'has' and 'get'");
                if (call.Args.size() != (member.Text == "has" ? 1u : 2u)) Fail("wrong argument count for variant set lookup");
                const auto& key = call.Args.front();
                if (key.Kind != expr::ENodeKind::Var || key.HasDollar) Fail("variant lookup requires an alternative name");
                const auto& alternative = Alternative(*variant->second, key.Text);
                if (member.Text == "has") {
                    return {.Type = "bool", .Code = "(" + owner.Code + ").Has(" + CppRuleType(VariantKindEnum(variant->first)) + "::" + alternative.Name + ")"};
                }
                const auto payloadType = PayloadStructName(variant->first, alternative.Name);
                auto fallback = Read(call.Args[1], payloadType);
                Match(fallback, payloadType);
                return {.Type = payloadType, .Code = "([](const auto& rule_set, " + CppRuleType(payloadType) +
                    " fallback) { const auto* value = rule_set.template Get<" + CppRuleType(payloadType) +
                    ">(); if (value == nullptr) { return fallback; } return *value; }(" + owner.Code + ", " + fallback.Code + "))"};
            }
            if (member.Text != "has" || call.Args.size() != 1) Fail("enum sets support only 'has' with one argument");
            auto key = Read(call.Args.front(), owner.ElementType);
            Match(key, owner.ElementType);
            return {.Type = "bool", .Code = "(" + owner.Code + ").contains(" + key.Code + ")"};
        }
        if (owner.Container == EContainer::Map) {
            if (member.Text != "get") Fail("map supports only the pure 'get' lookup");
            if (call.Args.size() != 2) Fail("map.get expects a key and a fallback value");
            auto key = Read(call.Args[0], owner.KeyType);
            auto fallback = Read(call.Args[1], owner.ElementType);
            Match(key, owner.KeyType);
            Match(fallback, owner.ElementType);
            const auto value_type = CppRuleType(CppTypeFor(owner.ElementType));
            const std::string code = "([](const auto& rule_map, " + CppRuleType(CppTypeFor(owner.KeyType)) +
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
            ValidateQueryConstruction(*cls->second, *method);
            code = "(" + owner.Code + ")." + method->Name + "(" + args + ")";
        } else {
            code = "ctx.Invoke(" + owner.Guard + ", &" + CppRuleType(owner.Type) + "::" + method->Name;
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
                const auto parameter_class = classes_.find(parameter.TypeName);
                if (!IsBuiltinPrimitive(parameter.TypeName) && !enums_.contains(parameter.TypeName) &&
                    (parameter_class == classes_.end() || parameter_class->second->GameEntity)) {
                    Fail("method parameters require primitive, enum, or ordinary class value types");
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
        auto occupied = RuleCppTypes();
        const auto flowName = [&](const std::string& base) {
            auto name = base;
            std::size_t suffix = 0;
            while (!occupied.insert(name).second) name = base + "_" + std::to_string(++suffix);
            return name;
        };
        result.Flow.FlowType = flowName("TFlow");
        result.Flow.FrameType = flowName("TFrame");
        result.Flow.RunName = flowName("Run");
        std::size_t first = 0;
        while (first < declaration.Body.size() &&
               (declaration.Body[first].Kind == TStatement::EKind::Cost || declaration.Body[first].Kind == TStatement::EKind::Requires)) {
            if (declaration.Body[first].Kind == TStatement::EKind::Cost) {
                result.Costs.push_back(ActionCost(declaration.Body[first]));
            }
            ++first;
        }
        result.Flow.Environment.push_back("[[maybe_unused]] const auto world = GeneratedActionWorld(Context);");
        result.Flow.Environment.push_back("[[maybe_unused]] auto action_self = ActionSelf<" + CppRuleType(self_.Name) + ">(Context);");
        result.Flow.Environment.push_back("[[maybe_unused]] auto ctx = Context->transformator->ExecutionContext();");
        if (!declaration.SourceType.empty()) {
            const auto source = classes_.find(declaration.SourceType);
            if (source == classes_.end() || source->second->GameEntity) Fail("action source requires an ordinary class type");
            const auto binding = "source_" + declaration.SourceName;
            result.Flow.Environment.push_back("[[maybe_unused]] auto " + binding + " = ActionSource<" + CppRuleType(self_.Name) + ", " + CppRuleType(declaration.SourceType) + ">(Context);");
            locals_.emplace(declaration.SourceName, TValue{.Type = declaration.SourceType, .Code = binding + ".Read()",
                .LocalCode = binding, .SourceBinding = binding});
        }
        result.Requirement = CompileActionRequirement(declaration, first);
        result.Flow.Blocks.resize(2);
        ActionFlowStatements(declaration.Body, first, result.Flow, 0, 1);
        result.Body = RenderActionFlow(result.Flow);
        method_ = nullptr;
        return result;
    }

    std::vector<std::string> CompileHookBody(const TAbilityDecl& feature, const TAbilityDecl::THook& hook) {
        hook_ = true;
        const auto found = events_.find(hook.EventName);
        if (found == events_.end()) Fail("unknown hook event '" + hook.EventName + "'");
        const auto& event = *found->second.Declaration;
        TMethod method;
        method.Name = feature.Name + ".on." + event.Name;
        method.ReturnType = event.ReturnType;
        method.Query = true;
        method_ = &method;
        std::vector<std::string> body;
        for (std::size_t i = 0; i < event.Parameters.size(); ++i) {
            const auto& parameter = event.Parameters[i];
            const auto name = "arg_" + parameter.Name;
            body.push_back("[[maybe_unused]] const auto& " + name + " = std::get<" + std::to_string(i) + ">(arguments);");
            const auto cls = classes_.find(parameter.TypeName);
            TValue value;
            if (cls != classes_.end() && cls->second->GameEntity) {
                value = EntityBinding(parameter.TypeName, name, false);
            } else {
                value.Type = parameter.TypeName;
                value.Code = name;
            }
            if (!locals_.emplace(parameter.Name, std::move(value)).second) Fail("duplicate event parameter '" + parameter.Name + "'");
        }
        if (!Statements(hook.Body, body, "")) Fail("feature hook must return a value on every path");
        method_ = nullptr;
        return body;
    }

private:
    const std::unordered_set<std::string>& RuleCppTypes() const {
        if (ruleCppTypes_.empty()) {
            for (const auto& [name, declaration] : classes_) ruleCppTypes_.insert(name);
            for (const auto& [name, declaration] : enums_) ruleCppTypes_.insert(name);
            for (const auto& [name, declaration] : variants_) {
                ruleCppTypes_.insert(name);
                ruleCppTypes_.insert(VariantKindEnum(name));
            }
            for (const auto& [name, declaration] : payloads_) ruleCppTypes_.insert(name);
            for (const auto& [name, declaration] : events_) ruleCppTypes_.insert(EventTagName(*declaration.Declaration));
        }
        return ruleCppTypes_;
    }

    std::string CppRuleType(const std::string& type) const {
        if (!action_ && !preflight_) return type;
        const auto& names = RuleCppTypes();
        std::string result;
        const auto identifier = [](char value) {
            return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                (value >= '0' && value <= '9') || value == '_';
        };
        for (std::size_t index = 0; index < type.size();) {
            if (!identifier(type[index])) {
                result += type[index++];
                continue;
            }
            const auto first = index;
            while (index < type.size() && identifier(type[index])) ++index;
            const auto token = type.substr(first, index - first);
            if (names.contains(token) && (first < 2 || type.substr(first - 2, 2) != "::")) result += "::";
            result += token;
        }
        return result;
    }

    void RequireWorldScope() const {
        if (!action_ && !hook_ && !preflight_) Fail("world queries require an action, feature hook, or action precondition");
    }

    std::vector<std::string> CompileActionRequirement(const TAbilityDecl& declaration, std::size_t first) {
        const bool leadingChoice = first < declaration.Body.size() && declaration.Body[first].Kind == TStatement::EKind::Choose;
        bool hasRequirements = false;
        for (std::size_t i = 0; i < first; ++i) {
            hasRequirements = hasRequirements || declaration.Body[i].Kind == TStatement::EKind::Requires;
        }
        if (!leadingChoice && !hasRequirements && declaration.SourceType.empty()) return {};
        TCompiler compiler(self_, loaded_);
        TMethod method;
        method.Name = declaration.Name + ".requires";
        method.ReturnType = "bool";
        method.Query = true;
        compiler.method_ = &method;
        compiler.preflight_ = true;
        std::vector<std::string> result;
        result.push_back("[[maybe_unused]] const auto action_self = world.Bind<" + compiler.CppRuleType(self_.Name) + ">(actor);");
        if (!declaration.SourceType.empty()) {
            result.push_back("const auto source_value = FindAbilitySource<" + compiler.CppRuleType(declaration.SourceType) + ">(action_self.Read(), source);");
            result.push_back("if (!source_value) { return false; }");
            compiler.locals_.emplace(declaration.SourceName, TValue{.Type = declaration.SourceType, .Code = "(*source_value->Value)"});
        }
        for (std::size_t i = 0; i < first; ++i) {
            if (declaration.Body[i].Kind != TStatement::EKind::Requires) continue;
            auto value = compiler.Read(*declaration.Body[i].Value);
            compiler.Match(value, "bool");
            result.push_back("if (!(" + value.Code + ")) { return false; }");
        }
        if (leadingChoice) {
            const auto& choice = declaration.Body[first];
            const bool entity = compiler.CheckChoiceType(choice);
            const auto candidates = compiler.ChoiceCandidates(choice, entity);
            auto local = entity ? compiler.EntityBinding(choice.TypeName, "candidate", false) :
                TValue{.Type = choice.TypeName, .Code = "candidate", .LocalCode = "candidate"};
            if (!compiler.locals_.emplace(choice.Name, std::move(local)).second) {
                Fail("duplicate local '" + choice.Name + "'");
            }
            std::string predicate = "true";
            if (choice.Value) {
                auto value = compiler.Read(*choice.Value);
                compiler.Match(value, "bool");
                predicate = value.Code;
            }
            result.push_back("for ([[maybe_unused]] const auto& candidate : " + candidates.Code + ") {");
            result.push_back("    if (" + predicate + ") { return true; }");
            result.push_back("}");
            result.push_back("return false;");
        } else {
            result.push_back("return true;");
        }
        return result;
    }

    void ValidateQueryConstruction(const TClassDecl& owner, const TMethod& method) {
        CheckConstruction("query " + owner.Name + "." + method.Name, [&] {
            TCompiler compiler(owner, loaded_, construction_);
            compiler.method_ = &method;
            for (const auto& parameter : method.Parameters) {
                if (!compiler.locals_.emplace(parameter.Name, TValue{.Type = parameter.TypeName, .Code = "arg_" + parameter.Name}).second) {
                    Fail("duplicate parameter '" + parameter.Name + "'");
                }
            }
            std::vector<std::string> output;
            compiler.Statements(method.Body, output, "");
        });
    }

    void CheckConstruction(const std::string& step, const std::function<void()>& validate) {
        if (std::find(construction_->begin(), construction_->end(), step) != construction_->end()) {
            std::string path;
            for (const auto& value : *construction_) path += value + " -> ";
            Fail("cyclic named value construction: " + path + step);
        }
        construction_->push_back(step);
        try {
            validate();
        } catch (...) {
            construction_->pop_back();
            throw;
        }
        construction_->pop_back();
    }

    TValue ReadNamedValue(const TInstanceDecl& instance) {
        const auto cls = classes_.find(instance.TypeName);
        if (cls == classes_.end()) Fail("named values require a declared class type");
        if (cls->second->GameEntity) Fail("named GameEntity factories cannot be used as value expressions");
        CheckConstruction("value " + instance.Name, [&] {
            TClassDecl scope;
            scope.Name = "value " + instance.Name;
            TCompiler compiler(scope, loaded_, construction_);
            auto value = compiler.Read(instance.Value, instance.TypeName);
            compiler.Match(value, instance.TypeName);
        });
        return {.Type = instance.TypeName, .Code = "Create" + instance.Name + "()"};
    }

    void ValidateConstructionDefaults(const TClassDecl& declaration,
                                      const std::unordered_map<std::string, std::string>& supplied) {
        for (const auto& field : declaration.Fields) {
            if (field.Derived || supplied.contains(field.Name)) continue;
            const bool nested = field.Container == EContainer::None && classes_.contains(field.TypeName);
            if (!field.Init && !nested) continue;
            CheckConstruction("default " + declaration.Name + "." + field.Name, [&] {
                TCompiler compiler(declaration, loaded_, construction_);
                if (field.Init) {
                    compiler.ReadInitializer(*field.Init, field);
                } else {
                    expr::TExprNode value;
                    value.Kind = expr::ENodeKind::ObjectInit;
                    value.Text = field.TypeName;
                    compiler.Read(value, field.TypeName);
                }
            });
        }
    }

    const TVariantAlt& Alternative(const TVariantDecl& variant, const std::string& name) const {
        for (const auto& alternative : variant.Alternatives) if (alternative.Name == name) return alternative;
        Fail("unknown alternative '" + variant.Name + "." + name + "'");
    }

    std::string ReadPayload(const expr::TExprNode& expression, const TVariantAlt& alternative, const std::string& type) {
        std::unordered_map<std::string, std::string> values;
        for (const auto& initializer : expression.Fields) {
            const TFieldDecl* field = nullptr;
            for (const auto& candidate : alternative.Fields) if (candidate.Name == initializer.Name) field = &candidate;
            if (!field) Fail("unknown variant initializer field '" + initializer.Name + "'");
            if (!values.emplace(initializer.Name, ReadInitializer(initializer.Value, *field).Code).second) {
                Fail("duplicate variant initializer field '" + initializer.Name + "'");
            }
        }
        std::string code = CppRuleType(type) + "{";
        bool first = true;
        for (const auto& field : alternative.Fields) {
            if (const auto value = values.find(field.Name); value != values.end()) {
                if (!first) code += ", ";
                first = false;
                code += "." + field.Name + " = " + value->second;
            }
        }
        return code + "}";
    }

    TValue ReadVariant(const expr::TExprNode& expression, const TVariantDecl& variant, const TVariantAlt& alternative) {
        return {.Type = variant.Name,
            .Code = CppRuleType(variant.Name) + "(" + ReadPayload(expression, alternative, PayloadStructName(variant.Name, alternative.Name)) + ")",
            .VariantAlternative = alternative.Name};
    }

    void ValidateActionStep(std::vector<std::string>& output, const std::string& indent) const {
        output.push_back(indent + "ValidateActionActor(Context);");
        output.push_back(indent + "action_self.View().Validate();");
        std::set<std::string> bindings;
        for (const auto& [name, value] : locals_) {
            if (!value.Binding.empty()) bindings.insert(value.Binding);
        }
        for (const auto& binding : bindings) output.push_back(indent + binding + ".View().Validate();");
        std::set<std::string> sources;
        for (const auto& [name, value] : locals_) {
            if (!value.SourceBinding.empty()) sources.insert(value.SourceBinding);
        }
        for (const auto& source : sources) output.push_back(indent + source + ".Validate();");
    }

    TValue EntityBinding(const std::string& type, const std::string& name, bool writable = true) const {
        return {.Type = type, .Code = name + ".Read()", .Persistent = true,
            .Guard = writable ? name + ".Guard()" : "", .Binding = name, .LocalCode = name};
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

    bool CheckChoiceType(const TStatement& statement) const {
        const auto type = classes_.find(statement.TypeName);
        const bool entity = type != classes_.end() && type->second->GameEntity;
        if (!entity && !IsBuiltinPrimitive(statement.TypeName) && !enums_.contains(statement.TypeName) && statement.TypeName != "Position") {
            Fail("choose requires a GameEntity, primitive, enum, or Position type");
        }
        if (!entity && !statement.Candidates) Fail("value choose requires a from sequence");
        return entity;
    }

    TValue ChoiceCandidates(const TStatement& statement, bool entity) {
        if (!statement.Candidates) {
            return {.Type = "list<" + statement.TypeName + ">",
                .Code = (preflight_ ? "world.Entities<" : "ActionEntities<") + CppRuleType(statement.TypeName) + (preflight_ ? ">()" : ">(Context)"),
                .SequenceElement = statement.TypeName, .SequenceKind = ESequenceKind::Entity};
        }
        auto candidates = Read(*statement.Candidates, "list<" + statement.TypeName + ">");
        if (candidates.SequenceElement != statement.TypeName ||
            (entity ? candidates.SequenceKind != ESequenceKind::Entity : candidates.SequenceKind != ESequenceKind::Value)) {
            Fail("choice from requires a sequence of '" + statement.TypeName + "' values");
        }
        return candidates;
    }

    std::string FrameCppType(const TValue& value) const {
        if (!value.Binding.empty()) return CppRuleType("TGeneratedEntityBinding<" + value.Type + ">");
        if (!value.SequenceElement.empty()) {
            auto element = CppTypeFor(value.SequenceElement);
            if (value.SequenceKind == ESequenceKind::Entity) element = "TGeneratedEntityBinding<" + element + ">";
            if (value.SequenceKind == ESequenceKind::Contribution) element = "TFeatureEventContribution<" + element + ">";
            return CppRuleType("std::vector<" + element + ">");
        }
        return CppRuleType(CppTypeFor(value.Type));
    }

    std::string NewLocalName(const std::string& name, const std::string& prefix = "local_") {
        const auto base = prefix + name;
        if (!action_) return base;
        auto local = base;
        std::size_t suffix = 0;
        while (!generatedLocals_.insert(local).second) local = base + "_" + std::to_string(++suffix);
        return local;
    }

    std::size_t AddFrameLocal(TCompiledActionFlow& flow, const std::string& name, const TValue& value,
                             const std::string& prefix = "local_") {
        if (locals_.contains(name)) Fail("duplicate local '" + name + "'");
        const auto slot = flow.Slots.size();
        const auto local = NewLocalName(name, prefix);
        flow.Slots.push_back({FrameCppType(value), local});
        frameSlots_.emplace(local, slot);
        if (!value.Binding.empty()) {
            locals_[name] = EntityBinding(value.Type, local, !value.Guard.empty());
        } else {
            locals_[name] = {.Type = value.Type, .Code = local,
                .Container = value.Container, .KeyType = value.KeyType, .ElementType = value.ElementType,
                .LocalCode = local, .SequenceElement = value.SequenceElement,
                .SequenceKind = value.SequenceKind};
        }
        return slot;
    }

    void SetFlowScope(TActionFlowBlock& block) const {
        for (const auto& [name, value] : locals_) {
            if (const auto slot = frameSlots_.find(value.LocalCode); slot != frameSlots_.end()) block.LiveSlots.push_back(slot->second);
        }
        std::sort(block.LiveSlots.begin(), block.LiveSlots.end());
    }

    std::size_t NewFlowBlock(TCompiledActionFlow& flow) const {
        const auto result = flow.Blocks.size();
        flow.Blocks.emplace_back();
        return result;
    }

    bool ActionFlowStatements(const std::vector<TStatement>& body, std::size_t first,
                              TCompiledActionFlow& flow, std::size_t entry, std::size_t exit) {
        using K = TStatement::EKind;
        using E = TActionFlowBlock::EExit;
        const auto outer = locals_;
        auto current = entry;
        bool returns = false;
        for (std::size_t index = first; index < body.size(); ++index) {
            if (returns) Fail("unreachable statement after return");
            const auto& statement = body[index];
            const auto next = NewFlowBlock(flow);
            SetFlowScope(flow.Blocks[current]);
            flow.Blocks[current].Next = next;
            flow.Blocks[current].Exit = E::Jump;
            if (statement.Kind == K::Let) {
                ValidateActionStep(flow.Blocks[current].Body, "");
                auto value = Read(*statement.Value, "", true);
                if (value.Type == "void") Fail("cannot bind void result");
                const auto slot = AddFrameLocal(flow, statement.Name, value);
                flow.Blocks[current].Body.push_back("frame.slot_" + std::to_string(slot) + ".emplace(" +
                    (value.Binding.empty() ? value.Code : value.Binding) + ");");
            } else if (statement.Kind == K::Choose) {
                ValidateActionStep(flow.Blocks[current].Body, "");
                const bool entity = CheckChoiceType(statement);
                const auto candidates = ChoiceCandidates(statement, entity);
                auto value = entity ? EntityBinding(statement.TypeName, "selected") : TValue{.Type = statement.TypeName};
                const auto slot = AddFrameLocal(flow, statement.Name, value, entity ? "bound_" : "local_");
                const auto& local = locals_.at(statement.Name);
                auto& choice = flow.Blocks[current].Choice;
                choice = {.Type = CppRuleType(entity ? statement.TypeName : CppTypeFor(statement.TypeName)),
                    .CppType = flow.Slots[slot].CppType, .Local = local.LocalCode,
                    .Candidates = candidates.Code, .Slot = slot, .Entity = entity};
                ValidateActionStep(choice.Predicate, "");
                std::string predicate = "true";
                if (statement.Value) {
                    const auto condition = Read(*statement.Value);
                    Match(condition, "bool");
                    predicate = condition.Code;
                }
                choice.Predicate.push_back("return " + predicate + ";");
                flow.Blocks[current].Exit = E::Choose;
            } else if (statement.Kind == K::If || statement.Kind == K::While) {
                ValidateActionStep(flow.Blocks[current].Body, "");
                const auto condition = Read(*statement.Value);
                Match(condition, "bool");
                const auto yes = NewFlowBlock(flow);
                flow.Blocks[current].Condition = condition.Code;
                flow.Blocks[current].Exit = E::Branch;
                flow.Blocks[current].Next = yes;
                if (statement.Kind == K::While) {
                    flow.Blocks[current].Otherwise = next;
                    ActionFlowStatements(statement.Body, 0, flow, yes, current);
                } else {
                    const auto no = NewFlowBlock(flow);
                    flow.Blocks[current].Otherwise = no;
                    const bool yesReturns = ActionFlowStatements(statement.Body, 0, flow, yes, next);
                    const bool noReturns = ActionFlowStatements(statement.ElseBody, 0, flow, no, next);
                    returns = yesReturns && noReturns;
                }
            } else if (statement.Kind == K::Return) {
                ValidateActionStep(flow.Blocks[current].Body, "");
                if (statement.Value) {
                    const auto value = Read(*statement.Value, "void", true);
                    Match(value, "void");
                    flow.Blocks[current].Body.push_back("(void)" + value.Code + ";");
                }
                flow.Blocks[current].Exit = E::Return;
                returns = true;
            } else {
                Statements({statement}, flow.Blocks[current].Body, "");
            }
            current = next;
        }
        if (!returns) {
            for (const auto& [name, value] : locals_) {
                if (outer.contains(name)) continue;
                if (const auto slot = frameSlots_.find(value.LocalCode); slot != frameSlots_.end()) {
                    flow.Blocks[current].Body.push_back("frame.slot_" + std::to_string(slot->second) + ".reset();");
                }
            }
            std::sort(flow.Blocks[current].Body.begin(), flow.Blocks[current].Body.end());
            flow.Blocks[current].Exit = E::Jump;
            flow.Blocks[current].Next = exit;
        }
        locals_ = outer;
        return returns;
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
                auto nested_scope = scope;
                if (statement.Kind == TStatement::EKind::ForEach) nested_scope.insert(statement.Name);
                statements(statement.Body, std::move(nested_scope));
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
        const std::unordered_set<std::string> reserved{"Create", "FromJson", "Restore", "ToJson", "GetAst", "RegisterDslProperties", "TInitialValues", "TUninitialized", "VisitGrantedAbilities", "VisitAbilitySources"};
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
                    const auto local = NewLocalName(statement.Name);
                    if ((action_ || hook_) && !value.Binding.empty()) {
                        line("[[maybe_unused]] auto " + local + " = " + value.Binding + ";");
                        locals_[statement.Name] = EntityBinding(value.Type, local, !value.Guard.empty());
                        break;
                    }
                    line("[[maybe_unused]] auto " + local + " = " + value.Code + ";");
                    locals_[statement.Name] = {.Type = value.Type, .Code = local,
                        .Container = value.Container, .KeyType = value.KeyType, .ElementType = value.ElementType,
                        .LocalCode = local, .SequenceElement = value.SequenceElement,
                        .SequenceKind = value.SequenceKind};
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
                    if (value.SequenceKind != target.SequenceKind) Fail("assignment requires matching sequence kinds");
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
                case K::Choose: Fail("choose requires resumable action control flow and cannot appear in a synchronous scope");
                case K::While: Fail("while requires resumable action control flow and cannot appear in a synchronous scope");
                case K::Cost: Fail("cost declarations must precede all statements in an action body");
                case K::Requires: Fail("requires declarations must be in the initial action prefix");
                case K::ForEach: {
                    if (locals_.contains(statement.Name)) Fail("duplicate local '" + statement.Name + "'");
                    auto sequence = Read(*statement.Value);
                    const bool collection = sequence.Container == EContainer::Collection;
                    if (sequence.SequenceElement.empty() && !collection) Fail("for requires an event contribution sequence, entity sequence, or read-only collection");
                    const auto elementType = collection ? sequence.ElementType : sequence.SequenceElement;
                    const auto number = std::to_string(loop_++);
                    const auto snapshot = "event_values_" + number;
                    const auto entry = "event_value_" + number;
                    const auto local = NewLocalName(statement.Name);
                    auto saved = locals_;
                    const bool borrowed = collection && method_->Query && !action_ && !hook_ && !preflight_ &&
                        sequence.Storage.starts_with("self.") && sequence.Field && !sequence.Field->Derived;
                    line("{");
                    line("    const auto" + std::string(borrowed ? "& " : " ") + snapshot + " = " + sequence.Code + ";");
                    line("    for (const auto& " + entry + " : " + snapshot + ") {");
                    const bool entities = sequence.SequenceKind == ESequenceKind::Entity;
                    line("        [[maybe_unused]] auto " + local + " = " + (collection ? "*" + entry : sequence.SequenceKind == ESequenceKind::Contribution ? entry + ".Value" : entry) + ";");
                    locals_[statement.Name] = entities ? EntityBinding(elementType, local, false) :
                        TValue{.Type = elementType, .Code = local, .LocalCode = local};
                    Statements(statement.Body, output, indent + "        ");
                    locals_ = std::move(saved);
                    line("    }");
                    line("}");
                    break;
                }
            }
        }
        return returns;
    }

    const TClassDecl& self_;
    const TLoadedSchemas& loaded_;
    TClasses classes_;
    TEnums enums_;
    TVariants variants_;
    std::unordered_map<std::string, const TVariantAlt*> payloads_;
    std::unordered_map<std::string, TEventInfo> events_;
    TInstances instances_;
    std::shared_ptr<std::vector<std::string>> construction_;
    std::unordered_map<std::string, TValue> locals_;
    std::unordered_set<std::string> dependencies_;
    const TMethod* method_ = nullptr;
    bool action_ = false;
    bool hook_ = false;
    bool preflight_ = false;
    std::size_t loop_ = 0;
    std::unordered_map<std::string, std::size_t> frameSlots_;
    std::unordered_set<std::string> generatedLocals_;
    mutable std::unordered_set<std::string> ruleCppTypes_;
};

}

bool UsesRuleSyntax(const TClassDecl& declaration, const TLoadedSchemas& loaded) {
    const auto classes = ClassTable(loaded);
    const auto instances = InstanceTable(loaded);
    std::unordered_set<std::string> visiting;
    std::function<bool(const TClassDecl&)> inspect = [&](const TClassDecl& c) {
        if (!visiting.insert(c.Name).second) return false;
        if (c.GameEntity || !c.Methods.empty() || !c.Grants.empty()) return true;
        for (const auto& field : c.Fields) {
            if (IsBuiltinFeature(field.TypeName)) return true;
            if (field.Init && NewExpression(*field.Init)) return true;
            if (field.Init && ContainsNamedValue(*field.Init, instances)) return true;
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

std::vector<std::string> CompileFeatureHook(const TAbilityDecl& feature, const TAbilityDecl::THook& hook,
                                          const TLoadedSchemas& loaded) {
    const auto events = EventSymbols(loaded);
    const auto event = events.find(hook.EventName);
    if (event == events.end()) throw std::runtime_error("unknown hook event '" + hook.EventName + "'");
    const auto classes = ClassTable(loaded);
    const auto owner = classes.find(event->second.Declaration->OwnerType);
    if (owner == classes.end() || !owner->second->GameEntity) throw std::runtime_error("event owner must be a GameEntity");
    return TCompiler(*owner->second, loaded).CompileHookBody(feature, hook);
}
