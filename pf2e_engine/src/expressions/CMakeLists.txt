target_sources(pf2e_engine
    PRIVATE
        dice_expression.cpp
        dice_expression_parser.cpp
        math_expression.cpp
        multi_dice_expression.cpp
        number_expression.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/expressions
)
