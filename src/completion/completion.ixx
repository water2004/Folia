export module folia.completion;
import std;

export namespace folia::completion {
inline constexpr std::size_t max_context_bytes = 64000;
inline constexpr std::uint32_t max_output_tokens = 4096;
// Limits include UTF-8 encoding expansion. No caller needs to copy a document.
struct Budget {
    std::size_t prefix_bytes = 4096;
    std::size_t suffix_bytes = 1024;
    std::size_t preceding_bytes = 1024;
    std::uint32_t output_tokens = 128;
    std::size_t proposal_characters = 512;
    std::size_t following_bytes = 1024;
};
struct Context {
    std::string preceding, prefix, suffix;
    std::size_t active_prefix_byte = 0;
    std::size_t active_suffix_bytes = std::string::npos;
    std::string following;
    bool operator==(Context const&) const = default;
};
struct Prompt {
    std::string instructions, context, cursor;
    std::uint32_t output_tokens = 128;
};
enum class Eagerness { Conservative, Balanced, Proactive };
Budget make_budget(std::size_t context_bytes, std::uint32_t output_tokens);
Context capture(std::u32string_view source, std::size_t caret,
    std::span<std::u32string_view const> preceding = {}, Budget budget = {},
    std::span<std::u32string_view const> following = {});
Prompt make_prompt(Context const&, Budget budget = {}, Eagerness eagerness = Eagerness::Balanced);

// Expected text is an exact prefix of the bounded following source. Models
// return text, never document offsets. The editor supplies the source position.
struct Edit {
    std::u32string before, after;
    bool empty() const { return before == after; }
    bool operator==(Edit const&) const = default;
};
struct Change {
    std::size_t before_begin, before_end, after_begin, after_end;
};
std::optional<Edit> decode_edit_response(std::u32string_view, Context const&, Budget budget = {});
std::vector<Change> diff(Edit const&);
Edit prefix_edit(Edit const&, std::size_t accepted_characters);

// Cache contents are document-derived and deliberately never persisted.
class Cache {
public:
    explicit Cache(std::size_t max_bytes = 256 * 1024, std::size_t max_entries = 32);
    std::optional<Edit> get(Context const&);
    void put(Context, Edit);
    void clear();
    std::size_t bytes() const { return bytes_; }
    std::size_t size() const { return entries_.size(); }
private:
    struct Entry { Context context; Edit proposal; std::size_t bytes; };
    std::list<Entry> entries_;
    std::size_t bytes_ = 0, max_bytes_, max_entries_;
};

// A proposal is attached to one authoritative source position, not display text.
class Proposal {
public:
    void set(std::uint64_t owner, std::size_t offset, Edit edit);
    void clear();
    bool consume(std::uint64_t owner, std::size_t offset, std::u32string_view inserted);
    bool at(std::uint64_t owner, std::size_t offset) const;
    bool active() const { return !edit_.empty(); }
    std::u32string_view text() const { return edit_.after; }
    Edit const& edit() const { return edit_; }
    std::uint64_t generation() const { return generation_; }
private:
    std::uint64_t owner_ = 0, generation_ = 0;
    std::size_t offset_ = 0;
    Edit edit_;
};
}
