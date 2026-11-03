#include "../Adapters.h"
#include "Internal.h"

namespace fairy::providers {
ModelResult ParseResponsesResponse(Json const& response) {
    if (!response.is_object() || String(response, "status") == "failed")
        Bad("Responses request failed.");
    auto status = String(response, "status");
    if (status != "completed" && status != "incomplete")
        Bad("Responses response is not terminal.");
    return ParseTerminal(response, status == "incomplete");
}
} // namespace fairy::providers
