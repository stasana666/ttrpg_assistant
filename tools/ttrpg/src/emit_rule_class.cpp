#include <ttrpg/emit_rule_class.h>
#include <ttrpg/emit_source.h>

#include <ttrpg/conventions.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

TFieldDecl ElementField(const TFieldDecl& field) {
    TFieldDecl result = field;
    if (field.Container == EContainer::Map) {
        result.TypeName = field.ValueTypeName;
    }
    result.ValueTypeName.clear();
    result.Container = EContainer::None;
    return result;
}

std::string MethodSignature(const TClassDecl& owner, const TClassDecl::TMethod& method,
                            bool qualified) {
    std::string result = CppTypeFor(method.ReturnType) + " ";
    if (qualified) {
        result += owner.Name + "::";
    }
    result += method.Name + "(";
    bool has_parameter = false;
    if (!method.Query) {
        result += "TGeneratedExecutionContext& ctx";
        has_parameter = true;
    }
    for (const auto& parameter : method.Parameters) {
        if (has_parameter) {
            result += ", ";
        }
        result += CppTypeFor(parameter.TypeName) + " arg_" + parameter.Name;
        has_parameter = true;
    }
    result += method.Query ? ") const" : ")";
    return result;
}

std::string MoveCondition(const TCompiledClass& compiled, bool assignment) {
    std::string result = "noexcept(";
    bool first = true;
    for (const auto& field : compiled.Fields) {
        if (field.Declaration.Derived) {
            continue;
        }
        if (!first) {
            result += " && ";
        }
        result += assignment ? "std::is_nothrow_move_assignable_v<" : "std::is_nothrow_move_constructible_v<";
        result += field.CppType + ">";
        first = false;
    }
    if (first) {
        result += "true";
    }
    return result + ")";
}

void EmitAst(TCppWriter& writer, const TClassDecl& declaration,
             const TCompiledClass& compiled, const TLoadedSchemas& loaded) {
    writer.Function("TAstNode " + declaration.Name +
        "::GetAst([[maybe_unused]] TAstContext& ctx) const", [&] {
        writer.Line("TAstNode node = TAstNode::MakeObject(\"" + declaration.Name + "\");");
        if (declaration.GameEntity) {
            writer.Block("if (ctx.IdentityOf(this).empty()) {", "}", [&] {
                writer.Line("ctx.RegisterIdentity(this, \"entity#\" + std::to_string(Id_.Value));");
            });
            writer.Line("AddValueField(node, \"id\", ctx.IdentityOf(this));");
        }
        writer.Line("[[maybe_unused]] const std::string owner_identity = ctx.IdentityOf(this);");
        for (const auto& compiled_field : compiled.Fields) {
            const auto& field = compiled_field.Declaration;
            if (field.Derived) {
                continue;
            }
            const std::string key = PascalToSnake(field.Name);
            const std::string source = field.Name + "_";
            if (field.Container == EContainer::None) {
                const auto kind = FieldKindOf(field, loaded.SymbolTable);
                if (kind == EFieldKind::Class || kind == EFieldKind::Variant ||
                    kind == EFieldKind::BoundedQuantity || kind == EFieldKind::Resource || kind == EFieldKind::Feature) {
                    writer.Block("if (!owner_identity.empty() && static_cast<const void*>(&" + source +
                        ") != static_cast<const void*>(this)) {", "}", [&] {
                        writer.Line("ctx.RegisterIdentity(&" + source + ", owner_identity + \"." + key + "\");");
                    });
                    writer.Line("AddOwnedObject(node, \"" + key + "\", " + source + ", ctx);");
                } else {
                    writer.Line("AddValueField(node, \"" + key + "\", " + source + ");");
                }
                continue;
            }
            writer.Block("{", "}", [&] {
                writer.Line("TAstNode container = TAstNode::MakeObject(\"container\");");
                if (field.Container == EContainer::Collection) {
                    writer.Line("AddValueField(container, \"next_id\", " + source + ".NextId().Value);");
                    writer.Block("for (auto entry : " + source + ") {", "}", [&] {
                        writer.Block("if (!owner_identity.empty()) {", "}", [&] {
                            writer.Line("ctx.RegisterIdentity(&*entry, owner_identity + \"." + key +
                                ".\" + std::to_string(entry.Id().Value));");
                        });
                        writer.Line("AddOwnedObject(container, std::to_string(entry.Id().Value), *entry, ctx);");
                    });
                } else if (field.Container == EContainer::Map ||
                    IsVariantType(field.TypeName, loaded.SymbolTable)) {
                    const auto kind = FieldKindOf(ElementField(field), loaded.SymbolTable);
                    writer.Block("for (const auto& [key, value] : " + source + ") {", "}", [&] {
                        if (kind == EFieldKind::Primitive || kind == EFieldKind::Enum) {
                            writer.Line("AddValueField(container, ToString(key), value);");
                        } else {
                            writer.Block("if (!owner_identity.empty()) {", "}", [&] {
                                writer.Line("ctx.RegisterIdentity(&value, owner_identity + \"." +
                                    PascalToSnake(field.Name) + ".\" + ToString(key));");
                            });
                            writer.Line("AddOwnedObject(container, ToString(key), value, ctx);");
                        }
                    });
                } else {
                    writer.Block("for (auto value : " + source + ") {", "}", [&] {
                        writer.Line("AddValueField(container, ToString(value), value);");
                    });
                }
                writer.Line("node.AddChild(\"" + key + "\", std::move(container));");
            });
        }
        writer.Line("return node;");
    });
    writer.EmptyLine();
}

}

void EmitInitialValueResolver(TCppWriter& writer, const TClassDecl& declaration,
                              const TCompiledClass& compiled, const TLoadedSchemas& loaded) {
    writer.Function(declaration.Name + "::TInitialValues " + declaration.Name + "::ResolveInitialValues(const TInitialValues& initial)", [&] {
        std::string construct = declaration.Name + " result(TUninitialized{}";
        for (const auto& field : compiled.Fields) {
            if (!field.Declaration.Derived) {
                construct += ", initial." + field.Declaration.Name + " ? *initial." + field.Declaration.Name + " : TGeneratedConstruction::Blank<" + QualifiedCppType(field.CppType, loaded.SymbolTable) + ">()";
            }
        }
        writer.Line(construct + ");");
        writer.Line("[[maybe_unused]] auto& self = result;");
        writer.Line("(void)initial;");
        for (const auto index : compiled.InitOrder) {
            const auto& field = compiled.Fields.at(index);
            const std::string name = field.Declaration.Name;
            std::string fallback = field.Initializer;
            if (fallback.empty()) {
                if (field.Declaration.Container == EContainer::None &&
                    FieldKindOf(field.Declaration, loaded.SymbolTable) == EFieldKind::Class) {
                    fallback = QualifiedCppType(field.CppType, loaded.SymbolTable) + "::Create({})";
                } else {
                    fallback = field.CppType + "{}";
                }
            }
            writer.Line("result." + name + "_ = initial." + name + " ? *initial." + name + " : " + fallback + ";");
        }
        std::string values = "return TInitialValues{";
        for (const auto& field : compiled.Fields) {
            if (!field.Declaration.Derived) {
                values += "." + field.Declaration.Name + " = std::move(result." + field.Declaration.Name + "_), ";
            }
        }
        writer.Line(values + "};");
    });
    writer.EmptyLine();
}

void EmitRuleClassDecl(TCppWriter& writer, const TClassDecl& declaration,
                       const TCompiledClass& compiled, const TLoadedSchemas& loaded) {
    (void)loaded;
    writer.Class(declaration.Name, [&] {
        writer.PublicSection([&] {
            EmitAbilitySourceVisitor(writer, declaration, loaded.SymbolTable);
            writer.Struct("TInitialValues", [&] {
                for (const auto& field : compiled.Fields) {
                    if (!field.Declaration.Derived) {
                        writer.Line("std::optional<" + field.CppType + "> " + field.Declaration.Name + "{};");
                    }
                }
            });
            writer.EmptyLine();
            writer.Line(declaration.Name + "();");
            if (declaration.GameEntity) {
                writer.Line(declaration.Name + "(const " + declaration.Name + "&) = delete;");
                writer.Line(declaration.Name + "& operator=(const " + declaration.Name + "&) = delete;");
                writer.Line(declaration.Name + "(" + declaration.Name + "&& other) " +
                    MoveCondition(compiled, false) + ";");
                writer.Line(declaration.Name + "& operator=(" + declaration.Name + "&& other) " +
                    MoveCondition(compiled, true) + ";");
            }
            writer.Line("static " + declaration.Name + " Create(const TInitialValues& initial);");
            writer.Line("static TInitialValues ResolveInitialValues(const TInitialValues& initial);");
            if (declaration.GameEntity) {
                writer.Line("TEntityId Id() const { return Id_; }");
            }
            for (const auto& field : compiled.Fields) {
                const auto& name = field.Declaration.Name;
                if (field.Declaration.Derived) {
                    writer.Line(field.CppType + " " + name + "() const;");
                } else {
                    writer.Line("const " + field.CppType + "& " + name + "() const { return " + name + "_; }");
                    writer.Line("TGuarded<" + field.CppType + "> " + name + "() { return TGuarded<" +
                        field.CppType + ">(" + name + "_); }");
                }
            }
            for (const auto& method : compiled.Methods) {
                writer.Line(MethodSignature(declaration, method.Declaration, false) + ";");
            }
            writer.EmptyLine();
            writer.Line("TAstNode GetAst(TAstContext& ctx) const;");
            writer.Line("void VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source = {}) const;");
        });
        writer.EmptyLine();
        writer.PrivateSection([&] {
            writer.Line("friend struct TGeneratedConstruction;");
            writer.Line("struct TUninitialized {};");
            std::string constructor = "explicit " + declaration.Name + "(TUninitialized";
            std::string initializers;
            for (const auto& field : compiled.Fields) {
                if (!field.Declaration.Derived) {
                    const std::string name = field.Declaration.Name;
                    constructor += ", " + field.CppType + " value_" + name + " = TGeneratedConstruction::Blank<" + QualifiedCppType(field.CppType, loaded.SymbolTable) + ">()";
                    if (!initializers.empty()) {
                        initializers += ", ";
                    }
                    initializers += name + "_(std::move(value_" + name + "))";
                }
            }
            constructor += ")";
            if (!initializers.empty()) {
                constructor += " : " + initializers;
            }
            writer.Line(constructor + " {}");
            if (declaration.GameEntity) {
                writer.Line("TEntityId Id_{};");
            }
            for (const auto& field : compiled.Fields) {
                if (!field.Declaration.Derived) {
                    writer.Line(field.CppType + " " + field.Declaration.Name + "_;");
                }
            }
            writer.Line("[[maybe_unused]] char ast_layout_sentinel_[1] = {};");
        });
    });
    writer.EmptyLine();
    writer.Line("template <>");
    writer.Line("struct TIsAstRecursive<" + declaration.Name + "> : std::true_type {};");
    writer.EmptyLine();
    writer.Function("inline void " + declaration.Name +
        "::VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source) const", [&] {
        writer.Line("(void)collector;");
        writer.Line("(void)source;");
        for (const auto& grant : compiled.Grants) {
            if (grant.Kind == TCompiledGrant::EKind::Definition) {
                writer.Line("collector.Grant(" + CppStringLiteral(grant.DefinitionId) + ", source, " + CppStringLiteral(grant.Site) + ");");
                continue;
            }
            std::string path = "source";
            for (const auto& field : grant.Fields) path += ".Field(" + CppStringLiteral(field) + ")";
            if (grant.Kind == TCompiledGrant::EKind::Collection) {
                writer.Block("for (auto entry : " + grant.Accessor + ") {", "}", [&] {
                    writer.Line("collector.Visit(*entry, " + path + ".Entry(entry.Id().Value), " + CppStringLiteral(grant.Site) + ");");
                });
            } else {
                writer.Line("collector.Visit(" + grant.Accessor + ", " + path + ", " + CppStringLiteral(grant.Site) + ");");
            }
        }
    });
    writer.EmptyLine();
}

void EmitRuleClassImpl(TCppWriter& writer, const TClassDecl& declaration,
                       const TCompiledClass& compiled, const TLoadedSchemas& loaded) {
    writer.Line(declaration.Name + "::" + declaration.Name + "() : " + declaration.Name + "(Create({})) {}");
    writer.EmptyLine();
    if (declaration.GameEntity) {
        std::string move_constructor = declaration.Name + "::" + declaration.Name + "(" +
            declaration.Name + "&& other) " + MoveCondition(compiled, false) +
            " : Id_(std::exchange(other.Id_, {}))";
        for (const auto& field : compiled.Fields) {
            if (!field.Declaration.Derived) {
                const std::string name = field.Declaration.Name;
                move_constructor += ", " + name + "_(std::move(other." + name + "_))";
            }
        }
        writer.Line(move_constructor + " {}");
        writer.EmptyLine();
        writer.Function(declaration.Name + "& " + declaration.Name + "::operator=(" +
            declaration.Name + "&& other) " + MoveCondition(compiled, true), [&] {
            writer.Block("if (this != &other) {", "}", [&] {
                writer.Line("Id_ = std::exchange(other.Id_, {});");
                for (const auto& field : compiled.Fields) {
                    if (!field.Declaration.Derived) {
                        const std::string name = field.Declaration.Name;
                        writer.Line(name + "_ = std::move(other." + name + "_);");
                    }
                }
            });
            writer.Line("return *this;");
        });
        writer.EmptyLine();
    }
    EmitInitialValueResolver(writer, declaration, compiled, loaded);
    writer.Function(declaration.Name + " " + declaration.Name + "::Create(const TInitialValues& initial)", [&] {
        writer.Line("[[maybe_unused]] auto values = ResolveInitialValues(initial);");
        std::string construct = declaration.Name + " result(TUninitialized{}";
        for (const auto& field : compiled.Fields) {
            if (!field.Declaration.Derived) construct += ", std::move(*values." + field.Declaration.Name + ")";
        }
        writer.Line(construct + ");");
        if (declaration.GameEntity) writer.Line("result.Id_ = NewEntityId();");
        writer.Line("return result;");
    });
    writer.EmptyLine();
    for (const auto& field : compiled.Fields) {
        if (field.Declaration.Derived) {
            writer.Function(field.CppType + " " + declaration.Name + "::" + field.Declaration.Name + "() const", [&] {
                writer.Line("[[maybe_unused]] const auto& self = *this;");
                writer.Line("return " + field.Initializer + ";");
            });
            writer.EmptyLine();
        }
    }
    for (const auto& method : compiled.Methods) {
        writer.Function(MethodSignature(declaration, method.Declaration, true), [&] {
            for (const auto& line : method.Body) {
                writer.Line(line);
            }
        });
        writer.EmptyLine();
    }
    EmitAst(writer, declaration, compiled, loaded);
}
