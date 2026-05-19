#include <gtest/gtest.h>

#include <pf2e_engine/common/continuation.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/transformation/transformator.h>

#include "test_lib/mock_interaction_system.h"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Unit tests for the continuation utility. They drive the helpers directly
// (no battle) and use TSavepointStackUnwind as the suspension signal, mirroring
// how TBattle::MakeTurn catches, then Resume()s, a savepoint.
class ContinuationTest : public ::testing::Test {
protected:
    // A savepoint carries a TState snapshot; TState can only be minted by a
    // TTransformator, which in turn needs an IInteractionSystem.
    TSavepointStackUnwind MakeSavepoint() {
        return TSavepointStackUnwind(transformator_.CurrentState(), []() {});
    }

    TMockInteractionSystem io_;
    TTransformator transformator_{io_};
};

TEST_F(ContinuationTest, ForEachVisitsEveryElementInOrder) {
    std::vector<int> items{1, 2, 3, 4, 5};
    std::vector<int> visited;

    continuation::ForEach(items.begin(), items.end(),
        [&](int x) { visited.push_back(x); });

    EXPECT_EQ(visited, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST_F(ContinuationTest, WhileRunsUntilConditionIsFalse) {
    int n = 0;

    continuation::While([&]() { return n < 5; }, [&]() { ++n; });

    EXPECT_EQ(n, 5);
}

TEST_F(ContinuationTest, ForEachResumesAfterSuspension) {
    std::vector<int> items{1, 2, 3, 4, 5};
    std::vector<int> visited;
    bool suspended = false;

    auto func = [&](int x) {
        visited.push_back(x);
        if (x == 3 && !suspended) {
            suspended = true;
            throw MakeSavepoint();
        }
    };

    try {
        continuation::ForEach(items.begin(), items.end(), func);
        FAIL() << "expected the step to suspend";
    } catch (TSavepointStackUnwind& savepoint) {
        savepoint.Resume();
    }

    // Element 3 was visited before it suspended; 4 and 5 ran on resume.
    EXPECT_EQ(visited, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST_F(ContinuationTest, ForEachResumesAcrossTwoSeparateSuspensions) {
    std::vector<int> items{1, 2, 3, 4, 5};
    std::vector<int> visited;
    std::set<int> suspended_on;

    auto func = [&](int x) {
        visited.push_back(x);
        if ((x == 3 || x == 4) && suspended_on.insert(x).second) {
            throw MakeSavepoint();
        }
    };

    // Drive loop modelled on TBattle::MakeTurn: keep resuming while suspended.
    std::optional<TSavepointStackUnwind> pending;
    int suspensions = 0;
    bool finished = false;
    while (!finished) {
        try {
            if (pending.has_value()) {
                TSavepointStackUnwind savepoint = *pending;
                pending.reset();
                savepoint.Resume();
            } else {
                continuation::ForEach(items.begin(), items.end(), func);
            }
            finished = true;
        } catch (TSavepointStackUnwind& savepoint) {
            pending = savepoint;
            ++suspensions;
        }
    }

    EXPECT_EQ(suspensions, 2);
    EXPECT_EQ(visited, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST_F(ContinuationTest, ThenReprotectsTailWhenStepSuspendsRepeatedly) {
    // The step is itself a continuation-aware While that suspends on every
    // iteration. Then must keep `tail` scheduled across each re-suspension --
    // a non-recursive Then would drop it after the first resume.
    std::vector<std::string> log;
    auto counter = std::make_shared<int>(3);

    auto step = [&]() {
        continuation::While(
            [counter]() { return *counter > 0; },
            [&]() {
                log.push_back("body");
                --*counter;
                throw MakeSavepoint();
            });
    };
    auto tail = [&]() { log.push_back("tail"); };

    std::optional<TSavepointStackUnwind> pending;
    int suspensions = 0;
    bool finished = false;
    while (!finished) {
        try {
            if (pending.has_value()) {
                TSavepointStackUnwind savepoint = *pending;
                pending.reset();
                savepoint.Resume();
            } else {
                continuation::Then(step, tail);
            }
            finished = true;
        } catch (TSavepointStackUnwind& savepoint) {
            pending = savepoint;
            ++suspensions;
        }
    }

    EXPECT_EQ(suspensions, 3);
    EXPECT_EQ(log, (std::vector<std::string>{"body", "body", "body", "tail"}));
}

TEST_F(ContinuationTest, ForEachOwnedKeepsElementsAliveAcrossSuspension) {
    std::vector<int> visited;
    bool suspended = false;

    auto func = [&](int x) {
        visited.push_back(x);
        if (x == 20 && !suspended) {
            suspended = true;
            throw MakeSavepoint();
        }
    };

    try {
        // The container is a temporary: ForEachOwned must keep its elements
        // alive across the stack unwinding that the throw below triggers.
        continuation::ForEachOwned(std::vector<int>{10, 20, 30, 40}, func);
        FAIL() << "expected the step to suspend";
    } catch (TSavepointStackUnwind& savepoint) {
        savepoint.Resume();
    }

    EXPECT_EQ(visited, (std::vector<int>{10, 20, 30, 40}));
}

TEST_F(ContinuationTest, NestedForEachResumesAtTheCorrectPosition) {
    std::vector<int> outer{1, 2};
    std::vector<int> inner{10, 20};
    std::vector<std::pair<int, int>> visited;
    bool suspended = false;

    auto body = [&](int o) {
        continuation::ForEach(inner.begin(), inner.end(), [&, o](int i) {
            visited.emplace_back(o, i);
            if (o == 1 && i == 20 && !suspended) {
                suspended = true;
                throw MakeSavepoint();
            }
        });
    };

    try {
        continuation::ForEach(outer.begin(), outer.end(), body);
        FAIL() << "expected the step to suspend";
    } catch (TSavepointStackUnwind& savepoint) {
        savepoint.Resume();
    }

    EXPECT_EQ(visited, (std::vector<std::pair<int, int>>{
        {1, 10}, {1, 20}, {2, 10}, {2, 20}}));
}
