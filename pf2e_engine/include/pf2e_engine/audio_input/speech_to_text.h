#pragma once

#include <vosk_api.h>
#include <string>
#include <vector>

class TVoskRecognizer {
public:
    explicit TVoskRecognizer(const std::string& model_path, float sample_rate = 16000.0f);
    ~TVoskRecognizer();

    std::string Recognize(const std::vector<int16_t>& audio);

private:
    VoskModel* model_;
    VoskRecognizer* recognizer_;
};
