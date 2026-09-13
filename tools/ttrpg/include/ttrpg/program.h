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

struct TCompiledClass {
    std::vector<TCompiledField> Fields;
    std::vector<std::size_t> InitOrder;
    std::vector<TCompiledMethod> Methods;
};

bool UsesRuleSyntax(const TClassDecl& declaration, const TLoadedSchemas& loaded);
TCompiledClass CompileClass(const TClassDecl& declaration, const TLoadedSchemas& loaded);
std::string CompileInstance(const TInstanceDecl& declaration, const TLoadedSchemas& loaded);
