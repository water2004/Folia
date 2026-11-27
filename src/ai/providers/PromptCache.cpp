module folia.ai.providers;
import std;
import :dispatch;

namespace folia::ai {
bool SupportsCacheBreakpoints(CacheDialect dialect, ApiProtocol protocol, std::string_view model,
                                     bool explicitOptIn) noexcept {
    if (protocol == ApiProtocol::AnthropicMessages) return false;
    if (dialect == CacheDialect::Custom) return explicitOptIn && protocol == ApiProtocol::Responses;
    if (dialect == CacheDialect::OpenRouter) {
        if (!model.starts_with("openai/")) return false;
        model.remove_prefix(7);
    } else if (dialect != CacheDialect::OpenAI) return false;
    if (!model.starts_with("gpt-")) return false;
    model.remove_prefix(4);
    int major = 0, minor = 0;
    auto end = model.data() + model.size();
    auto [next, error] = std::from_chars(model.data(), end, major);
    if (error != std::errc{}) return false;
    if (next != end && *next == '.') {
        auto version = std::from_chars(next + 1, end, minor);
        if (version.ec != std::errc{}) return false;
        next = version.ptr;
    }
    return (next == end || *next == '-' || *next == ':') && (major > 5 || (major == 5 && minor >= 6));
}

void MarkCacheBoundaries(Json& content, Message const& message, char const* textType) {
    for (std::size_t i = 0; i < message.content.size(); ++i) {
        if (!message.content[i].cacheBoundary) continue;
        if (content.is_string()) content = Json::array({{{"type", textType}, {"text", std::move(content)}}});
        content.at(i)["prompt_cache_breakpoint"] = {{"mode", "explicit"}};
    }
}

void ApplyPromptCache(Json& body, ModelRequest const& request, ProviderRequestOptions options, ApiProtocol protocol) {
    const auto& session = request.cacheSession;
    if (!session.empty()) {
        switch (options.cache) {
        case CacheDialect::OpenAI:
        case CacheDialect::RoutingKey: body["prompt_cache_key"] = session; break;
        case CacheDialect::OpenRouter: body["session_id"] = session; break;
        default: break;
        }
        if (options.cache == CacheDialect::Anthropic ||
            (options.cache == CacheDialect::OpenRouter && request.model.starts_with("anthropic/claude-")))
            body["cache_control"] = {{"type", "ephemeral"}};
    }
    if (UsesCacheBoundaries(request, options, protocol))
        body["prompt_cache_options"] = {{"mode", request.stablePrefixOnly ? "explicit" : "implicit"}};
}
}
