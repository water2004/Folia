#pragma once

#include "agent/Agent.h"

namespace folia::ai::agent {
    bool NeedsSeparateSearch(Options const& options, std::span<Tool const> localTools);

    // Run only the public query through the selected connection's hosted search.
    // The main turn retains its local tools, reasoning level, and conversation history.
    Tool PublicSearchTool(Options const&, Request const&, std::vector<Source>& sources);
}
