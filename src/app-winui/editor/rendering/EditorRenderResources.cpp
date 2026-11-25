#include "pch.h"
#include "editor/rendering/EditorRenderResources.h"
#include <winrt/Microsoft.Graphics.DirectX.h>
#include <winrt/Microsoft.UI.Composition.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

namespace winrt::Folia
{
    namespace
    {
        float RasterizationScale(winrt::Microsoft::UI::Xaml::FrameworkElement const& host)
        {
            auto root = host.XamlRoot();
            return root ? static_cast<float>(root.RasterizationScale()) : 1.f;
        }
    }

    bool EditorRenderResources::Ready() const
    {
        return d3dDevice && d3dContext && d2dDevice && d2dContext
            && dwriteFactory && wicFactory;
    }

    void EditorRenderResources::ResetBrushes()
    {
        textBrush = nullptr;
        mutedBrush = nullptr;
        lineNumberBrush = nullptr;
        lineNumberBackgroundBrush = nullptr;
        accentBrush = nullptr;
        previewBackgroundBrush = nullptr;
        previewBorderBrush = nullptr;
        popupShadow.Reset();
        mathBracketBrush.Reset();
        syntaxIcons.Reset();
        codeBrush = nullptr;
        mathDecorationBrush = nullptr;
        inlineCodeBrush = nullptr;
        inlineCodeBackgroundBrush = nullptr;
        panelBrush = nullptr;
        canvasBrush = nullptr;
        nestedQuoteBrush = nullptr;
        calloutNoteBackgroundBrush = nullptr;
        calloutNoteBorderBrush = nullptr;
        calloutTipBackgroundBrush = nullptr;
        calloutTipBorderBrush = nullptr;
        calloutWarningBackgroundBrush = nullptr;
        calloutWarningBorderBrush = nullptr;
        selectionBrush = nullptr;
        caretBrush = nullptr;
        for (auto& brush : syntaxBrushes) brush = nullptr;
    }

    void EditorRenderResources::ResetTargets()
    {
        if (d2dContext)
        {
            d2dContext->SetTarget(nullptr);
            d2dContext->Flush();
        }
        d2dTarget = nullptr;
        renderTexture = nullptr;
        ResetBrushes();
    }

    void EditorRenderResources::RebuildTextFormats(EditorStyleSheet const& styleSheet)
    {
        if (!dwriteFactory) return;
        auto createFormat = [&](EditorFontStyle const& font, ::Microsoft::WRL::ComPtr<IDWriteTextFormat>& target)
        {
            target = nullptr;
            winrt::check_hresult(dwriteFactory->CreateTextFormat(
                font.family.c_str(), nullptr, font.weight, font.style, DWRITE_FONT_STRETCH_NORMAL,
                font.size, L"en-us", target.GetAddressOf()));
            winrt::check_hresult(target->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
            winrt::check_hresult(target->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, font.lineHeight, font.size * 1.2f));
        };
        createFormat(styleSheet.body, textFormat);
        createFormat(styleSheet.heading1, heading1Format);
        createFormat(styleSheet.heading2, heading2Format);
        createFormat(styleSheet.heading3, heading3Format);
        createFormat(styleSheet.code, codeFormat);
        createFormat(styleSheet.code, lineNumberFormat);
        winrt::check_hresult(lineNumberFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
        winrt::check_hresult(lineNumberFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    }

    void EditorRenderResources::InitializeDeviceResources(EditorStyleSheet const& styleSheet)
    {
        if (d3dDevice) return;
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        };
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        winrt::check_hresult(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels,
            static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0])), D3D11_SDK_VERSION,
            d3dDevice.GetAddressOf(), nullptr, d3dContext.GetAddressOf()));

        ::Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        winrt::check_hresult(d3dDevice.As(&dxgiDevice));
        D2D1_FACTORY_OPTIONS d2dOptions{};
#if defined(_DEBUG)
        d2dOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dOptions, d2dFactory.GetAddressOf()));
        winrt::check_hresult(d2dFactory->CreateDevice(dxgiDevice.Get(), d2dDevice.GetAddressOf()));
        winrt::check_hresult(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dContext.GetAddressOf()));
        winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf())));
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wicFactory.GetAddressOf()))))
            CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wicFactory.GetAddressOf()));
        RebuildTextFormats(styleSheet);
    }

    void EditorRenderResources::Initialize(
        winrt::Microsoft::UI::Xaml::FrameworkElement const& host,
        EditorStyleSheet const& styleSheet)
    {
        if (drawingSurface) return;
        InitializeDeviceResources(styleSheet);

        surfaceScaleX = surfaceScaleY = RasterizationScale(host);
        surfaceWidthDip = static_cast<float>((std::max)(1.0, host.ActualWidth()));
        surfaceHeightDip = static_cast<float>((std::max)(1.0, host.ActualHeight()));
        surfaceWidth = static_cast<uint32_t>(std::ceil(surfaceWidthDip * surfaceScaleX));
        surfaceHeight = static_cast<uint32_t>(std::ceil(surfaceHeightDip * surfaceScaleY));

        // A composition drawing surface blends with the XAML paper underneath.
        // DXGI swap-chain surfaces are presented as external content in WinUI.
        using winrt::Microsoft::UI::Xaml::Hosting::ElementCompositionPreview;
        using namespace winrt::Microsoft::UI::Composition;
        using namespace winrt::Microsoft::Graphics::DirectX;
        auto compositor = ElementCompositionPreview::GetElementVisual(host).Compositor();
        CompositionGraphicsDevice graphicsDevice{nullptr};
        winrt::check_hresult(compositor.as<ICompositorInterop>()->CreateGraphicsDevice(d2dDevice.Get(), &graphicsDevice));
        drawingSurface = graphicsDevice.CreateDrawingSurface(
            {static_cast<float>(surfaceWidth), static_cast<float>(surfaceHeight)},
            DirectXPixelFormat::B8G8R8A8UIntNormalized, DirectXAlphaMode::Premultiplied);
        auto brush = compositor.CreateSurfaceBrush(drawingSurface);
        brush.Stretch(CompositionStretch::Fill);
        auto visual = compositor.CreateSpriteVisual();
        visual.RelativeSizeAdjustment({1.f, 1.f});
        visual.Brush(brush);
        ElementCompositionPreview::SetElementChildVisual(host, visual);
    }

    void EditorRenderResources::InitializeOffscreen(
        EditorStyleSheet const& styleSheet,
        float widthDip,
        float heightDip)
    {
        InitializeDeviceResources(styleSheet);
        surfaceScaleX = 1.0f;
        surfaceScaleY = 1.0f;
        surfaceWidthDip = (std::max)(1.0f, widthDip);
        surfaceHeightDip = (std::max)(1.0f, heightDip);
        surfaceWidth = static_cast<std::uint32_t>(std::ceil(surfaceWidthDip));
        surfaceHeight = static_cast<std::uint32_t>(std::ceil(surfaceHeightDip));
    }

    void EditorRenderResources::InitializeTextMeasurement(
        EditorStyleSheet const& styleSheet,
        float widthDip,
        float heightDip)
    {
        winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf())));
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(wicFactory.GetAddressOf()))))
            CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(wicFactory.GetAddressOf()));
        surfaceWidthDip = (std::max)(1.0f, widthDip);
        surfaceHeightDip = (std::max)(1.0f, heightDip);
        RebuildTextFormats(styleSheet);
    }

    EditorResizeResult EditorRenderResources::Resize(winrt::Microsoft::UI::Xaml::FrameworkElement const& host, double width, double height)
    {
        if (!drawingSurface || !std::isfinite(width) || !std::isfinite(height) || width <= 0.0 || height <= 0.0) return {};
        auto scale = RasterizationScale(host);
        auto newWidthDip = static_cast<float>((std::max)(1.0, width));
        auto newHeightDip = static_cast<float>((std::max)(1.0, height));
        auto newWidth = (std::max)(uint32_t{ 1 }, static_cast<uint32_t>(std::ceil(width * scale)));
        auto newHeight = (std::max)(uint32_t{ 1 }, static_cast<uint32_t>(std::ceil(height * scale)));
        if (newWidth == surfaceWidth && newHeight == surfaceHeight && newWidthDip == surfaceWidthDip
            && newHeightDip == surfaceHeightDip && scale == surfaceScaleX) return {};
        EditorResizeResult result{ true, newWidthDip != surfaceWidthDip };
        if (newWidth != surfaceWidth || newHeight != surfaceHeight)
            drawingSurface.Resize({static_cast<int32_t>(newWidth), static_cast<int32_t>(newHeight)});
        ResetTargets();
        surfaceWidth = newWidth;
        surfaceHeight = newHeight;
        surfaceWidthDip = newWidthDip;
        surfaceHeightDip = newHeightDip;
        surfaceScaleX = surfaceScaleY = scale;
        return result;
    }

    void EditorRenderResources::EnsureFrameResources(EditorStyleSheet const& styleSheet)
    {
        if (drawingSurface && !d2dTarget)
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = surfaceWidth;
            desc.Height = surfaceHeight;
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            winrt::check_hresult(d3dDevice->CreateTexture2D(&desc, nullptr, renderTexture.ReleaseAndGetAddressOf()));
            ::Microsoft::WRL::ComPtr<IDXGISurface> surface;
            winrt::check_hresult(renderTexture.As(&surface));
            D2D1_BITMAP_PROPERTIES1 properties{};
            properties.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
            properties.dpiX = 96.0f * surfaceScaleX;
            properties.dpiY = 96.0f * surfaceScaleY;
            properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::check_hresult(d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &properties, d2dTarget.GetAddressOf()));
            d2dContext->SetTarget(d2dTarget.Get());
            d2dContext->SetDpi(properties.dpiX, properties.dpiY);
        }
        if (!textBrush)
        {
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.textColor, textBrush.GetAddressOf()));
            auto background = styleSheet.previewBackgroundColor; background.a = 1;
            auto border = styleSheet.previewBorderColor;
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(background, previewBackgroundBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(border, previewBorderBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.accentColor, mathBracketBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.mutedColor, mutedBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.lineNumberColor, lineNumberBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.lineNumberBackgroundColor, lineNumberBackgroundBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.accentColor, accentBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.codeTextColor, codeBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.textColor, mathDecorationBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.inlineCodeTextColor, inlineCodeBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.inlineCodeBackgroundColor, inlineCodeBackgroundBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.panelColor, panelBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.canvasColor, canvasBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.nestedQuoteColor, nestedQuoteBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.calloutNoteBackgroundColor, calloutNoteBackgroundBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.calloutNoteBorderColor, calloutNoteBorderBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.calloutTipBackgroundColor, calloutTipBackgroundBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.calloutTipBorderColor, calloutTipBorderBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.calloutWarningBackgroundColor, calloutWarningBackgroundBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.calloutWarningBorderColor, calloutWarningBorderBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.selectionColor, selectionBrush.GetAddressOf()));
            winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.caretColor, caretBrush.GetAddressOf()));
            for (std::size_t index = 0; index < syntaxBrushes.size(); ++index)
                winrt::check_hresult(d2dContext->CreateSolidColorBrush(styleSheet.syntaxColors[index], syntaxBrushes[index].GetAddressOf()));
        }
    }

    HRESULT EditorRenderResources::Present()
    {
        if (!drawingSurface || !renderTexture) return S_FALSE;
        auto interop = drawingSurface.as<winrt::Microsoft::UI::Composition::ICompositionDrawingSurfaceInterop>();
        ::Microsoft::WRL::ComPtr<IDXGISurface> destination;
        POINT offset{};
        auto result = interop->BeginDraw(nullptr, IID_PPV_ARGS(destination.GetAddressOf()), &offset);
        if (FAILED(result)) return result;

        // The compositor may allocate this surface inside a larger texture.
        // Copy within its update rectangle without changing the editor's DIPs.
        ::Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        result = destination.As(&texture);
        if (SUCCEEDED(result)) {
            D3D11_BOX region{0, 0, 0, surfaceWidth, surfaceHeight, 1};
            d3dContext->CopySubresourceRegion(texture.Get(), 0, offset.x, offset.y, 0, renderTexture.Get(), 0, &region);
        }
        auto end = interop->EndDraw();
        return FAILED(result) ? result : end;
    }
}
