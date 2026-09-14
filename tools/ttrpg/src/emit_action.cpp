#include <ttrpg/emit_action.h>

#include <ttrpg/abilities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/program.h>
#include <ttrpg/events.h>

void EmitEventDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded) {
    if (!loaded.Primary.Events.empty()) {
        writer.Line("template <class T> class TGeneratedEntityBinding;");
        writer.Line("class TGeneratedWorldView;");
        writer.EmptyLine();
    }
    for (const auto& event : loaded.Primary.Events) {
        writer.Struct(EventTagName(event), [&] {
            writer.Line("using TOwner = " + event.OwnerType + ";");
            writer.Line("using TResult = " + CppTypeFor(event.ReturnType) + ";");
            std::string arguments = "std::tuple<";
            for (std::size_t i = 0; i < event.Parameters.size(); ++i) {
                if (i != 0) arguments += ", ";
                arguments += EventArgumentCppType(event.Parameters[i].TypeName, loaded);
            }
            writer.Line("using TArguments = " + arguments + ">;");
            writer.Line("using FHandler = std::function<TResult(const TGeneratedWorldView&, const TGeneratedEntityBinding<TOwner>&, const TArguments&)>;");
            writer.Line("static constexpr std::string_view kId = " + CppStringLiteral(loaded.PrimaryStem + "." + event.Name) + ";");
        });
        writer.EmptyLine();
    }
}

void EmitActionRegistration(TCppWriter& writer, const TLoadedSchemas& loaded) {
    writer.Function("void " + RulesRegistrationFunction(loaded.PrimaryStem) + "(TAbilityRules& rules)", [&] {
        writer.Line(AbilityRegistrationFunction(loaded.PrimaryStem) + "(rules.Catalog);");
        for (const auto& imported : ImportedModuleStems(loaded, loaded.PrimaryStem)) {
            writer.Line(RulesRegistrationFunction(imported) + "(rules);");
        }
        std::size_t hookIndex = 0;
        for (const auto& ability : loaded.Primary.Abilities) {
            for (const auto& hook : ability.Hooks) {
                const auto events = EventSymbols(loaded);
                const auto& event = *events.at(hook.EventName).Declaration;
                const auto tag = EventTagName(event);
                const auto variable = "hook_" + std::to_string(hookIndex++);
                const auto body = CompileFeatureHook(ability, hook, loaded);
                writer.Line("static const auto " + variable + " = std::make_shared<const " + tag + "::FHandler>(");
                writer.Indented([&] {
                    writer.Block("[]([[maybe_unused]] const TGeneratedWorldView& world, [[maybe_unused]] const TGeneratedEntityBinding<" + event.OwnerType +
                        ">& action_self, [[maybe_unused]] const " + tag + "::TArguments& arguments) -> " + CppTypeFor(event.ReturnType) + " {", "});", [&] {
                        for (const auto& line : body) writer.Line(line);
                    });
                });
                writer.Line("rules.Events.Register<" + tag + ">(" + CppStringLiteral(AbilityId(ability.Name, loaded)) + ", " + variable + ");");
            }
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
            if (compiled.Requirement.empty()) {
                writer.Line("rules.Executions.Register(" + CppStringLiteral(AbilityId(ability.Name, loaded)) + ", action_" + ability.Name + ");");
            } else {
                writer.Line("static const auto execution_" + ability.Name + " = std::make_shared<const TAbilityExecution>(TAbilityExecution{");
                writer.Indented([&] {
                    writer.Line(".Action = action_" + ability.Name + ",");
                    writer.Line(".Trigger = {},");
                    writer.Block(".Requirement = []([[maybe_unused]] const TGeneratedWorldView& world, [[maybe_unused]] const TPlayer& actor, [[maybe_unused]] const TAbilitySourcePath& source, const TTriggerContext*) {", "},", [&] {
                        for (const auto& line : compiled.Requirement) writer.Line(line);
                    });
                    if (ability.SourceType.empty()) {
                        writer.Line(".Prepare = {}");
                    } else {
                        const auto types = ability.OwnerType + ", " + ability.SourceType;
                        writer.Line(".Prepare = [](std::shared_ptr<TActionContext> Context, TPlayer& actor, const TGrantedAbility& grant) { PrepareActionSource<" + types + ">(Context, actor, grant); }");
                    }
                });
                writer.Line("});");
                writer.Line("rules.Executions.Register(" + CppStringLiteral(AbilityId(ability.Name, loaded)) + ", execution_" + ability.Name + ");");
            }
        }
    });
    writer.EmptyLine();
}
