#pragma once

#include <ttrpg/cpp_writer.h>
#include <ttrpg/module.h>

void EmitRecordDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded);
void EmitRecordImplementations(TCppWriter& writer, const TLoadedSchemas& loaded);
