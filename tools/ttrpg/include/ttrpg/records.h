#pragma once

#include <ttrpg/module.h>

#include <string>

using TRecordInfo = TDeclarationInfo<TRecordDecl>;

TDeclarations<TRecordDecl> RecordSymbols(const TLoadedSchemas& loaded);
TRecordInfo ResolveRecord(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded);
std::string RecordTagName(const TRecordDecl& declaration);
void ValidateRecordDefinitions(const TLoadedSchemas& loaded);
