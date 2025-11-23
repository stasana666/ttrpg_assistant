#pragma once

#include <SFML/Audio.hpp>

class TAudioInput : private sf::SoundRecorder {
public:
    explicit TAudioInput(unsigned sample_rate = 16000);
    ~TAudioInput();

    std::vector<int16_t> Get();

private:
    bool onProcessSamples(const std::int16_t* samples, std::size_t sample_count) override;

    mutable std::mutex mutex_;
    std::vector<int16_t> buffer_;
};
