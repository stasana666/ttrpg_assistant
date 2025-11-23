#include <interaction_system.h>
#include <chrono>

TInteractionSystem::TInteractionSystem(TChannel<TClickEvent>::TConsumer click_queue)
    : cin_reader_([&](){ this->CinReaderWorker(); })
    , cin_queue_(512)
    , click_queue_(click_queue)
{
}

void TInteractionSystem::CinReaderWorker()
{
    int result;
    while (std::cin >> result)
    {
        cin_queue_.Enqueue(TCinEvent{
            .value = result,
            .timepoint = std::chrono::steady_clock::now()
        });
    }
}

TInteractionSystem::~TInteractionSystem()
{
    cin_reader_.join();
}

std::ostream& TInteractionSystem::GameLog()
{
    return std::cout;
}

std::ostream& TInteractionSystem::DevLog()
{
    return std::cerr;
}
