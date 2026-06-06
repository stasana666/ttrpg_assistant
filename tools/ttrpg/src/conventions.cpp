#include <ttrpg/conventions.h>

#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

std::string PascalToSnake(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (std::isupper(static_cast<unsigned char>(c))) {
            if (i > 0) {
                out += '_';
            }
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            out += c;
        }
    }
    return out;
}

bool IsBuiltinInt(const std::string& t) { return t == "int"; }
bool IsBuiltinBool(const std::string& t) { return t == "bool"; }
bool IsBuiltinString(const std::string& t) { return t == "string"; }
bool IsBuiltinPrimitive(const std::string& t) {
    return IsBuiltinInt(t) || IsBuiltinBool(t) || IsBuiltinString(t);
}

bool IsBuiltinBoundedQuantity(const std::string& t) { return t == "BoundedQuantity"; }

std::string CppTypeFor(const std::string& schemaType) {
    if (schemaType == "string") {
        return "std::string";
    }
    if (schemaType == "BoundedQuantity") {
        return "TBoundedQuantity";
    }
    return schemaType;
}

EFieldKind FieldKindOf(const TFieldDecl& f,
                       const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    if (IsBuiltinPrimitive(f.TypeName)) {
        return EFieldKind::Primitive;
    }
    if (IsBuiltinBoundedQuantity(f.TypeName)) {
        return EFieldKind::BoundedQuantity;
    }
    auto it = symbols.find(f.TypeName);
    if (it == symbols.end()) {
        throw std::runtime_error("unknown type '" + f.TypeName + "'");
    }
    switch (it->second.Kind) {
        case ETypeKind::Enum:    return EFieldKind::Enum;
        case ETypeKind::Variant: return EFieldKind::Variant;
        case ETypeKind::Class:   return EFieldKind::Class;
    }
    throw std::runtime_error("unreachable: unknown ETypeKind");
}

bool IsDslSupported(const TFieldDecl& f) {
    if (f.Container != EContainer::None) {
        return false;
    }
    return IsBuiltinInt(f.TypeName) || IsBuiltinBool(f.TypeName);
}

std::string DefaultExprToCpp(const std::string& expr, const std::string& schemaType) {
    if (expr == "max_int") {
        return "std::numeric_limits<int>::max()";
    }
    if (expr == "min_int") {
        return "std::numeric_limits<int>::min()";
    }
    if (expr == "true" || expr == "false") {
        return expr;
    }
    if (!expr.empty() && (expr[0] == '-' || std::isdigit(static_cast<unsigned char>(expr[0])))) {
        return expr;
    }
    return schemaType + "::" + expr;
}

std::string CppPrimitiveType(const TFieldDecl& f) {
    if (IsBuiltinBool(f.TypeName)) {
        return "bool";
    }
    if (IsBuiltinString(f.TypeName)) {
        return "std::string";
    }
    return "int";
}

std::string ScalarParseExpr(const TFieldDecl& f,
                            EFieldKind kind,
                            const std::string& valExpr)
{
    switch (kind) {
        case EFieldKind::Primitive:
            return valExpr + ".get<" + CppPrimitiveType(f) + ">()";
        case EFieldKind::Enum:
            return f.TypeName + "FromString(" + valExpr + ".get<std::string>())";
        case EFieldKind::Class:
            return "factory.Create<" + f.TypeName +
                   ">(TGameObjectIdManager::Instance().Register(" + valExpr +
                   ".get<std::string>()))";
        case EFieldKind::Variant:
            return f.TypeName + "::FromJson(" + valExpr + ", factory)";
        case EFieldKind::BoundedQuantity:
            return "TBoundedQuantity::FromJson(" + valExpr + ", factory)";
    }
    throw std::runtime_error("unreachable: unknown EFieldKind");
}

std::string LoadFieldCall(const TFieldDecl& f,
                          EFieldKind kind,
                          const std::string& jsonKey)
{
    if (kind == EFieldKind::Primitive && f.Init) {
        return "j.value(\"" + jsonKey + "\", " +
               CppPrimitiveType(f) + "{" + DefaultExprToCpp(f.Init->Text, f.TypeName) +
               "})";
    }
    return ScalarParseExpr(f, kind, "j.at(\"" + jsonKey + "\")");
}

std::string DeriveSiblingInclude(const std::string& primaryOutH, const std::string& stem) {
    const std::string marker = "/include/";
    auto pos = primaryOutH.rfind(marker);
    std::string base;
    if (pos != std::string::npos) {
        base = primaryOutH.substr(pos + marker.size());
    } else {
        base = fs::path(primaryOutH).filename().string();
    }
    fs::path basePath(base);
    return (basePath.parent_path() / (stem + ".h")).generic_string();
}

std::string VariantKindEnum(const std::string& variantName) {
    std::string base = variantName;
    if (!base.empty() && base[0] == 'T') {
        base = base.substr(1);
    }
    return "E" + base + "Kind";
}

std::string PayloadStructName(const std::string& variantName, const std::string& altName) {
    return variantName + altName;
}

bool IsVariantType(const std::string& typeName,
                   const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    auto it = symbols.find(typeName);
    return it != symbols.end() && it->second.Kind == ETypeKind::Variant;
}

std::string CppMemberType(const TFieldDecl& f,
                          const std::unordered_map<std::string, TTypeInfo>& symbols)
{
    switch (f.Container) {
        case EContainer::None:
            return CppTypeFor(f.TypeName);
        case EContainer::Set:
            if (IsVariantType(f.TypeName, symbols)) {
                return "TVariantMap<" + VariantKindEnum(f.TypeName) + ", " + f.TypeName + ">";
            }
            return "std::set<" + f.TypeName + ">";
    }
    return CppTypeFor(f.TypeName);
}
