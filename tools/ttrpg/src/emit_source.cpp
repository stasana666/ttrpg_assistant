#include <ttrpg/emit_source.h>

#include <ttrpg/conventions.h>

void EmitAbilitySourceVisitor(TCppWriter& writer, const TClassDecl& declaration,
                             const std::unordered_map<std::string, TTypeInfo>& symbols) {
    writer.Line("template <class TVisitor>");
    writer.Function("void VisitAbilitySources(TVisitor& visitor, const TAbilitySourcePath& path = {}) const", [&] {
        writer.Block("if constexpr (requires { static_cast<bool>(visitor.ShouldVisit(path)); }) {", "}", [&] {
            writer.Line("if (!visitor.ShouldVisit(path)) { return; }");
        });
        writer.Line("visitor(*this, path);");
        for (const auto& field : declaration.Fields) {
            const auto type = symbols.find(field.TypeName);
            if (field.Derived || type == symbols.end() || type->second.Kind != ETypeKind::Class) continue;
            const auto path = "path.Field(" + CppStringLiteral(PascalToSnake(field.Name)) + ")";
            if (field.Container == EContainer::None) {
                writer.Line(field.Name + "_.VisitAbilitySources(visitor, " + path + ");");
            } else if (field.Container == EContainer::Collection) {
                writer.Block("for (auto entry : " + field.Name + "_) {", "}", [&] {
                    writer.Line("entry->VisitAbilitySources(visitor, " + path + ".Entry(entry.Id().Value));");
                });
            }
        }
    });
}
