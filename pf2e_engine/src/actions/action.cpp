#include <action.h>
#include "action_context.h"

void TAction::Apply(TActionContext& ctx)
{
    begin_->Apply(ctx);
}
