#pragma once

#include "agent/Agent.h"

namespace folia::ai::agent {
    bool NeedsIsolatedWebRequest(Options const& options, std::span<Tool const> localTools);

    // Only the public query or URL crosses into the isolated request.
    enum class WebTask { Search, ReadPage };
    Tool PublicWebTool(WebTask, Options const&, Request const&, std::vector<Source>& sources);
}
