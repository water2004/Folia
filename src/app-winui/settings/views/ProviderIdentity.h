#pragma once

#include "settings/storage/ModelCatalog.h"
#include <winrt/Windows.Storage.h>

namespace winrt::Folia::settings_ui {
    Windows::Foundation::IAsyncOperation<hstring> ImportProviderIcon(Windows::Storage::StorageFile file);
    hstring ProviderHost(models::ModelProfile const&);
    Microsoft::UI::Xaml::Controls::Border ProviderMark(models::ModelProfile const&);
}
