#pragma once

#include <ttrpg/module.h>

#include <string>
#include <unordered_map>

struct TActivityInfo {
    std::string OwnerStem;
    const TActivityDecl* Declaration = nullptr;
};

std::unordered_map<std::string, TActivityInfo> ActivitySymbols(const TLoadedSchemas& loaded);
TActivityInfo ResolveActivity(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded);
std::string ActivityFunctionName(const std::string& ownerStem, const TActivityDecl& declaration);
void ValidateActivityDefinitions(const TLoadedSchemas& loaded);
