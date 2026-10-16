#pragma once

#include <ttrpg/module.h>

#include <string>

using TRelationInfo = TDeclarationInfo<TRelationDecl>;

TDeclarations<TRelationDecl> RelationSymbols(const TLoadedSchemas& loaded);
TRelationInfo ResolveRelation(const std::string& name, const std::string& callerStem, const TLoadedSchemas& loaded);
std::string RelationTagName(const TRelationDecl& declaration);
void ValidateRelationDefinitions(const TLoadedSchemas& loaded);
bool UsesDurationSyntax(const TSchemaModule& module);
