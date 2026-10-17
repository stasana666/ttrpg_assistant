#pragma once

#include <ttrpg/cpp_writer.h>
#include <ttrpg/module.h>

void EmitActionRegistration(TCppWriter& writer, const TLoadedSchemas& loaded);
void EmitEventDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded);
