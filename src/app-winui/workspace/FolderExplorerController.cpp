#include "pch.h"
import std;
#include "workspace/FolderExplorerController.h"
#include "workspace/FolderExplorerView.h"
#include "workspace/FolderEntry.h"
#include "localization/Localization.h"
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

namespace winrt::Folia {
    namespace {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        using Windows::Foundation::IInspectable;

        using explorer_detail::FolderEntry;

        FolderEntry* Entry(TreeViewNode const& node) {
            if (!node) return nullptr;
            auto value = node.Content().try_as<Windows::Foundation::IStringable>();
            return value ? winrt::get_self<FolderEntry>(value) : nullptr;
        }

        bool Editable(std::filesystem::path const& path) {
            auto extension = path.extension().wstring();
            return _wcsicmp(extension.c_str(), L".md") == 0
                || _wcsicmp(extension.c_str(), L".markdown") == 0
                || _wcsicmp(extension.c_str(), L".txt") == 0;
        }

        TreeViewNode Node(std::filesystem::path path, bool directory, bool root = false) {
            auto entry = winrt::make_self<FolderEntry>();
            entry->path = std::move(path);
            entry->directory = directory;
            entry->root = root;
            TreeViewNode node;
            entry->node = winrt::make_weak(node);
            node.Content(entry.as<Windows::Foundation::IStringable>());
            node.HasUnrealizedChildren(directory);
            return node;
        }
    }

    struct FolderExplorerController::State {
        TreeView tree{nullptr};
        OpenFile open;
        ReportError report;
        std::filesystem::path root;
        std::uint64_t generation = 0;
        std::stop_source stop;
        Input::ICommand refresh{nullptr}, collapse{nullptr};
        event_token expanding{}, collapsed{}, invoked{}, doubleTapped{}, keyDown{};
    };

    FolderExplorerController::FolderExplorerController() : state_(std::make_shared<State>()) {}
    FolderExplorerController::~FolderExplorerController() { Detach(); }

    void FolderExplorerController::Attach(TreeView const& tree, OpenFile open, ReportError report) {
        Detach();
        state_->tree = tree;
        state_->open = std::move(open);
        state_->report = std::move(report);
        auto weak = std::weak_ptr<State>(state_);
        state_->refresh = winrt::make<explorer_detail::RootCommand>([weak] {
            if (auto state = weak.lock()) RefreshLoadedNodes(state);
        });
        state_->collapse = winrt::make<explorer_detail::RootCommand>([weak] {
            if (auto state = weak.lock()) CollapseNodes(state);
        });
        ConfigureFolderExplorerView(tree);
        state_->expanding = tree.Expanding([weak](auto const&, TreeViewExpandingEventArgs const& args) {
            if (auto state = weak.lock()) Populate(state, args.Node());
        });
        state_->collapsed = tree.Collapsed([weak](auto const&, TreeViewCollapsedEventArgs const& args) {
            if (auto entry = Entry(args.Node()); entry && entry->loading) {
                entry->stop.request_stop();
                ++entry->ticket;
                entry->loading = false;
                args.Node().Children().Clear();
                args.Node().HasUnrealizedChildren(true);
            }
        });
        state_->invoked = tree.ItemInvoked([weak](auto const&, TreeViewItemInvokedEventArgs const& args) {
            auto state = weak.lock();
            auto node = args.InvokedItem().try_as<TreeViewNode>();
            auto entry = Entry(node);
            if (!state || !entry) return;
            if (entry->directory) node.IsExpanded(!node.IsExpanded());
            else if (state->open) state->open(entry->path, false);
        });
        state_->doubleTapped = tree.DoubleTapped([weak](auto const&, Input::DoubleTappedRoutedEventArgs const& args) {
            if (auto state = weak.lock()) {
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
        state_->keyDown = tree.KeyDown([weak](auto const&, Input::KeyRoutedEventArgs const& args) {
            if (args.Key() != Windows::System::VirtualKey::Enter) return;
            if (auto state = weak.lock()) {
                auto entry = Entry(state->tree.SelectedNode());
                if (entry && !entry->directory && state->open) { state->open(entry->path, true); args.Handled(true); }
            }
        });
    }

    void FolderExplorerController::Detach() {
        state_->stop.request_stop();
        ++state_->generation;
        if (auto tree = state_->tree) {
            tree.Expanding(state_->expanding);
            tree.Collapsed(state_->collapsed);
            tree.ItemInvoked(state_->invoked);
            tree.DoubleTapped(state_->doubleTapped);
            tree.KeyDown(state_->keyDown);
            tree.RootNodes().Clear();
            tree.ItemTemplateSelector(nullptr);
        }
        state_->tree = nullptr;
        state_->open = {};
        state_->report = {};
        state_->refresh = nullptr;
        state_->collapse = nullptr;
    }

    void FolderExplorerController::Open(std::filesystem::path path) { OpenRoot(state_, std::move(path)); }

    void FolderExplorerController::OpenRoot(std::shared_ptr<State> const& state, std::filesystem::path path) {
        if (!state->tree || !path.is_absolute()) throw std::invalid_argument("Folder path must be absolute");
        state->stop.request_stop();
        state->stop = std::stop_source{};
        ++state->generation;
        state->root = path.lexically_normal();
        state->tree.RootNodes().Clear();
        auto node = Node(state->root, true, true);
        Entry(node)->actions = std::make_unique<explorer_detail::RootActions>(state->refresh, state->collapse, Localize(L"RefreshFiles"), Localize(L"CollapseFolders"));
        state->tree.RootNodes().Append(node);
        node.IsExpanded(true);
        Populate(state, node);
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

    winrt::fire_and_forget FolderExplorerController::Populate(std::shared_ptr<State> state, TreeViewNode node) {
        const auto startedGeneration = state->generation;
        auto dispatcher = state->tree ? state->tree.DispatcherQueue() : nullptr;
        try {
        auto entry = Entry(node);
        if (!entry || !entry->directory || entry->loaded || entry->loading || !state->tree) co_return;
        entry->loading = true;
        entry->stop = std::stop_source{};
        auto ticket = ++entry->ticket;
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
            if (!typeError && (directory || Editable(iterator->path()))) entries.emplace_back(iterator->path(), directory);
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
        } catch (winrt::hresult_error const& error) {
            if (dispatcher && dispatcher.HasThreadAccess() && state->tree && state->generation == startedGeneration && state->report) state->report(error.message());
        } catch (std::exception const& error) {
            if (dispatcher && dispatcher.HasThreadAccess() && state->tree && state->generation == startedGeneration && state->report) state->report(winrt::to_hstring(error.what()));
        }
    }
}
