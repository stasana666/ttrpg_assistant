#pragma once


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
};

enum class ENodeKind {
    IntLiteral,
    Var,
    Member,
    Call,
    Unary,
    Binary,
};

struct TExprNode {
    ENodeKind Kind = ENodeKind::IntLiteral;
    std::string Text;
    bool HasDollar = false;
    EBinaryOp BinOp = EBinaryOp::Add;
    EUnaryOp UnOp = EUnaryOp::Not;
    std::shared_ptr<TExprNode> Lhs;
    std::shared_ptr<TExprNode> Rhs;
    std::vector<TExprNode> Args;
};

}
