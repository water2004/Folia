#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "native-renderer.h"
import std;
import folia.platform.mermaid_renderer;

namespace {
std::expected<folia::mermaid::DiagramSvg, std::string> render(
    const std::uint8_t* source, std::size_t length, bool dark) {
    using namespace folia::mermaid;
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return std::unexpected("Mermaid source is too large");
    const auto bytes = reinterpret_cast<const char*>(source);
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        bytes, static_cast<int>(length), nullptr, 0);
    if (!count) return std::unexpected("Mermaid source must be nonempty UTF-8");
    std::wstring wide(count, L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes,
        static_cast<int>(length), wide.data(), count) != count)
        return std::unexpected("Mermaid UTF-8 conversion failed");
    const std::u16string text(wide.begin(), wide.end());
    auto session = ConfigSession::create();
    if (!session) return std::unexpected(session.error());
    ConfigValue site{.kind=ConfigKind::mapping};
    site.mapping.emplace(u"theme", ConfigValue{.kind=ConfigKind::string,.string=dark?u"dark":u"default"});
    site.mapping.emplace(u"look", ConfigValue{.kind=ConfigKind::string,.string=u"classic"});
    // Keep family layout defaults. In particular Class must not silently use
    // Dagre while its required default ELK route remains under construction.
    auto saved = session->save_initialize_config(site);
    if (!saved) return std::unexpected(saved.error());
    auto configured = session->set_site_config(site);
    if (!configured) return std::unexpected(configured.error());
    return folia::platform::render_mermaid(text, *session);
}
void copy(std::string_view text, std::uint8_t** output, std::size_t* length) {
    auto buffer = std::make_unique<std::uint8_t[]>(text.size());
    std::memcpy(buffer.get(), text.data(), text.size());
    *length = text.size();
    *output = buffer.release();
}
}

std::int32_t FoliaNativeMermaidRender(const std::uint8_t* source,
    std::size_t length, bool dark, std::uint8_t** output,
    std::size_t* output_length, float* width, float* height) noexcept {
    if (!source || !output || !output_length || !width || !height) return -1;
    *output = nullptr; *output_length = 0; *width = 0; *height = 0;
    try {
        auto result = render(source, length, dark);
        if (!result) { copy(result.error(), output, output_length); return 1; }
        copy(result->svg, output, output_length);
        *width = static_cast<float>(result->width);
        *height = static_cast<float>(result->height);
        return 0;
    } catch (...) { return 2; }
}

void FoliaNativeMermaidFree(std::uint8_t* output) noexcept { delete[] output; }
