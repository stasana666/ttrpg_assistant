#pragma once

#include <ttrpg/cpp_writer.h>
#include <ttrpg/program.h>

void EmitInitialValueResolver(TCppWriter& writer, const TClassDecl& declaration,
                              const TCompiledClass& compiled, const TLoadedSchemas& loaded);

void EmitRuleClassDecl(TCppWriter& writer, const TClassDecl& declaration,
                       const TCompiledClass& compiled, const TLoadedSchemas& loaded);
void EmitRuleClassImpl(TCppWriter& writer, const TClassDecl& declaration,
                       const TCompiledClass& compiled, const TLoadedSchemas& loaded);
