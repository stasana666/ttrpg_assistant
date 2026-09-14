#include <ttrpg/emit_action.h>

#include <ttrpg/abilities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/program.h>

void EmitActionRegistration(TCppWriter& writer, const TLoadedSchemas& loaded) {
    writer.Function("void " + RulesRegistrationFunction(loaded.PrimaryStem) + "(TAbilityRules& rules)", [&] {
        writer.Line(AbilityRegistrationFunction(loaded.PrimaryStem) + "(rules.Catalog);");
        for (const auto& imported : ImportedModuleStems(loaded, loaded.PrimaryStem)) {
            writer.Line(RulesRegistrationFunction(imported) + "(rules);");
        }
        for (const auto& ability : loaded.Primary.Abilities) {
            if (ability.OwnerType.empty()) continue;
            const auto compiled = CompileAction(ability, loaded);
            writer.Line("static const auto action_" + ability.Name + " = std::make_shared<const TAction>(");
            writer.Indented([&] {
                writer.Block("[](std::shared_ptr<TActionContext> Context) {", "},", [&] {
                    for (const auto& line : compiled.Body) writer.Line(line);
                });
                writer.Block("TAction::TBoundResources{", "},", [&] {
                    for (const auto& cost : compiled.Costs) {
                        writer.Block("TAction::TBoundResourceCost{", "},", [&] {
                            writer.Line(CppStringLiteral(cost.Key) + ", " + std::to_string(cost.Count) + ",");
                            writer.Line("[](const TPlayer& player) -> const TResource& { return " + cost.Read + "; },");
                            writer.Line("[](TPlayer& player) -> TGuarded<TResource> { return " + cost.Write + "; }");
                        });
                    }
                });
                writer.Line("std::string(" + CppStringLiteral(ability.Name) + "));");
            });
            writer.Line("rules.Executions.Register(" + CppStringLiteral(AbilityId(ability.Name, loaded)) + ", action_" + ability.Name + ");");
        }
    });
    writer.EmptyLine();
}
