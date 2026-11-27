#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "workspace/WorkspaceFileKind.h"
#include <wil/resource.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Windows::ApplicationModel::DataTransfer;

    void MainWindow::RegisterFileDrops() {
        auto root = Root();
        root.AllowDrop(true);
        root.DragOver(WindowEvent([](auto const&, DragEventArgs const& args) {
            if (!args.DataView().Contains(StandardDataFormats::StorageItems())) return;
            args.AcceptedOperation(DataPackageOperation::Copy);
            args.Handled(true);
        }));
        root.Drop(WindowEvent([this](auto const&, DragEventArgs const& args) {
            if (!args.DataView().Contains(StandardDataFormats::StorageItems())) return;
            args.AcceptedOperation(DataPackageOperation::Copy);
            args.Handled(true);
            DropWorkspaceFiles(args, GroupAtWorkspacePoint(args.GetPosition(Root())));
        }));
    }

    fire_and_forget MainWindow::DropWorkspaceFiles(DragEventArgs args, EditorGroup group) {
        auto lifetime = get_strong();
        auto deferral = args.GetDeferral();
        auto complete = wil::scope_exit([&] { deferral.Complete(); });
        try { co_await OpenDroppedItemsAsync(args.DataView(), group); }
        catch (hresult_error const& e) {
            if (!windowClosed) ShowError(LocalizeFormat(L"StatusOpenFailed", {e.message()}));
        }
    }

    Windows::Foundation::IAsyncAction MainWindow::OpenDroppedItemsAsync(DataPackageView data, EditorGroup group) {
        auto lifetime = get_strong();
        auto items = co_await data.GetStorageItemsAsync();
        if (closePending || closingDocument || windowClosed || relocatingFiles) co_return;
        bool first = true;
        for (auto const& item : items) {
            if (!item.IsOfType(Windows::Storage::StorageItemTypes::File)) continue;
            std::filesystem::path path(item.Path().c_str());
            if (!path.is_absolute() || ClassifyWorkspaceFile(path) == WorkspaceFileKind::Other) continue;
            if (first) {
                ActivateView(editorGroups.Selected(editorGroups.IsSplit() ? group : EditorGroup::Left));
                first = false;
            }
            OpenWorkspaceFile(std::move(path), WorkspaceOpenMode::NewTab);
        }
    }
}
