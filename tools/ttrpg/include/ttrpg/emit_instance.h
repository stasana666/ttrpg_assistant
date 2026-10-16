#pragma once

#include <ttrpg/constructors.h>
#include <ttrpg/cpp_writer.h>

void EmitInstanceDeclaration(TCppWriter& writer, const TInstanceDecl& declaration,
                             const TLoadedSchemas& loaded);
void EmitInstanceImplementation(TCppWriter& writer, const TInstanceDecl& declaration,
                                const TLoadedSchemas& loaded);
