#pragma once

#include <ttrpg/cpp_writer.h>
#include <ttrpg/module.h>

void EmitActivityDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded);
void EmitActivityImplementations(TCppWriter& writer, const TLoadedSchemas& loaded);
