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
    e.Location = lhs.Location;
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
        if (ts_.Peek().kind == ETok::Bang || ts_.Peek().kind == ETok::Minus) {
            const auto token = ts_.Consume();
            TExprNode e;
            e.Kind = ENodeKind::Unary;
            e.UnOp = token.kind == ETok::Bang ? EUnaryOp::Not : EUnaryOp::Negate;
            e.Location = token.loc;
            e.Lhs = std::make_shared<TExprNode>(ParseUnary());
            return e;
        }
        return ParsePostfix();
    }

    TExprNode ParsePostfix() {
        TExprNode e = ParsePrimary();
        while (true) {
            if (ts_.Match(ETok::Dot)) {
                if (ts_.Peek().kind != ETok::Ident) {
                    ts_.Throw("expr parser: expected identifier after '.'");
                }
                const auto token = ts_.Consume();
                TExprNode member;
                member.Kind = ENodeKind::Member;
                member.Text = token.text;
                member.Location = token.loc;
                member.Lhs = std::make_shared<TExprNode>(std::move(e));
                e = std::move(member);
                continue;
            }
            if (ts_.Match(ETok::LParen)) {
                TExprNode call;
                call.Kind = ENodeKind::Call;
                call.Location = e.Location;
                if (e.Kind == ENodeKind::Var && !e.HasDollar) {
                    call.Text = std::move(e.Text);
                } else {
                    call.Lhs = std::make_shared<TExprNode>(std::move(e));
                }
                if (ts_.Peek().kind != ETok::RParen) {
                    call.Args.push_back(ParseOr());
                    while (ts_.Match(ETok::Comma)) {
                        call.Args.push_back(ParseOr());
                    }
                }
                ts_.Expect(ETok::RParen, "')'");
                e = std::move(call);
                continue;
            }
            if (ts_.Match(ETok::LBracket)) {
                TExprNode index;
                index.Kind = ENodeKind::Index;
                index.Location = e.Location;
                index.Lhs = std::make_shared<TExprNode>(std::move(e));
                index.Rhs = std::make_shared<TExprNode>(ParseOr());
                ts_.Expect(ETok::RBracket, "']'");
                e = std::move(index);
                continue;
            }
            return e;
        }
    }

    TExprNode ParseObjectInitializer(std::string name, parse::TSourceLocation location) {
        TExprNode e;
        e.Kind = ENodeKind::ObjectInit;
        e.Text = std::move(name);
        e.Location = location;
        while (!ts_.Match(ETok::RBrace)) {
            if (ts_.Peek().kind != ETok::Ident) {
                ts_.Throw("expr parser: expected field name in object initializer");
            }
            const auto field = ts_.Consume();
            ts_.Expect(ETok::Assign, "'='");
            e.Fields.push_back({field.text, ParseOr(), field.loc});
            ts_.Expect(ETok::Semicolon, "';'");
        }
        return e;
    }

    TExprNode ParsePrimary() {
        const TToken& t = ts_.Peek();
        if (t.kind == ETok::LBrace) {
            const auto location = t.loc;
            ts_.Advance();
            return ParseObjectInitializer({}, location);
        }
        if (t.kind == ETok::LBracket) {
            TExprNode e;
            e.Kind = ENodeKind::ListLiteral;
            e.Location = t.loc;
            ts_.Advance();
            if (ts_.Peek().kind != ETok::RBracket) {
                do {
                    e.Args.push_back(ParseOr());
                } while (ts_.Match(ETok::Comma) && ts_.Peek().kind != ETok::RBracket);
            }
            ts_.Expect(ETok::RBracket, "']'");
            return e;
        }
        if (t.kind == ETok::IntLiteral || t.kind == ETok::StringLiteral) {
            TExprNode e;
            e.Kind = t.kind == ETok::IntLiteral ? ENodeKind::IntLiteral : ENodeKind::StringLiteral;
            e.Text = t.text;
            e.Location = t.loc;
            ts_.Advance();
            return e;
        }
        if (t.kind == ETok::Dollar || t.kind == ETok::At) {
            const auto prefix = t;
            ts_.Advance();
            if (ts_.Peek().kind != ETok::Ident) {
                ts_.Throw("expr parser: expected identifier after '" + prefix.text + "'");
            }
            TExprNode e;
            e.Kind = prefix.kind == ETok::Dollar ? ENodeKind::Var : ENodeKind::ContextVar;
            e.HasDollar = prefix.kind == ETok::Dollar;
            e.Location = prefix.loc;
            e.Text = ts_.Consume().text;
            return e;
        }
        if (t.kind == ETok::Ident) {
            const auto location = t.loc;
            std::string name = ts_.Consume().text;
            if (ts_.Match(ETok::LBrace)) {
                return ParseObjectInitializer(std::move(name), location);
            }
            TExprNode e;
            e.Kind = ENodeKind::Var;
            e.HasDollar = false;
            e.Text = std::move(name);
            e.Location = location;
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
