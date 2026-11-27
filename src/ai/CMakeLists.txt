add_library(folia_ai STATIC)
add_library(folia::ai ALIAS folia_ai)
target_compile_features(folia_ai PUBLIC cxx_std_23)
set_target_properties(folia_ai PROPERTIES CXX_MODULE_STD ON)
target_include_directories(folia_ai PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/nlohmann-json/include)

target_sources(folia_ai
    PUBLIC FILE_SET CXX_MODULES FILES
        agent/Agent.ixx
        agent/SearchQuery.ixx
        completion/completion.ixx
        providers/ModelTypes.ixx
        providers/providers.ixx
        providers/HostedTools.ixx
        providers/ModelAdapter.ixx
        providers/ModelCodec.ixx
        providers/ModelValidation.ixx
        providers/SseParser.ixx
        providers/StreamResponse.ixx
    PRIVATE FILE_SET ai_internals TYPE CXX_MODULES FILES
        agent/Context.ixx
        agent/HostedSearch.ixx
        agent/ToolOutput.ixx
        providers/JsonParsing.cpp
        providers/adapters/Adapters.cpp
        providers/adapters/SearchAction.cpp
        providers/adapters/anthropic/AnthropicParsing.cpp
        providers/adapters/chat_completions/HostedActivity.cpp
        providers/adapters/chat_completions/ChatCompletionsParsing.cpp
        providers/adapters/responses/ResponsesParsing.cpp
    PRIVATE
        agent/Agent.cpp
        agent/Context.cpp
        agent/HostedSearch.cpp
        agent/ToolOutput.cpp
        completion/completion.cpp
        completion/edit.cpp
        providers/ModelAdapter.cpp
        providers/ModelCodec.cpp
        providers/ModelTypes.cpp
        providers/ModelValidation.cpp
        providers/PromptCache.cpp
        providers/SseParser.cpp
        providers/StreamResponse.cpp
        providers/adapters/anthropic/Request.cpp
        providers/adapters/anthropic/Response.cpp
        providers/adapters/anthropic/StreamDecoder.cpp
        providers/adapters/chat_completions/Request.cpp
        providers/adapters/chat_completions/Response.cpp
        providers/adapters/chat_completions/StreamDecoder.cpp
        providers/adapters/responses/Request.cpp
        providers/adapters/responses/Response.cpp
        providers/adapters/responses/StreamDecoder.cpp
)
