#pragma once

// Backend-agnostic expression AST shared by the `.ttrpg` code generator (which
// lowers it to C++) and the runtime DSL (which evaluates it). It references no
// engine or codegen types -- each backend interprets the nodes it supports and
// rejects the rest.

#include <memory>
#include <string>
#include <vector>

namespace expr {

enum class EBinaryOp {
    Add, Sub, Mul, Div,        // arithmetic
    Or, And,                   // logical
    Eq, Ne, Lt, Le, Gt, Ge,    // comparison / equality
};

enum class EUnaryOp {
    Not,
};

enum class ENodeKind {
    IntLiteral,   // Text holds the literal (may have a leading '-')
    Var,          // Text = identifier; HasDollar = was it written `$name`
    Member,       // Lhs = receiver, Text = member name
    Call,         // Text = function name, Args = arguments
    Unary,        // UnOp, Lhs = operand
    Binary,       // BinOp, Lhs, Rhs
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

}  // namespace expr
