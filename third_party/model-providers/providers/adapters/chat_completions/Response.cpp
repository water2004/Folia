#include "../Adapters.h"
#include "Internal.h"

namespace fairy::providers {
ModelResult ParseChatCompletionsResponse(Json const& response) {
    try {
        return ParseFull(response);
    } catch (ProtocolError const&) {
        throw;
    } catch (...) {
        Invalid();
    }
}
} // namespace fairy::providers
