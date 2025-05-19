#include <gtest/gtest.h>

#include "dice.h"
#include "random.h"

#include <queue>

struct TMockRng final : public IRandomGenerator {
    int RollDice(int size) override {
        if (output.empty()) {
            throw std::logic_error("too many calls");
        }
        if (size != input.front()) {
            throw std::logic_error("unexpected input");
        }
        int res = output.front();
        output.pop();
        input.pop();
        return res;
    }

    void AddCall(int inputValue, int outputValue) {
        input.push(inputValue);
        output.push(outputValue);
    }

    void Verify() {
        if (!input.empty()) {
            throw std::logic_error("expected call");
        }
    }

    std::queue<int> output;
    std::queue<int> input;
};

TEST(DiceTest, OneDice) {
    TMockRng rng;
    rng.AddCall(20, 1);
    std::unique_ptr<IExpression> dice = std::make_unique<TDice>(20);

    EXPECT_EQ(dice->Value(rng), 1);
    rng.Verify();
}

TEST(DiceTest, DiceExpression) {
    TMockRng rng;
    rng.AddCall(20, 1);
    rng.AddCall(4, 1);
    std::unique_ptr<IExpression> skillCheck = std::make_unique<TSumExpression>(
        std::make_unique<TDice>(20),
        std::make_unique<TSumExpression>(
            std::make_unique<TNumber>(2),
            std::make_unique<TDice>(4)
        )
    );

    EXPECT_EQ(skillCheck->Value(rng), 4);
    rng.Verify();
}
