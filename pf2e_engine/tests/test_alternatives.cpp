#include <gtest/gtest.h>

#include <pf2e_engine/alternatives.h>

TEST(NumberDuplicateLabelsTest, NumbersEveryRepeatedLabelInOrder)
{
    EXPECT_EQ(NumberDuplicateLabels({"Blade", "Jaws", "Blade"}),
              (std::vector<std::string>{"Blade (1)", "Jaws", "Blade (2)"}));
}

TEST(NumberDuplicateLabelsTest, NumbersLabelsThatMatchAReservedLabel)
{
    EXPECT_EQ(NumberDuplicateLabels({"Decline", "Counter"}, {"Decline"}),
              (std::vector<std::string>{"Decline (1)", "Counter"}));
}

TEST(NumberDuplicateLabelsTest, SkipsSuffixesThatAnotherLabelAlreadyUses)
{
    EXPECT_EQ(NumberDuplicateLabels({"Blade (1)", "Blade", "Blade"}),
              (std::vector<std::string>{"Blade (1)", "Blade (2)", "Blade (3)"}));
}
