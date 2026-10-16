#pragma once

#include <ttrpg/module.h>

#include <string>

using TEventInfo = TDeclarationInfo<TEventDecl>;

TDeclarations<TEventDecl> EventSymbols(const TLoadedSchemas& loaded);
void ValidateEventDefinitions(const TLoadedSchemas& loaded);
std::string EventTagName(const TEventDecl& declaration);
std::string EventArgumentCppType(const std::string& type, const TLoadedSchemas& loaded);
