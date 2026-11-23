// Correlate a text-service commit with its later character-event fragments.
// The host supplies its document revision and monotonic time; no native text
// service or clock access is needed to test the ownership decision.
export module folia.core.input_text_echo;
import std;

export namespace folia {

class InputTextEcho {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr auto lifetime = std::chrono::milliseconds(250);

    void record(std::u32string text, std::uint64_t revision, TimePoint now) {
        text_ = std::move(text);
        consumed_ = 0;
        revision_ = revision;
        recorded_ = now;
    }

    void clear() {
        text_.clear();
        consumed_ = 0;
    }

    bool consume(std::u32string_view text, std::uint64_t revision, TimePoint now) {
        if (text.empty() || consumed_ == text_.size()) return false;
        if (revision != revision_ || now < recorded_ || now - recorded_ >= lifetime
            || !std::u32string_view(text_).substr(consumed_).starts_with(text)) {
            clear();
            return false;
        }
        consumed_ += text.size();
        if (consumed_ == text_.size()) clear();
        return true;
    }

private:
    std::u32string text_;
    std::size_t consumed_ = 0;
    std::uint64_t revision_ = 0;
    TimePoint recorded_{};
};

} // namespace folia
