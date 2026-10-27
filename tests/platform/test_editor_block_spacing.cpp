#include <dwrite.h>
#include <wrl/client.h>
#include "support/folia_test.hpp"
import folia.platform.editor_block_spacing;
import folia.markdown.utf;

using namespace boost::ut;
using namespace folia::platform::editor;
using Microsoft::WRL::ComPtr;

suite editor_block_spacing_tests = [] {

"nested block background ends before the following text with unicode and wrapping"_test = [] {
    ComPtr<IDWriteFactory> factory;
    expect(fatal(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf())))));
    ComPtr<IDWriteTextFormat> format;
    expect(fatal(SUCCEEDED(factory->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-US", format.GetAddressOf()))));
    for (auto const& prefix : std::vector<std::u32string>{U"before", U"😀before"}) {
        for (auto const width : {70.0f, 500.0f}) {
            const auto text = prefix + U"\n\uFFFC\ncode content that may wrap\n\uFFFC\nafter";
            const auto first = text.find(U'\uFFFC');
            const auto last = text.rfind(U'\uFFFC');
            const auto start = static_cast<UINT32>(folia::char_index_to_utf16(text, first));
            const auto end = static_cast<UINT32>(folia::char_index_to_utf16(text, last + 2));
            const std::vector<EditorBlockSpacing> spaces{
                {start, 2, 8.0f},
                {static_cast<UINT32>(folia::char_index_to_utf16(text, last)), 1, 8.0f}};
            const auto utf8 = folia::cps_to_utf8(text);
            const auto wideLength = MultiByteToWideChar(
                CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            std::wstring wide(wideLength, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                static_cast<int>(utf8.size()), wide.data(), wideLength);
            ComPtr<IDWriteTextLayout> layout;
            expect(fatal(SUCCEEDED(factory->CreateTextLayout(wide.c_str(),
                static_cast<UINT32>(wide.size()), format.Get(), width, 10000,
                layout.GetAddressOf()))));
            ApplyEditorBlockSpacing(layout.Get(), text, spaces);
            FLOAT x = 0, afterY = 0;
            DWRITE_HIT_TEST_METRICS after{};
            expect(fatal(SUCCEEDED(layout->HitTestTextPosition(end, FALSE, &x, &afterY, &after))));
            UINT32 count = 0;
            layout->HitTestTextRange(start, end-start, 0, 0, nullptr, 0, &count);
            std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
            expect(fatal(SUCCEEDED(layout->HitTestTextRange(start, end-start,
                0, 0, metrics.data(), count, &count))));
            for (auto const& metric : metrics)
                expect(metric.top + metric.height <= after.top + 0.01f);
            for (auto const& space : spaces) {
                DWRITE_HIT_TEST_METRICS hit{};
                FLOAT y = 0;
                layout->HitTestTextPosition(space.displayStart, FALSE, &x, &y, &hit);
                expect(hit.height >= space.height);
                expect(hit.height < space.height + 2.0f);
            }
        }
    }
};

};
