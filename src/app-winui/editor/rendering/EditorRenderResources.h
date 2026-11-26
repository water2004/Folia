#pragma once

#include "editor/rendering/EditorStyleSheet.h"
#include "editor/rendering/painting/EditorSyntaxIcons.h"
#include "editor/rendering/painting/EditorPopupShadow.h"

namespace winrt::Folia
{
    struct EditorRenderResources
    {
        void Initialize(winrt::Microsoft::UI::Xaml::FrameworkElement const& host, EditorStyleSheet const& styleSheet);
        void InitializeOffscreen(EditorStyleSheet const& styleSheet, float widthDip, float heightDip);
        // Background measurement uses the production fonts and DirectWrite
        // without creating a D3D device, swap chain, brushes or paint target.
        void InitializeTextMeasurement(EditorStyleSheet const& styleSheet, float widthDip, float heightDip);
        bool Resize(winrt::Microsoft::UI::Xaml::FrameworkElement const& host, double width, double height);
        void RebuildTextFormats(EditorStyleSheet const& styleSheet);
        void ResetBrushes();
        void ResetTargets();
        void EnsureFrameResources(EditorStyleSheet const& styleSheet);
        HRESULT Present();
        bool Ready() const;

        ::Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
        ::Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext;
        winrt::Microsoft::UI::Composition::CompositionDrawingSurface drawingSurface{nullptr};
        ::Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTexture;
        ::Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory;
        ::Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice;
        ::Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext;
        ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2dTarget;
        ::Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory;
        ::Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> heading1Format;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> heading2Format;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> heading3Format;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> codeFormat;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> lineNumberFormat;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> mutedBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> lineNumberBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> lineNumberBackgroundBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> accentBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> previewBackgroundBrush, previewBorderBrush;
        EditorPopupShadow popupShadow;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> mathBracketBrush;
        EditorSyntaxIcons syntaxIcons;
        EditorChromeIcons chromeIcons;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> codeBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> mathDecorationBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> inlineCodeBrush, inlineCodeBackgroundBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> panelBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> tableBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> canvasBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> nestedQuoteBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> calloutNoteBackgroundBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> calloutNoteBorderBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> calloutTipBackgroundBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> calloutTipBorderBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> calloutWarningBackgroundBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> calloutWarningBorderBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> selectionBrush;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> caretBrush;
        std::array<::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, 11> syntaxBrushes;
        uint32_t surfaceWidth = 0;
        uint32_t surfaceHeight = 0;
        float surfaceWidthDip = 1.0f;
        std::size_t tableTextLayoutsBuilt = 0;
        float surfaceHeightDip = 1.0f;
        float surfaceScaleX = 1.0f;
        float surfaceScaleY = 1.0f;

    private:
        void InitializeDeviceResources(EditorStyleSheet const& styleSheet);
    };
}
