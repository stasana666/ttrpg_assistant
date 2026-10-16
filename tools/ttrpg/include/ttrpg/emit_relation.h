#pragma once

#include <ttrpg/cpp_writer.h>
#include <ttrpg/module.h>

void EmitRelationDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded);
void EmitRelationImplementations(TCppWriter& writer, const TLoadedSchemas& loaded);
