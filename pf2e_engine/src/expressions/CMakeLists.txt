target_sources(pf2e_engine
    PRIVATE
        dice_expression.cpp
        math_expression.cpp
        number_expression.cpp
)

target_include_directories(pf2e_engine
    PRIVATE ${CMAKE_SOURCE_DIR}/pf2e_engine/include/pf2e_engine/expressions
)
