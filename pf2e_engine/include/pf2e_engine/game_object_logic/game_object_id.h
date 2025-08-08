#pragma once

#include <pf2e_engine/common/value_id.h>

struct TGameObjectTag;
using TGameObjectId = TValueId<TGameObjectTag>;
using TGameObjectIdManager = TValueIdManager<TGameObjectTag>;
using TGameObjectIdHash = TValueIdHash<TGameObjectTag>;
