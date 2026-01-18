#include <interaction_system.h>

#include <pf2e_engine/audio_input/prompt.h>

#include <chrono>
#include <sstream>
#include <thread>

TInteractionSystem::TInteractionSystem(TChannel<TClickEvent>::TConsumer click_queue)
    : cin_reader_([&](){ this->CinReaderWorker(); })
    , cin_queue_(512)
    , nlp_queue_(8)
    , click_queue_(click_queue)
{
}

void TInteractionSystem::CinReaderWorker()
{
    int result;
    while (std::cin >> result)
    {
        cin_queue_.Enqueue(TIndexEvent{
            .value = result,
            .timepoint = std::chrono::steady_clock::now()
        });
    }
}

TInteractionSystem::~TInteractionSystem()
{
    cin_reader_.join();
}

void TInteractionSystem::Add(std::unique_ptr<TAudioInputSystem>&& audio_input_system)
{
    audio_input_system_ = std::move(audio_input_system);
}

std::ostream& TInteractionSystem::GameLog()
{
    return std::cout;
}

std::ostream& TInteractionSystem::DevLog()
{
    return std::cerr;
}

size_t TInteractionSystem::ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives)
{
    TPrompt prompt;
    std::stringstream question;
    question << "Ask " << player_id << "\n";
    question << "Choose " << alternatives.Kind() << " and write it's number:" << std::endl;
    for (size_t i = 0; i < alternatives.Size(); ++i) {
        question << i << " - " << alternatives[i].name << std::endl;
        prompt.AddVariant(i, alternatives[i].name);
    }

    bool has_progress{false};
    int result{-1};
    std::vector<std::function<bool(void)>> input_ways;
    TClickEvent::Timepoint now = std::chrono::steady_clock::now();

    std::cout << question.str();
    std::cout.flush();

    if (alternatives.GetAskingStrategy() == EAskingStrategy::Console) {
        if (audio_input_system_ != nullptr) {
            audio_input_system_->AskQuestion(prompt, nlp_queue_.MakeProducer());
            input_ways.emplace_back([&]() {
                TIndexEvent event;
                if (!nlp_queue_.Dequeue(event)) {
                    return false;
                }
                has_progress = true;
                if (event.timepoint < now) {
                    return false;
                }
                if (event.value >= static_cast<ssize_t>(alternatives.Size()) || event.value < 0) {
                    audio_input_system_->RepeatQuestion();
                    return false;
                }
                result = event.value;
                int x;
                std::cout << "llm choose " << result << std::endl;
                std::cin >> x;
                if (x != 0) {
                    exit(0);
                }
                return true;
            });
        }
    }

    input_ways.emplace_back([&]() {
        TIndexEvent event;
        if (!cin_queue_.Dequeue(event)) {
            return false;
        }
        has_progress = true;
        if (event.timepoint < now) {
            return false;
        }
        if (event.value >= static_cast<ssize_t>(alternatives.Size()) || event.value < 0) {
            std::cout << question.str();
            return false;
        }
        result = event.value;
        return true;
    });

    if (alternatives.GetAskingStrategy() == EAskingStrategy::Gui) {
        input_ways.emplace_back([&]() {
            TClickEvent event;
            if (!click_queue_.Dequeue(event)) {
                return false;
            }
            std::cout << "Deque TClickEvent: x = " << event.value.x << ", y = " << event.value.y << std::endl;
            has_progress = true;
            if (event.timepoint < now) {
                return false;
            }
            for (size_t i = 0; i < alternatives.Size(); ++i) {
                if (alternatives[i].Get<TPosition>() == event.value) {
                    result = i;
                    return true;
                }
            }
            return false;
        });
    }

    // TODO: заменить spinlock на нормальное решение с condition_variable
    while (true) {
        has_progress = false;
        for (auto& way : input_ways) {
            if (way()) {
                return result;
            }
        }
        if (!has_progress) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return result;
}
