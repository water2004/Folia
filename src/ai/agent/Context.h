#pragma once
#include "agent/Agent.h"

namespace folia::ai::agent {
    std::vector<Message> ProjectHistory(History const&, Options const&);
    bool FitContext(ModelRequest&, std::size_t& historySize, History&, Options const&,
                    Request const&, Progress const&, std::stop_token, bool force = false);
}
