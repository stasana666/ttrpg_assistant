#include <gtest/gtest.h>

#include <ttrpg/cpp_writer.h>

#include <sstream>
#include <string>

namespace {

std::string Render(const std::function<void(TCppWriter&)>& build) {
    std::ostringstream os;
    TCppWriter w(os);
    build(w);
    return os.str();
}

}  // namespace

TEST(CppWriterTest, LineAndEmptyLine) {
    EXPECT_EQ(Render([](TCppWriter& w) {
        w.Line("int x = 1;");
        w.EmptyLine();
        w.Line("int y = 2;");
    }),
        "int x = 1;\n"
        "\n"
        "int y = 2;\n");
}

TEST(CppWriterTest, BlockIndentsBodyAndBalancesBraces) {
    EXPECT_EQ(Render([](TCppWriter& w) {
        w.Function("int f()", [&] {
            w.Line("return 1;");
        });
    }),
        "int f() {\n"
        "    return 1;\n"
        "}\n");
}

TEST(CppWriterTest, ClassWithSectionsPlacesLabelsAtClassIndent) {
    EXPECT_EQ(Render([](TCppWriter& w) {
        w.Class("TFoo", [&] {
            w.PublicSection([&] {
                w.Line("int X() const { return X_; }");
            });
            w.EmptyLine();
            w.PrivateSection([&] {
                w.Line("int X_{};");
            });
        });
    }),
        "class TFoo {\n"
        "public:\n"
        "    int X() const { return X_; }\n"
        "\n"
        "private:\n"
        "    int X_{};\n"
        "};\n");
}

TEST(CppWriterTest, StructIndentsItsBody) {
    EXPECT_EQ(Render([](TCppWriter& w) {
        w.Struct("TPayload", [&] {
            w.Line("int A{};");
        });
    }),
        "struct TPayload {\n"
        "    int A{};\n"
        "};\n");
}

TEST(CppWriterTest, SwitchAndCaseNest) {
    EXPECT_EQ(Render([](TCppWriter& w) {
        w.Switch("kind", [&] {
            w.Case("EFoo::A", [&] {
                w.Line("return 1;");
            });
        });
    }),
        "switch (kind) {\n"
        "    case EFoo::A: {\n"
        "        return 1;\n"
        "    }\n"
        "}\n");
}

TEST(CppWriterTest, NestedScopesRestoreIndent) {
    EXPECT_EQ(Render([](TCppWriter& w) {
        w.Class("TOuter", [&] {
            w.PublicSection([&] {
                w.Function("void M()", [&] {
                    w.Line("body();");
                });
            });
        });
        w.Line("after();");  // back at column 0
    }),
        "class TOuter {\n"
        "public:\n"
        "    void M() {\n"
        "        body();\n"
        "    }\n"
        "};\n"
        "after();\n");
}

TEST(CppWriterTest, IndentedContinuationLines) {
    EXPECT_EQ(Render([](TCppWriter& w) {
        w.Line("using T = std::variant<");
        w.Indented([&] {
            w.Line("A,");
            w.Line("B>;");
        });
    }),
        "using T = std::variant<\n"
        "    A,\n"
        "    B>;\n");
}

TEST(CppWriterTest, CommentAndInclude) {
    EXPECT_EQ(Render([](TCppWriter& w) {
        w.Comment("hello");
        w.Include("a/b.h");
        w.Include("local.h", false);
    }),
        "// hello\n"
        "#include <a/b.h>\n"
        "#include \"local.h\"\n");
}
