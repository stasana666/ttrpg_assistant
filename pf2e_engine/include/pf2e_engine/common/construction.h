#pragma once

#include <pf2e_engine/common/ability.h>

#include <optional>
#include <string>
#include <vector>

enum class EConstructionChoiceState {
    Enabled,
    Disabled,
    Pending,
};

struct TConstructionIssue {
    std::string Parameter;
    std::string Message;

    friend bool operator==(const TConstructionIssue&, const TConstructionIssue&) = default;
};

struct TFeatureChoiceDescriptor {
    std::string Name;
    EConstructionChoiceState State = EConstructionChoiceState::Pending;
    std::vector<TFeatureSelection> Candidates;
    std::optional<TFeatureSelection> Selected;
    bool UsesDefault = false;
    std::vector<std::string> Dependencies;

    friend bool operator==(const TFeatureChoiceDescriptor&, const TFeatureChoiceDescriptor&) = default;
};

struct TConstructionReport {
    std::vector<TFeatureChoiceDescriptor> Fields;
    std::vector<TConstructionIssue> Issues;
    bool Complete = true;

    bool IsValid() const
    {
        return Complete && Issues.empty();
    }

    friend bool operator==(const TConstructionReport&, const TConstructionReport&) = default;
};
