#pragma once

#include <filesystem>
#include <optional>

struct TConfig {
    std::optional<std::filesystem::path> speech_model;
    std::optional<std::filesystem::path> nlp_model;
};
