module folia.ai.providers;
import std;
import :dispatch;
import :chat_completions;

namespace folia::ai {
using namespace chat_completions;

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
