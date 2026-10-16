#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>

#include <cstdint>

class TTransformator;

class TGameDuration {
public:
    TGameDuration() = default;
    static TGameDuration FromSeconds(std::uint64_t seconds);
    std::uint64_t Seconds() const;
    friend bool operator==(const TGameDuration&, const TGameDuration&) = default;

private:
    std::uint64_t seconds_ = 0;
};

TGameDuration GameSeconds(int seconds);
TGameDuration GameMinutes(int minutes);
std::uint64_t GameDeadline(std::uint64_t elapsed, TGameDuration duration);

struct TGameTimeRules {
    int RoundDurationSeconds = 0;
    void Validate() const;
};

class TGameClock {
public:
    std::uint64_t ElapsedSeconds() const;
    void Advance(TGameDuration duration, TTransformator& journal);
    TAstNode GetAst(TAstContext& context) const;

private:
    std::uint64_t elapsed_seconds_ = 0;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TGameClock> : std::true_type {};
