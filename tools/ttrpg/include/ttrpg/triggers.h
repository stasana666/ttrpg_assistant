#pragma once

#include <ttrpg/module.h>

#include <string>
#include <unordered_map>

struct TTriggerInfo {
    std::string OwnerStem;
    const TTriggerDecl* Declaration = nullptr;
};

std::unordered_map<std::string, TTriggerInfo> TriggerSymbols(const TLoadedSchemas& loaded);
TTriggerInfo ResolveTrigger(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded);
std::string TriggerTagName(const TTriggerDecl& declaration);
std::string TriggerArgumentsCppType(const TTriggerDecl& declaration, const TLoadedSchemas& loaded);
std::string ReactionFunctionName(const std::string& stem, const TAbilityDecl& declaration);
void ValidateTriggerDefinitions(const TLoadedSchemas& loaded);
