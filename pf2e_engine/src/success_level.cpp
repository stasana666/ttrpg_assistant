#include <pf2e_engine/success_level.h>

#include <stdexcept>

std::string ToString(ESuccessLevel success_level)
{
    switch (success_level)
    {
        case ESuccessLevel::CriticalFailure:
            return "critical_failure";
        case ESuccessLevel::Failure:
            return "failure";
        case ESuccessLevel::Success:
            return "success";
        case ESuccessLevel::CriticalSuccess:
            return "critical_success";
    }
    throw std::logic_error("unknown enum ESuccessLevel value");
}
