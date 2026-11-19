// Port of pinned Mermaid 12.2.0 detector registration and builtin predicates.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
module folia.mermaid.diagram_registry;
import std;
import folia.mermaid.source_text;
import folia.mermaid.directive_lexer;
import folia.mermaid.config_support;

namespace folia::mermaid {
namespace {
using detail::is_ecmascript_whitespace;
using detail::is_word;
std::optional<std::uint32_t> index_key(std::u16string_view key) {
    if (key.empty() || key.size() > 10 || (key.size() > 1 && key.front() == u'0')) return {};
    std::uint64_t value = 0;
    for (const auto c : key) {
        if (c < u'0' || c > u'9') return {};
        value = value * 10 + (c - u'0');
    }
    if (value >= 4294967295ULL) return {};
    return static_cast<std::uint32_t>(value);
}
bool prototype_key(std::u16string_view key) {
    constexpr std::u16string_view names[] = {u"__proto__", u"constructor", u"__defineGetter__",
        u"__defineSetter__", u"hasOwnProperty", u"__lookupGetter__", u"__lookupSetter__",
        u"isPrototypeOf", u"propertyIsEnumerable", u"toString", u"valueOf", u"toLocaleString"};
    return std::ranges::find(names, key) != std::end(names);
}
char16_t ascii_lower(char16_t c) { return c >= u'A' && c <= u'Z' ? c + (u'a' - u'A') : c; }
bool prefix(std::u16string_view text, std::u16string_view word, bool insensitive = false) {
    if (text.size() < word.size()) return false;
    for (std::size_t i = 0; i < word.size(); ++i)
        if ((insensitive ? ascii_lower(text[i]) : text[i]) != word[i]) return false;
    return true;
}
enum class Match { prefix, insensitive, word, insensitive_word, space_end, space_colon_end, c4, flow, elk, error };
struct Builtin { std::u16string_view id, keyword; Match match = Match::prefix; };
// Original addDiagrams order. Optional suffixes with no following assertion do
// not affect regex.test's boolean result. ishikawa(-beta)?\b may backtrack to
// the boundary before '-', so testing the base keyword boundary is equivalent.
constexpr Builtin sentinels[] = {{u"error",u"error",Match::error},{u"---",u"---"}};
constexpr Builtin large[] = {{u"flowchart-elk",u"flowchart-elk",Match::elk},
    {u"mindmap",u"mindmap"},{u"architecture",u"architecture"}};
constexpr Builtin ordinary[] = {
    {u"agentflow",u"agentflow-beta",Match::word},{u"c4",u"C4Context",Match::c4},
    {u"kanban",u"kanban"},{u"classDiagram",u"classDiagram"},{u"er",u"erDiagram"},
    {u"gantt",u"gantt"},{u"info",u"info"},{u"pie",u"pie"},{u"requirement",u"requirement"},
    {u"sequence",u"sequenceDiagram"},{u"swimlane",u"swimlane-beta",Match::word},
    {u"flowchart-v2",u"flowchart",Match::flow},{u"timeline",u"timeline"},{u"gitGraph",u"gitGraph"},
    {u"stateDiagram",u"stateDiagram"},{u"journey",u"journey"},{u"quadrantChart",u"quadrantChart"},
    {u"sankey",u"sankey"},{u"packet",u"packet"},{u"xychart",u"xychart"},{u"block",u"block"},
    {u"eventmodeling",u"eventmodeling"},{u"treeView",u"treeView-beta"},{u"radar",u"radar-beta"},
    {u"ishikawa",u"ishikawa",Match::insensitive_word},{u"treemap",u"treemap"},
    {u"railroad",u"railroad-beta",Match::insensitive},
    {u"railroadEbnf",u"railroad-ebnf-beta",Match::insensitive},
    {u"railroadAbnf",u"railroad-abnf-beta",Match::insensitive},
    {u"railroadPeg",u"railroad-peg-beta",Match::insensitive},
    {u"venn",u"venn-beta"},{u"wardley",u"wardley-beta",Match::insensitive},
    {u"cynefin",u"cynefin-beta",Match::space_colon_end},{u"usecase",u"usecase-beta",Match::space_end}
};
bool builtin_match(const Builtin& rule, std::u16string_view text, ConfigValue& config) {
    while (!text.empty() && is_ecmascript_whitespace(text.front())) text.remove_prefix(1);
    if (rule.match == Match::error) {
        while (!text.empty() && is_ecmascript_whitespace(text.back())) text.remove_suffix(1);
        return text.size() == rule.keyword.size() && prefix(text, rule.keyword, true);
    }
    if (rule.match == Match::c4) {
        return text.starts_with(rule.keyword) || text.find(u"C4Container") != text.npos ||
            text.find(u"C4Component") != text.npos || text.find(u"C4Dynamic") != text.npos ||
            text.find(u"C4Deployment") != text.npos;
    }
    if (rule.match == Match::flow) return text.starts_with(u"graph") || text.starts_with(u"flowchart");
    if (!prefix(text, rule.keyword, rule.match == Match::insensitive || rule.match == Match::insensitive_word)) return false;
    const auto tail = text.substr(rule.keyword.size());
    if (rule.match == Match::word || rule.match == Match::insensitive_word)
        return tail.empty() || !is_word(tail.front()); // JS non-/u \b uses ASCII word chars.
    if (rule.match == Match::space_end || rule.match == Match::space_colon_end)
        return tail.empty() || is_ecmascript_whitespace(tail.front()) ||
            (rule.match == Match::space_colon_end && tail.front() == u':');
    if (rule.match == Match::elk) config.mapping[u"layout"] = config_detail::string(u"elk");
    return true;
}
std::u16string error_text(std::string_view text) { return {text.begin(), text.end()}; }
}

std::expected<bool, std::string> DiagramRegistry::add_detector(std::u16string_view id,
    DiagramDetector detector, std::optional<std::u16string> loader) {
    if (!detector) return std::unexpected("A diagram detector callback is required");
    if (prototype_key(id)) return std::unexpected("Object.prototype detector ids are unsupported");
    const auto found = std::ranges::find_if(entries_, [&](const auto& entry) { return entry->info.id == id; });
    const bool replaced = found != entries_.end();
    if (!replaced && entries_.size() >= limits_.detectors) return std::unexpected("Detector count budget exceeded");
    const auto removed = replaced ? (*found)->info.id.size() + ((*found)->info.loader_route ? (*found)->info.loader_route->size() : 0) : 0;
    const auto remaining = limits_.registry_units - (registry_units_ - removed);
    if (id.size() > remaining || (loader && loader->size() > remaining - id.size()))
        return std::unexpected("Detector registry string budget exceeded");
    auto entry = std::make_shared<const Entry>(Entry{{std::u16string{id},std::move(loader)},std::move(detector)});
    const auto added = entry->info.id.size() + (entry->info.loader_route ? entry->info.loader_route->size() : 0);
    if (replaced) *found = std::move(entry);
    else {
        // Non-index keys keep insertion order; only a new index key can precede
        // existing entries. Replacement never resorts or changes position.
        auto at = entries_.end();
        if (const auto index = index_key(id)) at = std::ranges::find_if(entries_, [&](const auto& old) {
            const auto other = index_key(old->info.id); return !other || *index < *other;
        });
        entries_.insert(at, std::move(entry));
    }
    registry_units_ = registry_units_ - removed + added;
    return replaced;
}
std::expected<std::size_t, std::string> DiagramRegistry::add_builtin_diagrams(bool include_large) {
    if (builtins_added_) return 0;
    auto next = *this;
    std::size_t replacements = 0;
    auto add = [&](const Builtin& rule, bool lazy) -> std::expected<void,std::string> {
        auto result = next.add_detector(rule.id, [rule](std::u16string_view text, ConfigValue& config) -> std::expected<bool,std::u16string> {
            return builtin_match(rule, text, config);
        }, lazy ? std::optional<std::u16string>{rule.id} : std::nullopt);
        if (!result) return std::unexpected(result.error());
        replacements += *result ? 1 : 0;
        return {};
    };
    for (const auto& rule : sentinels) if (auto result = add(rule,false); !result) return std::unexpected(result.error());
    if (include_large) for (const auto& rule : large) if (auto result = add(rule,true); !result) return std::unexpected(result.error());
    for (const auto& rule : ordinary) if (auto result = add(rule,true); !result) return std::unexpected(result.error());
    next.builtins_added_ = true;
    *this = std::move(next);
    return replacements;
}
std::vector<DiagramDetectorInfo> DiagramRegistry::detectors() const {
    std::vector<DiagramDetectorInfo> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_) result.push_back(entry->info);
    return result;
}
std::expected<std::optional<std::u16string>, std::string> DiagramRegistry::loader_route(std::u16string_view id) const {
    const auto found = std::ranges::find_if(entries_, [&](const auto& entry) { return entry->info.id == id; });
    if (found == entries_.end()) return std::unexpected("Diagram detector is not registered");
    return (*found)->info.loader_route;
}
std::expected<DiagramDetection, DiagramDetectionError> DiagramRegistry::detect(
    std::u16string_view text, const ConfigValue* config) const {
    using Kind = DiagramDetectionErrorKind;
    if (text.size() > limits_.source_units) return std::unexpected(DiagramDetectionError{Kind::invalid_input,u"Detector source budget exceeded"});
    try {
        config_detail::Budget budget{limits_.config};
        if (config) {
            if (config->kind != ConfigKind::mapping) return std::unexpected(DiagramDetectionError{Kind::invalid_input,u"Detector configuration must be a mapping"});
            budget.inspect(*config);
        }
        auto working = config ? *config : config_detail::map();
        const auto frontmatter = match_front_matter(text);
        const auto cleaned = strip_any_comments(remove_directives(frontmatter ? text.substr(frontmatter->length) : text));
        const auto snapshot = entries_;
        for (const auto& entry : snapshot) {
            auto match = entry->detector(cleaned, working);
            if (!match) {
                if (match.error().size() > limits_.source_units)
                    return std::unexpected(DiagramDetectionError{Kind::invalid_input,u"Detector error budget exceeded"});
                return std::unexpected(DiagramDetectionError{Kind::detector_failure,std::move(match.error())});
            }
            budget.output(working);
            if (working.kind != ConfigKind::mapping) return std::unexpected(DiagramDetectionError{Kind::invalid_input,u"Detector callback replaced the configuration mapping"});
            if (*match) return DiagramDetection{entry->info.id,std::move(working)};
        }
        return std::unexpected(DiagramDetectionError{Kind::unknown_diagram,
            u"No diagram type detected matching given configuration for text: " + cleaned});
    } catch (const std::exception& error) {
        return std::unexpected(DiagramDetectionError{Kind::invalid_input,error_text(error.what())});
    }
}
}
