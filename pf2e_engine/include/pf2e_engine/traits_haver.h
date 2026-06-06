#pragma once

#include <pf2e_engine/common/ast/ast_serialize.h>

#include <string>
#include <vector>
#include <variant>
#include <optional>

using TTraitValue = std::variant<std::monostate, int>;

template <typename TTraitEnum>
struct TTrait {
    TTraitEnum type;
    TTraitValue value;
};

template <typename TTraitEnum>
std::string AstSerialize(const TTrait<TTraitEnum>& trait)
{
    return "TTrait{" + AstSerialize(trait.type) + "," +
           AstSerialize(trait.value) + "}";
}

template <typename TTraitEnum>
class TTraitsHaver {
public:
    using TTraitType = TTrait<TTraitEnum>;

    const std::vector<TTraitType>& Traits() const {
        return traits_;
    }

    bool HasTrait(TTraitEnum trait) const {
        for (const auto& t : traits_) {
            if (t.type == trait) {
                return true;
            }
        }
        return false;
    }

    void AddTrait(TTraitType trait) {
        traits_.emplace_back(trait);
    }

    std::optional<int> GetTraitValue(TTraitEnum trait) const {
        for (const auto& t : traits_) {
            if (t.type == trait) {
                if (std::holds_alternative<int>(t.value)) {
                    return std::get<int>(t.value);
                }
            }
        }
        return std::nullopt;
    }

protected:
    std::vector<TTraitType> traits_;
};