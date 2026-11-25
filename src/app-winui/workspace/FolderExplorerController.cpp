#include "pch.h"
import std;
#include "workspace/FolderExplorerController.h"
#include "workspace/FolderExplorerView.h"
#include "workspace/FolderEntry.h"
#include "workspace/FolderExplorerState.h"
#include "localization/Localization.h"
#include <wil/resource.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

namespace winrt::Folia {
    namespace {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        using Windows::Foundation::IInspectable;

        using explorer_detail::FolderEntry;
        using explorer_detail::DirectoryChild;
        using explorer_detail::DirectoryListing;
        using explorer_detail::ExplorerCommand;

        using explorer_detail::Entry;
        using explorer_detail::Node;
        Windows::Foundation::IAsyncAction NoLoad() { co_return; }

        bool ContainsNode(TreeView const& tree, TreeViewNode node) {
            if (!node) return false;
            while (node.Depth() > 0) node = node.Parent();
            std::uint32_t index = 0;
            return tree.RootNodes().IndexOf(node, index);
        }

        void CancelBranchLoads(TreeViewNode const& node) {
            if (auto entry = Entry(node); entry && entry->directory) {
                if (entry->loading) {
                    entry->stop.request_stop();
                    ++entry->ticket;
                    entry->loading = false;
                    entry->pending = nullptr;
                }
                for (auto child : node.Children()) CancelBranchLoads(child);
            }
        }
    }

    FolderExplorerController::FolderExplorerController() : state_(std::make_shared<State>()) {}
    FolderExplorerController::~FolderExplorerController() { Detach(); }

    void FolderExplorerController::Attach(TreeView const& tree, OpenFile open, ReportError report, FrameworkElement const& header) {
        Detach();
        state_->tree = tree;
        state_->header = header;
        state_->open = std::move(open);
        state_->report = std::move(report);
        auto weak = std::weak_ptr<State>(state_);
        auto canMutate = [weak] {
            auto state = weak.lock();
            return state && state->tree && !state->root.empty() && !state->operationBusy
                && !state->preparingEdit && !state->editingNode && !state->committingEdit;
        };
        state_->refresh = winrt::make<ExplorerCommand>([weak] {
            if (auto state = weak.lock()) RefreshLoadedNodes(state);
        }, canMutate);
        state_->collapse = winrt::make<ExplorerCommand>([weak] {
            if (auto state = weak.lock()) { CancelEdit(state); CollapseNodes(state); }
        });
        state_->newFile = winrt::make<ExplorerCommand>([weak] {
            if (auto state = weak.lock()) BeginCreateAsync(state, false);
        }, canMutate);
        state_->newFolder = winrt::make<ExplorerCommand>([weak] {
            if (auto state = weak.lock()) BeginCreateAsync(state, true);
        }, canMutate);
        ConfigureFolderExplorerView(tree);
        state_->expanding = tree.Expanding([weak](auto const&, TreeViewExpandingEventArgs const& args) {
            if (auto state = weak.lock(); state && !state->publishing) Populate(state, args.Node());
        });
        state_->collapsed = tree.Collapsed([weak](auto const&, TreeViewCollapsedEventArgs const& args) {
            auto state = weak.lock();
            if (!state || state->publishing) return;
            auto node = args.Node();
            auto entry = Entry(node);
            if (!entry || !entry->directory || (!entry->loading && !node.Children().Size())) return;
            for (auto edit = state->editingNode; edit; edit = edit.Parent())
                if (edit == node) { CancelEdit(state); break; }
            CancelBranchLoads(node);
            entry->loaded = false;
            entry->listing.reset();
            node.Children().Clear();
            node.HasUnrealizedChildren(true);
            UpdateFolderExplorerGuides(state->tree, state->tree.SelectedNode(), state->guidesVisible);
        });
        state_->invoked = tree.ItemInvoked([weak](auto const&, TreeViewItemInvokedEventArgs const& args) {
            auto state = weak.lock();
            if (!state || state->operationBusy || state->editingNode) return;
            auto node = args.InvokedItem().try_as<TreeViewNode>();
            if (InvokePageLink(node)) return;
            auto entry = Entry(node);
            if (!entry || entry->edit) return;
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
        state_->rowPrepared.revoke();
        if (auto tree = state_->tree) {
            tree.Expanding(state_->expanding);
            tree.Collapsed(state_->collapsed);
            tree.ItemInvoked(state_->invoked);
            tree.DoubleTapped(state_->doubleTapped);
            tree.KeyDown(state_->keyDown);
            tree.LostFocus(state_->lostFocus);
            tree.LayoutUpdated(state_->layoutUpdated);
            tree.Loaded(state_->listLoaded);
            tree.UnregisterPropertyChangedCallback(Control::ForegroundProperty(), state_->foregroundChanged);
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
        if (state_->header) {
            state_->header.DataContext(nullptr);
            state_->header.Visibility(Visibility::Collapsed);
            state_->header = nullptr;
        }
        state_->rootNode = nullptr;
        state_->root.clear();
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
        state->stop.request_stop();
        state->stop = std::stop_source{};
        ++state->generation;
        state->root = path.lexically_normal();
        state->tree.RootNodes().Clear();
        auto node = Node(state->root, true, true);
        Entry(node)->actions = std::make_unique<explorer_detail::RootActions>(state->refresh, state->collapse,
            state->newFile, state->newFolder);
        state->rootNode = node;
        if (state->header) {
            state->header.DataContext(node.Content());
            state->header.Visibility(Visibility::Visible);
        }
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
        if (!state->tree || !state->rootNode) return;
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
            for (auto child : state->Children(node)) self(self, child);
        };
        visit(visit, state->rootNode);
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

    void FolderExplorerController::ShowDirectoryPage(std::shared_ptr<State> const& state, TreeViewNode const& node, std::size_t page) {
        auto entry = Entry(node);
        auto& listing = *entry->listing;
        listing.page = (std::min)(page, listing.LastPage());
        auto children = state->Children(node);
        const auto start = listing.page * DirectoryListing::PageSize;
        const auto end = (std::min)(start + DirectoryListing::PageSize, listing.children.size());

        std::unordered_map<std::wstring, TreeViewNode> existing;
        for (auto child : children) if (auto previous = Entry(child)) existing.emplace(previous->path.native(), child);
        std::vector<TreeViewNode> next;
        next.reserve(end - start + 2);
        auto addLink = [&](wchar_t const* message, std::size_t target) {
            TreeViewNode link;
            const auto weak = std::weak_ptr<State>(state);
            const auto weakLink = winrt::make_weak(link);
            auto navigate = [weak, weakLink, target] {
                auto owner = weak.lock();
                if (!owner || !owner->tree) return;
                owner->tree.DispatcherQueue().TryEnqueue([weak, weakLink, target] {
                    auto current = weak.lock();
                    auto row = weakLink.get();
                    if (!current || !current->tree || current->operationBusy || current->preparingEdit
                        || current->editingNode || !ContainsNode(current->tree, row)) return;
                    auto parent = current->Parent(row);
                    auto directory = Entry(parent);
                    if (!directory || !directory->listing || directory->loading) return;
                    ShowDirectoryPage(current, parent, target);
                    auto first = current->Children(parent).GetAt(target ? 1 : 0);
                    current->tree.SelectedNode(first);
                    ScrollFolderExplorerNodeIntoView(current->tree, first);
                    current->tree.Focus(FocusState::Programmatic);
                });
            };
            link.Content(winrt::make<ExplorerCommand>(std::move(navigate), std::function<bool()>{}, LocalizeFormat(message, {
                winrt::to_hstring(target + 1), winrt::to_hstring(listing.LastPage() + 1)})));
            next.push_back(link);
        };
        if (listing.page) addLink(L"ExplorerPreviousPage", listing.page - 1);
        for (auto const& child : std::span(listing.children).subspan(start, end - start)) {
            auto path = entry->path / child.name;
            auto found = existing.find(path.native());
            if (found != existing.end() && Entry(found->second)->directory == child.directory) {
                next.push_back(found->second);
                existing.erase(found);
            } else next.push_back(Node(std::move(path), child.directory));
        }
        if (listing.page < listing.LastPage()) addLink(L"ExplorerNextPage", listing.page + 1);
        if (std::ranges::equal(children, next)) return;

        for (auto const& removed : existing) CancelBranchLoads(removed.second);
        auto selected = state->tree.SelectedNode();
        const bool expanded = node.IsExpanded();
        state->publishing = true;
        auto finish = wil::scope_exit([&] { state->publishing = false; });
        // Reset the native view before adding the new page. Retained entries
        // keep their node identities, selection and expanded children.
        children.Clear();
        for (auto const& child : next) children.Append(child);
        node.IsExpanded(expanded);
        if (selected != state->tree.SelectedNode() && ContainsNode(state->tree, selected))
            state->tree.SelectedNode(selected);
    }

    bool FolderExplorerController::InvokePageLink(TreeViewNode const& link) {
        auto command = link ? link.Content().try_as<Input::ICommand>() : nullptr;
        if (!command) return false;
        command.Execute(nullptr);
        return true;
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
        std::vector<DirectoryChild> entries;
        std::error_code error;
        std::exception_ptr failure;
        co_await winrt::resume_background();
        try {
        std::filesystem::directory_iterator iterator(path, error), end;
        while (!error && iterator != end && !rootStop.stop_requested() && !stop.stop_requested()) {
            std::error_code typeError;
            bool directory = iterator->is_directory(typeError);
            if (!typeError) entries.emplace_back(iterator->path().filename(), directory);
            iterator.increment(error);
        }
        if (!rootStop.stop_requested() && !stop.stop_requested()) {
            std::ranges::sort(entries, [](auto const& left, auto const& right) {
                if (left.directory != right.directory) return left.directory > right.directory;
                const auto order = ::CompareStringOrdinal(left.name.c_str(), -1, right.name.c_str(), -1, TRUE);
                if (order == CSTR_EQUAL) return left.name.native() < right.name.native();
                return order == CSTR_LESS_THAN;
            });
        }
        } catch (...) { failure = std::current_exception(); }
        co_await wil::resume_foreground(dispatcher, Microsoft::UI::Dispatching::DispatcherQueuePriority::Low);
        auto current = [&] { return state->tree && state->generation == generation && entry->ticket == ticket && !rootStop.stop_requested() && !stop.stop_requested(); };
        if (!current()) co_return;
        if (failure) std::rethrow_exception(failure);
        auto page = entry->listing ? entry->listing->page : 0;
        auto selection = state->pendingSelection;
        if (selection.empty()) {
            if (auto selected = Entry(state->tree.SelectedNode())) selection = selected->path;
        }
        if (selection.parent_path() == entry->path) {
            auto name = selection.filename();
            auto selected = std::ranges::find(entries, name, &DirectoryChild::name);
            if (selected != entries.end()) page = (selected - entries.begin()) / DirectoryListing::PageSize;
        }
        entry->listing = std::make_unique<DirectoryListing>(DirectoryListing{std::move(entries)});
        ShowDirectoryPage(state, node, page);
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
