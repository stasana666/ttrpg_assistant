#pragma once

#include <ttrpg/module.h>

#include <optional>
#include <string>
#include <vector>

struct TConstructorExpression {
    std::string Code;
    std::vector<std::size_t> Dependencies;
};

struct TCompiledConstructorParameter {
    TInstanceDecl::TParameter Declaration;
    std::string CppType;
    std::string OptionCppType;
    std::optional<TConstructorExpression> Default;
    std::optional<TConstructorExpression> Pool;
    std::optional<TConstructorExpression> When;
};

struct TCompiledConstructor {
    std::vector<TCompiledConstructorParameter> Parameters;
    std::vector<TConstructorExpression> Requirements;
    std::string Construction;
};

void ValidateConstructorNames(const TLoadedSchemas& loaded);
TCompiledConstructor CompileConstructor(const TInstanceDecl& declaration, const TLoadedSchemas& loaded);
