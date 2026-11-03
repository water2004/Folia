#pragma once
#include "ModelTypes.h"
#include "SseParser.h"
#include <memory>
namespace fairy::providers {
class IResponseDecoder {
  public:
    virtual ~IResponseDecoder() = default;
    virtual std::vector<StreamEvent> OnEvent(SseEvent const& event) = 0;
    // Missing terminal events or incomplete tool arguments fail closed.
    virtual ModelResult Finish() = 0;
};
Json BuildRequest(ApiProtocol protocol, ModelRequest const& request);
using RequestHeaders = std::vector<std::pair<std::string, std::string>>;
RequestHeaders BuildHeaders(ApiProtocol protocol, std::string_view apiKey);
ModelResult ParseResponse(ApiProtocol protocol, Json const& response);
std::unique_ptr<IResponseDecoder> CreateStreamDecoder(ApiProtocol protocol);
} // namespace fairy::providers
