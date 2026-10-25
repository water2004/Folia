#include "pch.h"
#include "editor/rendering/EditorSyntaxIcons.h"

namespace winrt::Folia
{
    namespace
    {
        class SyntaxIconObject final : public ::Microsoft::WRL::RuntimeClass<
            ::Microsoft::WRL::RuntimeClassFlags<::Microsoft::WRL::ClassicCom>, IDWriteInlineObject>
        {
        public:
            SyntaxIconObject(float advance, float size) : advance(advance), size(size) {}
            IFACEMETHODIMP Draw(void*, IDWriteTextRenderer*, FLOAT, FLOAT, BOOL, BOOL, IUnknown*) override
            { return S_OK; } // The painter draws this indivisible SVG in the reserved bounds.
            IFACEMETHODIMP GetMetrics(DWRITE_INLINE_OBJECT_METRICS* metrics) override
            {
                if (!metrics) return E_POINTER;
                *metrics = {advance, size, size * .8f, FALSE};
                return S_OK;
            }
            IFACEMETHODIMP GetOverhangMetrics(DWRITE_OVERHANG_METRICS* metrics) override
            {
                if (!metrics) return E_POINTER;
                *metrics = {};
                return S_OK;
            }
            IFACEMETHODIMP GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after) override
            {
                if (!before || !after) return E_POINTER;
                *before = *after = DWRITE_BREAK_CONDITION_CAN_BREAK;
                return S_OK;
            }
        private:
            float advance, size;
        };

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
    }

    void ApplySyntaxIconObject(IDWriteTextLayout* layout, UINT32 start, UINT32 length, float size, float gap)
    {
        if (!layout || length == 0) return;
        auto object = ::Microsoft::WRL::Make<SyntaxIconObject>(size + 2 * gap, size);
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
        ::Microsoft::WRL::ComPtr<ID2D1SvgElement> root;
        document->GetRoot(root.GetAddressOf());
        winrt::check_hresult(root->SetAttributeValue(L"color", color));
        D2D1_MATRIX_3X2_F previous;
        context->GetTransform(&previous);
        context->SetTransform(D2D1::Matrix3x2F::Scale((bounds.right - bounds.left) / 24, (bounds.bottom - bounds.top) / 24)
            * D2D1::Matrix3x2F::Translation(bounds.left, bounds.top) * previous);
        context->DrawSvgDocument(document.Get());
        context->SetTransform(previous);
    }
}
