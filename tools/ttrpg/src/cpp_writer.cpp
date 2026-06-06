#include <ttrpg/cpp_writer.h>

#include <string>

void TCppWriter::WriteIndentedLine(std::string_view text) {
    for (int i = 0; i < indent_ * kIndentWidth; ++i) {
        os_ << ' ';
    }
    os_ << text << '\n';
}

TCppWriter& TCppWriter::Line(std::string_view text) {
    WriteIndentedLine(text);
    return *this;
}

TCppWriter& TCppWriter::EmptyLine() {
    os_ << '\n';
    return *this;
}

TCppWriter& TCppWriter::Comment(std::string_view text) {
    WriteIndentedLine("// " + std::string(text));
    return *this;
}

TCppWriter& TCppWriter::Include(std::string_view path, bool angled) {
    std::string line = "#include ";
    line += angled ? "<" : "\"";
    line += path;
    line += angled ? ">" : "\"";
    WriteIndentedLine(line);
    return *this;
}

void TCppWriter::Indented(const std::function<void()>& body) {
    ++indent_;
    body();
    --indent_;
}

void TCppWriter::Block(std::string_view opener, std::string_view closer,
                       const std::function<void()>& body) {
    WriteIndentedLine(opener);
    ++indent_;
    body();
    --indent_;
    WriteIndentedLine(closer);
}

void TCppWriter::Class(std::string_view name, const std::function<void()>& body) {
    Block("class " + std::string(name) + " {", "};", body);
}

void TCppWriter::Struct(std::string_view name, const std::function<void()>& body) {
    Block("struct " + std::string(name) + " {", "};", body);
}

void TCppWriter::Function(std::string_view signature, const std::function<void()>& body) {
    Block(std::string(signature) + " {", "}", body);
}

void TCppWriter::Switch(std::string_view expr, const std::function<void()>& body) {
    Block("switch (" + std::string(expr) + ") {", "}", body);
}

void TCppWriter::Case(std::string_view label, const std::function<void()>& body) {
    Block("case " + std::string(label) + ": {", "}", body);
}

void TCppWriter::Section(std::string_view label, const std::function<void()>& body) {
    --indent_;
    WriteIndentedLine(label);
    ++indent_;
    body();
}

void TCppWriter::PublicSection(const std::function<void()>& body) {
    Section("public:", body);
}

void TCppWriter::PrivateSection(const std::function<void()>& body) {
    Section("private:", body);
}
