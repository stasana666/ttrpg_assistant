#pragma once

#include <pf2e_engine/common/tagged_registry.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

class TFeatureEventRegistry {
public:
    template <class TEvent>
    void Register(std::string feature_id,
                  std::shared_ptr<const typename TEvent::FHandler> handler)
    {
        if (!handler || !*handler) {
            throw std::invalid_argument("feature event requires identifiers and a handler");
        }
        registry_.Register<TEvent>(std::move(feature_id), std::move(handler));
    }

    template <class TEvent>
    std::shared_ptr<const typename TEvent::FHandler> Find(std::string_view feature_id) const
    {
        return registry_.Find<TEvent, typename TEvent::FHandler>(feature_id);
    }

private:
    TTaggedRegistry registry_;
};
