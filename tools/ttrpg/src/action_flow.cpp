#include <ttrpg/action_flow.h>

#include <ttrpg/cpp_writer.h>

#include <sstream>

std::vector<std::string> RenderActionFlow(const TCompiledActionFlow& flow) {
    std::ostringstream output;
    TCppWriter writer(output);
    const auto resumeType = "std::function<void(" + (flow.ResultType == "void" ? std::string{} : flow.ResultType) + ")>";
    const auto aliases = [&](const TActionFlowBlock& block) {
        for (const auto index : block.LiveSlots) {
            writer.Line("[[maybe_unused]] auto& " + flow.Slots[index].Local + " = frame.slot_" + std::to_string(index) + ".value();");
        }
    };
    const auto resume = [&](std::size_t next) {
        const auto call = flow.FlowType + "::" + flow.RunName +
            "(Context, action_self, std::move(next), " + std::to_string(next) + ", on_resume)";
        if (flow.ResultType == "void") {
            writer.Line(call + ";");
            writer.Line("on_resume();");
        } else {
            writer.Line("on_resume(" + call + ");");
        }
    };
    writer.Struct(flow.FlowType, [&] {
        writer.Struct(flow.FrameType, [&] {
            for (std::size_t index = 0; index < flow.Slots.size(); ++index) {
                writer.Line("std::optional<" + flow.Slots[index].CppType + "> slot_" + std::to_string(index) + "{};");
            }
        });
        writer.Function("static " + flow.ResultType + " " + flow.RunName + "(std::shared_ptr<TActionContext> Context, " +
            "TGeneratedEntityBinding<" + flow.OwnerType + "> action_self, [[maybe_unused]] " + flow.FrameType +
            " frame, std::size_t pc, [[maybe_unused]] " + resumeType + " on_resume)", [&] {
            writer.Line("ValidateActionActorBinding(Context, action_self.View());");
            for (const auto& line : flow.Environment) writer.Line(line);
            writer.Block("while (true) {", "}", [&] {
                writer.Switch("pc", [&] {
                    for (std::size_t index = 0; index < flow.Blocks.size(); ++index) {
                        const auto& block = flow.Blocks[index];
                        writer.Block("case " + std::to_string(index) + ": {", "}", [&] {
                            aliases(block);
                            for (const auto& line : block.Body) writer.Line(line);
                            using E = TActionFlowBlock::EExit;
                            if (block.Exit == E::Return) {
                                if (!block.ReturnValue.empty()) writer.Line("return " + block.ReturnValue + ";");
                                else if (flow.ResultType == "void") writer.Line("return;");
                                else writer.Line("throw std::logic_error(\"unreachable generated activity exit\");");
                            } else if (block.Exit == E::Jump || block.Exit == E::Branch) {
                                const auto next = std::to_string(block.Next);
                                writer.Line("pc = " + (block.Exit == E::Jump ? next : "(" + block.Condition + ") ? " + next + " : " + std::to_string(block.Otherwise)) + ";");
                                writer.Line("continue;");
                            } else if (block.Exit == E::Choose) {
                                const auto& choice = block.Choice;
                                const auto slot = "slot_" + std::to_string(choice.Slot);
                                const auto helper = std::string(choice.Entity ? "SelectActionEntity<" : "SelectActionValue<") + choice.Type + ">";
                                writer.Line("frame." + slot + ".emplace(" + helper + "(Context, " + choice.Candidates + ",");
                                writer.Indented([&] {
                                    writer.Block("[Context, action_self, frame]([[maybe_unused]] const " + choice.CppType + "& " + choice.Local + ") {", "},", [&] {
                                        for (const auto& line : flow.Environment) writer.Line(line);
                                        aliases(block);
                                        for (const auto& line : choice.Predicate) writer.Line(line);
                                    });
                                    writer.Block("[Context, action_self, frame, on_resume](" + choice.CppType + " selected) {", "}));", [&] {
                                        writer.Line("auto next = frame;");
                                        writer.Line("next." + slot + ".emplace(std::move(selected));");
                                        resume(block.Next);
                                    });
                                });
                                writer.Line("pc = " + std::to_string(block.Next) + ";");
                                writer.Line("continue;");
                            } else {
                                const auto& invocation = block.Invocation;
                                std::string call = invocation.FunctionName + "(Context";
                                for (const auto& argument : invocation.Arguments) call += ", " + argument;
                                call += ",";
                                if (invocation.Slot) call = "frame.slot_" + std::to_string(*invocation.Slot) + ".emplace(" + call;
                                else call = "(void)" + call;
                                writer.Line(call);
                                const auto parameter = invocation.ResultType == "void" ? std::string{} :
                                    "[[maybe_unused]] " + invocation.ResultType + " result";
                                writer.Indented([&] {
                                    writer.Block("[Context, action_self, frame, on_resume](" + parameter + ") {", invocation.Slot ? "}));" : "});", [&] {
                                        writer.Line("auto next = frame;");
                                        if (invocation.Slot) writer.Line("next.slot_" + std::to_string(*invocation.Slot) + ".emplace(std::move(result));");
                                        resume(block.Next);
                                    });
                                });
                                writer.Line("pc = " + std::to_string(block.Next) + ";");
                                writer.Line("continue;");
                            }
                        });
                    }
                    writer.Line("default: throw std::logic_error(\"invalid generated action block\");");
                });
            });
        });
    });
    if (flow.Activity) writer.Line("if (!on_resume) throw std::invalid_argument(\"activity requires a resume callback\");");
    writer.Line("auto action_self = ActionSelf<" + flow.OwnerType + ">(Context);");
    writer.Line("ValidateActionActorBinding(Context, action_self.View());");
    for (const auto& line : flow.EntryChecks) writer.Line(line);
    writer.Line(flow.FlowType + "::" + flow.FrameType + " initial;");
    for (const auto& [slot, value] : flow.Initializers) writer.Line("initial.slot_" + std::to_string(slot) + ".emplace(std::move(" + value + "));");
    writer.Line("return " + flow.FlowType + "::" + flow.RunName + "(Context, action_self, std::move(initial), " +
        std::to_string(flow.Entry) + ", " + (flow.Activity ? "std::move(on_resume)" : "[] {}") + ");");
    std::vector<std::string> result;
    std::istringstream input(output.str());
    for (std::string line; std::getline(input, line);) result.push_back(std::move(line));
    return result;
}
