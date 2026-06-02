#include <ttrpg/emit.h>

#include <ttrpg/conventions.h>
#include <ttrpg/cpp_writer.h>

#include <string>
#include <unordered_set>
#include <vector>

// All emitters take a TCppWriter& and describe *structure* (Class / Function /
// Switch / sections) and *lines*; the writer owns indentation and braces.
// EmitHeader / EmitImpl (the public entry points) wrap the caller's ostream in
// a writer.

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

void EmitClassDecl(TCppWriter& w, const TClassDecl& c,
                   const std::unordered_map<std::string, TTypeInfo>& symbols) {
    w.Class(c.Name, [&] {
        w.PublicSection([&] {
            for (const auto& f : c.Fields) {
                std::string mt = CppMemberType(f, symbols);
                if (f.Container != EContainer::None) {
                    // Return by const-ref to avoid copying the container.
                    w.Line("const " + mt + "& " + f.Name + "() const { return " + f.Name + "_; }");
                } else {
                    w.Line(mt + " " + f.Name + "() const { return " + f.Name + "_; }");
                }
            }
            w.EmptyLine();
            w.Line("static " + c.Name +
                   " FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);");
            w.Line("TAstNode GetAst(TAstContext& ctx) const;");
            w.Line("static void RegisterDslProperties();");
        });
        w.EmptyLine();
        w.PrivateSection([&] {
            for (const auto& f : c.Fields) {
                std::string mt = CppMemberType(f, symbols);
                std::string init;
                if (f.Container != EContainer::None) {
                    init = "{}";  // empty container
                } else if (f.DefaultExpr) {
                    init = "{" + DefaultExprToCpp(*f.DefaultExpr, f.TypeName) + "}";
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

// Emit the header declarations for a `variant`:
//   - the kind/discriminant enum (+ ToString / FromString), via the normal
//     enum emitter so the JSON token casing matches `enum` exactly;
//   - one payload struct per alternative (each carries `static constexpr Kind`);
//   - the wrapper class holding a std::variant of all payload structs.
void EmitVariantDecl(TCppWriter& w, const TVariantDecl& v,
                     const std::unordered_map<std::string, TTypeInfo>& symbols) {
    const std::string kindEnum = VariantKindEnum(v.Name);

    // Kind enum reuses the enum emitter -> identical FromString/ToString casing.
    TEnumDecl kindDecl;
    kindDecl.Name = kindEnum;
    for (const auto& alt : v.Alternatives) {
        kindDecl.Values.push_back(alt.Name);
    }
    EmitEnumDecl(w, kindDecl);

    // One payload struct per alternative.
    for (const auto& alt : v.Alternatives) {
        w.Struct(PayloadStructName(v.Name, alt.Name), [&] {
            w.Line("static constexpr auto Kind = " + kindEnum + "::" + alt.Name + ";");
            for (const auto& f : alt.Fields) {
                std::string mt = CppMemberType(f, symbols);
                std::string init = f.DefaultExpr
                    ? "{" + DefaultExprToCpp(*f.DefaultExpr, f.TypeName) + "}"
                    : "{}";
                w.Line(mt + " " + f.Name + init + ";");
            }
        });
        w.EmptyLine();
    }

    // Wrapper class.
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
            // Converting constructor from any alternative payload. Constrained
            // so it never hijacks the copy/move constructors (which keeps the
            // wrapper copyable -- required for storing it by value in TVariantMap).
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
            w.Comment("Typed query: nullptr if this is not a T.");
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
    w.EmptyLine();
    w.Include("nlohmann/json_fwd.hpp");
    w.EmptyLine();
    w.Include("limits");
    w.Include("string");

    // Scan all field types (class fields + variant alternative fields) to
    // decide which standard headers and cross-file generated headers to pull.
    bool anyEnumSet = false;     // set<Enum> -> std::set
    bool anyVariantSet = false;  // set<Variant> -> TVariantMap
    bool anyVariant = !mod.Variants.empty();

    // Cross-file includes: any external type used as a field type (incl.
    // set element types and variant-payload field types) pulls in the
    // generated header that declares it.
    std::unordered_set<std::string> externalStems;

    auto inspectField = [&](const TFieldDecl& f, const std::string& ownerDesc) {
        if (IsBuiltinPrimitive(f.TypeName)) {
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
    if (anyVariant) {
        w.Include("type_traits");
        w.Include("utility");
        w.Include("variant");
    }
    if (anyVariantSet) {
        w.Include("pf2e_engine/common/variant_map.h");
    }
    w.EmptyLine();

    if (!externalStems.empty()) {
        for (const auto& stem : externalStems) {
            w.Include(DeriveSiblingInclude(primaryOutH, stem));
        }
        w.EmptyLine();
    }

    // Forward decl needed by FromJson signature
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
                   const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    // Pre-classify each scalar field once. Set fields are handled separately
    // by inspecting f.Container.
    std::vector<EFieldKind> kinds;
    kinds.reserve(c.Fields.size());
    bool usesFactory = false;
    for (const auto& f : c.Fields) {
        if (f.Container != EContainer::None) {
            // Sentinel; not used. A set<Variant> loads each element through
            // the variant's FromJson, which takes the factory.
            kinds.push_back(EFieldKind::Primitive);
            if (f.Container == EContainer::Set && IsVariantType(f.TypeName, symbols)) {
                usesFactory = true;
            }
            continue;
        }
        EFieldKind k = FieldKindOf(f, symbols);
        kinds.push_back(k);
        if (k == EFieldKind::Class || k == EFieldKind::Variant) {
            usesFactory = true;
        }
    }

    // ---- FromJson ----
    w.Function(c.Name + " " + c.Name +
               "::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory)", [&] {
        if (!usesFactory) {
            w.Line("(void)factory;");
        }
        w.Line(c.Name + " r;");
        for (size_t i = 0; i < c.Fields.size(); ++i) {
            const auto& f = c.Fields[i];
            std::string key = PascalToSnake(f.Name);
            if (f.Container == EContainer::Set) {
                // Absent key is treated as an empty set.
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
            } else {
                w.Line("r." + f.Name + "_ = " + LoadFieldCall(f, kinds[i], key) + ";");
            }
        }
        w.Line("return r;");
    });
    w.EmptyLine();

    // ---- GetAst ----
    w.Function("TAstNode " + c.Name + "::GetAst([[maybe_unused]] TAstContext& ctx) const", [&] {
        w.Line("TAstNode node = TAstNode::MakeObject(\"" + c.Name + "\");");
        for (size_t i = 0; i < c.Fields.size(); ++i) {
            const auto& f = c.Fields[i];
            std::string key = PascalToSnake(f.Name);
            if (f.Container == EContainer::Set && IsVariantType(f.TypeName, symbols)) {
                // TVariantMap iterates sorted by kind enum (std::map), so the
                // output is deterministic. Each value is an owned (recursive)
                // node keyed by its kind string.
                w.Block("{", "}", [&] {
                    w.Line("TAstNode set_node = TAstNode::MakeObject(\"container\");");
                    w.Block("for (const auto& [kind, value] : " + f.Name + "_) {", "}", [&] {
                        w.Line("AddOwnedObject(set_node, ToString(kind), value, ctx);");
                    });
                    w.Line("node.AddChild(\"" + key + "\", std::move(set_node));");
                });
            } else if (f.Container == EContainer::Set) {
                // std::set<E> iterates in sorted order by enum value, which is
                // deterministic per declaration order. Emit each as its own
                // value child under a container node keyed by ToString.
                w.Block("{", "}", [&] {
                    w.Line("TAstNode set_node = TAstNode::MakeObject(\"container\");");
                    w.Block("for (auto v : " + f.Name + "_) {", "}", [&] {
                        w.Line("AddValueField(set_node, ToString(v), v);");
                    });
                    w.Line("node.AddChild(\"" + key + "\", std::move(set_node));");
                });
            } else if (kinds[i] == EFieldKind::Class || kinds[i] == EFieldKind::Variant) {
                // Generated class/variant types are TIsAstRecursive::true_type,
                // so use AddOwnedObject which recurses; AddValueField would
                // static_assert.
                w.Line("AddOwnedObject(node, \"" + key + "\", " + f.Name + "_, ctx);");
            } else {
                w.Line("AddValueField(node, \"" + key + "\", " + f.Name + "_);");
            }
        }
        w.Line("return node;");
    });
    w.EmptyLine();

    // ---- RegisterDslProperties ----
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
            } else {
                std::string reasonPrefix = "unsupported type";
                if (f.Container == EContainer::Set) {
                    reasonPrefix = "set field";
                } else {
                    switch (kinds[i]) {
                        case EFieldKind::Class: reasonPrefix = "class field"; break;
                        case EFieldKind::Enum:  reasonPrefix = "enum field"; break;
                        case EFieldKind::Variant: reasonPrefix = "variant field"; break;
                        case EFieldKind::Primitive: reasonPrefix = "unsupported primitive"; break;
                    }
                }
                w.Comment("dsl: '" + key + "' skipped -- " + reasonPrefix +
                          " '" + f.TypeName + "'");
            }
        }
        // r might be unused if the class has no DSL-eligible fields.
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

// Per-alternative field classification, shared by the FromJson and GetAst
// emitters for a variant.
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

    // Kind enum ToString / FromString via the normal enum emitter.
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
            if (k == EFieldKind::Class || k == EFieldKind::Variant) {
                usesFactory = true;
            }
        }
    }

    // ---- FromJson ----
    w.Function(v.Name + " " + v.Name +
               "::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory)", [&] {
        if (!usesFactory) {
            w.Line("(void)factory;");
        }
        // Flag form: a bare string equal to the kind token.
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
        // Object form: single-key {kind: payload}.
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
                            // Single-field: value parsed directly as the field type.
                            const auto& [f, kind] = infos.front();
                            w.Line("a." + f->Name + " = " + ScalarParseExpr(*f, kind, "val") + ";");
                        } else {
                            // Multi-field: value is an object of named fields.
                            for (const auto& [f, kind] : infos) {
                                std::string fkey = PascalToSnake(f->Name);
                                if (kind == EFieldKind::Primitive && f->DefaultExpr) {
                                    w.Line("a." + f->Name + " = val.value(\"" + fkey + "\", " +
                                           CppPrimitiveType(*f) + "{" +
                                           DefaultExprToCpp(*f->DefaultExpr, f->TypeName) + "});");
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

    // ---- GetAst ----
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
                            if (info.Kind == EFieldKind::Class || info.Kind == EFieldKind::Variant) {
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
    w.EmptyLine();

    for (const auto& e : mod.Enums) {
        EmitEnumImpl(w, e);
    }
    for (const auto& v : mod.Variants) {
        EmitVariantImpl(w, v, loaded.SymbolTable);
    }
    for (const auto& c : mod.Classes) {
        EmitClassImpl(w, c, loaded.SymbolTable);
    }
}
