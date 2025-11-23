#pragma once

#include <pf2e_engine/audio_input/audio_input.h>
#include <pf2e_engine/audio_input/intent_recognizer.h>
#include <pf2e_engine/audio_input/speech_to_text.h>

#include <pf2e_engine/audio_input/prompt.h>
#include <pf2e_engine/common/channel.h>
#include <pf2e_engine/common/config.h>
#include <pf2e_engine/common/event.h>

#include <condition_variable>

class TAudioInputSystem {
public:
    explicit TAudioInputSystem(TConfig cfg);
    ~TAudioInputSystem();

    void AskQuestion(TPrompt question, TChannel<TIndexEvent>::TProducer send_end);

    void RepeatQuestion();
    void Finish();

private:
    enum class EState {
        Wait,
        Answer,
        Finish,
    };

    void Worker();
    void WorkerAnswer();

    TAudioInput audio_input_device_;
    TVoskRecognizer speech2text_;
    TUserIntentRecognizer intent_recognizer_;

    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable need_work_;

    EState state_{EState::Wait};
    std::optional<TPrompt> question_;
    std::optional<TChannel<TIndexEvent>::TProducer> send_end_;
    std::vector<int16_t> current_audio_input_;
};
