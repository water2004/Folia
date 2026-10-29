module folia.mermaid.frontend;
import std;
import folia.mermaid.source_text;
import folia.mermaid.config_support;
import folia.mermaid.config_init;
import folia.mermaid.config_clean_merge;
import folia.mermaid.diagram_registry;

namespace folia::mermaid {
static bool entity_word(char16_t c) {
    return (c>=u'0'&&c<=u'9')||(c>=u'a'&&c<=u'z')||(c>=u'A'&&c<=u'Z')||c==u'_';
}
std::u16string encode_diagram_source(std::u16string_view value) {
    // Diagram.fromText protects escapes before parsing. Frontmatter metadata
    // is supplied separately and must not be interpreted as encoded source.
    std::u16string out;
    for(std::size_t i=0;i<value.size();) {
        auto end=i+1;
        if(value[i]==u'#') {
            while(end<value.size() && entity_word(value[end])) ++end;
            if(end>i+1 && end<value.size() && value[end]==u';') {
                auto name=value.substr(i+1,end-i-1);
                const bool numeric=std::ranges::all_of(name,[](char16_t c){return c>=u'0'&&c<=u'9';});
                out+=numeric?u"\ufb02\u00b0\u00b0":u"\ufb02\u00b0";
                out+=name; out+=u"\u00b6\u00df"; i=end+1; continue;
            }
        }
        out+=value[i++];
    }
    return out+u"\n";
}

std::expected<PreparedDiagram, std::string> prepare_diagram(
    std::u16string_view source, const ConfigSession& session, FrontendLimits limits) {
    if (source.size() > limits.source_units) return std::unexpected("Frontend source budget exceeded");
    try {
        DiagramRegistry registry{{.source_units = limits.source_units, .config = limits.config}};
        if (auto registered = registry.add_builtin_diagrams(); !registered)
            return std::unexpected(registered.error());
        const auto normalized = cleanup_text(source);
        auto front = extract_front_matter(normalized, limits.yaml);
        if (!front) return std::unexpected(front.error());
        auto front_config = front->metadata.config
            ? std::move(*front->metadata.config) : config_detail::map();
        if (front->metadata.display_mode && !front->metadata.display_mode->empty()) {
            if (front_config.kind != ConfigKind::mapping)
                return std::unexpected("Frontmatter displayMode requires a configuration mapping");
            auto& gantt = front_config.mapping[u"gantt"];
            if (!config_truthy(gantt)) gantt = config_detail::map();
            if (gantt.kind != ConfigKind::mapping)
                return std::unexpected("Frontmatter displayMode requires a gantt mapping");
            gantt.mapping[u"displayMode"] = config_detail::string(*front->metadata.display_mode);
        }
        DirectiveScanState scan;
        auto init = detect_init_config(front->text, [&](std::u16string_view text)
            -> std::expected<std::u16string, std::string> {
            // preprocess.ts calls detectInit without a configuration argument.
            const auto detected = registry.detect(text);
            if (!detected) return std::unexpected("Cannot detect diagram type for init directive");
            return detected->id;
        }, limits.config, limits.directives, &scan);
        if (!init) return std::unexpected(init.error());
        auto directive = *init ? std::move(**init) : config_detail::map();
        auto wraps = detect_directives(front->text, DirectiveFilter::wrap, scan, limits.directives);
        if (!wraps) return std::unexpected(wraps.error());
        const auto is_wrap = [](const Directive& item) { return item.type && *item.type == u"wrap"; };
        if (wraps->size() > 1 || is_wrap(wraps->front())) {
            if (directive.kind != ConfigKind::mapping)
                return std::unexpected("Wrap directive requires a configuration mapping");
            directive.mapping[u"wrap"] = ConfigValue{.kind = ConfigKind::boolean,
                .boolean = std::ranges::any_of(*wraps, is_wrap)};
        }
        auto config = clean_and_merge_config(front_config, &directive, limits.config);
        if (!config) return std::unexpected(config.error());
        PreparedDiagram result;
        result.code.raw = source;
        result.code.with_comments = remove_directives(front->text);
        result.code.cleaned = cleanup_comments(result.code.with_comments);
        const auto removed = normalized.size() - front->text.size();
        result.code.frontmatter_line_offset = static_cast<std::size_t>(std::ranges::count(
            std::u16string_view{normalized}.substr(0, removed), u'\n'));
        result.title = std::move(front->metadata.title);
        result.config = std::move(*config);

        auto working = session;
        if (auto reset = working.reset(); !reset) return std::unexpected(reset.error());
        auto current = working.current_config();
        if (!current) return std::unexpected(current.error());
        const auto scope = registry.detect(result.code.cleaned, &*current);
        // The original catches scope detection failure, applies directives, then
        // lets the final detection report the error. Scope detector writes are
        // discarded because getConfig returned a copy.
        if (auto scoped = working.set_diagram_scope(scope
            ? std::optional<std::u16string_view>{scope->id} : std::nullopt); !scoped)
            return std::unexpected(scoped.error());
        if (auto applied = working.add_directive(result.config); !applied)
            return std::unexpected(applied.error());
        current = working.current_config();
        if (!current) return std::unexpected(current.error());
        auto detected = registry.detect(result.code.cleaned, &*current);
        if (!detected) return std::unexpected("No diagram type detected for prepared source");
        result.type = std::move(detected->id);
        result.effective_config = std::move(detected->config);
        return result;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
