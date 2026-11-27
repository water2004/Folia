#include <winrt/base.h>
#include "windows/completion/CompletionHttpTransport.h"
#include "windows/ai/ModelHttpClient.h"

import std;
import folia.markdown.utf;

import folia.ai.providers;

namespace folia::platform::completion {
CompletionResult RequestCompletionHttp(CompletionConfig const& config, folia::completion::Prompt const& prompt,
    std::stop_token stop, std::wstring key, std::chrono::milliseconds inactivityTimeout) {
    using namespace folia::ai;
    if (auto validation = ValidateCompletionConfig(config); !validation.empty()) throw std::runtime_error(validation);
    ModelRequest model; model.model = config.model; model.maxOutputTokens = prompt.output_tokens; model.stream = true;
    if (config.modelOutputLimit) model.maxOutputTokens = (std::min)(model.maxOutputTokens, *config.modelOutputLimit);
    model.reasoning.effort = config.reasoningEffort;
    auto add = [&](MessageRole role, std::string const& text) {
        Message message; message.role = role; message.content.push_back({ContentKind::Text, text}); model.messages.push_back(std::move(message));
    };
    add(MessageRole::System, prompt.instructions);
    for (auto const& [source, response] : prompt.examples) {
        add(MessageRole::User, source); add(MessageRole::Assistant, response);
    }
    auto protocol = static_cast<ApiProtocol>(config.protocol);
    model.stablePrefixOnly = config.explicitPromptCache && SupportsCacheBreakpoints(config.cacheDialect, protocol, config.model, true);
    if (model.stablePrefixOnly && !prompt.context.empty()) {
        add(MessageRole::User, prompt.context);
        model.messages.back().content.front().cacheBoundary = true;
        model.messages.back().content.push_back({ContentKind::Text, prompt.cursor});
    } else {
        add(MessageRole::User, prompt.context + prompt.cursor);
    }
    auto request = BuildRequest(protocol, model, {.explicitThinkingToggle = config.explicitThinkingToggle, .cache = config.cacheDialect});
    auto result = models::Request(config.baseUrl, protocol, request, std::move(key), stop, {}, inactivityTimeout, config.fullUrl);
    if (stop.stop_requested()) return {};
    CompletionResult output;
    for (auto const& part : result.message.content) if (part.kind == ContentKind::Text) output.text += folia::utf8_to_cps(part.text);
    output.inputTokens = result.usage.inputTokens; output.outputTokens = result.usage.outputTokens;
    output.cachedInputTokens = result.usage.cachedInputTokens;
    output.truncated = result.finish == FinishReason::Truncated;
    return output;
}
CompletionModelPage ParseCompletionModels(std::string_view json, int protocol, ai::ModelCatalogSchema schema) {
    if (json.size() > 1024 * 1024) throw std::runtime_error("Model metadata exceeds limit.");
    auto root = folia::ai::Json::parse(json);
    auto found = root.find("data");
    if (found == root.end() || !found->is_array() || found->size() > 4096) throw std::runtime_error("Invalid model metadata response.");
    CompletionModelPage page;
    auto addLevel = [&](CompletionModel& model, std::string name) {
        if (name.empty() || name.size() > 32 || !std::ranges::all_of(name, [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        })) throw std::runtime_error("Invalid reasoning level.");
        if (folia::ai::SupportsReasoningOptions(static_cast<folia::ai::ApiProtocol>(protocol), {name})
            && std::ranges::find(model.reasoningEfforts, name) == model.reasoningEfforts.end())
            model.reasoningEfforts.push_back(std::move(name));
    };
    for (auto const& item : *found) {
        if (!item.is_object()) throw std::runtime_error("Invalid model metadata.");
        CompletionModel model;
        model.id = item.value("id", std::string{});
        if (model.id.empty() || model.id.size() > 128 || model.id.find_first_of("\r\n\0", 0, 3) != std::string::npos)
            throw std::runtime_error("Invalid model identifier.");
        if (auto name = item.find(schema == ai::ModelCatalogSchema::OpenRouter ? "name" : "display_name");
            (schema == ai::ModelCatalogSchema::OpenRouter || protocol == 2) && name != item.end() && name->is_string())
            model.name = name->get<std::string>();
        if (model.name.size() > 256 || model.name.find_first_of("\r\n\0", 0, 3) != std::string::npos)
            throw std::runtime_error("Invalid model display name.");
        auto limit = [](ai::Json const& record, char const* key) -> std::optional<std::uint32_t> {
            auto value = record.find(key);
            if (value == record.end() || !value->is_number_integer()) return {};
            auto count = value->get<std::uint64_t>();
            if (!count || count > (std::numeric_limits<std::uint32_t>::max)()) return {};
            return static_cast<std::uint32_t>(count);
        };
        if (schema == ai::ModelCatalogSchema::OpenRouter) {
            model.maxInputTokens = limit(item, "context_length");
            if (auto top = item.find("top_provider"); top != item.end())
                model.maxOutputTokens = limit(*top, "max_completion_tokens");
            if (auto architecture = item.find("architecture"); architecture != item.end()) {
                auto inputs = architecture->find("input_modalities");
                model.vision = inputs != architecture->end() && inputs->is_array() && std::ranges::contains(*inputs, "image");
            }
        } else if (protocol == 2) {
            model.maxInputTokens = limit(item, "max_input_tokens");
            model.maxOutputTokens = limit(item, "max_tokens");
        }
        if (protocol == 2 && item.contains("capabilities") && !item["capabilities"].is_null()) {
            auto const& capabilities = item.at("capabilities");
            auto supported = [](auto const& group, char const* tool) {
                auto entry = group.find(tool);
                if (entry == group.end()) return false;
                auto flag = entry->find("supported");
                return flag != entry->end() && flag->is_boolean() && flag->template get<bool>();
            };
            model.vision = supported(capabilities, "image_input");
            if (auto tools = capabilities.find("server_tools"); tools != capabilities.end()) {
                bool standard = supported(*tools, "web_search");
                bool deepseek = supported(*tools, "deepseek_web_search");
                model.webSearch = standard || deepseek;
                const bool nativeFetch = supported(*tools, "web_fetch");
                const bool deepSeekFetch = supported(*tools, "deepseek_read_url");
                model.webFetch = nativeFetch || deepSeekFetch;
                standard |= nativeFetch;
                deepseek |= deepSeekFetch;
                if (standard != deepseek)
                    model.searchFormat = standard ? ai::WebSearchDialect::Standard : ai::WebSearchDialect::DeepSeek;
            }
            auto declared = capabilities.find("effort");
            if (declared != capabilities.end() && !declared->is_null()) {
                if (!declared->is_object() || declared->size() > 33) throw std::runtime_error("Invalid reasoning capabilities.");
                if (declared->value("supported", false)) {
                    for (auto const& effort : declared->items()) {
                        auto const& options = effort.value();
                        if (effort.key() != "supported" && options.is_object() && options.value("supported", false))
                            addLevel(model, effort.key());
                    }
                }
            }
        }
        if (std::ranges::find(page.models, model.id, &CompletionModel::id) == page.models.end()) page.models.push_back(std::move(model));
    }
    if (protocol == 2 && root.value("has_more", false)) {
        page.next = root.value("last_id", std::string{});
        if (page.models.empty() || page.next.empty() || page.next.size() > 128) throw std::runtime_error("Invalid model pagination.");
    }
    return page;
}
std::vector<CompletionModel> FetchCompletionModels(CompletionConfig const& config, std::stop_token stop, std::wstring key, ai::ModelCatalogSchema schema) {
    auto endpoint = ResolveCompletionModelsEndpoint(config);
    std::vector<CompletionModel> models;
    std::vector<std::string> cursors;
    auto url = endpoint;
    for (int count = 0; count < 16; ++count) {
        auto response = models::ReadCatalogPage(static_cast<ai::ApiProtocol>(config.protocol), url, key, stop);
        if (stop.stop_requested()) return {};
        auto page = ParseCompletionModels(response, config.protocol, schema);
        for (auto& model : page.models)
            if (std::ranges::find(models, model.id, &CompletionModel::id) == models.end()) models.push_back(std::move(model));
        if (models.size() > 4096) throw std::runtime_error("Model list exceeds limit.");
        if (page.next.empty()) return models;
        if (std::ranges::find(cursors, page.next) != cursors.end()) throw std::runtime_error("Repeated model pagination cursor.");
        cursors.push_back(page.next);
        std::string encoded;
        constexpr char hex[] = "0123456789ABCDEF";
        for (unsigned char c : page.next) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') encoded += c;
            else { encoded += '%'; encoded += hex[c >> 4]; encoded += hex[c & 15]; }
        }
        url = endpoint + (endpoint.find('?') == std::string::npos ? "?" : "&") + "after_id=" + encoded;
    }
    throw std::runtime_error("Too many model metadata pages.");
}
}
