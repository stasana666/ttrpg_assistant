#pragma once

#include <ttrpg/cpp_writer.h>
#include <ttrpg/program.h>

void EmitReactionDeclarations(TCppWriter& writer, const TLoadedSchemas& loaded);
void EmitReactionImplementations(TCppWriter& writer, const TLoadedSchemas& loaded);
void EmitReactionRegistration(TCppWriter& writer, const TAbilityDecl& declaration, const TLoadedSchemas& loaded);
