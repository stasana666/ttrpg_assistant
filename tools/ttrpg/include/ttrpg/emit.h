#pragma once


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
