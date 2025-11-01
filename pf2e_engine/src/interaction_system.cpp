#include <interaction_system.h>


std::ostream& TInteractionSystem::GameLog()
{
    return std::cout;
}

std::ostream& TInteractionSystem::DevLog()
{
    return std::cerr;
}
