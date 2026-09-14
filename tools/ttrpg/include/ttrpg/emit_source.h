#pragma once

#include <ttrpg/cpp_writer.h>
#include <ttrpg/module.h>

void EmitAbilitySourceVisitor(TCppWriter& writer, const TClassDecl& declaration,
                             const std::unordered_map<std::string, TTypeInfo>& symbols);
