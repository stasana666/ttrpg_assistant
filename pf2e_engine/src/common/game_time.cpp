#include <pf2e_engine/common/game_time.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>
#include <pf2e_engine/transformation/transformator.h>

#include <limits>
#include <stdexcept>

TGameDuration TGameDuration::FromSeconds(std::uint64_t seconds)
{
    TGameDuration duration;
    duration.seconds_ = seconds;
    return duration;
}

std::uint64_t TGameDuration::Seconds() const
{
    return seconds_;
}

TGameDuration GameSeconds(int seconds)
{
    if (seconds < 0) {
        throw std::invalid_argument("duration must not be negative");
    }
    return TGameDuration::FromSeconds(static_cast<std::uint64_t>(seconds));
}

TGameDuration GameMinutes(int minutes)
{
    const auto seconds = GameSeconds(minutes).Seconds();
    if (seconds > std::numeric_limits<std::uint64_t>::max() / 60) {
        throw std::overflow_error("duration exceeds the clock range");
    }
    return TGameDuration::FromSeconds(seconds * 60);
}

std::uint64_t GameDeadline(std::uint64_t elapsed, TGameDuration duration)
{
    if (duration.Seconds() > std::numeric_limits<std::uint64_t>::max() - elapsed) {
        throw std::overflow_error("deadline exceeds the clock range");
    }
    return elapsed + duration.Seconds();
}

void TGameTimeRules::Validate() const
{
    (void)GameSeconds(RoundDurationSeconds);
}

std::uint64_t TGameClock::ElapsedSeconds() const
{
    return elapsed_seconds_;
}

void TGameClock::Advance(TGameDuration duration, TTransformator& journal)
{
    const auto next = GameDeadline(elapsed_seconds_, duration);
    if (next == elapsed_seconds_) {
        return;
    }
    auto context = journal.ExecutionContext();
    context.Assign(elapsed_seconds_, next, this, "elapsed_seconds");
}

TAstNode TGameClock::GetAst(TAstContext&) const
{
    AST_ASSERT_LAYOUT_WITH_SENTINEL(TGameClock, 16, 8);
    auto node = TAstNode::MakeObject("TGameClock");
    AddValueField(node, "elapsed_seconds", elapsed_seconds_);
    return node;
}
