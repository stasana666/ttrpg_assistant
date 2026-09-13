#pragma once

#include <ttrpg/module.h>

#include <string>
#include <vector>

struct TCompiledField {
    TFieldDecl Declaration;
    std::string CppType;
    std::string Initializer;
};

struct TCompiledMethod {
    TClassDecl::TMethod Declaration;
    std::vector<std::string> Body;
};

struct TCompiledGrant {
    enum class EKind { Definition, Value, Collection };
    EKind Kind = EKind::Definition;
    std::string DefinitionId;
    std::string Accessor;
    std::vector<std::string> Fields;
    std::string Site;
};

struct TCompiledClass {
    std::vector<TCompiledField> Fields;
    std::vector<std::size_t> InitOrder;
    std::vector<TCompiledMethod> Methods;
    std::vector<TCompiledGrant> Grants;
};

bool UsesRuleSyntax(const TClassDecl& declaration, const TLoadedSchemas& loaded);
TCompiledClass CompileClass(const TClassDecl& declaration, const TLoadedSchemas& loaded);
std::string CompileInstance(const TInstanceDecl& declaration, const TLoadedSchemas& loaded);
