#pragma once

#include <cstddef>

#define AST_ASSERT_LAYOUT(Class, ExpectedSize) \
    static_assert(sizeof(Class) == (ExpectedSize), \
        "AST: sizeof(" #Class ") changed. See common/ast/CLAUDE.md.")

#define AST_ASSERT_OFFSET(Class, Field, ExpectedOffset) \
    static_assert(offsetof(Class, Field) == (ExpectedOffset), \
        "AST: offsetof(" #Class "::" #Field ") changed. See common/ast/CLAUDE.md.")

#define AST_ASSERT_LAYOUT_WITH_SENTINEL(Class, ExpectedSize, ExpectedSentinelOffset) \
    AST_ASSERT_LAYOUT(Class, ExpectedSize); \
    AST_ASSERT_OFFSET(Class, ast_layout_sentinel_, ExpectedSentinelOffset)
