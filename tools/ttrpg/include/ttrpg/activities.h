#pragma once

#include <ttrpg/module.h>

#include <string>

using TActivityInfo = TDeclarationInfo<TActivityDecl>;

TDeclarations<TActivityDecl> ActivitySymbols(const TLoadedSchemas& loaded);
TActivityInfo ResolveActivity(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded);
std::string ActivityFunctionName(const std::string& ownerStem, const TActivityDecl& declaration);
void ValidateActivityDefinitions(const TLoadedSchemas& loaded);

std::string ActivityClassResultDependency(const std::string& name, const TLoadedSchemas& loaded);
