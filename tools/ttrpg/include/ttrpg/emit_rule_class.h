#pragma once

#include <ttrpg/cpp_writer.h>
#include <ttrpg/program.h>

void EmitRuleClassDecl(TCppWriter& writer, const TClassDecl& declaration,
                       const TCompiledClass& compiled, const TLoadedSchemas& loaded);
void EmitRuleClassImpl(TCppWriter& writer, const TClassDecl& declaration,
                       const TCompiledClass& compiled, const TLoadedSchemas& loaded);
void EmitRuleJsonValue(TCppWriter& writer, const TFieldDecl& field,
                       const std::string& source, const std::string& target,
                       const TLoadedSchemas& loaded);
