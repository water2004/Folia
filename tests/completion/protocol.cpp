#include "providers/ModelAdapter.h"
#include "providers/StreamResponse.h"
#include "support/folia_test.hpp"

namespace {
using namespace boost::ut;
using namespace fairy::providers;
std::string text(ModelResult const& result) {
    std::string out;
    for (auto const& part : result.message.content) if (part.kind == ContentKind::Text) out += part.text;
    return out;
}
suite completion_protocol_tests = [] {
    "completion protocol requests contain no tools and enforce output budgets"_test = [] {
        ModelRequest request; request.model = "fixture"; request.maxOutputTokens = 128;
        Message message; message.content.push_back({ContentKind::Text, "before cursor"});
        request.messages.push_back(message);
        for (auto protocol : {ApiProtocol::ChatCompletions, ApiProtocol::Responses, ApiProtocol::AnthropicMessages}) {
            auto body = BuildRequest(protocol, request);
            expect(!body.contains("tools") || body["tools"].empty()); expect(body["stream"].get<bool>());
            expect(body[protocol == ApiProtocol::Responses ? "max_output_tokens" : "max_tokens"].get<int>() == 128_i);
            expect(body["model"].get<std::string>() == "fixture");
        }
    };
    "completion protocol handles fragmented UTF8 chat stream with actual cache usage"_test = [] {
        std::string stream = R"(data: {"id":"one","choices":[{"index":0,"delta":{"content":" 世界"},"finish_reason":null}]})" "\n\n"
            R"(data: {"id":"one","choices":[{"index":0,"delta":{},"finish_reason":"stop"}]})" "\n\n"
            R"(data: {"choices":[],"usage":{"prompt_tokens":50,"completion_tokens":2,"total_tokens":52,"prompt_tokens_details":{"cached_tokens":32}}})" "\n\n"
            "data: [DONE]\n\n";
        StreamResponse decoder(ApiProtocol::ChatCompletions);
        for (auto c : stream) (void)decoder.Feed(std::string_view(&c, 1));
        auto result = decoder.Finish();
        expect(text(result) == " 世界"); expect(result.usage.cachedInputTokens.value() == 32_u);
        expect(result.usage.inputTokens.value() == 50_u);
    };
    "completion protocol consumes terminal Responses snapshot without extra text"_test = [] {
        std::string stream = R"(event: response.completed
data: {"type":"response.completed","response":{"id":"r","status":"completed","output":[{"type":"message","role":"assistant","content":[{"type":"output_text","text":" next"}]}],"usage":{"input_tokens":10,"output_tokens":1,"total_tokens":11}}}

)";
        StreamResponse decoder(ApiProtocol::Responses);
        for (std::size_t start = 0; start < stream.size(); start += 7) (void)decoder.Feed(std::string_view(stream).substr(start, 7));
        auto result = decoder.Finish(); expect(text(result) == " next");
        expect(!result.usage.cachedInputTokens.has_value());
    };
    "completion protocol consumes Anthropic streaming text and terminal usage"_test = [] {
        std::string stream = R"(data: {"type":"message_start","message":{"id":"m","type":"message","role":"assistant","usage":{"input_tokens":10}}}

data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":" hello"}}

data: {"type":"content_block_stop","index":0}

data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":1}}

data: {"type":"message_stop"}

)";
        StreamResponse decoder(ApiProtocol::AnthropicMessages);
        (void)decoder.Feed(stream);
        auto result = decoder.Finish(); expect(text(result) == " hello");
        expect(result.usage.outputTokens.value() == 1_u);
    };
    "completion protocol rejects truncated streams instead of accepting a partial response"_test = [] {
        for (auto protocol : {ApiProtocol::ChatCompletions, ApiProtocol::Responses, ApiProtocol::AnthropicMessages}) {
            bool threw = false;
            try { StreamResponse decoder(protocol); (void)decoder.Finish(); } catch (std::exception const&) { threw = true; }
            expect(threw);
        }
    };
};
}
