#include <expr/parser.h>

#include <expr/lexer.h>

#include <parse/token_stream.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace expr {
namespace {

TExprNode MakeBinary(EBinaryOp op, TExprNode lhs, TExprNode rhs) {
    TExprNode e;
    e.Kind = ENodeKind::Binary;
    e.BinOp = op;
    e.Lhs = std::make_shared<TExprNode>(std::move(lhs));
    e.Rhs = std::make_shared<TExprNode>(std::move(rhs));
    return e;
}

class TParser {
public:
    explicit TParser(std::vector<TToken> tokens) : ts_(std::move(tokens)) {}

    TExprNode Parse() {
        TExprNode e = ParseOr();
        ts_.Expect(ETok::End, "end of expression");
        return e;
    }

private:
    TExprNode ParseOr() {
        TExprNode lhs = ParseAnd();
        while (ts_.Peek().kind == ETok::PipePipe) {
            ts_.Advance();
            lhs = MakeBinary(EBinaryOp::Or, std::move(lhs), ParseAnd());
        }
        return lhs;
    }

    TExprNode ParseAnd() {
        TExprNode lhs = ParseEquality();
        while (ts_.Peek().kind == ETok::AmpAmp) {
            ts_.Advance();
            lhs = MakeBinary(EBinaryOp::And, std::move(lhs), ParseEquality());
        }
        return lhs;
    }

    TExprNode ParseEquality() {
        TExprNode lhs = ParseComparison();
        while (ts_.Peek().kind == ETok::EqEq || ts_.Peek().kind == ETok::BangEq) {
            EBinaryOp op = ts_.Peek().kind == ETok::EqEq ? EBinaryOp::Eq : EBinaryOp::Ne;
            ts_.Advance();
            lhs = MakeBinary(op, std::move(lhs), ParseComparison());
        }
        return lhs;
    }

    TExprNode ParseComparison() {
        TExprNode lhs = ParseAddSub();
        while (true) {
            EBinaryOp op{};
            switch (ts_.Peek().kind) {
                case ETok::Lt: op = EBinaryOp::Lt; break;
                case ETok::Le: op = EBinaryOp::Le; break;
                case ETok::Gt: op = EBinaryOp::Gt; break;
                case ETok::Ge: op = EBinaryOp::Ge; break;
                default: return lhs;
            }
            ts_.Advance();
            lhs = MakeBinary(op, std::move(lhs), ParseAddSub());
        }
    }

    TExprNode ParseAddSub() {
        TExprNode lhs = ParseMulDiv();
        while (ts_.Peek().kind == ETok::Plus || ts_.Peek().kind == ETok::Minus) {
            EBinaryOp op = ts_.Peek().kind == ETok::Plus ? EBinaryOp::Add : EBinaryOp::Sub;
            ts_.Advance();
            lhs = MakeBinary(op, std::move(lhs), ParseMulDiv());
        }
        return lhs;
    }

    TExprNode ParseMulDiv() {
        TExprNode lhs = ParseUnary();
        while (ts_.Peek().kind == ETok::Star || ts_.Peek().kind == ETok::Slash) {
            EBinaryOp op = ts_.Peek().kind == ETok::Star ? EBinaryOp::Mul : EBinaryOp::Div;
            ts_.Advance();
            lhs = MakeBinary(op, std::move(lhs), ParseUnary());
        }
        return lhs;
    }

    TExprNode ParseUnary() {
        if (ts_.Peek().kind == ETok::Bang) {
            ts_.Advance();
            TExprNode e;
            e.Kind = ENodeKind::Unary;
            e.UnOp = EUnaryOp::Not;
            e.Lhs = std::make_shared<TExprNode>(ParseUnary());
            return e;
        }
        return ParsePostfix();
    }

    TExprNode ParsePostfix() {
        TExprNode e = ParsePrimary();
        while (ts_.Match(ETok::Dot)) {
            if (ts_.Peek().kind != ETok::Ident) {
                ts_.Throw("expr parser: expected identifier after '.'");
            }
            TExprNode m;
            m.Kind = ENodeKind::Member;
            m.Text = ts_.Consume().text;
            m.Lhs = std::make_shared<TExprNode>(std::move(e));
            e = std::move(m);
        }
        return e;
    }

    TExprNode ParsePrimary() {
        const TToken& t = ts_.Peek();
        if (t.kind == ETok::IntLiteral) {
            TExprNode e;
            e.Kind = ENodeKind::IntLiteral;
            e.Text = t.text;
            ts_.Advance();
            return e;
        }
        if (t.kind == ETok::Dollar) {
            ts_.Advance();
            if (ts_.Peek().kind != ETok::Ident) {
                ts_.Throw("expr parser: expected identifier after '$'");
            }
            TExprNode e;
            e.Kind = ENodeKind::Var;
            e.HasDollar = true;
            e.Text = ts_.Consume().text;
            return e;
        }
        if (t.kind == ETok::Ident) {
            std::string name = ts_.Consume().text;
            if (ts_.Peek().kind == ETok::LParen) {
                ts_.Advance();
                TExprNode e;
                e.Kind = ENodeKind::Call;
                e.Text = name;
                if (ts_.Peek().kind != ETok::RParen) {
                    e.Args.push_back(ParseOr());
                    while (ts_.Match(ETok::Comma)) {
                        e.Args.push_back(ParseOr());
                    }
                }
                ts_.Expect(ETok::RParen, "')'");
                return e;
            }
            TExprNode e;
            e.Kind = ENodeKind::Var;
            e.HasDollar = false;
            e.Text = name;
            return e;
        }
        if (t.kind == ETok::LParen) {
            ts_.Advance();
            TExprNode e = ParseOr();
            ts_.Expect(ETok::RParen, "')'");
            return e;
        }
        ts_.Throw("expr parser: unexpected token");
    }

    parse::TTokenStream<ETok> ts_;
};

}

TExprNode Parse(const std::string& src) {
    TParser parser(Tokenize(src));
    return parser.Parse();
}

}
