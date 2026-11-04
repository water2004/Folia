#pragma once

#include "ModelAdapter.h"

namespace fairy::providers {

class StreamResponse final {
  public:
    explicit StreamResponse(ApiProtocol protocol);

    StreamResponse(StreamResponse const&) = delete;
    StreamResponse& operator=(StreamResponse const&) = delete;

    [[nodiscard]] std::vector<StreamEvent> Feed(std::string_view bytes);
    [[nodiscard]] ModelResult Finish();
    [[nodiscard]] bool Complete() const noexcept { return decoder_->Complete(); }

  private:
    SseParser parser_;
    std::unique_ptr<IResponseDecoder> decoder_;
    bool finished_{};
};

} // namespace fairy::providers
