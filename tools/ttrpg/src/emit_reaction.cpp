#include <ttrpg/emit_reaction.h>

#include <ttrpg/abilities.h>
#include <ttrpg/conventions.h>
#include <ttrpg/triggers.h>

namespace {

std::string Signature(const TCompiledReaction& reaction) {
    return reaction.StateType + " " + reaction.FunctionName + "(std::shared_ptr<TActionContext> Context, " +
        reaction.StateType + " pending, [[maybe_unused]] " + reaction.ArgumentsType +
        " arguments, std::function<void(" + reaction.StateType + ")> on_resume)";
}

}

void EmitReactionDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded) {
    bool declared = !loaded.Primary.Triggers.empty();
    for (const auto& ability : loaded.Primary.Abilities) declared = declared || !ability.Triggers.empty();
    if (!declared) return;
    writer.Line("struct TActionContext;");
    writer.Line("template<class T> class TGeneratedEntityBinding;");
    for (const auto& trigger : loaded.Primary.Triggers) {
        writer.Struct(TriggerTagName(trigger), [&] {
            writer.Line("using TState = " + QualifiedCppType(CppTypeFor(trigger.ReturnType), loaded.SymbolTable) + ";");
            writer.Line("using TArguments = " + TriggerArgumentsCppType(trigger, loaded) + ";");
            writer.Line("static constexpr std::string_view kId = " + CppStringLiteral(loaded.PrimaryStem + "." + trigger.Name) + ";");
        });
        writer.EmptyLine();
    }
    for (const auto& ability : loaded.Primary.Abilities) {
        if (!ability.Triggers.empty()) writer.Line(Signature(CompileReaction(ability, loaded)) + ";");
    }
    writer.EmptyLine();
}

void EmitReactionImplementations(TCppWriter& writer, const TLoadedSchemas& loaded) {
    for (const auto& ability : loaded.Primary.Abilities) {
        if (ability.Triggers.empty()) continue;
        const auto compiled = CompileReaction(ability, loaded);
        writer.Function(Signature(compiled), [&] {
            for (const auto& line : compiled.Execution.Body) writer.Line(line);
        });
        writer.EmptyLine();
    }
}

void EmitReactionRegistration(TCppWriter& writer, const TAbilityDecl& declaration, const TLoadedSchemas& loaded) {
    const auto compiled = CompileReaction(declaration, loaded);
    for (const auto& name : declaration.Triggers) {
        const auto info = ResolveTrigger(name, ResolveAbility(declaration.Name, loaded).OwnerStem, loaded);
        const auto tag = "::" + TriggerTagName(*info.Declaration);
        const auto variable = "reaction_" + declaration.Name + "_" + name;
        writer.Line("static const auto " + variable + " = std::make_shared<const TReactionExecution<" + tag + ">>(TReactionExecution<" + tag + ">{");
        writer.Indented([&] {
            writer.Line(".OwnerType = typeid(::" + declaration.OwnerType + "),");
            writer.Line(".Name = " + CppStringLiteral(declaration.Name) + ",");
            writer.Line(".LimitKey = " + CppStringLiteral(info.Declaration->OncePerActivity ? info.OwnerStem + "." + name : "") + ",");
            writer.Block(".Payment = TResourcePayment(TBoundResources{", "}),", [&] {
                for (const auto& cost : compiled.Execution.Costs) {
                    writer.Block("TBoundResourceCost{", "},", [&] {
                        writer.Line(CppStringLiteral(cost.Key) + ", " + std::to_string(cost.Count) + ",");
                        writer.Line("[](const TPlayer& player) -> const TResource& { return " + cost.Read + "; },");
                        writer.Line("[](TPlayer& player) -> TGuarded<TResource> { return " + cost.Write + "; }");
                    });
                }
            });
            if (compiled.Execution.Requirement.empty()) {
                writer.Line(".Requirement = {},");
            } else {
                writer.Block(".Requirement = []([[maybe_unused]] const TGeneratedWorldView& world, [[maybe_unused]] const TPlayer& actor, [[maybe_unused]] const TAbilitySourcePath& source, [[maybe_unused]] const " +
                    compiled.StateType + "& pending, [[maybe_unused]] const " + compiled.ArgumentsType + "& arguments) {", "},", [&] {
                    for (const auto& line : compiled.Execution.Requirement) writer.Line(line);
                });
            }
            if (declaration.SourceType.empty()) {
                writer.Line(".Prepare = {},");
            } else {
                writer.Line(".Prepare = [](std::shared_ptr<TActionContext> Context, TPlayer& actor, const TGrantedAbility& grant) { PrepareActionSource<::" +
                    declaration.OwnerType + ", ::" + declaration.SourceType + ">(Context, actor, grant); },");
            }
            writer.Line(".Run = &::" + compiled.FunctionName);
        });
        writer.Line("});");
        writer.Line("rules.Reactions.Register<" + tag + ">(" + CppStringLiteral(AbilityId(declaration.Name, loaded)) + ", " + variable + ");");
    }
}
