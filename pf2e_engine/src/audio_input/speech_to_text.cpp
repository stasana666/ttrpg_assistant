#include <speech_to_text.h>
#include <stdexcept>

TVoskRecognizer::TVoskRecognizer(const std::string& model_path, float sample_rate)
{
    model_ = vosk_model_new(model_path.c_str());
    if (model_ == nullptr) {
        throw std::runtime_error("Failed to load Vosk model from " + model_path);
    }

    recognizer_ = vosk_recognizer_new(model_, sample_rate);
    if (recognizer_ == nullptr) {
        vosk_model_free(model_);
        throw std::runtime_error("Failed to create Vosk recognizer");
    }
}

TVoskRecognizer::~TVoskRecognizer()
{
    if (recognizer_ != nullptr) {
        vosk_recognizer_free(recognizer_);
    }
    if (model_ != nullptr) {
        vosk_model_free(model_);
    }
}

std::string TVoskRecognizer::Recognize(const std::vector<int16_t>& audio)
{
    const char* raw = reinterpret_cast<const char*>(audio.data());
    const int byte_size = static_cast<int>(audio.size() * sizeof(int16_t));

    const int status = vosk_recognizer_accept_waveform(recognizer_, raw, byte_size);

    if (status == -1) {
        throw std::runtime_error("Vosk: invalid waveform data passed to recognizer");
    }

    if (status == 1) {
        const char* text = vosk_recognizer_result(recognizer_);
        return (text != nullptr) ? std::string(text) : "";
    }

    const char* partial = vosk_recognizer_partial_result(recognizer_);
    return (partial != nullptr) ? std::string(partial) : "";
}
