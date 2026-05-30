#include <pf2e_engine/dsl/parser.h>

#include <pf2e_engine/dsl/ast_nodes.h>
#include <pf2e_engine/dsl/lexer.h>

#include <stdexcept>
#include <vector>

namespace {

class TParser {
public:
    explicit TParser(std::vector<TToken> tokens)
        : tokens_(std::move(tokens)) {}

    std::unique_ptr<IDslExpression> Parse() {
        auto expr = ParseExpression();
        Expect(ETokenType::End);
        return expr;
    }

private:
    const TToken& Peek() const { return tokens_[pos_]; }
    const TToken& Consume() { return tokens_[pos_++]; }
    bool Match(ETokenType t) {
        if (Peek().type == t) { ++pos_; return true; }
        return false;
    }
    void Expect(ETokenType t) {
        if (Peek().type != t) {
            throw std::runtime_error("dsl parser: expected token, got '" + Peek().text + "'");
        }
        ++pos_;
    }

    std::unique_ptr<IDslExpression> ParseExpression() {
        return ParseLogicalOr();
    }

    std::unique_ptr<IDslExpression> ParseLogicalOr() {
        auto lhs = ParseLogicalAnd();
        while (Peek().type == ETokenType::Or) {
            Consume();
            auto rhs = ParseLogicalAnd();
            lhs = std::make_unique<TBinaryExpr>(EBinaryOp::Or, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<IDslExpression> ParseLogicalAnd() {
        auto lhs = ParseEquality();
        while (Peek().type == ETokenType::And) {
            Consume();
            auto rhs = ParseEquality();
            lhs = std::make_unique<TBinaryExpr>(EBinaryOp::And, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<IDslExpression> ParseEquality() {
        auto lhs = ParseComparison();
        while (Peek().type == ETokenType::Eq || Peek().type == ETokenType::Ne) {
            EBinaryOp op = Peek().type == ETokenType::Eq ? EBinaryOp::Eq : EBinaryOp::Ne;
            Consume();
            auto rhs = ParseComparison();
            lhs = std::make_unique<TBinaryExpr>(op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<IDslExpression> ParseComparison() {
        auto lhs = ParseUnary();
        while (true) {
            EBinaryOp op;
            switch (Peek().type) {
                case ETokenType::Lt: op = EBinaryOp::Lt; break;
                case ETokenType::Le: op = EBinaryOp::Le; break;
                case ETokenType::Gt: op = EBinaryOp::Gt; break;
                case ETokenType::Ge: op = EBinaryOp::Ge; break;
                default: return lhs;
            }
            Consume();
            auto rhs = ParseUnary();
            lhs = std::make_unique<TBinaryExpr>(op, std::move(lhs), std::move(rhs));
        }
    }

    std::unique_ptr<IDslExpression> ParseUnary() {
        if (Peek().type == ETokenType::Not) {
            Consume();
            auto inner = ParseUnary();
            return std::make_unique<TUnaryNotExpr>(std::move(inner));
        }
        return ParsePostfix();
    }

    std::unique_ptr<IDslExpression> ParsePostfix() {
        auto expr = ParsePrimary();
        while (Match(ETokenType::Dot)) {
            if (Peek().type != ETokenType::Identifier) {
                throw std::runtime_error("dsl parser: expected identifier after '.'");
            }
            std::string name = Consume().text;
            expr = std::make_unique<TPropertyExpr>(std::move(expr), std::move(name));
        }
        return expr;
    }

    std::unique_ptr<IDslExpression> ParsePrimary() {
        const TToken& t = Peek();
        if (t.type == ETokenType::Number) {
            Consume();
            return std::make_unique<TLiteralExpr>(TDslValue(t.number));
        }
        if (t.type == ETokenType::Dollar) {
            Consume();
            if (Peek().type != ETokenType::Identifier) {
                throw std::runtime_error("dsl parser: expected identifier after '$'");
            }
            std::string name = Consume().text;
            return std::make_unique<TVariableExpr>(std::move(name));
        }
        if (t.type == ETokenType::Identifier) {
            std::string name = Consume().text;
            Expect(ETokenType::LParen);
            std::vector<std::unique_ptr<IDslExpression>> args;
            if (Peek().type != ETokenType::RParen) {
                args.push_back(ParseExpression());
                while (Match(ETokenType::Comma)) {
                    args.push_back(ParseExpression());
                }
            }
            Expect(ETokenType::RParen);
            return std::make_unique<TCallExpr>(std::move(name), std::move(args));
        }
        if (t.type == ETokenType::LParen) {
            Consume();
            auto inner = ParseExpression();
            Expect(ETokenType::RParen);
            return inner;
        }
        throw std::runtime_error("dsl parser: unexpected token '" + t.text + "'");
    }

    std::vector<TToken> tokens_;
    size_t pos_ = 0;
};

}  // namespace

std::unique_ptr<IDslExpression> ParseDsl(std::string_view src)
{
    TParser parser(Tokenize(src));
    return parser.Parse();
}
