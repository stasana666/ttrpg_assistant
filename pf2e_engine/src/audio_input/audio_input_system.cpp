#include "audio_input_system.h"
#include "event.h"
#include "intent_recognizer.h"

#include <chrono>
#include <iostream>

constexpr size_t kMaxSecondsInMemory = 30;
constexpr size_t kSampleRate = 16000;
constexpr size_t kMaxAudioInputSize = kMaxSecondsInMemory * kSampleRate;

TAudioInputSystem::TAudioInputSystem(TConfig cfg)
    : audio_input_device_(kSampleRate)
    , speech2text_(cfg.speech_model.value(), kSampleRate)
    , intent_recognizer_(cfg.nlp_model.value())
    , worker_([this]() { this->Worker(); })
{
}

TAudioInputSystem::~TAudioInputSystem()
{
    {
        std::unique_lock lock(mutex_);
        state_ = EState::Finish;
    }
    need_work_.notify_one();
    worker_.join();
}

void TAudioInputSystem::Worker()
{
    while (true) {
        std::unique_lock lock(mutex_);
        need_work_.wait(lock, [&]() {
            return state_ != EState::Wait;
        });

        switch (state_) {
            case EState::Wait:
                throw std::logic_error("wake while need waiting");
            case EState::Answer:
                WorkerAnswer();
                break;
            case EState::Finish:
                return;
        }
        state_ = EState::Wait;
    }
}

void TAudioInputSystem::WorkerAnswer()
{
    {
        auto new_audio_input = audio_input_device_.Get();
        current_audio_input_.insert(current_audio_input_.end(), new_audio_input.begin(), new_audio_input.end());
    }
    if (current_audio_input_.size() > kMaxAudioInputSize) {
        current_audio_input_.erase(current_audio_input_.begin(), current_audio_input_.begin() + current_audio_input_.size() - kMaxAudioInputSize);
    }

    std::string textual_representation = speech2text_.Recognize(current_audio_input_);
    std::cout << textual_representation << std::endl;

    question_.value().AddUserSpeech(textual_representation);
    int value = intent_recognizer_.Classify(question_.value().ToString());

    send_end_->Enqueue(TIndexEvent{
        .value = value,
        .timepoint = std::chrono::steady_clock::now()
    });
}

void TAudioInputSystem::AskQuestion(TPrompt question, TChannel<TIndexEvent>::TProducer send_end)
{
    {
        std::unique_lock lock(mutex_);
        state_ = EState::Answer;
        question_ = question;
        send_end_ = send_end;
    }
    need_work_.notify_one();
}

void TAudioInputSystem::RepeatQuestion()
{
    {
        std::unique_lock lock(mutex_);
        state_ = EState::Answer;
    }
    need_work_.notify_one();
}
