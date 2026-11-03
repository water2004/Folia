export module folia.completion;
import std;

export namespace folia::completion {
inline constexpr std::size_t max_context_bytes = 64000;
inline constexpr std::uint32_t max_output_tokens = 4096;
// Limits include UTF-8 encoding expansion. No caller needs to copy a document.
struct Budget {
    std::size_t prefix_bytes = 4096;
    std::size_t suffix_bytes = 1024;
    std::size_t preceding_bytes = 2048;
    std::uint32_t output_tokens = 128;
    std::size_t proposal_characters = 512;
};
struct Context {
    std::string preceding, prefix, suffix;
    bool operator==(Context const&) const = default;
};
struct Prompt {
    std::string instructions, context, cursor;
    std::uint32_t output_tokens = 128;
};
enum class Eagerness { Conservative, Balanced, Proactive };
Budget make_budget(std::size_t context_bytes, std::uint32_t output_tokens);
Context capture(std::u32string_view source, std::size_t caret,
    std::span<std::u32string_view const> preceding = {}, Budget budget = {});
Prompt make_prompt(Context const&, Budget budget = {}, Eagerness eagerness = Eagerness::Balanced);
std::u32string sanitize(std::u32string_view proposal, Context const&, Budget budget = {});

// Cache contents are document-derived and deliberately never persisted.
class Cache {
public:
    explicit Cache(std::size_t max_bytes = 256 * 1024, std::size_t max_entries = 32);
    std::optional<std::u32string> get(Context const&);
    void put(Context, std::u32string);
    void clear();
    std::size_t bytes() const { return bytes_; }
    std::size_t size() const { return entries_.size(); }
private:
    struct Entry { Context context; std::u32string proposal; std::size_t bytes; };
    std::list<Entry> entries_;
    std::size_t bytes_ = 0, max_bytes_, max_entries_;
};

// A proposal is attached to one authoritative source position, not display text.
class Proposal {
public:
    void set(std::uint64_t owner, std::size_t offset, std::u32string text);
    void clear();
    bool consume(std::uint64_t owner, std::size_t offset, std::u32string_view inserted);
    bool at(std::uint64_t owner, std::size_t offset) const;
    std::u32string_view text() const { return text_; }
    std::uint64_t generation() const { return generation_; }
private:
    std::uint64_t owner_ = 0, generation_ = 0;
    std::size_t offset_ = 0;
    std::u32string text_;
};
}
