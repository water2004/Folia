// GitGraph statement grammar from Mermaid 12.2.0's gitGraph.langium.
// See ../LICENSE for the Mermaid license.
module folia.mermaid.git_graph;
import std;

namespace folia::mermaid {
namespace {
constexpr bool space(char16_t ch) { return ch == u' ' || ch == u'\t'; }
constexpr bool newline(char16_t ch) { return ch == u'\n' || ch == u'\r'; }
constexpr bool digit(char16_t ch) { return ch >= u'0' && ch <= u'9'; }
constexpr bool reference_char(char16_t ch) {
    return digit(ch) || (ch >= u'a' && ch <= u'z') ||
        (ch >= u'A' && ch <= u'Z') || ch == u'_';
}

std::u16string normalize_metadata(std::u16string_view text) {
    std::u16string result;
    bool pending_space = false;
    bool pending_line = false;
    for (const auto ch : text) {
        if (newline(ch)) {
            pending_line = true;
            pending_space = false;
        } else if (space(ch)) {
            pending_space = true;
        } else {
            if (!result.empty() && pending_line) result += u'\n';
            else if (!result.empty() && pending_space) result += u' ';
            pending_line = pending_space = false;
            result += ch;
        }
    }
    return result;
}

class StatementReader {
public:
    explicit StatementReader(std::u16string_view source) : rest_(source) {}

    bool at_end() const { return rest_.empty(); }
    bool starts_with(std::u16string_view prefix) const { return rest_.starts_with(prefix); }

    void skip_space() {
        while (!at_end() && space(rest_.front())) rest_.remove_prefix(1);
    }

    bool consume(std::u16string_view token) {
        skip_space();
        if (!starts_with(token)) return false;
        rest_.remove_prefix(token.size());
        return true;
    }

    bool at_statement_end() {
        skip_space();
        return at_end() || newline(rest_.front()) || starts_with(u"%%");
    }

    void skip_empty_lines() {
        while (at_statement_end() && !at_end()) {
            if (starts_with(u"%%")) take_to_line_end();
            else rest_.remove_prefix(1);
        }
    }

    void finish_statement() {
        if (!at_statement_end()) throw std::runtime_error("Unexpected Git graph statement text");
    }

    std::u16string_view word() {
        skip_space();
        const auto end = rest_.find_first_of(u" \t\r\n:{}\"'%");
        return take(end);
    }

    bool next_is_string() {
        skip_space();
        return starts_with(u"\"") || starts_with(u"'");
    }

    std::u16string string() {
        if (!next_is_string()) throw std::runtime_error("Expected a quoted Git graph string");
        const auto quote = rest_.front();
        rest_.remove_prefix(1);
        std::u16string result;
        while (!at_end()) {
            auto ch = rest_.front();
            rest_.remove_prefix(1);
            if (ch == quote) return result;
            if (ch == u'\\') {
                if (at_end() || newline(rest_.front())) break;
                ch = rest_.front();
                rest_.remove_prefix(1);
                constexpr std::u16string_view escapes = u"bfnrtv0";
                constexpr std::u16string_view values{u"\b\f\n\r\t\v\0", 7};
                const auto index = escapes.find(ch);
                if (index != escapes.npos) ch = values[index];
            }
            result += ch;
        }
        throw std::runtime_error("Incomplete Git graph string");
    }

    std::u16string branch_name() {
        if (next_is_string()) return string();
        const auto name = word();
        constexpr std::array reserved{u"gitGraph", u"commit", u"branch", u"merge",
            u"checkout", u"switch", u"cherry-pick", u"LR", u"TB", u"BT",
            u"NORMAL", u"REVERSE", u"HIGHLIGHT", u"title"};
        if (name.empty() || !reference_char(name.front()) ||
            !(reference_char(name.back()) || name.back() == u'-') ||
            !std::ranges::all_of(name, [](char16_t ch) {
                return reference_char(ch) || ch == u'-' || ch == u'.' || ch == u'/';
            }) || std::ranges::all_of(name, digit) || std::ranges::contains(reserved, name))
            throw std::runtime_error("Invalid Git graph branch name");
        return std::u16string(name);
    }

    std::u16string_view take_to_line_end() { return take(rest_.find_first_of(u"\r\n")); }

    std::u16string metadata_line() {
        const auto line = take_to_line_end();
        return normalize_metadata(line.substr(0, line.find(u"%%")));
    }

    std::u16string title() {
        if (!at_end() && !space(rest_.front()) && !newline(rest_.front()) && !starts_with(u"%%"))
            throw std::runtime_error("Expected whitespace after Git graph title");
        return metadata_line();
    }

    std::u16string description() {
        if (consume(u":")) return metadata_line();
        take(rest_.find_first_not_of(u" \t\r\n"));
        if (!consume(u"{")) throw std::runtime_error("Expected a Git graph accessibility description");
        const auto close = rest_.find(u'}');
        if (close == rest_.npos)
            throw std::runtime_error("Incomplete Git graph accessibility description");
        const auto body = take(close);
        rest_.remove_prefix(1);
        return normalize_metadata(body);
    }

private:
    std::u16string_view take(std::size_t count) {
        const auto value = rest_.substr(0, count);
        rest_.remove_prefix(value.size());
        return value;
    }

    std::u16string_view rest_;
};

double parse_order(std::u16string_view value) {
    if (value.empty() || (value.size() > 1 && value.front() == u'0') ||
        !std::ranges::all_of(value, digit))
        throw std::runtime_error("Invalid Git graph branch order");
    const std::string digits(value.begin(), value.end());
    double order = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), order);
    if (parsed.ec != std::errc{}) throw std::runtime_error("Invalid Git graph branch order");
    return order;
}

GitCommitType parse_type(std::u16string_view value) {
    if (value == u"NORMAL") return GitCommitType::normal;
    if (value == u"REVERSE") return GitCommitType::reverse;
    if (value == u"HIGHLIGHT") return GitCommitType::highlight;
    throw std::runtime_error("Invalid Git graph commit type");
}

enum class Command { commit = 1, merge = 2, cherry_pick = 4 };
enum class Option { id, message, tag, type, parent };
struct CommitOptions {
    GitCommit commit;
    std::optional<GitCommitType> type;
    std::u16string parent;
};

CommitOptions parse_options(StatementReader& reader, Command command) {
    // Fields shared by the commit-producing statements. A bare string is
    // shorthand for msg: only on commit.
    constexpr auto commit = static_cast<int>(Command::commit);
    constexpr auto merge = static_cast<int>(Command::merge);
    constexpr auto cherry_pick = static_cast<int>(Command::cherry_pick);
    constexpr auto all = commit | merge | cherry_pick;
    static constexpr struct { std::u16string_view token; Option option; int commands; } fields[]{
        {u"id:", Option::id, all}, {u"msg:", Option::message, commit},
        {u"tag:", Option::tag, all}, {u"type:", Option::type, commit | merge},
        {u"parent:", Option::parent, cherry_pick},
    };
    CommitOptions options;
    while (!reader.at_statement_end()) {
        if (command == Command::commit && reader.next_is_string()) {
            options.commit.message = reader.string();
            continue;
        }
        const auto field = std::ranges::find_if(fields, [&](const auto& candidate) {
            return reader.starts_with(candidate.token);
        });
        if (field == std::end(fields) || !(field->commands & static_cast<int>(command)))
            throw std::runtime_error("Unknown Git graph statement option");
        reader.consume(field->token);
        switch (field->option) {
        case Option::id: options.commit.id = reader.string(); break;
        case Option::message: options.commit.message = reader.string(); break;
        case Option::tag: options.commit.tags.push_back(reader.string()); break;
        case Option::type: options.type = parse_type(reader.word()); break;
        case Option::parent: options.parent = reader.string(); break;
        }
    }
    return options;
}

std::u16string generated_id(std::size_t sequence) {
    const auto prefix = std::to_string(sequence);
    std::u16string result(prefix.begin(), prefix.end());
    result += u'-';
    std::random_device random;
    constexpr std::u16string_view chars = u"0123456789abcdef";
    for (int i = 0; i < 7; ++i) result += chars[random() & 15];
    return result;
}
}

std::expected<GitGraphDocument, std::string> parse_git_graph(
    std::u16string_view source, std::u16string_view main_branch, double main_order) {
    try {
        GitGraphDocument graph;
        graph.current = main_branch;
        graph.branches.push_back({std::u16string(main_branch), main_order, {}});
        StatementReader reader(source);
        reader.skip_empty_lines();
        if (reader.word() != u"gitGraph") throw std::runtime_error("Expected Git graph header");
        if (!reader.consume(u":")) {
            auto header_tail = reader;
            const auto direction = header_tail.word();
            if (direction == u"LR" || direction == u"TB" || direction == u"BT") {
                if (!header_tail.consume(u":")) throw std::runtime_error("Expected Git graph direction colon");
                graph.direction = direction;
                reader = header_tail;
            }
        }

        std::size_t sequence = 0;
        const auto branch_index = [&](std::u16string_view name) {
            const auto found = std::ranges::find(graph.branches, name, &GitBranch::name);
            if (found == graph.branches.end())
                throw std::runtime_error("Git graph branch does not exist");
            return static_cast<std::size_t>(found - graph.branches.begin());
        };
        const auto append = [&](GitCommit commit) {
            commit.seq = sequence++;
            commit.branch = graph.current;
            if (commit.id.empty()) {
                commit.generated = true;
                commit.id = generated_id(commit.seq);
            }
            const auto found = std::ranges::find(graph.commits, commit.id, &GitCommit::id);
            graph.head = static_cast<std::size_t>(found - graph.commits.begin());
            if (found == graph.commits.end()) graph.commits.push_back(std::move(commit));
            else *found = std::move(commit);
            graph.branches[branch_index(graph.current)].head = graph.head;
        };

        for (reader.skip_empty_lines(); !reader.at_end(); reader.skip_empty_lines()) {
            const auto statement = reader.word();
            if (statement == u"title") {
                graph.title = reader.title();
            } else if (statement == u"accTitle") {
                if (!reader.consume(u":")) throw std::runtime_error("Expected Git graph accTitle:");
                graph.acc_title = reader.metadata_line();
            } else if (statement == u"accDescr") {
                graph.acc_description = reader.description();
            } else if (statement == u"branch") {
                auto name = reader.branch_name();
                const auto order = reader.consume(u"order:") ? parse_order(reader.word()) : 0;
                if (std::ranges::find(graph.branches, name, &GitBranch::name) != graph.branches.end())
                    throw std::runtime_error("Git graph branch already exists");
                graph.current = name;
                graph.branches.push_back({std::move(name), order, graph.head});
            } else if (statement == u"checkout" || statement == u"switch") {
                const auto& branch = graph.branches[branch_index(reader.branch_name())];
                graph.current = branch.name;
                graph.head = branch.head;
            } else if (statement == u"commit") {
                auto options = parse_options(reader, Command::commit);
                if (options.type) options.commit.type = *options.type;
                if (graph.head) options.commit.parents.push_back(*graph.head);
                append(std::move(options.commit));
            } else if (statement == u"merge") {
                const auto other = reader.branch_name();
                auto options = parse_options(reader, Command::merge);
                auto& commit = options.commit;
                const auto other_head = graph.branches[branch_index(other)].head;
                if (other == graph.current || !graph.head || !other_head ||
                    graph.head == other_head || graph.commits[*graph.head].branch == other)
                    throw std::runtime_error("Invalid Git graph merge");
                if (!commit.id.empty() &&
                    std::ranges::find(graph.commits, commit.id, &GitCommit::id) != graph.commits.end())
                    throw std::runtime_error("Git graph merge id already exists");
                commit.custom_id = !commit.id.empty();
                commit.custom_type = options.type;
                commit.type = GitCommitType::merge;
                commit.message = u"merged branch " + other + u" into " + graph.current;
                commit.parents = {*graph.head, *other_head};
                append(std::move(commit));
            } else if (statement == u"cherry-pick") {
                auto options = parse_options(reader, Command::cherry_pick);
                auto& commit = options.commit;
                const auto original = std::ranges::find(graph.commits, commit.id, &GitCommit::id);
                if (original == graph.commits.end())
                    throw std::runtime_error("Git graph cherry-pick needs an existing commit id");
                if (original->branch == graph.current || !graph.head)
                    throw std::runtime_error("Invalid Git graph cherry-pick branch");
                if (!options.parent.empty()) {
                    const auto immediate = [&](std::size_t index) {
                        return graph.commits[index].id == options.parent;
                    };
                    if (!std::ranges::any_of(original->parents, immediate))
                        throw std::runtime_error("Git graph cherry-pick parent is not immediate");
                } else if (original->type == GitCommitType::merge) {
                    throw std::runtime_error("Git graph cherry-pick of merge needs parent");
                }
                commit.type = GitCommitType::cherry_pick;
                commit.message = u"cherry-picked " + original->message + u" into " + graph.current;
                commit.parents = {*graph.head, static_cast<std::size_t>(original - graph.commits.begin())};
                if (commit.tags.empty()) {
                    commit.tags.push_back(u"cherry-pick:" + original->id);
                    if (original->type == GitCommitType::merge)
                        commit.tags.back() += u"|parent:" + options.parent;
                } else {
                    std::erase(commit.tags, std::u16string{});
                }
                commit.id.clear();
                append(std::move(commit));
            } else {
                throw std::runtime_error("Unknown Git graph statement");
            }
            reader.finish_statement();
        }
        return graph;
    } catch (const std::runtime_error& error) {
        return std::unexpected(error.what());
    }
}
}
