#pragma once


#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace parse {

struct TSourceLocation {
    int line = 1;
    int col = 1;
};

inline bool IsIdentStart(char c) {
    return (std::isalpha(static_cast<unsigned char>(c)) != 0) || c == '_';
}

inline bool IsIdentCont(char c) {
    return (std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_';
}

inline bool IsDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

inline bool IsSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

class TScanner {
public:
    explicit TScanner(std::string src) : src_(std::move(src)) {}

    bool Eof() const { return pos_ >= src_.size(); }

    char Peek() const {
        return Eof() ? '\0' : src_[pos_];
    }

    char PeekAt(size_t offset) const {
        return pos_ + offset < src_.size() ? src_[pos_ + offset] : '\0';
    }

    void Advance() {
        if (Eof()) {
            return;
        }
        if (src_[pos_] == '\n') {
            ++line_;
            col_ = 1;
        } else {
            ++col_;
        }
        ++pos_;
    }

    TSourceLocation Loc() const { return {line_, col_}; }

    void SkipWhitespace() {
        while (!Eof() && IsSpace(src_[pos_])) {
            Advance();
        }
    }

    bool SkipLineComment(std::string_view prefix) {
        if (!StartsWith(prefix)) {
            return false;
        }
        while (!Eof() && src_[pos_] != '\n') {
            Advance();
        }
        return true;
    }

    bool Try(char c) {
        if (Peek() == c) {
            Advance();
            return true;
        }
        return false;
    }

    bool Try(std::string_view s) {
        if (!StartsWith(s)) {
            return false;
        }
        for (size_t i = 0; i < s.size(); ++i) {
            Advance();
        }
        return true;
    }

    std::string ScanIdent() {
        std::string out;
        while (!Eof() && IsIdentCont(src_[pos_])) {
            out += src_[pos_];
            Advance();
        }
        return out;
    }

    std::string ScanInteger() {
        std::string out;
        if (Peek() == '-') {
            out += '-';
            Advance();
        }
        while (!Eof() && IsDigit(src_[pos_])) {
            out += src_[pos_];
            Advance();
        }
        return out;
    }

    std::string ScanQuotedString(char quote = '"') {
        TSourceLocation start = Loc();
        if (Peek() != quote) {
            Throw("expected quoted string");
        }
        Advance();
        std::string out;
        while (!Eof() && src_[pos_] != quote) {
            if (src_[pos_] == '\n') {
                ThrowAt(start, "newline inside string literal");
            }
            out += src_[pos_];
            Advance();
        }
        if (Eof()) {
            ThrowAt(start, "unterminated string literal");
        }
        Advance();
        return out;
    }

    [[noreturn]] void Throw(const std::string& msg) const {
        ThrowAt(Loc(), msg);
    }

    [[noreturn]] void ThrowAt(TSourceLocation loc, const std::string& msg) const {
        throw std::runtime_error(
            msg + " at line " + std::to_string(loc.line) +
            ", col " + std::to_string(loc.col));
    }

private:
    bool StartsWith(std::string_view s) const {
        if (pos_ + s.size() > src_.size()) {
            return false;
        }
        for (size_t i = 0; i < s.size(); ++i) {
            if (src_[pos_ + i] != s[i]) {
                return false;
            }
        }
        return true;
    }

    std::string src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
};

}
