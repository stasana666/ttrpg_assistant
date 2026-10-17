#include <ttrpg/emit.h>
#include <ttrpg/emit_rule_class.h>
#include <ttrpg/emit_action.h>
#include <ttrpg/emit_activity.h>
#include <ttrpg/activities.h>
#include <ttrpg/abilities.h>
#include <ttrpg/events.h>
#include <ttrpg/emit_source.h>
#include <ttrpg/emit_instance.h>
#include <ttrpg/emit_reaction.h>
#include <ttrpg/triggers.h>
#include <ttrpg/records.h>
#include <ttrpg/relations.h>
#include <ttrpg/emit_relation.h>
#include <ttrpg/emit_record.h>

#include <ttrpg/conventions.h>
#include <ttrpg/cpp_writer.h>

#include <algorithm>
#include <functional>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::vector<const TClassDecl*> OrderedClasses(const TLoadedSchemas& loaded) {
    std::unordered_map<std::string, const TClassDecl*> classes;
    for (const auto& c : loaded.Primary.Classes) classes[c.Name] = &c;
    std::unordered_map<std::string, int> marks;
    std::vector<const TClassDecl*> result;
    std::function<void(const TClassDecl&)> visit = [&](const TClassDecl& c) {
        if (marks[c.Name] == 2) return;
        if (marks[c.Name] == 1) throw std::runtime_error("recursive value type '" + c.Name + "'");
        marks[c.Name] = 1;
        for (const auto& field : c.Fields) {
            const auto type = field.Container == EContainer::Map ? field.ValueTypeName : field.TypeName;
            auto it = classes.find(type);
            if (it != classes.end()) visit(*it->second);
        }
        marks[c.Name] = 2;
        result.push_back(&c);
    };
    for (const auto& c : loaded.Primary.Classes) visit(c);
    return result;
}

void ValidateNames(const TLoadedSchemas& loaded) {
    std::map<std::string, std::string> owners;
    const auto add = [&](const std::string& name, const std::string& kind) {
        const auto [existing, inserted] = owners.emplace(name, kind);
        if (inserted) return;
        if (existing->second == kind) throw std::runtime_error("duplicate " + kind + " '" + name + "'");
        throw std::runtime_error(kind + " '" + name + "' conflicts with existing symbol (" + existing->second + ")");
    };
    for (const auto* name : {
             "void", "int", "bool", "string", "Resource", "BoundedQuantity", "Feature", "Position", "Duration",
             "roll", "min", "max", "entities", "participants", "grid_distance", "same_team", "has_line_of_effect", "sources",
             "position_x", "position_y", "position", "adjacent_positions", "map_positions", "contains_position",
             "can_occupy", "move",
             "move_before_current_turn", "same_source", "seconds", "minutes",
             "TGeneratedConstruction", "TConstructionReport", "TConstructionIssue", "TFeatureChoiceDescriptor",
             "EConstructionChoiceState", "TReactionScope", "TReactionWindowResult", "TReactionExecution",
             "TActionTicket", "TActionRecordStamp", "TActionRecordRegistry", "TActionHistory", "TRecordedSource",
             "TRecordedEntity", "TGameDuration", "TGameClock", "TGameTimeRules", "TRelationRegistry",
             "TTimedRelationStore", "GameSeconds", "GameMinutes", "HasRelation", "GetRelation", "SetRelation",
             "EraseRelation"}) {
        add(name, "reserved builtin");
    }
    const auto modules = AllModules(loaded);
    for (const auto& [stem, module] : modules) {
        for (const auto& value : module->Classes) add(value.Name, "type");
        for (const auto& value : module->Enums) add(value.Name, "type");
        for (const auto& value : module->Variants) add(value.Name, "type");
    }
    for (const auto& [stem, module] : modules) {
        add(AbilityRegistrationFunction(stem), "generated name");
        add(RulesRegistrationFunction(stem), "generated name");
        for (const auto& value : module->Enums) add(value.Name + "FromString", "generated name");
        for (const auto& value : module->Variants) {
            add(VariantKindEnum(value.Name), "generated name");
            for (const auto& alternative : value.Alternatives) add(PayloadStructName(value.Name, alternative.Name), "generated name");
        }
        for (const auto& value : module->Abilities) {
            add(value.Name, "ability definition");
            if (!value.Triggers.empty()) add(ReactionFunctionName(stem, value), "generated name");
        }
        for (const auto& value : module->Events) { add(value.Name, "event"); add(EventTagName(value), "generated name"); }
        for (const auto& value : module->Triggers) { add(value.Name, "trigger"); add(TriggerTagName(value), "generated name"); }
        for (const auto& value : module->Records) { add(value.Name, "record"); add(RecordTagName(value), "generated name"); }
        for (const auto& value : module->Relations) { add(value.Name, "relation"); add(RelationTagName(value), "generated name"); }
        for (const auto& value : module->Activities) { add(value.Name, "activity"); add(ActivityFunctionName(stem, value), "generated name"); }
        for (const auto& value : module->Instances) {
            add(value.Name, "named value");
            add("Create" + value.Name, "generated name");
            if (!value.Parameterized) continue;
            for (const auto& name : {"T" + value.Name + "Options", "TResolved" + value.Name, "Resolve" + value.Name,
                                     "Describe" + value.Name, "Validate" + value.Name}) {
                add(name, "generated name");
            }
        }
    }
}

void ValidateRules(const TLoadedSchemas& loaded) {
    ValidateNames(loaded);
    ValidateAbilityDefinitions(loaded);
    ValidateRecordDefinitions(loaded);
    ValidateRelationDefinitions(loaded);
    ValidateEventDefinitions(loaded);
    ValidateTriggerDefinitions(loaded);
    ValidateActivityDefinitions(loaded);
    ValidateConstructorNames(loaded);
    (void)OrderedClasses(loaded);
    for (const auto& c : loaded.Primary.Classes) {
        (void)CompileClass(c, loaded);
    }
    for (const auto& ability : loaded.Primary.Abilities) {
        if (!ability.OwnerType.empty()) {
            if (ability.Kind == EAbilityDeclarationKind::Reaction) (void)CompileReaction(ability, loaded);
            else (void)CompileAction(ability, loaded);
        }
        for (const auto& hook : ability.Hooks) (void)CompileFeatureHook(ability, hook, loaded);
    }
    for (const auto& activity : loaded.Primary.Activities) (void)CompileActivity(activity, loaded);
    for (const auto& instance : loaded.Primary.Instances) {
        if (instance.Parameterized) (void)CompileConstructor(instance, loaded);
        else (void)CompileInstance(instance, loaded);
    }
}

}

void EmitEnumDecl(TCppWriter& w, const TEnumDecl& e) {
    w.Block("enum class " + e.Name + " {", "};", [&] {
        for (const auto& v : e.Values) {
            w.Line(v + ",");
        }
    });
    w.EmptyLine();
    w.Line("std::string ToString(" + e.Name + " v);");
    w.Line(e.Name + " " + e.Name + "FromString(const std::string& s);");
    w.EmptyLine();
}

TFieldDecl MapValueField(const TFieldDecl& f) {
    TFieldDecl value = f;
    value.TypeName = f.ValueTypeName;
    value.ValueTypeName.clear();
    value.Container = EContainer::None;
    return value;
}

void EmitVariantDecl(TCppWriter& w, const TVariantDecl& v,
                     const std::unordered_map<std::string, TTypeInfo>& symbols) {
    const std::string kindEnum = VariantKindEnum(v.Name);

    TEnumDecl kindDecl;
    kindDecl.Name = kindEnum;
    for (const auto& alt : v.Alternatives) {
        kindDecl.Values.push_back(alt.Name);
    }
    EmitEnumDecl(w, kindDecl);

    for (const auto& alt : v.Alternatives) {
        w.Struct(PayloadStructName(v.Name, alt.Name), [&] {
            w.Line("static constexpr auto Kind = " + kindEnum + "::" + alt.Name + ";");
            for (const auto& f : alt.Fields) {
                if (f.Derived) {
                    throw std::runtime_error(
                        "'derive' is not supported in variant alternative '" + alt.Name +
                        "' (field '" + f.Name + "')");
                }
                std::string mt = CppMemberType(f, symbols);
                std::string init = "{}";
                if (f.Init) {
                    if (f.Init->Kind != expr::ENodeKind::IntLiteral &&
                        f.Init->Kind != expr::ENodeKind::Var) {
                        throw std::runtime_error(
                            "computed initializers are not supported in variant alternative '" +
                            alt.Name + "' (field '" + f.Name + "')");
                    }
                    init = "{" + DefaultExprToCpp(f.Init->Text, f.TypeName) + "}";
                }
                w.Line(mt + " " + f.Name + init + ";");
            }
        });
        w.EmptyLine();
    }

    w.Class(v.Name, [&] {
        w.PublicSection([&] {
            w.Line("using TPayload = std::variant<");
            w.Indented([&] {
                for (size_t i = 0; i < v.Alternatives.size(); ++i) {
                    bool last = (i + 1 == v.Alternatives.size());
                    w.Line(PayloadStructName(v.Name, v.Alternatives[i].Name) +
                           (last ? ">;" : ","));
                }
            });
            w.EmptyLine();
            w.Line(v.Name + "() = default;");
            w.Line("template <class T>");
            w.Indented([&] {
                w.Line("requires (!std::is_same_v<std::decay_t<T>, " + v.Name + ">)");
            });
            w.Line(v.Name + "(T alt) : Payload_(std::move(alt)) {}");
            w.EmptyLine();
            w.Function(kindEnum + " Kind() const", [&] {
                w.Line("return std::visit(");
                w.Indented([&] {
                    w.Line("[](const auto& a) { return std::decay_t<decltype(a)>::Kind; }, Payload_);");
                });
            });
            w.Line("const TPayload& Payload() const { return Payload_; }");
            w.EmptyLine();
            w.Line("template <class T> const T* TryGet() const { return std::get_if<T>(&Payload_); }");
            w.EmptyLine();
            w.Line("TAstNode GetAst(TAstContext& ctx) const;");
        });
        w.EmptyLine();
        w.PrivateSection([&] {
            w.Line("TPayload Payload_;");
        });
    });
    w.EmptyLine();
    w.Line("template <>");
    w.Line("struct TIsAstRecursive<" + v.Name + "> : std::true_type {};");
    w.EmptyLine();
}

void EmitHeader(std::ostream& os,
                const TLoadedSchemas& loaded,
                const std::string& sourceName,
                const std::string& primaryOutH)
{
    ValidateRules(loaded);
    TCppWriter w(os);
    const TSchemaModule& mod = loaded.Primary;

    w.Comment("AUTO-GENERATED FROM " + sourceName + " -- DO NOT EDIT.");
    w.Comment("Source of truth: pf2e_engine/data/schemas/" + sourceName);
    w.Line("#pragma once");
    w.EmptyLine();
    w.Include("pf2e_engine/common/ast/ast_constructable.h");
    w.Include("pf2e_engine/common/ability.h");
    if (!mod.Classes.empty()) {
        w.Include("pf2e_engine/common/guarded.h");
    }
    w.EmptyLine();
    w.Include("limits");
    w.Include("string");
    if (!mod.Records.empty()) {
        w.Include("pf2e_engine/actions/generated_history.h");
        w.Include("tuple");
        w.Include("string_view");
    }
    if (!mod.Relations.empty()) {
        w.Include("pf2e_engine/actions/generated_relation.h");
        w.Include("string_view");
    }
    if (!mod.Activities.empty()) {
        w.Include("functional");
        w.Include("memory");
        w.Include("optional");
        w.Include("utility");
        w.Include("pf2e_engine/position.h");
    }
    if (!mod.Events.empty()) {
        w.Include("functional");
        w.Include("tuple");
        w.Include("string_view");
    }
    const bool reactions = std::any_of(mod.Abilities.begin(), mod.Abilities.end(), [](const auto& ability) { return !ability.Triggers.empty(); });
    if (!mod.Triggers.empty() || reactions) {
        w.Include("functional");
        w.Include("memory");
        w.Include("tuple");
        w.Include("string_view");
        w.Include("pf2e_engine/position.h");
    }
    if (!mod.Classes.empty()) {
        w.Include("utility");
        w.Include("optional");
    }

    bool anyEnumSet = false;
    bool anyVariantSet = false;
    bool anyVariant = !mod.Variants.empty();
    bool anyBoundedQuantity = false;
    bool anyResource = false;
    bool anyCollection = false;
    bool anyMap = false;

    std::set<std::string> externalStems;
    for (const auto& imported : ImportedModuleStems(loaded, loaded.PrimaryStem)) {
        externalStems.insert(imported);
    }

    auto inspectField = [&](const TFieldDecl& f, const std::string& ownerDesc) {
        if (f.Container == EContainer::Map) {
            auto keyIt = loaded.SymbolTable.find(f.TypeName);
            if (keyIt == loaded.SymbolTable.end()) {
                throw std::runtime_error(
                    "unknown map key type '" + f.TypeName + "' referenced in " + ownerDesc +
                    " (declare it in this file or import another .ttrpg)");
            }
            if (keyIt->second.Kind != ETypeKind::Enum) {
                throw std::runtime_error(
                    "map<K, V> key type '" + f.TypeName +
                    "' must be a schema-declared enum");
            }
            if (keyIt->second.OwnerStem != loaded.PrimaryStem) {
                externalStems.insert(keyIt->second.OwnerStem);
            }
            anyMap = true;
            TFieldDecl value = MapValueField(f);
            if (IsBuiltinPrimitive(value.TypeName)) {
                return;
            }
            if (IsBuiltinBoundedQuantity(value.TypeName)) {
                anyBoundedQuantity = true;
                return;
            }
            if (IsBuiltinResource(value.TypeName)) {
                anyResource = true;
                return;
            }
            auto valueIt = loaded.SymbolTable.find(value.TypeName);
            if (valueIt == loaded.SymbolTable.end()) {
                throw std::runtime_error(
                    "unknown map value type '" + value.TypeName + "' referenced in " +
                    ownerDesc + " (declare it in this file or import another .ttrpg)");
            }
            if (valueIt->second.OwnerStem != loaded.PrimaryStem) {
                externalStems.insert(valueIt->second.OwnerStem);
            }
            return;
        }
        if (IsBuiltinPrimitive(f.TypeName)) {
            return;
        }
        if (IsBuiltinFeature(f.TypeName)) {
            if (f.Container != EContainer::None && f.Container != EContainer::Collection) {
                throw std::runtime_error("Feature supports scalar and collection fields only");
            }
            anyCollection = anyCollection || f.Container == EContainer::Collection;
            return;
        }
        if (IsBuiltinBoundedQuantity(f.TypeName)) {
            if (f.Container != EContainer::None) {
                throw std::runtime_error(
                    "BoundedQuantity cannot be used inside set<> (in " + ownerDesc + ")");
            }
            anyBoundedQuantity = true;
            return;
        }
        if (IsBuiltinResource(f.TypeName)) {
            if (f.Container != EContainer::None) {
                throw std::runtime_error(
                    "Resource cannot be used inside set<> (in " + ownerDesc + ")");
            }
            anyResource = true;
            return;
        }
        auto it = loaded.SymbolTable.find(f.TypeName);
        if (it == loaded.SymbolTable.end()) {
            throw std::runtime_error(
                "unknown type '" + f.TypeName + "' referenced in " + ownerDesc +
                " (declare it in this file or import another .ttrpg)");
        }
        if (f.Container == EContainer::Set) {
            if (it->second.Kind == ETypeKind::Variant) {
                anyVariantSet = true;
            } else if (it->second.Kind == ETypeKind::Enum) {
                anyEnumSet = true;
            } else {
                throw std::runtime_error(
                    "set<T> element type '" + f.TypeName +
                    "' must be a schema-declared enum or variant");
            }
        }
        if (f.Container == EContainer::Collection) {
            if (it->second.Kind != ETypeKind::Class) {
                throw std::runtime_error(
                    "collection<T> element type '" + f.TypeName +
                    "' must be a schema-declared class");
            }
            anyCollection = true;
        }
        if (it->second.OwnerStem != loaded.PrimaryStem) {
            externalStems.insert(it->second.OwnerStem);
        }
    };

    for (const auto& v : mod.Variants) {
        for (const auto& alt : v.Alternatives) {
            for (const auto& f : alt.Fields) {
                if (IsBuiltinFeature(f.TypeName) || IsBuiltinFeature(f.ValueTypeName)) {
                    throw std::runtime_error("Feature selections are supported in class fields, not variant payloads");
                }
                inspectField(f, "variant '" + v.Name + "' alternative '" + alt.Name + "'");
            }
        }
    }
    for (const auto& c : mod.Classes) {
        for (const auto& f : c.Fields) {
            inspectField(f, "class '" + c.Name + "'");
        }
    }

    const bool ruleClasses = !mod.Classes.empty();
    bool entities = false;
    for (const auto& c : mod.Classes) {
        entities = entities || c.GameEntity;
        for (const auto& method : c.Methods) {
            if (method.ReturnType != "void") {
                TFieldDecl field;
                field.TypeName = method.ReturnType;
                inspectField(field, "method '" + c.Name + "." + method.Name + "'");
            }
        }
    }
    for (const auto& instance : mod.Instances) {
        TFieldDecl field;
        field.TypeName = instance.TypeName;
        inspectField(field, "instance '" + instance.Name + "'");
        for (const auto& parameter : instance.Parameters) inspectField(parameter.Field, "constructor parameter '" + parameter.Field.Name + "'");
    }
    for (const auto& trigger : mod.Triggers) {
        for (const auto& parameter : trigger.Parameters) {
            if (IsBuiltinPosition(parameter.TypeName)) continue;
            TFieldDecl field;
            field.TypeName = parameter.TypeName;
            inspectField(field, "trigger '" + trigger.Name + "'");
        }
    }
    for (const auto& record : mod.Records) {
        std::vector<TClassDecl::TParameter> types = record.Parameters;
        types.push_back({record.OwnerType, "owner"});
        types.push_back({record.SourceType, "source"});
        for (const auto& parameter : types) {
            if (IsBuiltinPosition(parameter.TypeName)) continue;
            TFieldDecl field;
            field.TypeName = parameter.TypeName;
            inspectField(field, "record '" + record.Name + "'");
        }
    }
    for (const auto& relation : mod.Relations) {
        for (const auto& type : {relation.OwnerType, relation.Parameters.front().TypeName, relation.ValueType}) {
            if (IsBuiltinPosition(type)) continue;
            TFieldDecl field;
            field.TypeName = type;
            inspectField(field, "relation '" + relation.Name + "'");
        }
    }
    if (std::any_of(mod.Instances.begin(), mod.Instances.end(), [](const auto& instance) { return instance.Parameterized; })) {
        w.Include("pf2e_engine/common/construction.h");
        w.Include("pf2e_engine/common/rule_integer.h");
        w.Include("algorithm");
    }
    if (!mod.Classes.empty()) {
        w.Include("pf2e_engine/common/rule_integer.h");
    }
    if (ruleClasses) {
        w.Include("pf2e_engine/common/generated_execution.h");
        w.Include("pf2e_engine/common/generated_construction.h");
        w.Include("pf2e_engine/common/rule_integer.h");
        w.Include("algorithm");
        w.Include("type_traits");
        w.Include("cstdint");
    }
    if (entities) w.Include("pf2e_engine/common/entity_id.h");

    if (anyEnumSet) {
        w.Include("set");
    }
    if (anyMap) {
        w.Include("map");
    }
    if (anyVariant) {
        w.Include("type_traits");
        w.Include("utility");
        w.Include("variant");
    }
    if (anyVariantSet) {
        w.Include("pf2e_engine/common/variant_map.h");
    }
    if (anyBoundedQuantity) {
        w.Include("pf2e_engine/common/bounded_quantity.h");
    }
    if (anyResource) {
        w.Include("pf2e_engine/common/resource.h");
    }
    if (anyCollection) {
        w.Include("pf2e_engine/common/id_collection.h");
    }
    w.EmptyLine();

    if (!externalStems.empty()) {
        for (const auto& stem : externalStems) {
            w.Include(DeriveSiblingInclude(primaryOutH, stem));
        }
        w.EmptyLine();
    }

    for (const auto& declaration : mod.Classes) w.Line("class " + declaration.Name + ";");
    w.EmptyLine();

    for (const auto& e : mod.Enums) {
        EmitEnumDecl(w, e);
    }
    for (const auto& v : mod.Variants) {
        EmitVariantDecl(w, v, loaded.SymbolTable);
    }
    for (const auto* c : OrderedClasses(loaded)) {
        EmitRuleClassDecl(w, *c, CompileClass(*c, loaded), loaded);
    }
    for (const auto& instance : mod.Instances) {
        EmitInstanceDeclaration(w, instance, loaded);
    }
    EmitEventDeclarations(w, loaded);
    EmitRecordDeclarations(w, loaded);
    EmitRelationDeclarations(w, loaded);
    EmitActivityDeclarations(w, loaded);
    EmitReactionDeclarations(w, loaded);
    w.Line("void " + AbilityRegistrationFunction(loaded.PrimaryStem) + "(TAbilityCatalog& catalog);");
    w.Line("struct TAbilityRules;");
    w.Line("void " + RulesRegistrationFunction(loaded.PrimaryStem) + "(TAbilityRules& rules);");
}

void EmitEnumImpl(TCppWriter& w, const TEnumDecl& e) {
    w.Function("std::string ToString(" + e.Name + " v)", [&] {
        w.Switch("v", [&] {
            for (const auto& v : e.Values) {
                w.Line("case " + e.Name + "::" + v + ": return \"" + v + "\";");
            }
        });
        w.Line("throw std::runtime_error(\"invalid " + e.Name + " value\");");
    });
    w.EmptyLine();
    w.Function(e.Name + " " + e.Name + "FromString(const std::string& s)", [&] {
        for (const auto& v : e.Values) {
            w.Block("if (s == \"" + v + "\") {", "}", [&] {
                w.Line("return " + e.Name + "::" + v + ";");
            });
        }
        w.Line("throw std::runtime_error(\"unknown " + e.Name + ": \\\"\" + s + \"\\\"\");");
    });
    w.EmptyLine();
}

struct TAltFieldInfo {
    const TFieldDecl* Field;
    EFieldKind Kind;
};

std::vector<TAltFieldInfo> ClassifyAltFields(
    const TVariantAlt& alt,
    const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    std::vector<TAltFieldInfo> infos;
    infos.reserve(alt.Fields.size());
    for (const auto& f : alt.Fields) {
        infos.push_back({&f, FieldKindOf(f, symbols)});
    }
    return infos;
}

void EmitVariantImpl(TCppWriter& w, const TVariantDecl& v,
                     const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    const std::string kindEnum = VariantKindEnum(v.Name);

    TEnumDecl kindDecl;
    kindDecl.Name = kindEnum;
    for (const auto& alt : v.Alternatives) {
        kindDecl.Values.push_back(alt.Name);
    }
    EmitEnumImpl(w, kindDecl);

    w.Function("TAstNode " + v.Name + "::GetAst([[maybe_unused]] TAstContext& ctx) const", [&] {
        w.Line("TAstNode node = TAstNode::MakeObject(\"" + v.Name + "\");");
        w.Line("AddValueField(node, \"kind\", Kind());");
        w.Switch("Kind()", [&] {
            for (const auto& alt : v.Alternatives) {
                w.Case(kindEnum + "::" + alt.Name, [&] {
                    if (!alt.Fields.empty()) {
                        const std::string structName = PayloadStructName(v.Name, alt.Name);
                        w.Line("const auto& a = std::get<" + structName + ">(Payload_);");
                        for (const auto& info : ClassifyAltFields(alt, symbols)) {
                            const TFieldDecl& f = *info.Field;
                            std::string fkey = PascalToSnake(f.Name);
                            if (info.Kind == EFieldKind::Class || info.Kind == EFieldKind::Variant ||
                                info.Kind == EFieldKind::BoundedQuantity) {
                                w.Line("AddOwnedObject(node, \"" + fkey + "\", a." + f.Name + ", ctx);");
                            } else {
                                w.Line("AddValueField(node, \"" + fkey + "\", a." + f.Name + ");");
                            }
                        }
                    }
                    w.Line("break;");
                });
            }
        });
        w.Line("return node;");
    });
    w.EmptyLine();
}

void EmitImpl(std::ostream& os,
              const TLoadedSchemas& loaded,
              const std::string& sourceName,
              const std::string& headerInclude) {
    ValidateRules(loaded);
    TCppWriter w(os);
    const TSchemaModule& mod = loaded.Primary;

    w.Comment("AUTO-GENERATED FROM " + sourceName + " -- DO NOT EDIT.");
    w.Include(headerInclude);
    w.EmptyLine();
    w.Include("pf2e_engine/common/ast/ast_helpers.h");
    w.Include("pf2e_engine/actions/ability.h");
    if (!RelationSymbols(loaded).empty()) w.Include("pf2e_engine/actions/generated_relation.h");
    if (UsesDurationSyntax(mod)) w.Include("pf2e_engine/common/game_time.h");
    if (!mod.Activities.empty() || std::any_of(mod.Abilities.begin(), mod.Abilities.end(), [](const auto& ability) { return !ability.OwnerType.empty() || !ability.Hooks.empty(); })) {
        w.Include("pf2e_engine/actions/generated_action.h");
        w.Include("pf2e_engine/actions/generated_world.h");
        w.Include("pf2e_engine/actions/generated_movement.h");
        w.Include("pf2e_engine/actions/generated_initiative.h");
        w.Include("pf2e_engine/actions/generated_history.h");
        w.Include("pf2e_engine/actions/reaction_window.h");
        w.Include("pf2e_engine/actions/feature_event.h");
    }
    if (!mod.Activities.empty() || std::any_of(mod.Abilities.begin(), mod.Abilities.end(), [](const auto& ability) { return !ability.OwnerType.empty(); })) {
        w.Include("pf2e_engine/actions/generated_source.h");
    }
    w.EmptyLine();
    w.Include("stdexcept");
    w.Include("string");
    if (!mod.Classes.empty()) {
        w.Include("utility");
    }
    w.EmptyLine();

    for (const auto& e : mod.Enums) {
        EmitEnumImpl(w, e);
    }
    for (const auto& v : mod.Variants) {
        EmitVariantImpl(w, v, loaded.SymbolTable);
    }
    for (const auto* c : OrderedClasses(loaded)) {
        EmitRuleClassImpl(w, *c, CompileClass(*c, loaded), loaded);
    }
    for (const auto& instance : mod.Instances) {
        EmitInstanceImplementation(w, instance, loaded);
    }
    w.Function("void " + AbilityRegistrationFunction(loaded.PrimaryStem) + "(TAbilityCatalog& catalog)", [&] {
        w.Line("(void)catalog;");
        for (const auto& imported : ImportedModuleStems(loaded, loaded.PrimaryStem)) {
            w.Line(AbilityRegistrationFunction(imported) + "(catalog);");
        }
        for (const auto& ability : mod.Abilities) {
            const std::string kind = ability.Kind == EAbilityDeclarationKind::Action ? "Action" :
                ability.Kind == EAbilityDeclarationKind::Reaction ? "Reaction" : "Feature";
            std::string grants = "{";
            for (std::size_t i = 0; i < ability.Grants.size(); ++i) {
                if (i != 0) grants += ", ";
                grants += CppStringLiteral(AbilityId(ability.Grants[i].Source.Text, loaded));
            }
            w.Line("catalog.Register(TAbilityDefinition{" + CppStringLiteral(AbilityId(ability.Name, loaded)) +
                ", EAbilityKind::" + kind + ", " + grants + "}});");
        }
    });
    w.EmptyLine();
    EmitActivityImplementations(w, loaded);
    EmitRecordImplementations(w, loaded);
    EmitRelationImplementations(w, loaded);
    EmitReactionImplementations(w, loaded);
    EmitActionRegistration(w, loaded);
}
