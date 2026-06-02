#pragma once

// C++ emission: turn a loaded schema module into a generated header / source.
// `EmitHeader` writes the declarations; `EmitImpl` writes the definitions.

#include <ttrpg/module.h>

#include <ostream>
#include <string>

void EmitHeader(std::ostream& os,
                const TLoadedSchemas& loaded,
                const std::string& sourceName,
                const std::string& primaryOutH);

void EmitImpl(std::ostream& os,
              const TLoadedSchemas& loaded,
              const std::string& sourceName,
              const std::string& headerInclude);
