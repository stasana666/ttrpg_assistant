#pragma once

#include <ttrpg/program.h>

#include <string>
#include <unordered_map>

std::unordered_map<std::string, TAbilityInfo> AbilitySymbols(const TLoadedSchemas& loaded);
TAbilityInfo ResolveAbility(const std::string& name, const TLoadedSchemas& loaded);
std::string AbilityId(const std::string& name, const TLoadedSchemas& loaded);
std::string AbilityRegistrationFunction(const std::string& stem);
std::string RulesRegistrationFunction(const std::string& stem);
void ValidateAbilityDefinitions(const TLoadedSchemas& loaded);
std::vector<TCompiledGrant> CompileGrants(const TClassDecl& declaration, const TLoadedSchemas& loaded);
