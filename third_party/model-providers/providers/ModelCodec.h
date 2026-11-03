#pragma once

#include "ModelTypes.h"

namespace fairy::providers {
Json ToJson(ModelRequest const& request);
ModelRequest RequestFromJson(Json const& value);
Json ToJson(ModelResult const& result);
ModelResult ResultFromJson(Json const& value);
Json ToJson(StreamEvent const& event);
StreamEvent StreamEventFromJson(Json const& value);
} // namespace fairy::providers
