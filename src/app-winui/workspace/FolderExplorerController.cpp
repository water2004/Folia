#include "pch.h"
import std;
#include "workspace/FolderExplorerController.h"
#include "workspace/WorkspaceIcons.h"

namespace winrt::Folia {
    namespace {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        using Windows::Foundation::IInspectable;

        struct FolderEntry : winrt::implements<FolderEntry, Windows::Foundation::IStringable> {
            std::filesystem::path path;
            bool directory = false;
            std::uint64_t ticket = 0;
            bool loading = false;
            bool loaded = false;
            std::stop_source stop;
            winrt::hstring ToString() const { return winrt::hstring(path.filename().c_str()); }
        };

        FolderEntry* Entry(TreeViewNode const& node) {
            if (!node) return nullptr;
            auto value = node.Content().try_as<Windows::Foundation::IStringable>();
            return value ? winrt::get_self<FolderEntry>(value) : nullptr;
        }

        struct EntryTemplateSelector : DataTemplateSelectorT<EntryTemplateSelector> {
            DataTemplate folder = Template(WorkspaceIcon::Folder);
            DataTemplate file = Template(WorkspaceIcon::Document);
            static DataTemplate Template(WorkspaceIcon icon) {
                auto xaml = std::wstring(L"<DataTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'><StackPanel Orientation='Horizontal' Spacing='8' Height='28'><Viewbox Width='16' Height='16' Stretch='Uniform' Opacity='0.75'><Canvas Width='20' Height='20'><Path Fill='{ThemeResource TextFillColorPrimaryBrush}' Data='")
                    + std::wstring(WorkspaceIconPath(icon)) + L"'/></Canvas></Viewbox><TextBlock Text='{Binding Content}' FontSize='13' TextTrimming='CharacterEllipsis' VerticalAlignment='Center'/></StackPanel></DataTemplate>";
                return Markup::XamlReader::Load(xaml).as<DataTemplate>();
            }
            DataTemplate SelectTemplateCore(IInspectable const& item) {
                auto node = item.try_as<TreeViewNode>();
                auto entry = Entry(node);
                return entry && entry->directory ? folder : file;
            }
            DataTemplate SelectTemplateCore(IInspectable const& item, DependencyObject const&) {
                return SelectTemplateCore(item);
            }
        };

        bool Editable(std::filesystem::path const& path) {
            auto extension = path.extension().wstring();
            return _wcsicmp(extension.c_str(), L".md") == 0
                || _wcsicmp(extension.c_str(), L".markdown") == 0
                || _wcsicmp(extension.c_str(), L".txt") == 0;
        }

        TreeViewNode Node(std::filesystem::path path, bool directory) {
            auto entry = winrt::make_self<FolderEntry>();
            entry->path = std::move(path);
            entry->directory = directory;
            TreeViewNode node;
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
        event_token expanding{}, collapsed{}, invoked{}, doubleTapped{}, keyDown{};
    };

    FolderExplorerController::FolderExplorerController() : state_(std::make_shared<State>()) {}
    FolderExplorerController::~FolderExplorerController() { Detach(); }

    void FolderExplorerController::Attach(TreeView const& tree, OpenFile open, ReportError report) {
        Detach();
        state_->tree = tree;
        state_->open = std::move(open);
        state_->report = std::move(report);
        tree.ItemTemplateSelector(winrt::make<EntryTemplateSelector>());
        auto weak = std::weak_ptr<State>(state_);
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
            if (state && entry && !entry->directory && state->open) state->open(entry->path, false);
        });
        state_->doubleTapped = tree.DoubleTapped([weak](auto const&, Input::DoubleTappedRoutedEventArgs const&) {
            if (auto state = weak.lock()) {
                auto entry = Entry(state->tree.SelectedNode());
                if (entry && !entry->directory && state->open) state->open(entry->path, true);
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
    }

    void FolderExplorerController::Open(std::filesystem::path path) {
        if (!state_->tree || !path.is_absolute()) throw std::invalid_argument("Folder path must be absolute");
        state_->stop.request_stop();
        state_->stop = std::stop_source{};
        ++state_->generation;
        state_->root = path.lexically_normal();
        state_->tree.RootNodes().Clear();
        auto node = Node(state_->root, true);
        state_->tree.RootNodes().Append(node);
        Expand(node);
    }

    void FolderExplorerController::Expand(TreeViewNode const& node) {
        if (!node || !state_->tree) return;
        node.IsExpanded(true);
        Populate(state_, node);
    }

    void FolderExplorerController::Refresh() { if (!state_->root.empty()) Open(state_->root); }
    std::filesystem::path FolderExplorerController::RootPath() const { return state_->root; }
    void FolderExplorerController::Collapse() {
        if (!state_->tree) return;
        // The native tree only contains directories already visited by the user.
        auto collapse = [&](auto const& self, TreeViewNode const& node) -> void {
            for (auto child : node.Children()) self(self, child);
            node.IsExpanded(false);
        };
        for (auto node : state_->tree.RootNodes()) collapse(collapse, node);
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
        node.Children().Clear();
        for (std::size_t index = 0; index < entries.size(); ++index) {
            if (index != 0 && index % 128 == 0) {
                co_await wil::resume_foreground(dispatcher, Microsoft::UI::Dispatching::DispatcherQueuePriority::Low);
                if (!current()) co_return;
            }
            node.Children().Append(Node(std::move(entries[index].first), entries[index].second));
        }
        entry->loading = false;
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
