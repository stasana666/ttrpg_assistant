#include <ttrpg/emit_relation.h>

#include <ttrpg/conventions.h>
#include <ttrpg/relations.h>

void EmitRelationDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded) {
    for (const auto& declaration : loaded.Primary.Relations) {
        const auto type = [&](const std::string& name) { return QualifiedCppType(CppTypeFor(name), loaded.SymbolTable); };
        writer.Struct(RelationTagName(declaration), [&] {
            writer.Line("using TOwner = " + type(declaration.OwnerType) + ";");
            writer.Line("using TKey = " + type(declaration.Parameters.front().TypeName) + ";");
            writer.Line("using TValue = " + type(declaration.ValueType) + ";");
            writer.Line("static constexpr std::string_view kId = " + CppStringLiteral(loaded.PrimaryStem + "." + declaration.Name) + ";");
            writer.Line("static constexpr std::string_view kOwnerTypeName = " + CppStringLiteral(declaration.OwnerType) + ";");
            writer.Line("static constexpr std::string_view kKeyTypeName = " + CppStringLiteral(declaration.Parameters.front().TypeName) + ";");
            writer.Line("static TAstNode GetAst(const TValue& value, TAstContext& context);");
        });
        writer.EmptyLine();
    }
}

void EmitRelationImplementations(TCppWriter& writer, const TLoadedSchemas& loaded) {
    for (const auto& declaration : loaded.Primary.Relations) {
        const auto tag = RelationTagName(declaration);
        const auto found = loaded.SymbolTable.find(declaration.ValueType);
        const bool ordinary = found != loaded.SymbolTable.end() && found->second.Kind == ETypeKind::Class;
        writer.Function("TAstNode " + tag + "::GetAst(const TValue& value, [[maybe_unused]] TAstContext& context)", [&] {
            writer.Line("auto node = TAstNode::MakeObject(" + CppStringLiteral(tag) + ");");
            writer.Line(ordinary ? "AddOwnedObject(node, \"value\", value, context);" : "AddValueField(node, \"value\", value);");
            writer.Line("return node;");
        });
        writer.EmptyLine();
    }
}
