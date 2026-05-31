#include <pf2e_engine/dsl/parser.h>

#include <pf2e_engine/dsl/ast_nodes.h>
#include <pf2e_engine/dsl/lexer.h>

#include <parse/token_stream.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

class TParser {
public:
    explicit TParser(std::vector<TToken> tokens)
        : ts_(std::move(tokens)) {}

    std::unique_ptr<IDslExpression> Parse() {
        auto expr = ParseExpression();
        ts_.Expect(ETokenType::End, "end of expression");
        return expr;
    }

private:
    std::unique_ptr<IDslExpression> ParseExpression() {
        return ParseLogicalOr();
    }

    std::unique_ptr<IDslExpression> ParseLogicalOr() {
        auto lhs = ParseLogicalAnd();
        while (ts_.Peek().kind == ETokenType::Or) {
            ts_.Advance();
            auto rhs = ParseLogicalAnd();
            lhs = std::make_unique<TBinaryExpr>(EBinaryOp::Or, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<IDslExpression> ParseLogicalAnd() {
        auto lhs = ParseEquality();
        while (ts_.Peek().kind == ETokenType::And) {
            ts_.Advance();
            auto rhs = ParseEquality();
            lhs = std::make_unique<TBinaryExpr>(EBinaryOp::And, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<IDslExpression> ParseEquality() {
        auto lhs = ParseComparison();
        while (ts_.Peek().kind == ETokenType::Eq || ts_.Peek().kind == ETokenType::Ne) {
            EBinaryOp op = ts_.Peek().kind == ETokenType::Eq ? EBinaryOp::Eq : EBinaryOp::Ne;
            ts_.Advance();
            auto rhs = ParseComparison();
            lhs = std::make_unique<TBinaryExpr>(op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<IDslExpression> ParseComparison() {
        auto lhs = ParseUnary();
        while (true) {
            EBinaryOp op;
            switch (ts_.Peek().kind) {
                case ETokenType::Lt: op = EBinaryOp::Lt; break;
                case ETokenType::Le: op = EBinaryOp::Le; break;
                case ETokenType::Gt: op = EBinaryOp::Gt; break;
                case ETokenType::Ge: op = EBinaryOp::Ge; break;
                default: return lhs;
            }
            ts_.Advance();
            auto rhs = ParseUnary();
            lhs = std::make_unique<TBinaryExpr>(op, std::move(lhs), std::move(rhs));
        }
    }

    std::unique_ptr<IDslExpression> ParseUnary() {
        if (ts_.Peek().kind == ETokenType::Not) {
            ts_.Advance();
            auto inner = ParseUnary();
            return std::make_unique<TUnaryNotExpr>(std::move(inner));
        }
        return ParsePostfix();
    }

    std::unique_ptr<IDslExpression> ParsePostfix() {
        auto expr = ParsePrimary();
        while (ts_.Match(ETokenType::Dot)) {
            if (ts_.Peek().kind != ETokenType::Identifier) {
                ts_.Throw("dsl parser: expected identifier after '.'");
            }
            std::string name = ts_.Consume().text;
            expr = std::make_unique<TPropertyExpr>(std::move(expr), std::move(name));
        }
        return expr;
    }

    std::unique_ptr<IDslExpression> ParsePrimary() {
        const TToken& t = ts_.Peek();
        if (t.kind == ETokenType::Number) {
            int value = std::stoi(t.text);
            ts_.Advance();
            return std::make_unique<TLiteralExpr>(TDslValue(value));
        }
        if (t.kind == ETokenType::Dollar) {
            ts_.Advance();
            if (ts_.Peek().kind != ETokenType::Identifier) {
                ts_.Throw("dsl parser: expected identifier after '$'");
            }
            std::string name = ts_.Consume().text;
            return std::make_unique<TVariableExpr>(std::move(name));
        }
        if (t.kind == ETokenType::Identifier) {
            std::string name = ts_.Consume().text;
            ts_.Expect(ETokenType::LParen, "'(' after function name");
            std::vector<std::unique_ptr<IDslExpression>> args;
            if (ts_.Peek().kind != ETokenType::RParen) {
                args.push_back(ParseExpression());
                while (ts_.Match(ETokenType::Comma)) {
                    args.push_back(ParseExpression());
                }
            }
            ts_.Expect(ETokenType::RParen, "')'");
            return std::make_unique<TCallExpr>(std::move(name), std::move(args));
        }
        if (t.kind == ETokenType::LParen) {
            ts_.Advance();
            auto inner = ParseExpression();
            ts_.Expect(ETokenType::RParen, "')'");
            return inner;
        }
        ts_.Throw("dsl parser: unexpected token");
    }

    parse::TTokenStream<ETokenType> ts_;
};

}  // namespace

std::unique_ptr<IDslExpression> ParseDsl(std::string_view src)
{
    TParser parser(Tokenize(src));
    return parser.Parse();
}
