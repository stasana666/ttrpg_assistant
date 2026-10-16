#include <ttrpg/emit_record.h>

#include <ttrpg/conventions.h>
#include <ttrpg/events.h>
#include <ttrpg/records.h>

namespace {

std::string Type(const std::string& type, const TLoadedSchemas& loaded) {
    return QualifiedCppType(CppTypeFor(type), loaded.SymbolTable);
}

std::string Arguments(const TRecordDecl& declaration, const TLoadedSchemas& loaded) {
    std::string result = "std::tuple<";
    for (std::size_t i = 0; i < declaration.Parameters.size(); ++i) {
        if (i) result += ", ";
        result += QualifiedCppType(EventArgumentCppType(declaration.Parameters[i].TypeName, loaded), loaded.SymbolTable);
    }
    return result + ">";
}

}

void EmitRecordDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded) {
    for (const auto& declaration : loaded.Primary.Records) {
        writer.Struct(RecordTagName(declaration), [&] {
            writer.Line("using TOwner = " + Type(declaration.OwnerType, loaded) + ";");
            writer.Line("using TSource = " + Type(declaration.SourceType, loaded) + ";");
            writer.Line("using TArguments = " + Arguments(declaration, loaded) + ";");
            writer.Line("static constexpr std::string_view kId = " + CppStringLiteral(loaded.PrimaryStem + "." + declaration.Name) + ";");
            writer.Line("static constexpr std::string_view kOwnerTypeName = " + CppStringLiteral(declaration.OwnerType) + ";");
            writer.Line("static constexpr std::string_view kSourceTypeName = " + CppStringLiteral(declaration.SourceType) + ";");
            writer.Struct("TStored", [&] {
                writer.Line("::TRecordedSource<TOwner, TSource> " + declaration.SourceName + ";");
                for (const auto& parameter : declaration.Parameters) {
                    const auto type = Type(parameter.TypeName, loaded);
                    writer.Line((IsGameEntity(loaded, parameter.TypeName) ? "::TRecordedEntity<" + type + ">" : type) + " " + parameter.Name + ";");
                }
            });
            writer.Struct("TInputs", [&] {
                writer.Line("::TGeneratedSourceBinding<TSource> " + declaration.SourceName + ";");
                for (const auto& parameter : declaration.Parameters) {
                    const auto type = Type(parameter.TypeName, loaded);
                    writer.Line((IsGameEntity(loaded, parameter.TypeName) ? "::TGeneratedEntityBinding<" + type + ">" : type) + " " + parameter.Name + ";");
                }
            });
            writer.Line("static TStored Detach(const std::shared_ptr<TActionContext>& context, const ::TGeneratedSourceBinding<TSource>& source, const TArguments& arguments);");
            writer.Line("static TInputs Rebind(const TGeneratedWorldView& world, const TPlayer& owner, const TStored& stored);");
            writer.Line("static TInputs Rebind(const std::shared_ptr<TActionContext>& context, const TStored& stored);");
            writer.Line("static void Validate(const std::shared_ptr<TActionContext>& context, const TInputs& inputs);");
            writer.Line("static TAstNode GetAst(const TStored& stored, TAstContext& context);");
        });
        writer.EmptyLine();
    }
}

void EmitRecordImplementations(TCppWriter& writer, const TLoadedSchemas& loaded) {
    for (const auto& declaration : loaded.Primary.Records) {
        const auto tag = RecordTagName(declaration);
        writer.Function(tag + "::TStored " + tag + "::Detach(const std::shared_ptr<TActionContext>& context, const ::TGeneratedSourceBinding<TSource>& source, [[maybe_unused]] const TArguments& arguments)", [&] {
            writer.Block("return TStored{", "};", [&] {
                writer.Line("." + declaration.SourceName + " = ::TRecordedSource<TOwner, TSource>::Capture(context, source),");
                for (std::size_t i = 0; i < declaration.Parameters.size(); ++i) {
                    const auto& parameter = declaration.Parameters[i];
                    auto value = "std::get<" + std::to_string(i) + ">(arguments)";
                    if (IsGameEntity(loaded, parameter.TypeName)) value = "::TRecordedEntity<" + Type(parameter.TypeName, loaded) + ">::Capture(context, " + value + ")";
                    writer.Line("." + parameter.Name + " = " + value + ",");
                }
            });
        });
        for (const bool readonly : {true, false}) {
            const auto signature = readonly ? "const TGeneratedWorldView& world, const TPlayer& owner, const TStored& stored" :
                "const std::shared_ptr<TActionContext>& context, const TStored& stored";
            writer.Function(tag + "::TInputs " + tag + "::Rebind(" + signature + ")", [&] {
                writer.Block("return TInputs{", "};", [&] {
                    writer.Line("." + declaration.SourceName + " = stored." + declaration.SourceName + (readonly ? ".ReadOnly(world, owner)," : ".Bind(context),"));
                    for (const auto& parameter : declaration.Parameters) {
                        auto value = "stored." + parameter.Name;
                        if (IsGameEntity(loaded, parameter.TypeName)) value += readonly ? ".ReadOnly(world)" : ".Bind(context)";
                        writer.Line("." + parameter.Name + " = " + value + ",");
                    }
                });
            });
        }
        writer.Function("void " + tag + "::Validate(const std::shared_ptr<TActionContext>& context, const TInputs& inputs)", [&] {
            writer.Line("inputs." + declaration.SourceName + ".Validate(context);");
            for (const auto& parameter : declaration.Parameters) if (IsGameEntity(loaded, parameter.TypeName)) {
                writer.Line("::ValidateActionEntity(context, inputs." + parameter.Name + ".View());");
            }
        });
        writer.Function("TAstNode " + tag + "::GetAst(const TStored& stored, TAstContext& context)", [&] {
            writer.Line("auto node = TAstNode::MakeObject(" + CppStringLiteral(tag) + ");");
            writer.Line("AddOwnedObject(node, " + CppStringLiteral(PascalToSnake(declaration.SourceName)) + ", stored." + declaration.SourceName + ", context);");
            for (const auto& parameter : declaration.Parameters) {
                const auto label = CppStringLiteral(PascalToSnake(parameter.Name));
                writer.Line(std::string(FindClass(loaded, parameter.TypeName) ? "AddOwnedObject" : "AddValueField") + "(node, " + label + ", stored." + parameter.Name +
                    (FindClass(loaded, parameter.TypeName) ? ", context);" : ");"));
            }
            writer.Line("return node;");
        });
        writer.EmptyLine();
    }
}
