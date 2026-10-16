#pragma once

#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/i_interaction_system.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <any>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class TScriptedIo final : public IInteractionSystem {
public:
    using FMatch = std::function<bool(const TAlternative&)>;
    using FObserve = std::function<void(const TAlternatives&)>;

    void Expect(int actor, std::string kind, FMatch matches, bool suspend = false, FObserve observe = {})
    {
        expected_.push_back({actor, std::move(kind), std::move(matches), suspend, std::move(observe)});
    }

    void ExpectName(int actor, std::string kind, std::string name, bool suspend = false, FObserve observe = {})
    {
        Expect(actor, std::move(kind), [name = std::move(name)](const TAlternative& alternative) {
            return alternative.name == name;
        }, suspend, std::move(observe));
    }

    template <class T>
    void ExpectValue(int actor, T value, bool suspend = false, FObserve observe = {})
    {
        Expect(actor, {}, [value](const TAlternative& alternative) {
            const auto* actual = std::any_cast<T>(&alternative.value);
            return actual != nullptr && *actual == value;
        }, suspend, std::move(observe));
    }

    void RequestChoice(int actor, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> receive) override
    {
        const auto [index, suspend] = Select(actor, alternatives);
        ++requests_;
        if (suspend) {
            ++suspensions_;
            throw TSavepointStackUnwind(state, [this, alternatives, index, receive = std::move(receive)] {
                receive(resume_name_ ? Find(alternatives, *std::exchange(resume_name_, std::nullopt)) : index);
            });
        }
        ++depth_;
        max_depth_ = std::max(max_depth_, depth_);
        try {
            receive(index);
        } catch (...) {
            --depth_;
            throw;
        }
        --depth_;
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override { ++legacy_calls_; }

    std::ostream& GameLog() override
    {
        if (observe_log_) {
            observe_log_();
        }
        return log_;
    }

    std::ostream& DevLog() override { return log_; }

    void ObserveLog(std::function<void()> observe) { observe_log_ = std::move(observe); }
    void ResumeAs(std::string name) { resume_name_ = std::move(name); }
    void Verify() const { EXPECT_TRUE(expected_.empty()); }
    size_t Requests() const { return requests_; }
    size_t Suspensions() const { return suspensions_; }
    int MaxDepth() const { return max_depth_; }
    int LegacyCalls() const { return legacy_calls_; }

    const std::vector<std::string>& Candidates(const std::string& kind = {}) const
    {
        static const std::vector<std::string> kNone;
        const auto found = candidates_.find(kind.empty() ? last_kind_ : kind);
        return found == candidates_.end() ? kNone : found->second;
    }

protected:
    size_t ChooseAlternativeIndex(int actor, const TAlternatives& alternatives) override
    {
        const auto [index, suspend] = Select(actor, alternatives);
        if (suspend) {
            throw std::logic_error("a synchronous choice cannot suspend");
        }
        return index;
    }

private:
    struct TExpected {
        int Actor;
        std::string Kind;
        FMatch Matches;
        bool Suspend;
        FObserve Observe;
    };

    static size_t Find(const TAlternatives& alternatives, const std::string& name)
    {
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            if (alternatives[index].name == name) {
                return index;
            }
        }
        throw std::logic_error("missing alternative: " + name);
    }

    std::pair<size_t, bool> Select(int actor, const TAlternatives& alternatives)
    {
        const std::string kind(alternatives.Kind());
        if (expected_.empty()) {
            throw std::logic_error("unexpected choice: " + kind);
        }
        auto expected = std::move(expected_.front());
        expected_.pop_front();
        EXPECT_EQ(actor, expected.Actor);
        if (!expected.Kind.empty()) {
            EXPECT_EQ(kind, expected.Kind);
        }
        last_kind_ = kind;
        auto& names = candidates_[kind];
        names.clear();
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            names.push_back(alternatives[index].name);
        }
        size_t index = 0;
        while (index < alternatives.Size() && !expected.Matches(alternatives[index])) {
            ++index;
        }
        if (index == alternatives.Size()) {
            std::string available;
            for (const auto& name : names) {
                available += " [" + name + "]";
            }
            throw std::logic_error("missing expected alternative in " + kind + "; available:" + available);
        }
        if (expected.Observe) {
            expected.Observe(alternatives);
        }
        return {index, expected.Suspend};
    }

    std::deque<TExpected> expected_;
    std::map<std::string, std::vector<std::string>> candidates_;
    std::string last_kind_;
    std::optional<std::string> resume_name_;
    std::function<void()> observe_log_;
    std::ostringstream log_;
    size_t requests_ = 0;
    size_t suspensions_ = 0;
    int depth_ = 0;
    int max_depth_ = 0;
    int legacy_calls_ = 0;
};
