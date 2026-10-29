module;
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

export module folia.platform.mermaid_host;
import std;
export import folia.platform.mermaid_renderer;

export namespace folia::platform {
// The application supplies UTF-8 source and a light/dark choice. Initialize
// a fresh source configuration per request; preserve family layout defaults.
std::expected<folia::mermaid::DiagramSvg, std::string> render_mermaid_utf8(
    std::string_view source, bool dark) {
    using namespace folia::mermaid;
    if (source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return std::unexpected("Mermaid source is too large");
    const auto length = static_cast<int>(source.size());
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        source.data(), length, nullptr, 0);
    if (!count) return std::unexpected("Mermaid source must be nonempty UTF-8");
    std::wstring wide(count, L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        source.data(), length, wide.data(), count) != count)
        return std::unexpected("Mermaid UTF-8 conversion failed");
    const std::u16string text(wide.begin(), wide.end());
    auto session = ConfigSession::create();
    if (!session) return std::unexpected(session.error());
    ConfigValue site{.kind=ConfigKind::mapping};
    site.mapping.emplace(u"theme", ConfigValue{.kind=ConfigKind::string,.string=dark?u"dark":u"default"});
    site.mapping.emplace(u"look", ConfigValue{.kind=ConfigKind::string,.string=u"classic"});
    // Class/Flowchart must not silently use Dagre while their required default
    // ELK route is under construction.
    auto saved = session->save_initialize_config(site);
    if (!saved) return std::unexpected(saved.error());
    auto configured = session->set_site_config(site);
    if (!configured) return std::unexpected(configured.error());
    return render_mermaid(text, *session);
}
}
