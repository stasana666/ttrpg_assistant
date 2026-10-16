#include <pf2e_engine/alternatives.h>

#include <map>

std::vector<std::string> NumberDuplicateLabels(std::vector<std::string> labels,
                                               const std::set<std::string>& reserved)
{
    std::map<std::string, size_t> totals;
    for (const auto& label : labels) {
        ++totals[label];
    }
    std::set<std::string> used = reserved;
    std::map<std::string, size_t> occurrences;
    for (auto& label : labels) {
        const auto base = label;
        if (totals[base] > 1 || reserved.contains(base)) {
            label = base + " (" + std::to_string(++occurrences[base]) + ")";
        }
        while (!used.insert(label).second) {
            label = base + " (" + std::to_string(++occurrences[base]) + ")";
        }
    }
    return labels;
}
