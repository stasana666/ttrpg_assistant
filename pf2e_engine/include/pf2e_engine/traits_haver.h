#pragma once

#include <vector>
#include <variant>
#include <optional>

// Generic trait value that can be empty or hold an integer value
using TTraitValue = std::variant<std::monostate, int>;

// Generic trait structure that stores trait type and optional value
template <typename TTraitEnum>
struct TTrait {
    TTraitEnum type;
    TTraitValue value;
};

// Base class for game objects that can have traits
// TTraitEnum: enum class defining available trait types
template <typename TTraitEnum>
class TTraitsHaver {
public:
    using TTraitType = TTrait<TTraitEnum>;

    // Get all traits
    const std::vector<TTraitType>& Traits() const {
        return traits_;
    }

    // Check if object has a specific trait
    bool HasTrait(TTraitEnum trait) const {
        for (const auto& t : traits_) {
            if (t.type == trait) {
                return true;
            }
        }
        return false;
    }

    // Add a trait to the object
    void AddTrait(TTraitType trait) {
        traits_.emplace_back(trait);
    }

    // Get the value associated with a trait (if it exists)
    // Returns std::nullopt if trait is not present or has no value
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