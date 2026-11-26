#include "pch.h"

import folia.platform.editor_inline_layout;
#include "editor/rendering/painting/EditorSyntaxIcons.h"
#include "theme/ThemeConfig.h"

namespace winrt::Folia
{
    namespace
    {
        ::Microsoft::WRL::ComPtr<ID2D1SvgDocument> LoadIcon(ID2D1DeviceContext5* context, std::string const& source)
        {
            ::Microsoft::WRL::ComPtr<IStream> stream;
            winrt::check_hresult(CreateStreamOnHGlobal(nullptr, TRUE, stream.GetAddressOf()));
            winrt::check_hresult(stream->Write(source.data(), static_cast<ULONG>(source.size()), nullptr));
            winrt::check_hresult(stream->Seek({}, STREAM_SEEK_SET, nullptr));
            ::Microsoft::WRL::ComPtr<ID2D1SvgDocument> document;
            winrt::check_hresult(context->CreateSvgDocument(stream.Get(), {24, 24}, document.GetAddressOf()));
            return document;
        }

        void PaintIcon(ID2D1DeviceContext5& context, ID2D1SvgDocument& document,
            D2D1_RECT_F bounds, D2D1_COLOR_F color)
        {
            ::Microsoft::WRL::ComPtr<ID2D1SvgElement> root;
            document.GetRoot(root.GetAddressOf());
            winrt::check_hresult(root->SetAttributeValue(L"color", color));
            D2D1_MATRIX_3X2_F previous;
            context.GetTransform(&previous);
            context.SetTransform(D2D1::Matrix3x2F::Scale((bounds.right - bounds.left) / 24, (bounds.bottom - bounds.top) / 24)
                * D2D1::Matrix3x2F::Translation(bounds.left, bounds.top) * previous);
            context.DrawSvgDocument(&document);
            context.SetTransform(previous);
        }
    }

    void ApplySyntaxIconObject(IDWriteTextLayout* layout, UINT32 start, UINT32 length, float size, float gap)
    {
        if (!layout || length == 0) return;
        auto object = folia::platform::editor::CreateInlinePlaceholder(
            {size + 2 * gap, size, size * .8f, FALSE}, DWRITE_BREAK_CONDITION_CAN_BREAK, DWRITE_BREAK_CONDITION_CAN_BREAK);
        winrt::check_hresult(layout->SetInlineObject(object.Get(), {start, length}));
    }

    void EditorSyntaxIcons::Reset()
    {
        for (auto& document : documents) document.Reset();
        context.Reset();
        theme.reset();
    }

    void EditorSyntaxIcons::Draw(ID2D1DeviceContext* target, SyntaxIcon icon, D2D1_RECT_F bounds, D2D1_COLOR_F color,
        std::shared_ptr<const folia::SyntaxMarkerTheme> const& assets)
    {
        if (!target || !assets || icon == SyntaxIcon::None || icon >= SyntaxIcon::Count) return;
        ::Microsoft::WRL::ComPtr<ID2D1DeviceContext5> current;
        winrt::check_hresult(target->QueryInterface(IID_PPV_ARGS(current.GetAddressOf())));
        if (context != current || theme != assets) { Reset(); context = current; theme = assets; }
        auto& document = documents[static_cast<std::size_t>(icon)];
        if (!document) document = LoadIcon(context.Get(), theme->svg[static_cast<std::size_t>(icon)]);
        PaintIcon(*context.Get(), *document.Get(), bounds, color);
    }

    void EditorChromeIcons::Reset()
    {
        documents.clear();
        context.Reset();
    }

    void EditorChromeIcons::Draw(ID2D1DeviceContext* target, std::wstring_view key, D2D1_RECT_F bounds, D2D1_COLOR_F color)
    {
        if (!context) winrt::check_hresult(target->QueryInterface(IID_PPV_ARGS(context.GetAddressOf())));
        auto found = documents.find(key);
        if (found == documents.end()) {
            auto glyph = FindThemeGlyph(key);
            if (!glyph) return;
            found = documents.emplace(key, LoadIcon(context.Get(), glyph->svg)).first;
        }
        PaintIcon(*context.Get(), *found->second.Get(), bounds, color);
    }
}
