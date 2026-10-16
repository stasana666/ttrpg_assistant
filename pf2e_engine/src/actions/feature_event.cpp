#include <pf2e_engine/actions/feature_event.h>

#include <stdexcept>

namespace {

thread_local bool dispatch_active = false;

}

TFeatureEventDispatchScope::TFeatureEventDispatchScope()
{
    if (dispatch_active) {
        throw std::logic_error("nested pure feature event dispatch is not supported");
    }
    dispatch_active = true;
}

TFeatureEventDispatchScope::~TFeatureEventDispatchScope()
{
    dispatch_active = false;
}
