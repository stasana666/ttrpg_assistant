#pragma once

#include <cstddef>

// Layout guard. Pair these inside GetAst() with:
//   1. AST_ASSERT_LAYOUT(Class, kExpectedSize): catches any sizeof change.
//   2. AST_ASSERT_OFFSET(Class, field, kExpectedOffset): catches reordering
//      of non-reference fields.
//   3. A trailing sentinel: declare
//          [[maybe_unused]] char ast_layout_sentinel_[1] = {};
//      as the last private member, then add
//          AST_ASSERT_OFFSET(Class, ast_layout_sentinel_, kExpectedSentinelOffset);
//      to catch fields added between the last real field and the sentinel
//      (when alignment hides the change from sizeof).
//
// Reference members cannot use offsetof (undefined behavior). For classes
// holding references, rely on AST_ASSERT_LAYOUT + the trailing sentinel.

#define AST_ASSERT_LAYOUT(Class, ExpectedSize) \
    static_assert(sizeof(Class) == (ExpectedSize), \
        "AST: sizeof(" #Class ") changed. Re-audit " #Class "::GetAst(), " \
        "update the expected-size constant, and add any new field with the " \
        "correct ownership category (AddValueField / AddOwnedObject / AddReference).")

#define AST_ASSERT_OFFSET(Class, Field, ExpectedOffset) \
    static_assert(offsetof(Class, Field) == (ExpectedOffset), \
        "AST: offsetof(" #Class "::" #Field ") changed. A field was reordered " \
        "or had its layout changed; re-audit AST and update expected offsets.")

// Combined check: both sizeof (catches alignment changes) AND sentinel offset
// (catches new fields added between the last real field and the sentinel,
// when alignment hides the change from sizeof). Use this — not bare
// AST_ASSERT_LAYOUT — for any class with a trailing ast_layout_sentinel_.
#define AST_ASSERT_LAYOUT_WITH_SENTINEL(Class, ExpectedSize, ExpectedSentinelOffset) \
    AST_ASSERT_LAYOUT(Class, ExpectedSize); \
    AST_ASSERT_OFFSET(Class, ast_layout_sentinel_, ExpectedSentinelOffset)
