#include <ttrpg/constructors.h>

#include <ttrpg/conventions.h>

#include <set>
#include <stdexcept>

void ValidateConstructorNames(const TLoadedSchemas& loaded) {
    const std::set<std::string> reserved{"min_int", "max_int", "from", "when"};
    for (const auto& [stem, module] : AllModules(loaded)) {
        for (const auto& instance : module->Instances) {
            std::set<std::string> parameters;
            for (const auto& parameter : instance.Parameters) {
                const auto& name = parameter.Field.Name;
                if (CppKeywords().contains(name) || reserved.contains(name) || name == "T" + instance.Name + "Options" || name.starts_with("__")) {
                    throw std::runtime_error("reserved constructor parameter name '" + name + "'");
                }
                if (!parameters.insert(name).second) throw std::runtime_error("duplicate constructor parameter '" + name + "'");
            }
        }
    }
}
