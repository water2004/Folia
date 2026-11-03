#include "StreamResponse.h"

#include <iterator>

namespace fairy::providers {

StreamResponse::StreamResponse(ApiProtocol protocol) : decoder_(CreateStreamDecoder(protocol)) {}

std::vector<StreamEvent> StreamResponse::Feed(std::string_view bytes) {
    if (finished_)
        throw ProtocolError("Stream is already finished");
    std::vector<StreamEvent> output;
    for (auto const& event : parser_.Feed(bytes)) {
        auto decoded = decoder_->OnEvent(event);
        output.insert(output.end(), std::make_move_iterator(decoded.begin()),
                      std::make_move_iterator(decoded.end()));
    }
    return output;
}

ModelResult StreamResponse::Finish() {
    if (finished_)
        throw ProtocolError("Stream is already finished");
    finished_ = true;
    parser_.Finish();
    return decoder_->Finish();
}

} // namespace fairy::providers
