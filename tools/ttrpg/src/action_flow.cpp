#include <ttrpg/action_flow.h>

#include <ttrpg/cpp_writer.h>

#include <sstream>

std::vector<std::string> RenderActionFlow(const TCompiledActionFlow& flow) {
    std::ostringstream output;
    TCppWriter writer(output);
    const auto aliases = [&](const TActionFlowBlock& block) {
        for (const auto index : block.LiveSlots) {
            writer.Line("[[maybe_unused]] auto& " + flow.Slots[index].Local + " = frame.slot_" + std::to_string(index) + ".value();");
        }
    };
    writer.Struct(flow.FlowType, [&] {
        writer.Struct(flow.FrameType, [&] {
            for (std::size_t index = 0; index < flow.Slots.size(); ++index) {
                writer.Line("std::optional<" + flow.Slots[index].CppType + "> slot_" + std::to_string(index) + "{};");
            }
        });
        writer.Function("static void " + flow.RunName + "(std::shared_ptr<TActionContext> Context, [[maybe_unused]] " + flow.FrameType + " frame, std::size_t pc)", [&] {
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
                                writer.Line("return;");
                            } else if (block.Exit == E::Jump || block.Exit == E::Branch) {
                                const auto next = std::to_string(block.Next);
                                writer.Line("pc = " + (block.Exit == E::Jump ? next : "(" + block.Condition + ") ? " + next + " : " + std::to_string(block.Otherwise)) + ";");
                                writer.Line("continue;");
                            } else {
                                const auto& choice = block.Choice;
                                const auto slot = "slot_" + std::to_string(choice.Slot);
                                const auto helper = std::string(choice.Entity ? "SelectActionEntity<" : "SelectActionValue<") + choice.Type + ">";
                                writer.Line("frame." + slot + ".emplace(" + helper + "(Context, " + choice.Candidates + ",");
                                writer.Indented([&] {
                                    writer.Block("[Context, frame]([[maybe_unused]] const " + choice.CppType + "& " + choice.Local + ") {", "},", [&] {
                                        for (const auto& line : flow.Environment) writer.Line(line);
                                        aliases(block);
                                        for (const auto& line : choice.Predicate) writer.Line(line);
                                    });
                                    writer.Block("[Context, frame](" + choice.CppType + " selected) {", "}));", [&] {
                                        writer.Line("auto next = frame;");
                                        writer.Line("next." + slot + ".emplace(std::move(selected));");
                                        writer.Line(flow.FlowType + "::" + flow.RunName + "(Context, std::move(next), " + std::to_string(block.Next) + ");");
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
    writer.Line(flow.FlowType + "::" + flow.RunName + "(Context, {}, " + std::to_string(flow.Entry) + ");");
    std::vector<std::string> result;
    std::istringstream input(output.str());
    for (std::string line; std::getline(input, line);) result.push_back(std::move(line));
    return result;
}
