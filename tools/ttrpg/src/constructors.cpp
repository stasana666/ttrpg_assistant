#include <ttrpg/constructors.h>

#include <ttrpg/abilities.h>
#include <ttrpg/activities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/events.h>

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

void ValidateConstructorNames(const TLoadedSchemas& loaded) {
    std::map<std::string, const TSchemaModule*> modules;
    for (const auto& [stem, module] : loaded.ByStem) modules.emplace(stem, &module);
    modules[loaded.PrimaryStem] = &loaded.Primary;
    std::set<std::string> names{
        "TConstructionReport", "TConstructionIssue", "TFeatureChoiceDescriptor", "EConstructionChoiceState", "TGeneratedConstruction"
    };
    const bool constructors = std::any_of(modules.begin(), modules.end(), [](const auto& module) {
        return std::any_of(module.second->Instances.begin(), module.second->Instances.end(), [](const auto& instance) { return instance.Parameterized; });
    });
    if (constructors) {
        for (const auto& name : names) if (loaded.SymbolTable.contains(name)) {
            throw std::runtime_error("constructor runtime type name collision '" + name + "'");
        }
    }
    for (const auto& [name, info] : loaded.SymbolTable) names.insert(name);
    for (const auto& [stem, module] : modules) {
        names.insert(AbilityRegistrationFunction(stem));
        names.insert(RulesRegistrationFunction(stem));
        for (const auto& value : module->Enums) names.insert(value.Name + "FromString");
        for (const auto& value : module->Variants) {
            names.insert(VariantKindEnum(value.Name));
            for (const auto& alternative : value.Alternatives) names.insert(PayloadStructName(value.Name, alternative.Name));
        }
        for (const auto& value : module->Events) names.insert(EventTagName(value));
        for (const auto& value : module->Activities) names.insert(ActivityFunctionName(stem, value));
    }
    const std::set<std::string> keywords{
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break",
        "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept", "const",
        "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await", "co_return", "co_yield",
        "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern",
        "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast",
        "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch",
        "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
        "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq", "min_int", "max_int", "from", "when"
    };
    std::set<std::string> instances;
    for (const auto& [stem, module] : modules) {
        for (const auto& instance : module->Instances) {
            if (!instances.insert(instance.Name).second) throw std::runtime_error("duplicate named value '" + instance.Name + "'");
            std::vector<std::string> generated{"Create" + instance.Name};
            if (instance.Parameterized) {
                generated.insert(generated.end(), {"T" + instance.Name + "Options", "TResolved" + instance.Name,
                    "Resolve" + instance.Name, "Describe" + instance.Name, "Validate" + instance.Name});
            }
            for (const auto& name : generated) {
                if (!names.insert(name).second) throw std::runtime_error("constructor generated name collision '" + name + "'");
            }
            std::set<std::string> parameters;
            for (const auto& parameter : instance.Parameters) {
                const auto& name = parameter.Field.Name;
                if (keywords.contains(name) || name == "T" + instance.Name + "Options" || name.starts_with("__")) {
                    throw std::runtime_error("reserved constructor parameter name '" + name + "'");
                }
                if (!parameters.insert(name).second) throw std::runtime_error("duplicate constructor parameter '" + name + "'");
            }
        }
    }
}
