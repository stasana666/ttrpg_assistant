#include <base_function.h>

FBaseFunction::FBaseFunction(TBlockInput&& input, TGameObjectId output)
    : input_(std::move(input))
    , output_(output)
{
}
