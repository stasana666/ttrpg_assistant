#include <interaction_system.h>
#include <chrono>

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
