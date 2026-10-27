#include "pch.h"
import std;
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <winrt/Microsoft.UI.Content.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    void MainWindow::SynchronizeTabOrder() {
        auto items = DocumentTabs().TabItems();
        if (updatingTabs || items.Size() != workspaceDocuments.Items().size()) return;
        std::vector<std::uint64_t> order;
        for (auto item : items) {
            auto id = winrt::unbox_value<std::uint64_t>(item.as<TabViewItem>().Tag());
            if (!workspaceDocuments.Find(id) || std::ranges::find(order, id) != order.end()) return;
            order.push_back(id);
        }
        workspaceDocuments.Reorder(order);
    }
    void MainWindow::QueueTabOrderSync() {
        if (updatingTabs || tabOrderSyncPending || windowClosed) return;
        tabOrderSyncPending = true;
        DispatcherQueue().TryEnqueue([weak = get_weak()] {
            if (auto window = weak.get()) {
                window->tabOrderSyncPending = false;
                if (!window->windowClosed) window->SynchronizeTabOrder();
            }
        });
    }
    bool MainWindow::CanAcceptTab(std::shared_ptr<DocumentTab> const& tab) const {
        if (!tab || closePending || closingDocument || relocatingFiles || windowClosed) return false;
        auto source = tab->owner.get();
        if (!source || source.get() == this || source->windowClosed || source->closePending || source->closingDocument || source->relocatingFiles) return false;
        auto id = winrt::unbox_value<std::uint64_t>(tab->view.Tag());
        auto document = source->workspaceDocuments.Find(id);
        return document && !workspaceDocuments.Find(id)
            && (document->path.empty() || !workspaceDocuments.Find(document->path));
    }
    bool MainWindow::TransferTab(std::shared_ptr<DocumentTab> tab, std::size_t index) {
        if (!CanAcceptTab(tab)) return false;
        auto source = tab->owner.get();
        auto id = winrt::unbox_value<std::uint64_t>(tab->view.Tag());
        auto document = source->workspaceDocuments.Find(id);
        source->SynchronizeTabOrder(); SynchronizeTabOrder();
        tabItems.reserve(tabItems.size() + 1);
        struct Mutating {
            MainWindow& from; MainWindow& to;
            Mutating(MainWindow& a, MainWindow& b) : from(a), to(b) { from.updatingTabs = to.updatingTabs = true; }
            ~Mutating() { from.updatingTabs = to.updatingTabs = false; }
        };
        {
            Mutating mutation(*source, *this);
            if (source->IsActiveDocument(id)) {
                auto next = source->workspaceDocuments.Adjacent(id, 1);
                if (next == document) {
                    next = source->workspaceDocuments.New(winrt::Folia::WorkspaceOpenMode::Preview);
                    source->transferPlaceholder = next->id;
                }
                source->ActivateDocument(next->id);
            }
            source->workspaceDocuments.TransferTo(id, workspaceDocuments, index);
            std::uint32_t at = 0;
            if (source->DocumentTabs().TabItems().IndexOf(tab->view, at)) source->DocumentTabs().TabItems().RemoveAt(at);
            source->tabItems.erase(id);
            tab->owner = get_weak();
            tabItems.emplace(id, tab);
            tab->view.Style(Root().Resources().Lookup(winrt::box_value(L"WorkspaceTabStyle")).as<Style>());
            tab->view.BorderBrush(Root().Resources().Lookup(winrt::box_value(L"FoliaShellAccentBrush")).as<Media::Brush>());
            auto items = DocumentTabs().TabItems();
            items.InsertAt(static_cast<std::uint32_t>((std::min)(index, std::size_t{items.Size()})), tab->view);
            document->themeRevision = 0;
            ActivateDocument(id);
            if (transferPlaceholder && *transferPlaceholder != id) {
                auto placeholder = workspaceDocuments.Find(*transferPlaceholder);
                if (placeholder && !placeholder->session.HasFile() && !placeholder->controller.IsModified()
                    && !placeholder->kept) workspaceDocuments.Remove(placeholder->id);
                transferPlaceholder.reset();
            }
        }
        source->RefreshDocumentTabs(); RefreshDocumentTabs();
        return true;
    }
    MainWindow* MainWindow::CreateTabWindow() {
        auto app = App::Current();
        auto window = app->CreateMainWindow();
        auto target = winrt::get_self<MainWindow>(window.as<winrt::Folia::MainWindow>());
        target->transferPlaceholder = target->workspaceDocuments.Active()->id;
        return target;
    }
    void MainWindow::RegisterTabTransferHandlers() {
        DocumentTabs().CanTearOutTabs(true);
        DocumentTabs().TabItemsChanged(WindowEvent([this](auto const&, auto const&) { QueueTabOrderSync(); }));
        DocumentTabs().TabTearOutWindowRequested(WindowEvent([this](auto const&, auto const& args) {
            args.NewWindowId(CreateTabWindow()->AppWindow().Id());
        }));
        DocumentTabs().TabTearOutRequested(WindowEvent([this](auto const&, auto const& args) {
            if (auto target = App::Current()->FindMainWindow(args.NewWindowId()))
                for (auto const& item : args.Tabs()) {
                    auto id = winrt::unbox_value<std::uint64_t>(item.template as<TabViewItem>().Tag());
                    if (auto found = tabItems.find(id); found != tabItems.end()) target->TransferTab(found->second, 0);
                }
        }));
        auto binding = [](winrt::com_array<UIElement> const& tabs) -> std::shared_ptr<DocumentTab> {
            if (tabs.size() != 1) return {};
            auto view = tabs[0].as<TabViewItem>();
            auto id = winrt::unbox_value<std::uint64_t>(view.Tag());
            // TabView owns the parent chain while moving the same tab container.
            auto parent = Media::VisualTreeHelper::GetParent(view);
            while (parent && !parent.try_as<TabView>()) parent = Media::VisualTreeHelper::GetParent(parent);
            if (!parent) return {};
            auto root = parent.as<TabView>().XamlRoot();
            auto app = App::Current();
            if (auto owner = app->FindMainWindow(root.ContentIslandEnvironment().AppWindowId()))
                if (auto found = owner->tabItems.find(id); found != owner->tabItems.end() && found->second->view == view) return found->second;
            return {};
        };
        DocumentTabs().ExternalTornOutTabsDropping(WindowEvent([this, binding](auto const&, auto const& args) {
            args.AllowDrop(CanAcceptTab(binding(args.Tabs())));
        }));
        DocumentTabs().ExternalTornOutTabsDropped(WindowEvent([this, binding](auto const&, auto const& args) {
            TransferTab(binding(args.Tabs()), static_cast<std::size_t>((std::max)(0, args.DropIndex())));
        }));
    }
}
