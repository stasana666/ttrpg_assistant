#include <ttrpg/emit.h>

#include <ttrpg/analyze.h>
#include <ttrpg/conventions.h>
#include <ttrpg/cpp_writer.h>

#include <string>
#include <unordered_set>
#include <vector>

void EmitEnumDecl(TCppWriter& w, const TEnumDecl& e) {
    w.Block("enum class " + e.Name + " {", "};", [&] {
        for (const auto& v : e.Values) {
            w.Line(v + ",");
        }
    });
    w.EmptyLine();
    w.Line("std::string ToString(" + e.Name + " v);");
    w.Line(e.Name + " " + e.Name + "FromString(const std::string& s);");
    w.EmptyLine();
}

TFieldDecl MapValueField(const TFieldDecl& f) {
    TFieldDecl value = f;
    value.TypeName = f.ValueTypeName;
    value.ValueTypeName.clear();
    value.Container = EContainer::None;
    return value;
}

void EmitClassDecl(TCppWriter& w, const TClassDecl& c,
                   const std::unordered_map<std::string, TTypeInfo>& symbols) {
    w.Class(c.Name, [&] {
        w.PublicSection([&] {
            for (const auto& f : c.Fields) {
                std::string mt = CppMemberType(f, symbols);
                if (f.Derived) {
                    w.Line(mt + " " + f.Name + "() const { return " +
                           InitExprToCpp(*f.Init, "") + "; }");
                } else {
                    w.Line("const " + mt + "& " + f.Name + "() const { return " + f.Name + "_; }");
                    w.Line("TGuarded<" + mt + "> " + f.Name + "() { return TGuarded<" + mt +
                           ">(" + f.Name + "_); }");
                }
            }
            w.EmptyLine();
            w.Line("static " + c.Name +
                   " FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);");
            w.Line("TAstNode GetAst(TAstContext& ctx) const;");
            w.Line("static void RegisterDslProperties();");
        });
        w.EmptyLine();
        std::unordered_set<std::string> fieldNames;
        for (const auto& f : c.Fields) {
            fieldNames.insert(f.Name);
        }
        w.PrivateSection([&] {
            for (const auto& f : c.Fields) {
                if (f.Derived) {
                    continue;
                }
                std::string mt = CppMemberType(f, symbols);
                std::string init;
                if (f.Container != EContainer::None) {
                    init = "{}";
                } else if (f.Init && !InitIsComputed(*f.Init, fieldNames)) {
                    init = "{" + DefaultExprToCpp(f.Init->Text, f.TypeName) + "}";
                } else {
                    init = "{}";
                }
                w.Line(mt + " " + f.Name + "_" + init + ";");
            }
            w.Line("[[maybe_unused]] char ast_layout_sentinel_[1] = {};");
        });
    });
    w.EmptyLine();
    w.Line("template <>");
    w.Line("struct TIsAstRecursive<" + c.Name + "> : std::true_type {};");
    w.EmptyLine();
}

void EmitVariantDecl(TCppWriter& w, const TVariantDecl& v,
                     const std::unordered_map<std::string, TTypeInfo>& symbols) {
    const std::string kindEnum = VariantKindEnum(v.Name);

    TEnumDecl kindDecl;
    kindDecl.Name = kindEnum;
    for (const auto& alt : v.Alternatives) {
        kindDecl.Values.push_back(alt.Name);
    }
    EmitEnumDecl(w, kindDecl);

    for (const auto& alt : v.Alternatives) {
        w.Struct(PayloadStructName(v.Name, alt.Name), [&] {
            w.Line("static constexpr auto Kind = " + kindEnum + "::" + alt.Name + ";");
            for (const auto& f : alt.Fields) {
                if (f.Derived) {
                    throw std::runtime_error(
                        "'derive' is not supported in variant alternative '" + alt.Name +
                        "' (field '" + f.Name + "')");
                }
                std::string mt = CppMemberType(f, symbols);
                std::string init = "{}";
                if (f.Init) {
                    if (f.Init->Kind != expr::ENodeKind::IntLiteral &&
                        f.Init->Kind != expr::ENodeKind::Var) {
                        throw std::runtime_error(
                            "computed initializers are not supported in variant alternative '" +
                            alt.Name + "' (field '" + f.Name + "')");
                    }
                    init = "{" + DefaultExprToCpp(f.Init->Text, f.TypeName) + "}";
                }
                w.Line(mt + " " + f.Name + init + ";");
            }
        });
        w.EmptyLine();
    }

    w.Class(v.Name, [&] {
        w.PublicSection([&] {
            w.Line("using TPayload = std::variant<");
            w.Indented([&] {
                for (size_t i = 0; i < v.Alternatives.size(); ++i) {
                    bool last = (i + 1 == v.Alternatives.size());
                    w.Line(PayloadStructName(v.Name, v.Alternatives[i].Name) +
                           (last ? ">;" : ","));
                }
            });
            w.EmptyLine();
            w.Line(v.Name + "() = default;");
            w.Line("template <class T>");
            w.Indented([&] {
                w.Line("requires (!std::is_same_v<std::decay_t<T>, " + v.Name + ">)");
            });
            w.Line(v.Name + "(T alt) : Payload_(std::move(alt)) {}");
            w.EmptyLine();
            w.Function(kindEnum + " Kind() const", [&] {
                w.Line("return std::visit(");
                w.Indented([&] {
                    w.Line("[](const auto& a) { return std::decay_t<decltype(a)>::Kind; }, Payload_);");
                });
            });
            w.Line("const TPayload& Payload() const { return Payload_; }");
            w.EmptyLine();
            w.Line("template <class T> const T* TryGet() const { return std::get_if<T>(&Payload_); }");
            w.EmptyLine();
            w.Line("static " + v.Name +
                   " FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);");
            w.Line("TAstNode GetAst(TAstContext& ctx) const;");
        });
        w.EmptyLine();
        w.PrivateSection([&] {
            w.Line("TPayload Payload_;");
        });
    });
    w.EmptyLine();
    w.Line("template <>");
    w.Line("struct TIsAstRecursive<" + v.Name + "> : std::true_type {};");
    w.EmptyLine();
}

void EmitHeader(std::ostream& os,
                const TLoadedSchemas& loaded,
                const std::string& sourceName,
                const std::string& primaryOutH)
{
    TCppWriter w(os);
    const TSchemaModule& mod = loaded.Primary;

    w.Comment("AUTO-GENERATED FROM " + sourceName + " -- DO NOT EDIT.");
    w.Comment("Source of truth: pf2e_engine/data/schemas/" + sourceName);
    w.Line("#pragma once");
    w.EmptyLine();
    w.Include("pf2e_engine/common/ast/ast_constructable.h");
    if (!mod.Classes.empty()) {
        w.Include("pf2e_engine/common/guarded.h");
    }
    w.EmptyLine();
    w.Include("nlohmann/json_fwd.hpp");
    w.EmptyLine();
    w.Include("limits");
    w.Include("string");
    if (!mod.Classes.empty()) {
        w.Include("utility");
    }

    bool anyEnumSet = false;
    bool anyVariantSet = false;
    bool anyVariant = !mod.Variants.empty();
    bool anyBoundedQuantity = false;
    bool anyCollection = false;
    bool anyMap = false;

    std::unordered_set<std::string> externalStems;

    auto inspectField = [&](const TFieldDecl& f, const std::string& ownerDesc) {
        if (f.Container == EContainer::Map) {
            auto keyIt = loaded.SymbolTable.find(f.TypeName);
            if (keyIt == loaded.SymbolTable.end()) {
                throw std::runtime_error(
                    "unknown map key type '" + f.TypeName + "' referenced in " + ownerDesc +
                    " (declare it in this file or import another .ttrpg)");
            }
            if (keyIt->second.Kind != ETypeKind::Enum) {
                throw std::runtime_error(
                    "map<K, V> key type '" + f.TypeName +
                    "' must be a schema-declared enum");
            }
            if (keyIt->second.OwnerStem != loaded.PrimaryStem) {
                externalStems.insert(keyIt->second.OwnerStem);
            }
            anyMap = true;
            TFieldDecl value = MapValueField(f);
            if (IsBuiltinPrimitive(value.TypeName)) {
                return;
            }
            if (IsBuiltinBoundedQuantity(value.TypeName)) {
                anyBoundedQuantity = true;
                return;
            }
            auto valueIt = loaded.SymbolTable.find(value.TypeName);
            if (valueIt == loaded.SymbolTable.end()) {
                throw std::runtime_error(
                    "unknown map value type '" + value.TypeName + "' referenced in " +
                    ownerDesc + " (declare it in this file or import another .ttrpg)");
            }
            if (valueIt->second.OwnerStem != loaded.PrimaryStem) {
                externalStems.insert(valueIt->second.OwnerStem);
            }
            return;
        }
        if (IsBuiltinPrimitive(f.TypeName)) {
            return;
        }
        if (IsBuiltinBoundedQuantity(f.TypeName)) {
            if (f.Container != EContainer::None) {
                throw std::runtime_error(
                    "BoundedQuantity cannot be used inside set<> (in " + ownerDesc + ")");
            }
            anyBoundedQuantity = true;
            return;
        }
        auto it = loaded.SymbolTable.find(f.TypeName);
        if (it == loaded.SymbolTable.end()) {
            throw std::runtime_error(
                "unknown type '" + f.TypeName + "' referenced in " + ownerDesc +
                " (declare it in this file or import another .ttrpg)");
        }
        if (f.Container == EContainer::Set) {
            if (it->second.Kind == ETypeKind::Variant) {
                anyVariantSet = true;
            } else if (it->second.Kind == ETypeKind::Enum) {
                anyEnumSet = true;
            } else {
                throw std::runtime_error(
                    "set<T> element type '" + f.TypeName +
                    "' must be a schema-declared enum or variant");
            }
        }
        if (f.Container == EContainer::Collection) {
            if (it->second.Kind != ETypeKind::Class) {
                throw std::runtime_error(
                    "collection<T> element type '" + f.TypeName +
                    "' must be a schema-declared class");
            }
            anyCollection = true;
        }
        if (it->second.OwnerStem != loaded.PrimaryStem) {
            externalStems.insert(it->second.OwnerStem);
        }
    };

    for (const auto& v : mod.Variants) {
        for (const auto& alt : v.Alternatives) {
            for (const auto& f : alt.Fields) {
                inspectField(f, "variant '" + v.Name + "' alternative '" + alt.Name + "'");
            }
        }
    }
    for (const auto& c : mod.Classes) {
        for (const auto& f : c.Fields) {
            inspectField(f, "class '" + c.Name + "'");
        }
    }

    if (anyEnumSet) {
        w.Include("set");
    }
    if (anyMap) {
        w.Include("map");
    }
    if (anyVariant) {
        w.Include("type_traits");
        w.Include("utility");
        w.Include("variant");
    }
    if (anyVariantSet) {
        w.Include("pf2e_engine/common/variant_map.h");
    }
    if (anyBoundedQuantity) {
        w.Include("pf2e_engine/common/bounded_quantity.h");
    }
    if (anyCollection) {
        w.Include("pf2e_engine/common/id_collection.h");
    }
    w.EmptyLine();

    if (!externalStems.empty()) {
        for (const auto& stem : externalStems) {
            w.Include(DeriveSiblingInclude(primaryOutH, stem));
        }
        w.EmptyLine();
    }

    w.Line("class TGameObjectFactory;");
    w.EmptyLine();

    for (const auto& e : mod.Enums) {
        EmitEnumDecl(w, e);
    }
    for (const auto& v : mod.Variants) {
        EmitVariantDecl(w, v, loaded.SymbolTable);
    }
    for (const auto& c : mod.Classes) {
        EmitClassDecl(w, c, loaded.SymbolTable);
    }
}

void EmitEnumImpl(TCppWriter& w, const TEnumDecl& e) {
    w.Function("std::string ToString(" + e.Name + " v)", [&] {
        w.Switch("v", [&] {
            for (const auto& v : e.Values) {
                w.Line("case " + e.Name + "::" + v + ": return \"" + v + "\";");
            }
        });
        w.Line("throw std::runtime_error(\"invalid " + e.Name + " value\");");
    });
    w.EmptyLine();
    w.Function(e.Name + " " + e.Name + "FromString(const std::string& s)", [&] {
        for (const auto& v : e.Values) {
            w.Block("if (s == \"" + v + "\") {", "}", [&] {
                w.Line("return " + e.Name + "::" + v + ";");
            });
        }
        w.Line("throw std::runtime_error(\"unknown " + e.Name + ": \\\"\" + s + \"\\\"\");");
    });
    w.EmptyLine();
}

void EmitClassImpl(TCppWriter& w,
                   const TClassDecl& c,
                   const std::unordered_map<std::string, TTypeInfo>& symbols,
                   const std::unordered_map<std::string, const TClassDecl*>& classes)
{
    std::vector<EFieldKind> kinds;
    kinds.reserve(c.Fields.size());
    bool usesFactory = false;
    for (const auto& f : c.Fields) {
        if (f.Container != EContainer::None) {
            kinds.push_back(EFieldKind::Primitive);
            if (f.Container == EContainer::Set && IsVariantType(f.TypeName, symbols)) {
                usesFactory = true;
            }
            if (f.Container == EContainer::Collection) {
                usesFactory = true;
            }
            if (f.Container == EContainer::Map) {
                TFieldDecl value = MapValueField(f);
                EFieldKind valueKind = FieldKindOf(value, symbols);
                if (valueKind == EFieldKind::Class || valueKind == EFieldKind::Variant ||
                    valueKind == EFieldKind::BoundedQuantity) {
                    usesFactory = true;
                }
            }
            continue;
        }
        EFieldKind k = FieldKindOf(f, symbols);
        kinds.push_back(k);
        if (k == EFieldKind::Class || k == EFieldKind::Variant ||
            k == EFieldKind::BoundedQuantity) {
            usesFactory = true;
        }
    }

    std::unordered_set<std::string> fieldNames;
    for (const auto& f : c.Fields) {
        fieldNames.insert(f.Name);
    }
    std::vector<size_t> initOrder = FieldInitOrder(c, classes);

    w.Function(c.Name + " " + c.Name +
               "::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory)", [&] {
        if (!usesFactory) {
            w.Line("(void)factory;");
        }
        w.Line(c.Name + " r;");
        for (size_t idx : initOrder) {
            const auto& f = c.Fields[idx];
            std::string key = PascalToSnake(f.Name);
            bool computed = f.Init && InitIsComputed(*f.Init, fieldNames);
            if (f.Container == EContainer::Set) {
                w.Block("if (j.contains(\"" + key + "\")) {", "}", [&] {
                    w.Block("for (const auto& item : j.at(\"" + key + "\")) {", "}", [&] {
                        if (IsVariantType(f.TypeName, symbols)) {
                            w.Line("r." + f.Name + "_.Set(" + f.TypeName + "::FromJson(item, factory));");
                        } else {
                            w.Line("r." + f.Name + "_.insert(" + f.TypeName +
                                   "FromString(item.get<std::string>()));");
                        }
                    });
                });
            } else if (f.Container == EContainer::Collection) {
                w.Block("if (j.contains(\"" + key + "\")) {", "}", [&] {
                    w.Block("for (const auto& item : j.at(\"" + key + "\")) {", "}", [&] {
                        w.Line("r." + f.Name + "_.Add(" +
                               ScalarParseExpr(f, EFieldKind::Class, "item") + ");");
                    });
                });
            } else if (f.Container == EContainer::Map) {
                TFieldDecl value = MapValueField(f);
                EFieldKind valueKind = FieldKindOf(value, symbols);
                w.Block("if (j.contains(\"" + key + "\")) {", "}", [&] {
                    w.Block("for (const auto& [map_key, map_value] : j.at(\"" + key + "\").items()) {", "}", [&] {
                        w.Line("r." + f.Name + "_.emplace(" + f.TypeName +
                               "FromString(map_key), " +
                               ScalarParseExpr(value, valueKind, "map_value") + ");");
                    });
                });
            } else if (computed) {
                std::string present = ScalarParseExpr(f, kinds[idx], "j.at(\"" + key + "\")");
                std::string fallback = (kinds[idx] == EFieldKind::BoundedQuantity)
                    ? "TBoundedQuantity(" + InitExprToCpp(*f.Init) + ")"
                    : InitExprToCpp(*f.Init);
                w.Line("r." + f.Name + "_ = j.contains(\"" + key + "\") ? " +
                       present + " : " + fallback + ";");
            } else {
                w.Line("r." + f.Name + "_ = " + LoadFieldCall(f, kinds[idx], key) + ";");
            }
        }
        w.Line("return r;");
    });
    w.EmptyLine();

    w.Function("TAstNode " + c.Name + "::GetAst([[maybe_unused]] TAstContext& ctx) const", [&] {
        w.Line("TAstNode node = TAstNode::MakeObject(\"" + c.Name + "\");");
        for (size_t i = 0; i < c.Fields.size(); ++i) {
            const auto& f = c.Fields[i];
            if (f.Derived) {
                continue;
            }
            std::string key = PascalToSnake(f.Name);
            if (f.Container == EContainer::Set && IsVariantType(f.TypeName, symbols)) {
                w.Block("{", "}", [&] {
                    w.Line("TAstNode set_node = TAstNode::MakeObject(\"container\");");
                    w.Block("for (const auto& [kind, value] : " + f.Name + "_) {", "}", [&] {
                        w.Line("AddOwnedObject(set_node, ToString(kind), value, ctx);");
                    });
                    w.Line("node.AddChild(\"" + key + "\", std::move(set_node));");
                });
            } else if (f.Container == EContainer::Set) {
                w.Block("{", "}", [&] {
                    w.Line("TAstNode set_node = TAstNode::MakeObject(\"container\");");
                    w.Block("for (auto v : " + f.Name + "_) {", "}", [&] {
                        w.Line("AddValueField(set_node, ToString(v), v);");
                    });
                    w.Line("node.AddChild(\"" + key + "\", std::move(set_node));");
                });
            } else if (f.Container == EContainer::Collection) {
                w.Block("{", "}", [&] {
                    w.Line("TAstNode coll_node = TAstNode::MakeObject(\"container\");");
                    w.Block("for (auto entry : " + f.Name + "_) {", "}", [&] {
                        w.Line("AddOwnedObject(coll_node, std::to_string(entry.Id().Value), "
                               "*entry, ctx);");
                    });
                    w.Line("node.AddChild(\"" + key + "\", std::move(coll_node));");
                });
            } else if (f.Container == EContainer::Map) {
                TFieldDecl value = MapValueField(f);
                EFieldKind valueKind = FieldKindOf(value, symbols);
                w.Block("{", "}", [&] {
                    w.Line("TAstNode map_node = TAstNode::MakeObject(\"container\");");
                    w.Block("for (const auto& [map_key, map_value] : " + f.Name + "_) {", "}", [&] {
                        if (valueKind == EFieldKind::Class || valueKind == EFieldKind::Variant ||
                            valueKind == EFieldKind::BoundedQuantity) {
                            w.Line("AddOwnedObject(map_node, ToString(map_key), map_value, ctx);");
                        } else {
                            w.Line("AddValueField(map_node, ToString(map_key), map_value);");
                        }
                    });
                    w.Line("node.AddChild(\"" + key + "\", std::move(map_node));");
                });
            } else if (kinds[i] == EFieldKind::Class || kinds[i] == EFieldKind::Variant ||
                       kinds[i] == EFieldKind::BoundedQuantity) {
                w.Line("AddOwnedObject(node, \"" + key + "\", " + f.Name + "_, ctx);");
            } else {
                w.Line("AddValueField(node, \"" + key + "\", " + f.Name + "_);");
            }
        }
        w.Line("return node;");
    });
    w.EmptyLine();

    w.Function("void " + c.Name + "::RegisterDslProperties()", [&] {
        w.Line("auto& r = TPropertyRegistry<" + c.Name + ">::Instance();");
        for (size_t i = 0; i < c.Fields.size(); ++i) {
            const auto& f = c.Fields[i];
            std::string key = PascalToSnake(f.Name);
            if (IsDslSupported(f)) {
                w.Block("r.Register(\"" + key + "\", [](const " + c.Name +
                        "* obj, TEvalContext&) {", "});", [&] {
                    w.Line("return TDslValue(obj->" + f.Name + "());");
                });
            }
        }
        bool anyDsl = false;
        for (const auto& f : c.Fields) {
            if (IsDslSupported(f)) { anyDsl = true; break; }
        }
        if (!anyDsl) {
            w.Line("(void)r;");
        }
    });
    w.EmptyLine();
}

struct TAltFieldInfo {
    const TFieldDecl* Field;
    EFieldKind Kind;
};

std::vector<TAltFieldInfo> ClassifyAltFields(
    const TVariantAlt& alt,
    const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    std::vector<TAltFieldInfo> infos;
    infos.reserve(alt.Fields.size());
    for (const auto& f : alt.Fields) {
        infos.push_back({&f, FieldKindOf(f, symbols)});
    }
    return infos;
}

void EmitVariantImpl(TCppWriter& w, const TVariantDecl& v,
                     const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    const std::string kindEnum = VariantKindEnum(v.Name);

    TEnumDecl kindDecl;
    kindDecl.Name = kindEnum;
    for (const auto& alt : v.Alternatives) {
        kindDecl.Values.push_back(alt.Name);
    }
    EmitEnumImpl(w, kindDecl);

    bool usesFactory = false;
    for (const auto& alt : v.Alternatives) {
        for (const auto& f : alt.Fields) {
            EFieldKind k = FieldKindOf(f, symbols);
            if (k == EFieldKind::Class || k == EFieldKind::Variant ||
                k == EFieldKind::BoundedQuantity) {
                usesFactory = true;
            }
        }
    }

    w.Function(v.Name + " " + v.Name +
               "::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory)", [&] {
        if (!usesFactory) {
            w.Line("(void)factory;");
        }
        w.Block("if (j.is_string()) {", "}", [&] {
            w.Switch(kindEnum + "FromString(j.get<std::string>())", [&] {
                for (const auto& alt : v.Alternatives) {
                    if (!alt.Fields.empty()) {
                        continue;
                    }
                    w.Line("case " + kindEnum + "::" + alt.Name + ": return " +
                           PayloadStructName(v.Name, alt.Name) + "{};");
                }
                w.Line("default:");
                w.Indented([&] {
                    w.Line("throw std::runtime_error(\"parameterized " + v.Name +
                           " given without parameters\");");
                });
            });
        });
        w.Block("if (j.is_object() && j.size() == 1) {", "}", [&] {
            w.Line("auto it = j.begin();");
            w.Line("const std::string& key = it.key();");
            w.Line("const nlohmann::json& val = it.value();");
            w.Switch(kindEnum + "FromString(key)", [&] {
                for (const auto& alt : v.Alternatives) {
                    if (alt.Fields.empty()) {
                        continue;
                    }
                    const std::string structName = PayloadStructName(v.Name, alt.Name);
                    std::vector<TAltFieldInfo> infos = ClassifyAltFields(alt, symbols);
                    w.Case(kindEnum + "::" + alt.Name, [&] {
                        w.Line(structName + " a;");
                        if (infos.size() == 1) {
                            const auto& [f, kind] = infos.front();
                            w.Line("a." + f->Name + " = " + ScalarParseExpr(*f, kind, "val") + ";");
                        } else {
                            for (const auto& [f, kind] : infos) {
                                std::string fkey = PascalToSnake(f->Name);
                                if (kind == EFieldKind::Primitive && f->Init) {
                                    w.Line("a." + f->Name + " = val.value(\"" + fkey + "\", " +
                                           CppPrimitiveType(*f) + "{" +
                                           DefaultExprToCpp(f->Init->Text, f->TypeName) + "});");
                                } else {
                                    w.Line("a." + f->Name + " = " +
                                           ScalarParseExpr(*f, kind, "val.at(\"" + fkey + "\")") + ";");
                                }
                            }
                        }
                        w.Line("return a;");
                    });
                }
                w.Line("default:");
                w.Indented([&] {
                    w.Line("throw std::runtime_error(\"flag " + v.Name +
                           " given with parameters\");");
                });
            });
        });
        w.Line("throw std::runtime_error(\"malformed " + v.Name + "\");");
    });
    w.EmptyLine();

    w.Function("TAstNode " + v.Name + "::GetAst([[maybe_unused]] TAstContext& ctx) const", [&] {
        w.Line("TAstNode node = TAstNode::MakeObject(\"" + v.Name + "\");");
        w.Line("AddValueField(node, \"kind\", Kind());");
        w.Switch("Kind()", [&] {
            for (const auto& alt : v.Alternatives) {
                w.Case(kindEnum + "::" + alt.Name, [&] {
                    if (!alt.Fields.empty()) {
                        const std::string structName = PayloadStructName(v.Name, alt.Name);
                        w.Line("const auto& a = std::get<" + structName + ">(Payload_);");
                        for (const auto& info : ClassifyAltFields(alt, symbols)) {
                            const TFieldDecl& f = *info.Field;
                            std::string fkey = PascalToSnake(f.Name);
                            if (info.Kind == EFieldKind::Class || info.Kind == EFieldKind::Variant ||
                                info.Kind == EFieldKind::BoundedQuantity) {
                                w.Line("AddOwnedObject(node, \"" + fkey + "\", a." + f.Name + ", ctx);");
                            } else {
                                w.Line("AddValueField(node, \"" + fkey + "\", a." + f.Name + ");");
                            }
                        }
                    }
                    w.Line("break;");
                });
            }
        });
        w.Line("return node;");
    });
    w.EmptyLine();
}

void EmitImpl(std::ostream& os,
              const TLoadedSchemas& loaded,
              const std::string& sourceName,
              const std::string& headerInclude) {
    TCppWriter w(os);
    const TSchemaModule& mod = loaded.Primary;

    w.Comment("AUTO-GENERATED FROM " + sourceName + " -- DO NOT EDIT.");
    w.Include(headerInclude);
    w.EmptyLine();
    w.Include("pf2e_engine/common/ast/ast_helpers.h");
    w.Include("pf2e_engine/dsl/property_registry.h");
    w.Include("pf2e_engine/dsl/value.h");
    w.Include("pf2e_engine/game_object_logic/game_object_factory.h");
    w.Include("pf2e_engine/game_object_logic/game_object_id.h");
    w.EmptyLine();
    w.Include("nlohmann/json.hpp");
    w.EmptyLine();
    w.Include("stdexcept");
    w.Include("string");
    if (!mod.Classes.empty()) {
        w.Include("utility");
    }
    w.EmptyLine();

    std::unordered_map<std::string, const TClassDecl*> classes;
    for (const auto& [stem, module] : loaded.ByStem) {
        for (const auto& c : module.Classes) {
            classes[c.Name] = &c;
        }
    }

    for (const auto& e : mod.Enums) {
        EmitEnumImpl(w, e);
    }
    for (const auto& v : mod.Variants) {
        EmitVariantImpl(w, v, loaded.SymbolTable);
    }
    for (const auto& c : mod.Classes) {
        EmitClassImpl(w, c, loaded.SymbolTable, classes);
    }
}
