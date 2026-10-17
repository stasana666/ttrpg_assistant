#include <ttrpg/emit_activity.h>

#include <ttrpg/program.h>

namespace {

std::string Signature(const TCompiledActivity& activity) {
    std::string result = activity.ResultType + " " + activity.FunctionName + "(std::shared_ptr<TActionContext> Context";
    for (const auto& parameter : activity.Parameters) result += ", " + parameter.CppType + " " + parameter.Name;
    return result + ", std::function<void(" + (activity.ResultType == "void" ? std::string{} : activity.ResultType) + ")> on_resume)";
}

}

void EmitActivityDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded) {
    if (loaded.Primary.Activities.empty()) return;
    writer.Line("struct TActionContext;");
    writer.Line("template<class T> class TGeneratedEntityBinding;");
    for (const auto& declaration : loaded.Primary.Activities) writer.Line(Signature(CompileActivity(declaration, loaded)) + ";");
    writer.EmptyLine();
}

void EmitActivityImplementations(TCppWriter& writer, const TLoadedSchemas& loaded) {
    for (const auto& declaration : loaded.Primary.Activities) {
        const auto activity = CompileActivity(declaration, loaded);
        writer.Function(Signature(activity), [&] {
            for (const auto& line : activity.Body) writer.Line(line);
        });
        writer.EmptyLine();
    }
}
