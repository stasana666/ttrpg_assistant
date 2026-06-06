#pragma once


#include <ttrpg/schema_ast.h>

#include <expr/ast.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

bool InitIsComputed(const expr::TExprNode& e, const std::unordered_set<std::string>& fieldNames);

std::vector<std::size_t> FieldInitOrder(
    const TClassDecl& c,
    const std::unordered_map<std::string, const TClassDecl*>& classes);

std::string InitExprToCpp(const expr::TExprNode& e);
