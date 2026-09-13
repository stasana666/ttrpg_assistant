#pragma once


#include <parse/scanner.h>

#include <memory>
#include <string>
#include <vector>

namespace expr {

enum class EBinaryOp {
    Add, Sub, Mul, Div,
    Or, And,
    Eq, Ne, Lt, Le, Gt, Ge,
};

enum class EUnaryOp {
    Not,
    Negate,
};

enum class ENodeKind {
    IntLiteral,
    Var,
    Member,
    Call,
    Unary,
    Binary,
    ContextVar,
    ObjectInit,
    Index,
    StringLiteral,
    ListLiteral,
};

struct TFieldInitializer;

struct TExprNode {
    ENodeKind Kind = ENodeKind::IntLiteral;
    std::string Text;
    bool HasDollar = false;
    EBinaryOp BinOp = EBinaryOp::Add;
    EUnaryOp UnOp = EUnaryOp::Not;
    std::shared_ptr<TExprNode> Lhs;
    std::shared_ptr<TExprNode> Rhs;
    std::vector<TExprNode> Args;
    std::vector<TFieldInitializer> Fields;
    parse::TSourceLocation Location;
};

struct TFieldInitializer {
    std::string Name;
    TExprNode Value;
    parse::TSourceLocation Location;
};

}
