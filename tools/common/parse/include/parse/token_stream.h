#pragma once


#include <parse/scanner.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace parse {

template <class Kind>
struct TToken {
    Kind kind;
    std::string text;
    TSourceLocation loc;
};

template <class Kind>
class TTokenStream {
public:
    explicit TTokenStream(std::vector<TToken<Kind>> tokens)
        : tokens_(std::move(tokens)) {}

    const TToken<Kind>& Peek() const { return tokens_[pos_]; }

    const TToken<Kind>& Consume() {
        const TToken<Kind>& t = tokens_[pos_];
        ++pos_;
        return t;
    }

    void Advance() { ++pos_; }

    bool Match(Kind k) {
        if (tokens_[pos_].kind == k) {
            ++pos_;
            return true;
        }
        return false;
    }

    void Expect(Kind k, std::string_view what) {
        if (tokens_[pos_].kind != k) {
            Throw("expected " + std::string(what));
        }
        ++pos_;
    }

    [[noreturn]] void Throw(const std::string& msg) const {
        const TToken<Kind>& t = tokens_[pos_];
        throw std::runtime_error(
            msg + " (got '" + t.text + "' at line " +
            std::to_string(t.loc.line) + ", col " + std::to_string(t.loc.col) + ")");
    }

private:
    std::vector<TToken<Kind>> tokens_;
    size_t pos_ = 0;
};

}
