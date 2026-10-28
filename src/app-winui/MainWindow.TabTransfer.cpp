#include "pch.h"
import std;
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <dwmapi.h>
#include <winrt/Microsoft.UI.Content.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    void MainWindow::SynchronizeTabOrder() {
        auto items = tabSource;
        if (updatingTabs || items.Size() != workspaceDocuments.Items().size()) return;
        std::vector<std::uint64_t> order;
        for (auto item : items) {
            auto id = winrt::unbox_value<std::uint64_t>(item);
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
        auto id = winrt::unbox_value<std::uint64_t>(tab->item);
        auto document = source->workspaceDocuments.Find(id);
        return document && !workspaceDocuments.Find(id)
            && (document->path.empty() || !workspaceDocuments.Find(document->path));
    }
    bool MainWindow::TransferTab(std::shared_ptr<DocumentTab> tab, std::size_t index) {
        if (!CanAcceptTab(tab)) return false;
        auto source = tab->owner.get();
        auto id = winrt::unbox_value<std::uint64_t>(tab->item);
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
            if (source->tabSource.IndexOf(tab->item, at)) source->tabSource.RemoveAt(at);
            source->tabItems.erase(id);
            tab->owner = get_weak();
            // XAML containers retain their TabView parent and XamlRoot. Only
            // move the stable data item; the target template owns a fresh view.
            tabItems.emplace(id, tab);
            auto items = tabSource;
            items.InsertAt(static_cast<std::uint32_t>((std::min)(index, std::size_t{items.Size()})), tab->item);
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
        // The native move loop resumes immediately after this callback. Its
        // next bounds check must use the receiving strip's completed layout.
        DocumentTabs().UpdateLayout();
        RevealTabWindow();
        return true;
    }
    MainWindow* MainWindow::CreateTabWindow() {
        auto app = App::Current();
        auto window = app->CreateMainWindow();
        auto target = winrt::get_self<MainWindow>(window.as<winrt::Folia::MainWindow>());
        target->transferPlaceholder = target->workspaceDocuments.Active()->id;
        target->preparedTabWindow = true;
        target->preparedTabActivation.Attach(target->WindowHandle());
        target->tabWindowStyle = ::GetWindowLongPtrW(target->WindowHandle(), GWL_EXSTYLE);
        ::SetWindowLongPtrW(target->WindowHandle(), GWL_EXSTYLE,
            target->tabWindowStyle | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
        target->AppWindow().IsShownInSwitchers(false);
        BOOL hidden = TRUE;
        winrt::check_hresult(::DwmSetWindowAttribute(target->WindowHandle(), DWMWA_CLOAK, &hidden, sizeof(hidden)));
        return target;
    }
    void MainWindow::RevealTabWindow() {
        if (!preparedTabWindow) return;
        preparedTabActivation.Reset();
        ::SetWindowLongPtrW(WindowHandle(), GWL_EXSTYLE, tabWindowStyle);
        AppWindow().IsShownInSwitchers(true);
        BOOL hidden = FALSE;
        winrt::check_hresult(::DwmSetWindowAttribute(WindowHandle(), DWMWA_CLOAK, &hidden, sizeof(hidden)));
        preparedTabWindow = false;
    }
    std::shared_ptr<MainWindow::DocumentTab> MainWindow::FindTransferTab(Windows::Foundation::IInspectable const& item) {
        auto id = winrt::unbox_value<std::uint64_t>(item);
        if (auto owner = App::Current()->FindDocumentWindow(id))
            if (auto found = owner->tabItems.find(id); found != owner->tabItems.end() && found->second->item == item) return found->second;
        return {};
    }
    MainWindow* MainWindow::BeginTabTearOut(std::shared_ptr<DocumentTab> const& tab) {
        EndTabDrag();
        auto target = CreateTabWindow();
        draggingTab = tab;
        tab->tearOutWindow = target->get_weak();
        return target;
    }
    bool MainWindow::CompleteTabTearOut(std::shared_ptr<DocumentTab> const& tab) {
        if (!tab) return false;
        if (auto target = tab->tearOutWindow.get()) return target->TransferTab(tab, 0);
        return false;
    }
    void MainWindow::EndTabDrag() {
        if (draggingTab) draggingTab->tearOutWindow = {};
        draggingTab.reset();
    }
    bool MainWindow::PrepareExternalTab(Windows::Foundation::IInspectable const& item) {
        auto tab = FindTransferTab(item);
        // WinUI enumerates loaded TabViews, including hidden/preparation hosts.
        // Such windows are not user-visible drop destinations.
        if (preparedTabWindow || !AppWindow().IsVisible() || !CanAcceptTab(tab)) return false;
        if (!tab->tearOutWindow.get()) {
            // A one-tab drag moves its existing window and raises no
            // TabTearOutWindowRequested event.
            if (auto source = tab->owner.get()) {
                source->draggingTab = tab;
                tab->tearOutWindow = source;
            }
        }
        return true;
    }
    bool MainWindow::ContainsTabDragPoint(Windows::Graphics::PointInt32 point) {
        auto tabs = DocumentTabs();
        auto list = tabs.as<IControlProtected>().GetTemplateChild(L"TabListView").try_as<FrameworkElement>();
        if (!list || !list.IsLoaded() || list.ActualWidth() <= 0 || list.ActualHeight() <= 0) return false;
        // Use precisely the element and coordinate system used by WinUI's
        // DragTabWithinTabView. PopulateTabViewList also includes AddButton;
        // accepting that extra region can cause an immediate tear-out again.
        auto bounds = list.TransformToVisual(nullptr).TransformBounds({0, 0,
            static_cast<float>(list.ActualWidth()), static_cast<float>(list.ActualHeight())});
        auto local = list.XamlRoot().CoordinateConverter().ConvertScreenToLocal(point);
        return local.X >= bounds.X && local.X <= bounds.X + bounds.Width
            && local.Y >= bounds.Y && local.Y <= bounds.Y + bounds.Height;
    }
    void MainWindow::RegisterTabTransferHandlers() {
        DocumentTabs().CanTearOutTabs(true);
        tabDragEnded.Attach(Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(AppWindow().Id()),
            WindowEvent([this](auto const&, auto const&) { EndTabDrag(); }));
        DocumentTabs().TabItemsChanged(WindowEvent([this](auto const&, auto const&) { QueueTabOrderSync(); }));
        DocumentTabs().TabTearOutWindowRequested(WindowEvent([this](auto const&, auto const& args) {
            auto items = args.Items();
            if (items.size() == 1)
                if (auto tab = FindTransferTab(items[0])) args.NewWindowId(BeginTabTearOut(tab)->AppWindow().Id());
        }));
        DocumentTabs().TabTearOutRequested(WindowEvent([this](auto const&, auto const& args) {
            // The shipped SDK never fills args.NewWindowId(). The gesture's
            // host follows the data item even after it merges into another view.
            auto items = args.Items();
            if (items.size() == 1) CompleteTabTearOut(FindTransferTab(items[0]));
        }));
        DocumentTabs().ExternalTornOutTabsDropping(WindowEvent([this](auto const&, auto const& args) {
            auto items = args.Items();
            POINT point{};
            args.AllowDrop(items.size() == 1 && ::GetCursorPos(&point)
                && ContainsTabDragPoint({point.x, point.y}) && PrepareExternalTab(items[0]));
        }));
        DocumentTabs().ExternalTornOutTabsDropped(WindowEvent([this](auto const&, auto const& args) {
            auto items = args.Items();
            if (items.size() == 1) TransferTab(FindTransferTab(items[0]), static_cast<std::size_t>((std::max)(0, args.DropIndex())));
        }));
    }
}
