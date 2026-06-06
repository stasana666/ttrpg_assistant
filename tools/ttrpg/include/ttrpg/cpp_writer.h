#pragma once


#include <functional>
#include <ostream>
#include <string>
#include <string_view>

class TCppWriter {
public:
    explicit TCppWriter(std::ostream& os) : os_(os) {}

    TCppWriter& Line(std::string_view text);
    TCppWriter& EmptyLine();
    TCppWriter& Comment(std::string_view text);
    TCppWriter& Include(std::string_view path, bool angled = true);

    void Indented(const std::function<void()>& body);

    void Block(std::string_view opener, std::string_view closer,
               const std::function<void()>& body);

    void Class(std::string_view name, const std::function<void()>& body);
    void Struct(std::string_view name, const std::function<void()>& body);
    void Function(std::string_view signature, const std::function<void()>& body);
    void Switch(std::string_view expr, const std::function<void()>& body);
    void Case(std::string_view label, const std::function<void()>& body);

    void PublicSection(const std::function<void()>& body);
    void PrivateSection(const std::function<void()>& body);

private:
    void WriteIndentedLine(std::string_view text);
    void Section(std::string_view label, const std::function<void()>& body);

    std::ostream& os_;
    int indent_ = 0;
    static constexpr int kIndentWidth = 4;
};
