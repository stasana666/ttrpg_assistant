#pragma once

#include <ttrpg/module.h>

#include <string>
#include <unordered_map>

struct TEventInfo {
    std::string OwnerStem;
    const TEventDecl* Declaration;
};

std::unordered_map<std::string, TEventInfo> EventSymbols(const TLoadedSchemas& loaded);
void ValidateEventDefinitions(const TLoadedSchemas& loaded);
std::string EventTagName(const TEventDecl& declaration);
std::string EventArgumentCppType(const std::string& type, const TLoadedSchemas& loaded);
