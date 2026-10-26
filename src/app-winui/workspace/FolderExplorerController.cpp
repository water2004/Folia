#include "pch.h"
import std;
#include "workspace/FolderExplorerController.h"
#include "workspace/FolderExplorerView.h"
#include "workspace/FolderEntry.h"
#include "workspace/FolderExplorerState.h"
#include "localization/Localization.h"
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

namespace winrt::Folia {
    namespace {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        using Windows::Foundation::IInspectable;

        using explorer_detail::FolderEntry;

        using explorer_detail::Entry;
        using explorer_detail::Node;
        Windows::Foundation::IAsyncAction NoLoad() { co_return; }


    }

    FolderExplorerController::FolderExplorerController() : state_(std::make_shared<State>()) {}
    FolderExplorerController::~FolderExplorerController() { Detach(); }

    void FolderExplorerController::Attach(TreeView const& tree, OpenFile open, ReportError report) {
        Detach();
        state_->tree = tree;
        state_->open = std::move(open);
        state_->report = std::move(report);
        auto weak = std::weak_ptr<State>(state_);
        auto canMutate = [weak] {
            auto state = weak.lock();
            return state && state->tree && !state->root.empty() && !state->operationBusy
                && !state->preparingEdit && !state->editingNode && !state->committingEdit;
        };
        state_->refresh = winrt::make<explorer_detail::RootCommand>([weak] {
            if (auto state = weak.lock()) RefreshLoadedNodes(state);
        }, canMutate);
        state_->collapse = winrt::make<explorer_detail::RootCommand>([weak] {
            if (auto state = weak.lock()) { CancelEdit(state); CollapseNodes(state); }
        });
        state_->newFile = winrt::make<explorer_detail::RootCommand>([weak] {
            if (auto state = weak.lock()) BeginCreateAsync(state, false);
        }, canMutate);
        state_->newFolder = winrt::make<explorer_detail::RootCommand>([weak] {
            if (auto state = weak.lock()) BeginCreateAsync(state, true);
        }, canMutate);
        ConfigureFolderExplorerView(tree);
        state_->expanding = tree.Expanding([weak](auto const&, TreeViewExpandingEventArgs const& args) {
            if (auto state = weak.lock()) Populate(state, args.Node());
        });
        state_->collapsed = tree.Collapsed([weak](auto const&, TreeViewCollapsedEventArgs const& args) {
            if (auto entry = Entry(args.Node()); entry && entry->loading) {
                entry->stop.request_stop();
                ++entry->ticket;
                entry->loading = false;
                entry->pending = nullptr;
                args.Node().Children().Clear();
                args.Node().HasUnrealizedChildren(true);
            }
        });
        state_->invoked = tree.ItemInvoked([weak](auto const&, TreeViewItemInvokedEventArgs const& args) {
            auto state = weak.lock();
            auto node = args.InvokedItem().try_as<TreeViewNode>();
            auto entry = Entry(node);
            if (!state || !entry || entry->edit || state->operationBusy || state->editingNode) return;
            if (entry->directory) node.IsExpanded(!node.IsExpanded());
            else if (state->open) state->open(entry->path, false);
        });
        state_->doubleTapped = tree.DoubleTapped([weak](auto const&, Input::DoubleTappedRoutedEventArgs const& args) {
            if (auto state = weak.lock(); state && !state->operationBusy && !state->editingNode) {
                auto target = args.OriginalSource().try_as<DependencyObject>();
                while (target && target != state->tree) {
                    if (auto container = target.try_as<TreeViewItem>()) {
                        auto entry = Entry(state->tree.NodeFromContainer(container));
                        if (entry && !entry->directory && state->open) {
                            state->open(entry->path, true);
                            args.Handled(true);
                        }
                        break;
                    }
                    target = Media::VisualTreeHelper::GetParent(target);
                }
            }
        });
        RegisterEditingHandlers(state_);
        RegisterDragDropHandlers(state_);
    }

    void FolderExplorerController::Detach() {
        CancelEdit(state_, true);
        if (state_->hoverTimer) { state_->hoverTimer.Stop(); state_->hoverTimer = nullptr; }
        state_->stop.request_stop();
        ++state_->generation;
        if (auto tree = state_->tree) {
            tree.Expanding(state_->expanding);
            tree.Collapsed(state_->collapsed);
            tree.ItemInvoked(state_->invoked);
            tree.DoubleTapped(state_->doubleTapped);
            tree.KeyDown(state_->keyDown);
            tree.LostFocus(state_->lostFocus);
            tree.LayoutUpdated(state_->layoutUpdated);
            tree.ContextRequested(state_->contextRequested);
            tree.PointerEntered(state_->pointerEntered);
            tree.PointerExited(state_->pointerExited);
            tree.SelectionChanged(state_->selectionChanged);
            tree.DragItemsStarting(state_->dragStarting);
            tree.DragOver(state_->dragOver);
            tree.Drop(state_->drop);
            tree.DragLeave(state_->dragLeave);
            tree.RootNodes().Clear();
            tree.ItemTemplateSelector(nullptr);
        }
        state_->tree = nullptr;
        state_->open = {};
        state_->report = {};
        state_->refresh = nullptr;
        state_->collapse = nullptr;
        state_->newFile = nullptr;
        state_->newFolder = nullptr;
        state_->operate = {};
    }

    void FolderExplorerController::Open(std::filesystem::path path) { OpenRoot(state_, std::move(path)); }

    void FolderExplorerController::OpenRoot(std::shared_ptr<State> const& state, std::filesystem::path path) {
        if (!state->tree || !path.is_absolute()) throw std::invalid_argument("Folder path must be absolute");
        CancelEdit(state, true);
        if (state->hoverTimer) state->hoverTimer.Stop();
        state->hoverFolder = nullptr;
        state->pendingSelection.clear();
        state->revealNode = {};
        state->stop.request_stop();
        state->stop = std::stop_source{};
        ++state->generation;
        state->root = path.lexically_normal();
        state->tree.RootNodes().Clear();
        auto node = Node(state->root, true, true);
        Entry(node)->actions = std::make_unique<explorer_detail::RootActions>(state->refresh, state->collapse,
            Localize(L"RefreshFiles"), Localize(L"CollapseFolders"), state->newFile, state->newFolder,
            Localize(L"NewFile"), Localize(L"NewFolder"));
        state->tree.RootNodes().Append(node);
        node.IsExpanded(true);
        Populate(state, node);
        state->NotifyCommands();
    }

    void FolderExplorerController::Expand(TreeViewNode const& node) {
        if (!node || !state_->tree) return;
        node.IsExpanded(true);
        Populate(state_, node);
    }

    void FolderExplorerController::Refresh() { RefreshLoadedNodes(state_); }

    void FolderExplorerController::RefreshLoadedNodes(std::shared_ptr<State> const& state) {
        if (!state->tree || state->root.empty()) return;
        state->stop.request_stop();
        state->stop = std::stop_source{};
        ++state->generation;
        std::vector<TreeViewNode> expanded;
        auto visit = [&](auto const& self, TreeViewNode const& node) -> void {
            auto entry = Entry(node);
            if (!entry || !entry->directory) return;
            entry->stop.request_stop();
            ++entry->ticket;
            entry->loading = false;
            entry->pending = nullptr;
            if (!entry->root && !node.IsExpanded()) return;
            entry->loaded = false;
            expanded.push_back(node);
            if (node.IsExpanded()) for (auto child : node.Children()) self(self, child);
        };
        for (auto node : state->tree.RootNodes()) visit(visit, node);
        for (auto node : expanded) Populate(state, node);
    }
    std::filesystem::path FolderExplorerController::RootPath() const { return state_->root; }
    void FolderExplorerController::Collapse() { CollapseNodes(state_); }

    void FolderExplorerController::CollapseNodes(std::shared_ptr<State> const& state) {
        if (!state->tree) return;
        // The native tree only contains directories already visited by the user.
        auto collapse = [&](auto const& self, TreeViewNode const& node) -> void {
            for (auto child : node.Children()) self(self, child);
            node.IsExpanded(false);
        };
        for (auto node : state->tree.RootNodes()) collapse(collapse, node);
    }

    Windows::Foundation::IAsyncAction FolderExplorerController::Populate(std::shared_ptr<State> state, TreeViewNode node) {
        auto entry = Entry(node);
        if (!entry || !entry->directory || entry->loaded || !state->tree) return NoLoad();
        if (entry->loading && entry->pending) return entry->pending;
        auto action = PopulateAsync(std::move(state), node);
        if (entry->loading) entry->pending = action;
        return action;
    }

    Windows::Foundation::IAsyncAction FolderExplorerController::PopulateAsync(std::shared_ptr<State> state, TreeViewNode node) {
        const auto startedGeneration = state->generation;
        auto dispatcher = state->tree ? state->tree.DispatcherQueue() : nullptr;
        try {
        auto entry = Entry(node);
        if (!entry || !entry->directory || entry->loaded || entry->loading || !state->tree) co_return;
        entry->loading = true;
        entry->stop = std::stop_source{};
        auto ticket = ++entry->ticket;
        struct PendingLoad {
            FolderEntry* entry;
            std::uint64_t ticket;
            ~PendingLoad() { if (entry->ticket == ticket) { entry->loading = false; entry->pending = nullptr; } }
        } pending{entry, ticket};
        auto generation = state->generation;
        auto rootStop = state->stop.get_token();
        auto stop = entry->stop.get_token();
        auto path = entry->path;
        std::vector<std::pair<std::filesystem::path, bool>> entries;
        std::error_code error;
        std::exception_ptr failure;
        co_await winrt::resume_background();
        try {
        std::filesystem::directory_iterator iterator(path, error), end;
        while (!error && iterator != end && !rootStop.stop_requested() && !stop.stop_requested()) {
            std::error_code typeError;
            bool directory = iterator->is_directory(typeError);
            if (!typeError && true) entries.emplace_back(iterator->path(), directory);
            iterator.increment(error);
        }
        std::ranges::sort(entries, [](auto const& left, auto const& right) {
            if (left.second != right.second) return left.second > right.second;
            return ::CompareStringOrdinal(left.first.c_str(), -1, right.first.c_str(), -1, TRUE) == CSTR_LESS_THAN;
        });
        } catch (...) { failure = std::current_exception(); }
        co_await wil::resume_foreground(dispatcher, Microsoft::UI::Dispatching::DispatcherQueuePriority::Low);
        auto current = [&] { return state->tree && state->generation == generation && entry->ticket == ticket && !rootStop.stop_requested() && !stop.stop_requested(); };
        if (!current()) co_return;
        if (failure) std::rethrow_exception(failure);
        auto children = node.Children();
        std::unordered_map<std::wstring, TreeViewNode> existing;
        for (auto child : children) if (auto previous = Entry(child)) existing.emplace(previous->path.native(), child);
        // Clearing even an empty TreeViewNodeVector collapses its parent in
        // WinUI. Reconcile this level instead, retaining expansion/selection.
        for (std::size_t index = 0; index < entries.size(); ++index) {
            if (index != 0 && index % 128 == 0) {
                co_await wil::resume_foreground(dispatcher, Microsoft::UI::Dispatching::DispatcherQueuePriority::Low);
                if (!current()) co_return;
            }
            auto const& [childPath, directory] = entries[index];
            auto found = existing.find(childPath.native());
            auto child = found != existing.end() && Entry(found->second)->directory == directory
                ? found->second : Node(childPath, directory);
            const auto target = static_cast<std::uint32_t>(index);
            if (target < children.Size() && children.GetAt(target) == child) continue;
            std::uint32_t previous = 0;
            if (found != existing.end() && children.IndexOf(child, previous)) children.RemoveAt(previous);
            children.InsertAt(target, child);
        }
        entry->loading = false;
        while (children.Size() > entries.size()) children.RemoveAtEnd();
        entry->loaded = !error;
        node.HasUnrealizedChildren(bool(error));
        if (error && state->report) state->report(winrt::to_hstring(error.message()));
        RevealPendingSelection(state);
        } catch (winrt::hresult_error const& error) {
            if (dispatcher && dispatcher.HasThreadAccess() && state->tree && state->generation == startedGeneration && state->report) state->report(error.message());
        } catch (std::exception const& error) {
            if (dispatcher && dispatcher.HasThreadAccess() && state->tree && state->generation == startedGeneration && state->report) state->report(winrt::to_hstring(error.what()));
        }
    }
}
