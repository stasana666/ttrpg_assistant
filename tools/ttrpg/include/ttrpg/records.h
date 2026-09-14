#pragma once

#include <ttrpg/module.h>

#include <string>
#include <unordered_map>

struct TRecordInfo {
    std::string OwnerStem;
    const TRecordDecl* Declaration;
};

std::unordered_map<std::string, TRecordInfo> RecordSymbols(const TLoadedSchemas& loaded);
TRecordInfo ResolveRecord(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded);
std::string RecordTagName(const TRecordDecl& declaration);
void ValidateRecordDefinitions(const TLoadedSchemas& loaded);
