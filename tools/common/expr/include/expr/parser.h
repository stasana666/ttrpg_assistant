#pragma once


#include <expr/ast.h>

#include <string>

namespace expr {

TExprNode Parse(const std::string& src);

}
