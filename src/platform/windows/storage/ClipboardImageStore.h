#pragma once

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

namespace folia::platform {
    struct ClipboardImageStore {
        static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile> SavePngAsync(
            winrt::Windows::Storage::Streams::IRandomAccessStream input, winrt::hstring directory);
        static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile> CopyFileAsync(
            winrt::Windows::Storage::StorageFile input, winrt::hstring directory);
    };
}
