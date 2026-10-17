#pragma once

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/transformation/transformator.h>

#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>

struct TField {
    TField(int value) : Text(std::to_string(value)) {}
    TField(bool value) : Text(value ? "1" : "0") {}
    TField(const char* value) : Text(value) {}
    TField(std::string value) : Text(std::move(value)) {}

    std::string Text;
};

using TFields = std::map<std::string, std::string>;

inline std::string PlainAstValue(const std::string& value)
{
    for (std::string_view prefix : {"int:", "u64:", "bool:", "enum:", "string:", "path:"}) {
        if (value.starts_with(prefix)) {
            return value.substr(prefix.size());
        }
    }
    return value;
}

inline TFields AstFields(const TAstNode& snapshot)
{
    TFields result;
    for (auto& [path, value] : snapshot.Leaves()) {
        if (value == "{}") {
            continue;
        }
        if (path.ends_with(".kind")) {
            path.resize(path.size() - 5);
            value = "1";
        }
        result.emplace(std::move(path), PlainAstValue(value));
    }
    return result;
}

inline TFields ChangedFields(const TFields& before, TFields after)
{
    for (const auto& [path, value] : before) {
        if (!after.contains(path)) {
            after.emplace(path, "<removed>");
        } else if (after.at(path) == value) {
            after.erase(path);
        }
    }
    return after;
}

inline TFields ExpectedFields(const std::map<std::string, TField>& changes)
{
    TFields result;
    for (const auto& [path, value] : changes) {
        result.emplace(path, value.Text);
    }
    return result;
}

inline void ExpectSame(const TAstNode& expected, const TAstNode& actual, std::string_view what)
{
    EXPECT_TRUE(actual == expected) << what << ": " << expected.DiffWith(actual);
}

template <class T>
TAstNode StateSnapshot(const T& object, const TTransformator& journal)
{
    TAstContext context;
    context.RegisterIdentity(&object, "object");
    auto node = TAstNode::MakeObject("state");
    AddOwnedObject(node, "object", object, context);
    AddOwnedObject(node, "journal", journal, context);
    return node;
}

template <class T>
TFields ObjectFields(const T& object)
{
    TAstContext context;
    context.RegisterIdentity(&object, "object");
    auto node = TAstNode::MakeObject("state");
    AddOwnedObject(node, "object", object, context);
    TFields result;
    for (auto& [path, value] : AstFields(node)) {
        result.emplace(path.substr(std::string_view("object.").size()), std::move(value));
    }
    return result;
}

template <class T>
void ExpectStep(const T& object, TTransformator& journal, const std::function<void()>& operation,
                const std::map<std::string, TField>& changes)
{
    const auto before = StateSnapshot(object, journal);
    const auto fields = ObjectFields(object);
    const auto start = journal.CurrentState();
    operation();
    const auto after = StateSnapshot(object, journal);
    EXPECT_EQ(ChangedFields(fields, ObjectFields(object)), ExpectedFields(changes));
    journal.Undo(start);
    ExpectSame(before, StateSnapshot(object, journal), "undo");
    operation();
    ExpectSame(after, StateSnapshot(object, journal), "repeated operation");
}
