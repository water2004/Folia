#pragma once

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

namespace winrt::Folia {
    struct ClipboardImageStore {
        static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile> SavePngAsync(
            winrt::Windows::Storage::Streams::IRandomAccessStream input, winrt::hstring directory);
        static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile> CopyFileAsync(
            winrt::Windows::Storage::StorageFile input, winrt::hstring directory);
    };
}
