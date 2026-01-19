#pragma once

#include <pf2e_engine/random.h>
#include <stdexcept>
#include <queue>
#include <string>

class TMockRng final : public IRandomGenerator {
public:
    int RollDice(int size) override {
        if (output_.empty()) {
            throw std::logic_error("TMockRng: Unexpected RollDice call - no more calls expected");
        }
        if (size != input_.front()) {
            throw std::logic_error(
                "TMockRng: Unexpected dice size - expected d" + std::to_string(input_.front()) +
                ", got d" + std::to_string(size)
            );
        }
        int result = output_.front();
        output_.pop();
        input_.pop();
        return result;
    }

    void ExpectCall(int dice_size, int roll_result) {
        input_.push(dice_size);
        output_.push(roll_result);
    }

    void Verify() const {
        if (!input_.empty()) {
            throw std::logic_error(
                "TMockRng: Missing expected RollDice calls - " +
                std::to_string(input_.size()) + " call(s) not made"
            );
        }
    }

    size_t RemainingCalls() const {
        return input_.size();
    }

    void Reset() {
        input_ = {};
        output_ = {};
    }

private:
    std::queue<int> input_;
    std::queue<int> output_;
};
