#include "audio_input.h"

#include <iostream>
#include <mutex>
#include <stdexcept>

TAudioInput::TAudioInput(unsigned sample_rate)
{
    if (!sf::SoundRecorder::isAvailable()) {
        std::runtime_error("No audio recording device available");
    }
    std::vector<std::string> available_devices = sf::SoundRecorder::getAvailableDevices();
    for (auto& d : available_devices) {
        std::cout << "Available device: \"" << d << "\"" << std::endl;
    }

    if (!start(sample_rate)) {
        throw std::runtime_error("Failed to start audio recording");
    }
}

TAudioInput::~TAudioInput()
{
    stop();
}

bool TAudioInput::onProcessSamples(const std::int16_t* samples, std::size_t sample_count)
{
    std::lock_guard lock(mutex_);
    buffer_.insert(buffer_.end(), samples, samples + sample_count);
    return true;
}

std::vector<int16_t> TAudioInput::Get()
{
    std::lock_guard lock(mutex_);
    std::vector<int16_t> result(std::move(buffer_));
    return result;
}
