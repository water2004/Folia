#include "../Adapters.h"
#include "Internal.h"

namespace folia::ai {
ModelResult ParseChatCompletionsResponse(Json const& response) {
    try {
        return ParseFull(response);
    } catch (ProtocolError const&) {
        throw;
    } catch (...) {
        Invalid();
    }
}
} // namespace folia::ai
