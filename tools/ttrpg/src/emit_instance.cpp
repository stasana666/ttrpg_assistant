#include <ttrpg/emit_instance.h>

#include <ttrpg/conventions.h>
#include <ttrpg/program.h>

#include <set>

namespace {

std::string Slot(std::size_t index) {
    return "resolved.Input" + std::to_string(index);
}

std::string Known(const TConstructorExpression& expression) {
    std::string result;
    for (const auto dependency : expression.Dependencies) {
        if (!result.empty()) result += " && ";
        result += Slot(dependency) + ".has_value()";
    }
    return result.empty() ? "true" : result;
}

void Issue(TCppWriter& writer, const std::string& parameter, const std::string& message) {
    writer.Line("resolved.Report.Issues.push_back({" + CppStringLiteral(parameter) + ", " + message + "});");
}

void Pending(TCppWriter& writer, const TCompiledConstructorParameter& parameter, bool missing = false) {
    writer.Line("resolved.Report.Complete = false;");
    if (parameter.Declaration.Choice) writer.Line("descriptor.State = EConstructionChoiceState::Pending;");
    if (missing) Issue(writer, parameter.Declaration.Field.Name, CppStringLiteral("required input is missing"));
    writer.Line("return;");
}

void Dependencies(TCppWriter& writer, const TConstructorExpression& expression,
                  const TCompiledConstructorParameter& parameter) {
    if (expression.Dependencies.empty()) return;
    writer.Block("if (!(" + Known(expression) + ")) {", "}", [&] { Pending(writer, parameter); });
}

void EmitParameter(TCppWriter& writer, const TCompiledConstructor& compiled, std::size_t index) {
    const auto& parameter = compiled.Parameters[index];
    const auto& declaration = parameter.Declaration;
    const auto& name = declaration.Field.Name;
    const auto option = "options." + name;
    writer.Block("[&] {", "}();", [&] {
        if (declaration.Choice) {
            writer.Line("auto& descriptor = resolved.Report.Fields.emplace_back();");
            writer.Line("descriptor.Name = " + CppStringLiteral(name) + ";");
            writer.Line("descriptor.State = EConstructionChoiceState::Enabled;");
            std::set<std::size_t> dependencies;
            for (const auto* expression : {&parameter.When, &parameter.Pool, &parameter.Default}) {
                if (*expression) dependencies.insert((*expression)->Dependencies.begin(), (*expression)->Dependencies.end());
            }
            for (const auto dependency : dependencies) {
                writer.Line("descriptor.Dependencies.push_back(" +
                    CppStringLiteral(compiled.Parameters[dependency].Declaration.Field.Name) + ");");
            }
        }
        writer.Block("try {", "}", [&] {
            if (parameter.When) {
                Dependencies(writer, *parameter.When, parameter);
                writer.Block("if (!(" + parameter.When->Code + ")) {", "}", [&] {
                    writer.Line("descriptor.State = EConstructionChoiceState::Disabled;");
                    writer.Line(Slot(index) + " = TFeatureSelection{};");
                    writer.Block("if (" + option + ") {", "}", [&] {
                        Issue(writer, name, CppStringLiteral("disabled choice cannot be overridden"));
                    });
                    writer.Line("return;");
                });
            }
            if (parameter.Pool) {
                Dependencies(writer, *parameter.Pool, parameter);
                writer.Line("const auto candidates = " + parameter.Pool->Code + ";");
                writer.Block("for (const auto& entry : candidates) {", "}", [&] {
                    writer.Block("if (!entry->DefinitionId().empty() && std::find(descriptor.Candidates.begin(), descriptor.Candidates.end(), *entry) == descriptor.Candidates.end()) {", "}", [&] {
                        writer.Line("descriptor.Candidates.push_back(*entry);");
                    });
                });
            }
            writer.Block("if (" + option + ") {", "}", [&] {
                if (declaration.Field.Container == EContainer::Collection) {
                    writer.Line(parameter.CppType + " value;");
                    writer.Block("for (const auto& item : *" + option + ") {", "}", [&] {
                        if (IsBuiltinFeature(declaration.Field.TypeName)) {
                            writer.Line("if (item.DefinitionId().empty()) { continue; }");
                        }
                        writer.Line("value.Add(item);");
                    });
                    writer.Line(Slot(index) + " = std::move(value);");
                } else {
                    writer.Line(Slot(index) + " = *" + option + ";");
                }
            });
            writer.Block("else {", "}", [&] {
                if (parameter.Default) {
                    Dependencies(writer, *parameter.Default, parameter);
                    if (declaration.Choice) writer.Line("descriptor.UsesDefault = true;");
                    writer.Line(Slot(index) + " = " + parameter.Default->Code + ";");
                } else {
                    Pending(writer, parameter, true);
                }
            });
            if (declaration.Choice) {
                writer.Line("descriptor.Selected = " + Slot(index) + ";");
                writer.Block("if (" + Slot(index) + "->DefinitionId().empty() || std::find(descriptor.Candidates.begin(), descriptor.Candidates.end(), *" + Slot(index) + ") == descriptor.Candidates.end()) {", "}", [&] {
                    Issue(writer, name, CppStringLiteral("choice must select a nonempty feature from its declared pool"));
                    writer.Line(Slot(index) + ".reset();");
                    writer.Line("descriptor.Selected.reset();");
                });
            }
        });
        writer.Block("catch (const std::exception& error) {", "}", [&] {
            writer.Line(Slot(index) + ".reset();");
            Issue(writer, name, "error.what()");
        });
    });
}

}

void EmitInstanceDeclaration(TCppWriter& writer, const TInstanceDecl& declaration,
                             const TLoadedSchemas& loaded) {
    if (!declaration.Parameterized) {
        writer.Line(declaration.TypeName + " Create" + declaration.Name + "();");
        return;
    }
    const auto compiled = CompileConstructor(declaration, loaded);
    const auto options = "T" + declaration.Name + "Options";
    writer.Struct(options, [&] {
        for (const auto& parameter : compiled.Parameters) {
            writer.Line("std::optional<" + parameter.OptionCppType + "> " + parameter.Declaration.Field.Name + "{};");
        }
    });
    writer.Line("::" + declaration.TypeName + " Create" + declaration.Name + "(const " + options + "& options = {});");
    writer.Line("TConstructionReport Describe" + declaration.Name + "(const " + options + "& options = {});");
    writer.Line("TConstructionReport Validate" + declaration.Name + "(const " + options + "& options = {});");
}

void EmitInstanceImplementation(TCppWriter& writer, const TInstanceDecl& declaration,
                                const TLoadedSchemas& loaded) {
    if (!declaration.Parameterized) {
        writer.Function(declaration.TypeName + " Create" + declaration.Name + "()", [&] {
            writer.Line("return " + CompileInstance(declaration, loaded) + ";");
        });
        writer.EmptyLine();
        return;
    }
    const auto compiled = CompileConstructor(declaration, loaded);
    const auto options = "T" + declaration.Name + "Options";
    const auto resolvedType = "TResolved" + declaration.Name;
    writer.Block("namespace {", "}", [&] {
        writer.Struct(resolvedType, [&] {
            writer.Line("TConstructionReport Report;");
            for (std::size_t i = 0; i < compiled.Parameters.size(); ++i) {
                writer.Line("std::optional<" + compiled.Parameters[i].CppType + "> Input" + std::to_string(i) + "{};");
            }
            writer.Line("std::optional<::" + declaration.TypeName + "::TInitialValues> Initial{};");
        });
        writer.Function(resolvedType + " Resolve" + declaration.Name + "(const ::" + options + "& options)", [&] {
            writer.Line("(void)options;");
            writer.Line(resolvedType + " resolved;");
            for (std::size_t i = 0; i < compiled.Parameters.size(); ++i) EmitParameter(writer, compiled, i);
            for (std::size_t i = 0; i < compiled.Requirements.size(); ++i) {
                const auto& requirement = compiled.Requirements[i];
                writer.Block("if (" + Known(requirement) + ") {", "}", [&] {
                    writer.Block("try {", "}", [&] {
                        writer.Block("if (!(" + requirement.Code + ")) {", "}", [&] {
                            Issue(writer, "", CppStringLiteral("requires failed in " + declaration.Name + " at line " + std::to_string(declaration.Requirements[i].Location.line)));
                        });
                    });
                    writer.Block("catch (const std::exception& error) {", "}", [&] { Issue(writer, "", "error.what()"); });
                });
            }
            writer.Block("if (resolved.Report.IsValid()) {", "}", [&] {
                writer.Block("try {", "}", [&] { writer.Line("resolved.Initial = " + compiled.Construction + ";"); });
                writer.Block("catch (const std::exception& error) {", "}", [&] { Issue(writer, "", "error.what()"); });
            });
            writer.Line("return resolved;");
        });
    });
    writer.EmptyLine();
    for (const auto& prefix : {"Describe", "Validate"}) {
        writer.Function("TConstructionReport " + std::string(prefix) + declaration.Name + "(const " + options + "& options)", [&] {
            writer.Line("return ::Resolve" + declaration.Name + "(options).Report;");
        });
        writer.EmptyLine();
    }
    writer.Function("::" + declaration.TypeName + " Create" + declaration.Name + "(const " + options + "& options)", [&] {
        writer.Line("auto resolved = ::Resolve" + declaration.Name + "(options);");
        writer.Block("if (!resolved.Report.IsValid()) {", "}", [&] {
            writer.Line("std::string message = " + CppStringLiteral("invalid construction of " + declaration.Name) + ";");
            writer.Block("for (const auto& issue : resolved.Report.Issues) {", "}", [&] {
                writer.Line("message += \"; \" + issue.Parameter + \": \" + issue.Message;");
            });
            writer.Line("throw std::invalid_argument(message);");
        });
        writer.Line("return ::" + declaration.TypeName + "::Create(*resolved.Initial);");
    });
    writer.EmptyLine();
}
