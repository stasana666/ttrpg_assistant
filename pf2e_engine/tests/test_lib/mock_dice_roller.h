#pragma once

#include <pf2e_engine/random.h>
#include <stdexcept>
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

    void ExpectCall(int inputValue, int outputValue) {
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
