#pragma once

#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/audio_input/audio_input_system.h>
#include <pf2e_engine/common/channel.h>
#include <pf2e_engine/common/event.h>

#include <iostream>

class TInteractionSystem : public IInteractionSystem {
public:
    explicit TInteractionSystem(TChannel<TClickEvent>::TConsumer click_queue);
    ~TInteractionSystem() override;

    void CinReaderWorker();

    void Add(std::unique_ptr<TAudioInputSystem>&& audio_input_system);

    std::ostream& GameLog() override;
    std::ostream& DevLog() override;

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override;

private:
    std::unique_ptr<TAudioInputSystem> audio_input_system_;
    std::thread cin_reader_;
    TChannel<TIndexEvent> cin_queue_;
    TChannel<TIndexEvent> nlp_queue_;
    TChannel<TClickEvent>::TConsumer click_queue_;
};
