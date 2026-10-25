#include "pch.h"
#include "storage/ClipboardImageStore.h"
#include <winrt/Windows.Graphics.Imaging.h>

namespace winrt::Folia {
    namespace {
        winrt::hstring ImageName(winrt::hstring const& extension) {
            GUID id{};
            winrt::check_hresult(CoCreateGuid(&id));
            wchar_t name[40]{};
            if (!StringFromGUID2(id, name, static_cast<int>(std::size(name)))) throw std::runtime_error("Cannot name clipboard image");
            return winrt::hstring(L"image-" + std::wstring(name + 1, 36) + std::wstring(extension));
        }
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile> ClipboardImageStore::CopyFileAsync(
        winrt::Windows::Storage::StorageFile input, winrt::hstring directory) {
        co_await winrt::resume_background();
        auto folder = co_await winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory);
        co_return co_await input.CopyAsync(folder, ImageName(input.FileType()), winrt::Windows::Storage::NameCollisionOption::FailIfExists);
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile> ClipboardImageStore::SavePngAsync(
        winrt::Windows::Storage::Streams::IRandomAccessStream input, winrt::hstring directory) {
        co_await winrt::resume_background();
        using namespace winrt::Windows::Graphics::Imaging;
        using namespace winrt::Windows::Storage;
        StorageFile file{nullptr};
        std::exception_ptr failure;
        try {
            auto decoder = co_await BitmapDecoder::CreateAsync(input);
            auto bitmap = co_await decoder.GetSoftwareBitmapAsync(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied);
            auto folder = co_await StorageFolder::GetFolderFromPathAsync(directory);
            file = co_await folder.CreateFileAsync(ImageName(L".png"), CreationCollisionOption::FailIfExists);
            auto output = co_await file.OpenAsync(FileAccessMode::ReadWrite);
            auto encoder = co_await BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), output);
            encoder.SetSoftwareBitmap(bitmap);
            co_await encoder.FlushAsync();
            co_await output.FlushAsync();
            output.Close();
        } catch (...) { failure = std::current_exception(); }
        if (failure) {
            if (file) {
                try { co_await file.DeleteAsync(StorageDeleteOption::PermanentDelete); } catch (...) {}
            }
            std::rethrow_exception(failure);
        }
        co_return file;
    }
}
