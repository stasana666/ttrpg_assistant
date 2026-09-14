#include <ttrpg/emit_rule_class.h>
#include <ttrpg/emit_source.h>

#include <ttrpg/conventions.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

const TClassDecl* FindClass(const std::string& name, const TLoadedSchemas& loaded) {
    for (const auto& [stem, module] : loaded.ByStem) {
        for (const auto& declaration : module.Classes) {
            if (declaration.Name == name) {
                return &declaration;
            }
        }
    }
    for (const auto& declaration : loaded.Primary.Classes) {
        if (declaration.Name == name) {
            return &declaration;
        }
    }
    return nullptr;
}

bool IsRuleClass(const std::string& name, const TLoadedSchemas& loaded) {
    const auto* declaration = FindClass(name, loaded);
    return declaration != nullptr && UsesRuleSyntax(*declaration, loaded);
}

const TVariantDecl* FindVariant(const std::string& name, const TLoadedSchemas& loaded) {
    for (const auto& [stem, module] : loaded.ByStem) {
        for (const auto& declaration : module.Variants) {
            if (declaration.Name == name) {
                return &declaration;
            }
        }
    }
    for (const auto& declaration : loaded.Primary.Variants) {
        if (declaration.Name == name) {
            return &declaration;
        }
    }
    return nullptr;
}

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

std::string ParseScalar(const TFieldDecl& field, const std::string& value,
                        const TLoadedSchemas& loaded, bool restore) {
    const auto kind = FieldKindOf(field, loaded.SymbolTable);
    if (restore && kind == EFieldKind::Class && IsRuleClass(field.TypeName, loaded)) {
        return field.TypeName + "::Restore(" + value + ", factory)";
    }
    return ScalarParseExpr(field, kind, value);
}

void EmitValidId(TCppWriter& writer, const std::string& value) {
    writer.Block("if (!" + value + ".is_number_integer()) {", "}", [&] {
        writer.Line("throw std::invalid_argument(\"Invalid collection identity\");");
    });
    writer.Block("if (!" + value + ".is_number_unsigned() && " + value +
        ".get<std::int64_t>() < 0) {", "}", [&] {
        writer.Line("throw std::invalid_argument(\"Invalid collection identity\");");
    });
    writer.Block("if (" + value + ".get<std::uint64_t>() > "
        "std::numeric_limits<std::size_t>::max()) {", "}", [&] {
        writer.Line("throw std::invalid_argument(\"Invalid collection identity\");");
    });
}

void EmitReadField(TCppWriter& writer, const TCompiledField& compiled,
                   const TLoadedSchemas& loaded, bool restore) {
    const auto& field = compiled.Declaration;
    const std::string key = PascalToSnake(field.Name);
    const std::string input = "j.at(\"" + key + "\")";
    const std::string target = "initial." + field.Name;
    const auto read = [&] {
        if (field.Container == EContainer::None) {
            writer.Line(target + " = " + ParseScalar(field, input, loaded, restore) + ";");
            return;
        }
        writer.Line(target + ".emplace();");
        const auto value_field = ElementField(field);
        if (restore && field.Container == EContainer::Collection) {
            writer.Block("if (!" + input + ".is_object() || !" + input +
                ".at(\"entries\").is_array()) {", "}", [&] {
                writer.Line("throw std::invalid_argument(\"Invalid collection snapshot\");");
            });
            const std::string next_id = input + ".at(\"next_id\")";
            EmitValidId(writer, next_id);
            writer.Block("for (const auto& entry : " + input + ".at(\"entries\")) {", "}", [&] {
                EmitValidId(writer, "entry.at(\"id\")");
                writer.Line(target + "->RestoreEntry(TId<" + CppTypeFor(field.TypeName) +
                    ">{entry.at(\"id\").get<std::size_t>()}, " +
                    ParseScalar(value_field, "entry.at(\"value\")", loaded, true) + ");");
            });
            writer.Line(target + "->RestoreNextId(TId<" + CppTypeFor(field.TypeName) + ">{" +
                next_id + ".get<std::size_t>()});");
            return;
        }
        if (field.Container == EContainer::Map) {
            writer.Block("for (const auto& [map_key, map_value] : " + input + ".items()) {", "}", [&] {
                writer.Line(target + "->emplace(" + field.TypeName + "FromString(map_key), " +
                    ParseScalar(value_field, "map_value", loaded, restore) + ");");
            });
            return;
        }
        writer.Block("for (const auto& item : " + input + ") {", "}", [&] {
            const std::string value = ParseScalar(value_field, "item", loaded, restore);
            if (field.Container == EContainer::Collection) {
                writer.Line(target + "->Add(" + value + ");");
            } else if (IsVariantType(field.TypeName, loaded.SymbolTable)) {
                writer.Line(target + "->Set(" + value + ");");
            } else {
                writer.Line(target + "->insert(" + value + ");");
            }
        });
    };
    if (restore) {
        read();
    } else {
        writer.Block("if (j.contains(\"" + key + "\")) {", "}", read);
    }
}

void EmitJsonValue(TCppWriter& writer, const TFieldDecl& field,
                   const std::string& source, const std::string& target,
                   const TLoadedSchemas& loaded, std::size_t& sequence) {
    const std::string suffix = std::to_string(sequence++);
    const std::string value = "json_value_" + suffix;
    const std::string result = "json_target_" + suffix;
    writer.Block("{", "}", [&] {
        writer.Line("const auto& " + value + " = " + source + ";");
        writer.Line("auto& " + result + " = " + target + ";");
        const auto element = ElementField(field);
        if (field.Container == EContainer::Map) {
            writer.Line(result + " = nlohmann::json::object();");
            writer.Block("for (const auto& entry : " + value + ") {", "}", [&] {
                EmitJsonValue(writer, element, "entry.second", result + "[ToString(entry.first)]", loaded, sequence);
            });
            return;
        }
        if (field.Container == EContainer::Collection) {
            writer.Line(result + " = nlohmann::json::object();");
            writer.Line(result + "[\"next_id\"] = " + value + ".NextId().Value;");
            writer.Line(result + "[\"entries\"] = nlohmann::json::array();");
            writer.Block("for (auto entry : " + value + ") {", "}", [&] {
                writer.Line("nlohmann::json item = nlohmann::json::object();");
                writer.Line("item[\"id\"] = entry.Id().Value;");
                EmitJsonValue(writer, element, "*entry", "item[\"value\"]", loaded, sequence);
                writer.Line(result + "[\"entries\"].push_back(std::move(item));");
            });
            return;
        }
        if (field.Container == EContainer::Set) {
            writer.Line(result + " = nlohmann::json::array();");
            writer.Block("for (const auto& entry : " + value + ") {", "}", [&] {
                writer.Line("nlohmann::json item;");
                const std::string item = IsVariantType(field.TypeName, loaded.SymbolTable) ? "entry.second" : "entry";
                EmitJsonValue(writer, element, item, "item", loaded, sequence);
                writer.Line(result + ".push_back(std::move(item));");
            });
            return;
        }
        switch (FieldKindOf(field, loaded.SymbolTable)) {
            case EFieldKind::Feature:
                writer.Line(result + " = " + value + ".ToJson();");
                return;
            case EFieldKind::Primitive:
                writer.Line(result + " = " + value + ";");
                return;
            case EFieldKind::Enum:
                writer.Line(result + " = ToString(" + value + ");");
                return;
            case EFieldKind::Resource:
                writer.Line(result + " = " + value + ".Count();");
                return;
            case EFieldKind::BoundedQuantity:
                writer.Line(result + " = nlohmann::json::object();");
                writer.Line(result + "[\"current_value\"] = " + value + ".CurrentValue();");
                writer.Line(result + "[\"max_value\"] = " + value + ".MaxValue();");
                return;
            case EFieldKind::Class: {
                if (IsRuleClass(field.TypeName, loaded)) {
                    writer.Line(result + " = " + value + ".ToJson();");
                    return;
                }
                const auto* declaration = FindClass(field.TypeName, loaded);
                if (declaration == nullptr) {
                    throw std::runtime_error("Unknown class while emitting JSON: " + field.TypeName);
                }
                writer.Line(result + " = nlohmann::json::object();");
                for (const auto& nested : declaration->Fields) {
                    if (!nested.Derived) {
                        EmitJsonValue(writer, nested, value + "." + nested.Name + "()",
                            result + "[\"" + PascalToSnake(nested.Name) + "\"]", loaded, sequence);
                    }
                }
                return;
            }
            case EFieldKind::Variant: {
                const auto* declaration = FindVariant(field.TypeName, loaded);
                if (declaration == nullptr) {
                    throw std::runtime_error("Unknown variant while emitting JSON: " + field.TypeName);
                }
                writer.Switch(value + ".Kind()", [&] {
                    for (const auto& alternative : declaration->Alternatives) {
                        writer.Case(VariantKindEnum(field.TypeName) + "::" + alternative.Name, [&] {
                            if (alternative.Fields.empty()) {
                                writer.Line(result + " = ToString(" + value + ".Kind());");
                            } else {
                                writer.Line("const auto& payload = std::get<" +
                                    PayloadStructName(field.TypeName, alternative.Name) + ">(" + value + ".Payload());");
                                writer.Line(result + " = nlohmann::json::object();");
                                const std::string alternative_target = result + "[\"" + alternative.Name + "\"]";
                                if (alternative.Fields.size() == 1) {
                                    const auto& nested = alternative.Fields.front();
                                    EmitJsonValue(writer, nested, "payload." + nested.Name,
                                        alternative_target, loaded, sequence);
                                } else {
                                    writer.Line(alternative_target + " = nlohmann::json::object();");
                                    for (const auto& nested : alternative.Fields) {
                                        EmitJsonValue(writer, nested, "payload." + nested.Name,
                                            alternative_target + "[\"" + PascalToSnake(nested.Name) + "\"]",
                                            loaded, sequence);
                                    }
                                }
                            }
                            writer.Line("break;");
                        });
                    }
                });
                return;
            }
        }
    });
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

void EmitRuleJsonValue(TCppWriter& writer, const TFieldDecl& field,
                       const std::string& source, const std::string& target,
                       const TLoadedSchemas& loaded) {
    std::size_t sequence = 0;
    EmitJsonValue(writer, field, source, target, loaded, sequence);
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
            writer.Line("static " + declaration.Name +
                " FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);");
            writer.Line("static " + declaration.Name +
                " Restore(const nlohmann::json& j, const TGameObjectFactory& factory);");
            writer.Line("nlohmann::json ToJson() const;");
            writer.Line("TAstNode GetAst(TAstContext& ctx) const;");
            writer.Line("static void RegisterDslProperties();");
            writer.Line("void VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source = {}) const;");
        });
        writer.EmptyLine();
        writer.PrivateSection([&] {
            writer.Line("struct TUninitialized {};");
            std::string constructor = "explicit " + declaration.Name + "(TUninitialized";
            std::string initializers;
            for (const auto& field : compiled.Fields) {
                if (!field.Declaration.Derived) {
                    const std::string name = field.Declaration.Name;
                    constructor += ", " + field.CppType + " value_" + name + " = {}";
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
    writer.Function(declaration.Name + " " + declaration.Name + "::Create(const TInitialValues& initial)", [&] {
        writer.Line(declaration.Name + " result(TUninitialized{});");
        writer.Line("[[maybe_unused]] auto& self = result;");
        writer.Line("(void)initial;");
        if (declaration.GameEntity) {
            writer.Line("result.Id_ = NewEntityId();");
        }
        for (const auto index : compiled.InitOrder) {
            const auto& field = compiled.Fields.at(index);
            const std::string name = field.Declaration.Name;
            std::string fallback = field.Initializer;
            if (fallback.empty()) {
                if (field.Declaration.Container == EContainer::None &&
                    FieldKindOf(field.Declaration, loaded.SymbolTable) == EFieldKind::Class) {
                    fallback = field.CppType + "::Create({})";
                } else {
                    fallback = field.CppType + "{}";
                }
            }
            writer.Line("result." + name + "_ = initial." + name + " ? *initial." + name + " : " + fallback + ";");
        }
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
    for (const bool restore : {false, true}) {
        const std::string function = restore ? "Restore" : "FromJson";
        writer.Function(declaration.Name + " " + declaration.Name + "::" + function +
            "(const nlohmann::json& j, const TGameObjectFactory& factory)", [&] {
            writer.Line("(void)factory;");
            writer.Block("if (!j.is_object()) {", "}", [&] {
                writer.Line("throw std::invalid_argument(\"Expected an object for " + declaration.Name + "\");");
            });
            writer.Line("TInitialValues initial;");
            for (const auto& field : compiled.Fields) {
                if (!field.Declaration.Derived) {
                    EmitReadField(writer, field, loaded, restore);
                }
            }
            if (!restore) {
                writer.Line("return Create(initial);");
                return;
            }
            std::string construct = declaration.Name + " result(TUninitialized{}";
            for (const auto& field : compiled.Fields) {
                if (!field.Declaration.Derived) {
                    construct += ", std::move(*initial." + field.Declaration.Name + ")";
                }
            }
            writer.Line(construct + ");");
            if (declaration.GameEntity) {
                writer.Block("if (!j.at(\"id\").is_number_integer()) {", "}", [&] {
                    writer.Line("throw std::invalid_argument(\"Invalid entity identity\");");
                });
                writer.Block("if (!j.at(\"id\").is_number_unsigned() && "
                    "j.at(\"id\").get<std::int64_t>() < 0) {", "}", [&] {
                    writer.Line("throw std::invalid_argument(\"Invalid entity identity\");");
                });
                writer.Line("result.Id_ = RestoreEntityId(j.at(\"id\").get<std::uint64_t>());");
            }
            writer.Line("return result;");
        });
        writer.EmptyLine();
    }
    writer.Function("nlohmann::json " + declaration.Name + "::ToJson() const", [&] {
        writer.Line("nlohmann::json result = nlohmann::json::object();");
        if (declaration.GameEntity) {
            writer.Line("result[\"id\"] = Id_.Value;");
        }
        for (const auto& field : compiled.Fields) {
            if (!field.Declaration.Derived) {
                EmitRuleJsonValue(writer, field.Declaration, field.Declaration.Name + "_",
                    "result[\"" + PascalToSnake(field.Declaration.Name) + "\"]", loaded);
            }
        }
        writer.Line("return result;");
    });
    writer.EmptyLine();
    EmitAst(writer, declaration, compiled, loaded);
    writer.Function("void " + declaration.Name + "::RegisterDslProperties()", [&] {
        writer.Line("[[maybe_unused]] auto& registry = TPropertyRegistry<" + declaration.Name + ">::Instance();");
        for (const auto& field : compiled.Fields) {
            if (IsDslSupported(field.Declaration)) {
                const std::string name = field.Declaration.Name;
                writer.Block("registry.Register(\"" + PascalToSnake(name) + "\", [](const " +
                    declaration.Name + "* object, TEvalContext&) {", "});", [&] {
                    writer.Line("return TDslValue(object->" + name + "());");
                });
            }
        }
    });
    writer.EmptyLine();
}
