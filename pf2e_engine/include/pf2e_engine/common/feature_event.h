#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>

class TFeatureEventRegistry {
public:
    template <class TEvent>
    void Register(std::string feature_id,
                  std::shared_ptr<const typename TEvent::FHandler> handler)
    {
        const std::string event_id(TEvent::kId);
        if (event_id.empty() || feature_id.empty() || !handler || !*handler) {
            throw std::invalid_argument("feature event requires identifiers and a handler");
        }
        const auto event = events_.find(event_id);
        if (event == events_.end()) {
            TEventHandlers handlers{typeid(TEvent), {}};
            handlers.Handlers.emplace(std::move(feature_id), std::move(handler));
            events_.emplace(event_id, std::move(handlers));
            return;
        }
        if (event->second.Type != typeid(TEvent)) {
            throw std::invalid_argument("feature event identifier has a different type");
        }
        const auto existing = event->second.Handlers.find(feature_id);
        if (existing != event->second.Handlers.end()) {
            if (existing->second.get() != handler.get()) {
                throw std::invalid_argument("conflicting feature event handler");
            }
            return;
        }
        event->second.Handlers.emplace(std::move(feature_id), std::move(handler));
    }

    template <class TEvent>
    std::shared_ptr<const typename TEvent::FHandler> Find(std::string_view feature_id) const
    {
        const auto event = events_.find(TEvent::kId);
        if (event == events_.end()) {
            return {};
        }
        if (event->second.Type != typeid(TEvent)) {
            throw std::invalid_argument("feature event identifier has a different type");
        }
        const auto handler = event->second.Handlers.find(feature_id);
        if (handler == event->second.Handlers.end()) {
            return {};
        }
        return std::static_pointer_cast<const typename TEvent::FHandler>(handler->second);
    }

private:
    struct TEventHandlers {
        std::type_index Type;
        std::map<std::string, std::shared_ptr<const void>, std::less<>> Handlers;
    };

    std::map<std::string, TEventHandlers, std::less<>> events_;
};
