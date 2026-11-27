#include "pch.h"
#include "settings/views/ProviderIdentity.h"
#include "settings/views/SettingsViewSupport.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Security.Cryptography.h>

namespace winrt::Folia::settings_ui {
    using namespace Microsoft::UI::Xaml::Controls;
    using models::ModelProfile;
    using namespace Windows::Storage::Streams;
    namespace image = Windows::Graphics::Imaging;
    using Windows::Security::Cryptography::CryptographicBuffer;
    constexpr std::uint32_t iconSize = 96;
    constexpr std::size_t maxEncodedIconBytes = 64 * 1024;

    Windows::Foundation::IAsyncOperation<hstring> ImportProviderIcon(Windows::Storage::StorageFile file) {
        auto input = co_await file.OpenReadAsync();
        if (input.Size() > 4 * 1024 * 1024) throw hresult_invalid_argument();
        auto decoder = co_await image::BitmapDecoder::CreateAsync(image::BitmapDecoder::PngDecoderId(), input);
        auto width = decoder.PixelWidth();
        auto height = decoder.PixelHeight();
        auto longest = (std::max)(width, height);
        if (!width || !height || longest > 4096) throw hresult_invalid_argument();
        image::BitmapTransform transform;
        transform.ScaledWidth((std::max)(1u, width * iconSize / longest));
        transform.ScaledHeight((std::max)(1u, height * iconSize / longest));
        transform.InterpolationMode(image::BitmapInterpolationMode::Fant);
        auto pixels = co_await decoder.GetPixelDataAsync(image::BitmapPixelFormat::Bgra8, image::BitmapAlphaMode::Straight,
            transform, image::ExifOrientationMode::IgnoreExifOrientation, image::ColorManagementMode::ColorManageToSRgb);
        InMemoryRandomAccessStream output;
        auto encoder = co_await image::BitmapEncoder::CreateAsync(image::BitmapEncoder::PngEncoderId(), output);
        auto bytes = pixels.DetachPixelData();
        encoder.SetPixelData(image::BitmapPixelFormat::Bgra8, image::BitmapAlphaMode::Straight,
            transform.ScaledWidth(), transform.ScaledHeight(), 96, 96, bytes);
        co_await encoder.FlushAsync();
        if (output.Size() > maxEncodedIconBytes) throw hresult_invalid_argument();
        auto size = static_cast<std::uint32_t>(output.Size());
        output.Seek(0);
        auto encoded = co_await output.ReadAsync(Buffer(size), size, InputStreamOptions::None);
        co_return CryptographicBuffer::EncodeToBase64String(encoded);
    }

    static fire_and_forget LoadProviderIcon(Border mark, std::string png) {
        if (png.size() > (maxEncodedIconBytes + 2) / 3 * 4) co_return;
        try {
            InMemoryRandomAccessStream stream;
            co_await stream.WriteAsync(CryptographicBuffer::DecodeFromBase64String(to_hstring(png)));
            stream.Seek(0);
            Microsoft::UI::Xaml::Media::Imaging::BitmapImage source;
            source.DecodePixelWidth(iconSize);
            source.DecodePixelHeight(iconSize);
            co_await source.SetSourceAsync(stream);
            Image icon;
            icon.Source(source);
            icon.Stretch(Microsoft::UI::Xaml::Media::Stretch::Uniform);
            icon.Style(ResourceStyle(L"SettingsProviderImageStyle"));
            mark.Child(icon);
        } catch (hresult_error const&) {}
    }

    static hstring ProviderInitials(std::string const& name) {
        auto text = to_hstring(name);
        std::wstring initials;
        std::size_t count = 0;
        bool atWordStart = true;
        for (std::size_t index = 0; index < text.size() && count < 2;) {
            auto character = text.data()[index];
            WORD type{};
            bool space = GetStringTypeW(CT_CTYPE1, &character, 1, &type) && (type & C1_SPACE);
            auto length = IS_HIGH_SURROGATE(character) && index + 1 < text.size() ? 2u : 1u;
            if (!space && atWordStart) {
                initials.append(text.data() + index, length);
                ++count;
            }
            atWordStart = space;
            index += length;
        }
        if (!initials.empty()) CharUpperBuffW(initials.data(), static_cast<DWORD>(initials.size()));
        return hstring(initials);
    }

    hstring ProviderHost(ModelProfile const& profile) {
        try { return Windows::Foundation::Uri(to_hstring(profile.baseUrl)).Host(); }
        catch (hresult_error const&) { return {}; }
    }

    Border ProviderMark(ModelProfile const& profile) {
        Border mark;
        mark.Style(ResourceStyle(L"SettingsProviderMarkStyle"));
        auto letters = ProviderInitials(profile.name);
        if (letters.empty()) {
            auto glyph = MakeWorkspaceIcon(WorkspaceIcon::ProviderService, L"TextFillColorSecondaryBrush");
            glyph.Style(ResourceStyle(L"SettingsProviderActionIconStyle"));
            mark.Child(glyph);
        } else {
            auto initials = Text(letters);
            initials.Style(ResourceStyle(L"SettingsProviderInitialsStyle"));
            mark.Child(initials);
        }
        if (!profile.iconPng.empty()) LoadProviderIcon(mark, profile.iconPng);
        return mark;
    }
}
