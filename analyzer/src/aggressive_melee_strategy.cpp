#include <analyzer/aggressive_melee_strategy.h>

#include <pf2e_engine/player.h>

#include <string_view>

namespace {

size_t ChooseAction(const TAlternatives& alternatives)
{
    for (size_t i = 0; i < alternatives.Size(); ++i) {
        if (alternatives[i].name.find("attack") != std::string::npos) {
            return i;
        }
    }
    return 0;  // "End of turn"
}

size_t ChooseTarget(int player_id, const TAlternatives& alternatives)
{
    for (size_t i = 0; i < alternatives.Size(); ++i) {
        if (alternatives[i].Get<TPlayer*>()->GetId() != player_id) {
            return i;
        }
    }
    return 0;
}

}  // namespace

size_t TAggressiveMeleeStrategy::Decide(int player_id, const TAlternatives& alternatives) const
{
    std::string_view kind = alternatives.Kind();
    if (kind == "next action") {
        return ChooseAction(alternatives);
    }
    if (kind == "target") {
        return ChooseTarget(player_id, alternatives);
    }
    return 0;
}
