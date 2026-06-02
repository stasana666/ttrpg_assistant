#pragma once

// A small fluent builder for emitting C++ source. It owns indentation and
// brace/scope structure so emitters never write '\n' or leading spaces by hand
// -- they describe *structure* (Class / Struct / Function / Switch / sections)
// and *lines*, and the writer produces correctly-formatted text.
//
// Scope methods take a body lambda: the writer emits the opener at the current
// indent, runs the body one level deeper, then emits the closer. Access-specifier
// sections (public:/private:) place the label one level out from their members,
// matching the surrounding class body.

#include <functional>
#include <ostream>
#include <string>
#include <string_view>

class TCppWriter {
public:
    explicit TCppWriter(std::ostream& os) : os_(os) {}

    TCppWriter& Line(std::string_view text);            // indented text + '\n'
    TCppWriter& EmptyLine();                            // a bare '\n'
    TCppWriter& Comment(std::string_view text);         // "// <text>"
    TCppWriter& Include(std::string_view path, bool angled = true);

    // Run `body` one indent level deeper, with no opener/closer. Use for
    // continuation lines of a single statement (e.g. a wrapped argument list).
    void Indented(const std::function<void()>& body);

    // Generic "<opener>\n  <body>\n<closer>" with the body indented one level.
    void Block(std::string_view opener, std::string_view closer,
               const std::function<void()>& body);

    void Class(std::string_view name, const std::function<void()>& body);    // class X { ... };
    void Struct(std::string_view name, const std::function<void()>& body);   // struct X { ... };
    void Function(std::string_view signature, const std::function<void()>& body); // <sig> { ... }
    void Switch(std::string_view expr, const std::function<void()>& body);   // switch (e) { ... }
    void Case(std::string_view label, const std::function<void()>& body);    // case L: { ... }

    void PublicSection(const std::function<void()>& body);   // "public:" (out one level) + body
    void PrivateSection(const std::function<void()>& body);

private:
    void WriteIndentedLine(std::string_view text);
    void Section(std::string_view label, const std::function<void()>& body);

    std::ostream& os_;
    int indent_ = 0;
    static constexpr int kIndentWidth = 4;
};
