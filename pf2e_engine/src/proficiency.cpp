#include <pf2e_engine/proficiency.h>

TProficiency::TProficiency(int level)
    : level_(level)
{
}

int TProficiency::GetProficiency(const TWeapon&) const
{
    return 0;
}

int TProficiency::GetProficiency(const TArmor&) const
{
    return 0;
}

int TProficiency::GetLevel() const
{
    return level_;
}
