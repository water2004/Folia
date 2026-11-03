#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace fairy::providers {
struct SseEvent {
    std::string event;
    std::string data;
    std::string id;
};
class SseParser {
  public:
    std::vector<SseEvent> Feed(std::string_view bytes);
    void Finish() const;

  private:
    std::string pending_;
    SseEvent current_;
    std::size_t eventBytes_{};
    bool hasData_{};
};
} // namespace fairy::providers
