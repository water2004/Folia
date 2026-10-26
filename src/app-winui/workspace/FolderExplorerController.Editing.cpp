#include "pch.h"
import std;
#include "workspace/FolderExplorerState.h"
#include "workspace/FolderExplorerView.h"
#include "localization/Localization.h"

namespace winrt::Folia {
    namespace {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        using explorer_detail::Entry;
        bool SamePath(std::filesystem::path a, std::filesystem::path b) {
            auto left = a.lexically_normal().make_preferred().native(), right = b.lexically_normal().make_preferred().native();
            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
        }
        winrt::fire_and_forget IgnoreResult(Windows::Foundation::IAsyncOperation<bool> action) {
            try { co_await action; } catch (...) {}
        }
    }

    void FolderExplorerController::SetOperationHandler(OperationHandler handler) { state_->operate = std::move(handler); }
    void FolderExplorerController::NewFile() { BeginCreateAsync(state_, false); }
    void FolderExplorerController::NewFolder() { BeginCreateAsync(state_, true); }
    void FolderExplorerController::RenameSelected() { if (state_->tree) BeginRename(state_, state_->tree.SelectedNode()); }
    void FolderExplorerController::CancelNameEdit() { CancelEdit(state_); }
    bool FolderExplorerController::IsEditing() const noexcept { return state_->preparingEdit || bool(state_->editingNode); }
    bool FolderExplorerController::IsBusy() const noexcept { return state_->operationBusy; }
    Windows::Foundation::IAsyncOperation<bool> FolderExplorerController::CommitNameAsync(winrt::hstring name) {
        return CommitEditAsync(state_, std::move(name));
    }

    winrt::fire_and_forget FolderExplorerController::BeginCreateAsync(std::shared_ptr<State> state, bool directory, TreeViewNode parent) {
        if (!state->tree || state->root.empty() || state->operationBusy || state->preparingEdit || state->editingNode) co_return;
        if (!parent) parent = state->tree.SelectedNode();
        if (auto entry = Entry(parent); entry && !entry->directory) parent = parent.Parent();
        if (!parent && state->tree.RootNodes().Size()) parent = state->tree.RootNodes().GetAt(0);
        if (!parent || !Entry(parent)) co_return;
        const auto generation = state->generation, serial = ++state->editSerial;
        state->preparingEdit = true;
        state->NotifyCommands();
        struct Preparing { std::shared_ptr<State> state; ~Preparing() { state->preparingEdit = false; state->NotifyCommands(); } } preparing{state};
        try {
            parent.IsExpanded(true);
            co_await Populate(state, parent);
            if (!state->tree || state->generation != generation || state->editSerial != serial) co_return;
            auto node = explorer_detail::Node(Entry(parent)->path, directory);
            node.HasUnrealizedChildren(false);
            auto entry = Entry(node);
            entry->edit = std::make_unique<explorer_detail::NameEdit>();
            entry->edit->creating = true;
            entry->edit->name = Localize(directory ? L"DefaultNewFolderName" : L"UntitledMarkdown");
            state->editingNode = node;
            parent.Children().InsertAt(0, node);
            state->tree.SelectedNode(node);
            state->pendingEditFocus = true;
            state->selectEditName = true;
            if (auto container = state->tree.ContainerFromNode(parent).try_as<FrameworkElement>()) {
                BringIntoViewOptions options;
                options.AnimationDesired(false);
                options.VerticalAlignmentRatio(0);
                container.StartBringIntoView(options);
            }
        } catch (winrt::hresult_error const& error) { if (state->tree && state->report) state->report(error.message()); }
    }

    void FolderExplorerController::BeginRename(std::shared_ptr<State> const& state, TreeViewNode node) {
        if (!state->tree || state->operationBusy || state->preparingEdit || state->editingNode || !node) return;
        auto entry = Entry(node);
        if (!entry || entry->path.filename().empty()) return;
        entry->edit = std::make_unique<explorer_detail::NameEdit>();
        entry->edit->name = entry->ToString();
        state->editingNode = node;
        ++state->editSerial;
        state->pendingEditFocus = true;
        state->selectEditName = true;
        UpdateFolderExplorerRow(state->tree, node);
        state->NotifyCommands();
    }

    void FolderExplorerController::CancelEdit(std::shared_ptr<State> const& state, bool force) {
        if (state->committingEdit && !force) return;
        ++state->editSerial;
        state->pendingEditFocus = false;
        auto node = state->editingNode;
        state->editingNode = nullptr;
        if (auto entry = Entry(node); entry && entry->edit) {
            const bool draft = entry->edit->creating;
            entry->edit.reset();
            if (draft) {
                auto parent = node.Parent();
                if (parent) {
                    std::uint32_t index = 0;
                    if (parent.Children().IndexOf(node, index)) parent.Children().RemoveAt(index);
                    if (state->tree) state->tree.SelectedNode(parent);
                }
            } else if (state->tree) UpdateFolderExplorerRow(state->tree, node);
        }
        state->NotifyCommands();
    }

    Windows::Foundation::IAsyncOperation<bool> FolderExplorerController::CommitEditAsync(std::shared_ptr<State> state, winrt::hstring name) {
        auto node = state->editingNode;
        auto entry = Entry(node);
        if (!state->tree || state->operationBusy || !entry || !entry->edit) co_return false;
        entry->edit->name = name;
        if (!ValidWorkspaceEntryName(name)) {
            entry->edit->error = Localize(L"InvalidEntryName");
            entry->NotifyEdit();
            state->pendingEditFocus = true;
            co_return false;
        }
        const bool creating = entry->edit->creating;
        auto target = (creating ? entry->path : entry->path.parent_path()) / std::wstring(name);
        if (!creating && target.native() == entry->path.native()) { CancelEdit(state); co_return true; }
        WorkspaceFileAction action{creating ? (entry->directory ? WorkspaceFileActionKind::NewFolder : WorkspaceFileActionKind::NewFile)
                                           : WorkspaceFileActionKind::Rename,
            creating ? std::filesystem::path{} : entry->path, target, entry->directory};
        const auto generation = state->generation;
        state->operationBusy = true;
        state->committingEdit = true;
        entry->edit->busy = true;
        entry->NotifyEdit();
        state->NotifyCommands();
        struct Pending { std::shared_ptr<State> state; ~Pending() { state->operationBusy = false; state->committingEdit = false; state->NotifyCommands(); } } pending{state};
        try {
            auto result = co_await (state->operate ? state->operate(action) : ApplyWorkspaceFileActionAsync(action));
            if (!state->tree || state->generation != generation || state->editingNode != node) co_return true;
            entry->path = std::filesystem::path(result.c_str());
            entry->edit.reset();
            state->editingNode = nullptr;
            state->pendingEditFocus = false;
            node.HasUnrealizedChildren(entry->directory && !entry->loaded);
            UpdateFolderExplorerRow(state->tree, node);
            entry->Notify(L"Name");
            entry->Notify(L"FullPath");
            PublishAction(state, action, entry->path);
            if (creating && !entry->directory && state->open) state->open(entry->path, true);
            co_return true;
        } catch (winrt::hresult_error const& error) {
            if (state->tree && state->generation == generation && state->editingNode == node && entry->edit) {
                entry->edit->busy = false;
                entry->edit->error = error.message();
                entry->NotifyEdit();
                state->pendingEditFocus = true;
            }
        } catch (std::exception const& error) {
            if (state->tree && state->report) state->report(winrt::to_hstring(error.what()));
            if (entry->edit) { entry->edit->busy = false; entry->edit->error = winrt::to_hstring(error.what()); entry->NotifyEdit(); }
        }
        co_return false;
    }

    void FolderExplorerController::PublishAction(std::shared_ptr<State> const& state, WorkspaceFileAction const& action, std::filesystem::path const& result) {
        TreeViewNode source{nullptr}, destination{nullptr};
        auto find = [&](auto const& self, TreeViewNode const& node) -> void {
            if (auto entry = Entry(node)) {
                if (SamePath(entry->path, action.source)) source = node;
                if (entry->directory && SamePath(entry->path, result.parent_path())) destination = node;
            }
            for (auto child : node.Children()) self(self, child);
        };
        for (auto node : state->tree.RootNodes()) find(find, node);
        if (action.kind == WorkspaceFileActionKind::Move && source && destination && source.Parent() != destination && !Entry(source)->root) {
            auto parent = source.Parent();
            std::uint32_t index = 0;
            if (parent && parent.Children().IndexOf(source, index)) parent.Children().RemoveAt(index);
            destination.Children().Append(source);
        }
        auto update = [&](auto const& self, TreeViewNode const& node) -> void {
            if (auto entry = Entry(node)) if (auto path = RelocatedWorkspacePath(action, entry->path)) {
                entry->path = *path;
                entry->Notify(L"Name"); entry->Notify(L"FullPath"); entry->Notify(L"Indent");
            }
            for (auto child : node.Children()) self(self, child);
        };
        for (auto node : state->tree.RootNodes()) update(update, node);
        if (auto root = RelocatedWorkspacePath(action, state->root)) state->root = *root;
        if (destination) destination.IsExpanded(true);
        state->pendingSelection = result;
        RefreshLoadedNodes(state);
        RevealPendingSelection(state);
    }

    void FolderExplorerController::RevealPendingSelection(std::shared_ptr<State> const& state) {
        if (!state->tree || state->pendingSelection.empty()) return;
        auto find = [&](auto const& self, TreeViewNode const& node) -> TreeViewNode {
            if (auto entry = Entry(node); entry && !entry->edit && SamePath(entry->path, state->pendingSelection)) return node;
            for (auto child : node.Children()) if (auto found = self(self, child)) return found;
            return nullptr;
        };
        for (auto root : state->tree.RootNodes()) if (auto node = find(find, root)) {
            for (auto parent = node.Parent(); parent && parent.Depth() >= 0; parent = parent.Parent()) parent.IsExpanded(true);
            state->tree.SelectedNode(node);
            state->revealNode = winrt::make_weak(node);
            state->revealParentRequested = false;
            state->pendingSelection.clear();
            break;
        }
    }

    void FolderExplorerController::RegisterEditingHandlers(std::shared_ptr<State> const& state) {
        auto weak = std::weak_ptr<State>(state);
        state->pointerEntered = state->tree.PointerEntered([weak](auto const&, auto const&) {
            if (auto state = weak.lock()) { state->guidesVisible = true; UpdateFolderExplorerGuides(state->tree, state->tree.SelectedNode(), true); }
        });
        state->pointerExited = state->tree.PointerExited([weak](auto const&, auto const&) {
            if (auto state = weak.lock()) { state->guidesVisible = false; UpdateFolderExplorerGuides(state->tree, state->tree.SelectedNode(), false); }
        });
        state->selectionChanged = state->tree.SelectionChanged([weak](auto const&, auto const&) {
            if (auto state = weak.lock()) UpdateFolderExplorerGuides(state->tree, state->tree.SelectedNode(), state->guidesVisible);
        });
        state->keyDown = state->tree.KeyDown([weak](auto const&, Input::KeyRoutedEventArgs const& args) {
            auto state = weak.lock();
            if (!state) return;
            auto editor = ExplorerNameEditorFromElement(args.OriginalSource().try_as<DependencyObject>());
            if (editor) {
                if (args.Key() == Windows::System::VirtualKey::Enter) { IgnoreResult(CommitEditAsync(state, editor.Text())); args.Handled(true); }
                else if (args.Key() == Windows::System::VirtualKey::Escape) { CancelEdit(state); args.Handled(true); }
                return;
            }
            if (args.Key() == Windows::System::VirtualKey::F2) { BeginRename(state, state->tree.SelectedNode()); args.Handled(true); }
            else if (args.Key() == Windows::System::VirtualKey::Enter) {
                if (auto entry = Entry(state->tree.SelectedNode()); entry && !entry->directory && state->open) {
                    state->open(entry->path, true); args.Handled(true);
                }
            }
        });
        state->lostFocus = state->tree.LostFocus([weak](auto const&, RoutedEventArgs const& args) {
            auto state = weak.lock();
            if (!state || state->committingEdit) return;
            if (auto editor = ExplorerNameEditorFromElement(args.OriginalSource().try_as<DependencyObject>());
                editor && (!editor.ContextFlyout() || !editor.ContextFlyout().IsOpen())) IgnoreResult(CommitEditAsync(state, editor.Text()));
        });
        state->layoutUpdated = state->tree.LayoutUpdated([weak](auto const&, auto const&) {
            auto state = weak.lock();
            if (!state || !state->tree) return;
            if (state->guidesVisible || state->tree.SelectedNode()) UpdateFolderExplorerGuides(state->tree, state->tree.SelectedNode(), true);
            if (state->pendingEditFocus && state->editingNode) {
                if (auto container = state->tree.ContainerFromNode(state->editingNode).try_as<TreeViewItem>())
                    if (auto root = container.ContentTemplateRoot().try_as<FrameworkElement>())
                        if (auto editor = root.FindName(L"EntryEditor").try_as<TextBox>()) {
                            state->pendingEditFocus = false;
                            editor.Focus(FocusState::Programmatic);
                            if (state->selectEditName) {
                                auto entry = Entry(state->editingNode);
                                auto count = entry && !entry->directory ? std::filesystem::path(editor.Text().c_str()).stem().native().size() : editor.Text().size();
                                editor.Select(0, static_cast<int>(count));
                                state->selectEditName = false;
                            }
                        }
            }
            if (auto node = state->revealNode.get()) {
                auto container = state->tree.ContainerFromNode(node).try_as<FrameworkElement>();
                if (container) {
                    state->revealNode = {};
                    BringIntoViewOptions options; options.AnimationDesired(false); container.StartBringIntoView(options);
                } else if (!state->revealParentRequested && node.Parent()) {
                    if (auto parent = state->tree.ContainerFromNode(node.Parent()).try_as<FrameworkElement>()) {
                        state->revealParentRequested = true;
                        BringIntoViewOptions options; options.AnimationDesired(false); options.VerticalAlignmentRatio(0); parent.StartBringIntoView(options);
                    }
                }
            }
        });
        state->contextRequested = state->tree.ContextRequested([weak](auto const&, Input::ContextRequestedEventArgs const& args) {
            auto state = weak.lock();
            if (!state || !state->tree || ExplorerNameEditorFromElement(args.OriginalSource().try_as<DependencyObject>())) return;
            auto node = ExplorerNodeFromElement(state->tree, args.OriginalSource().try_as<DependencyObject>());
            Windows::Foundation::Point point{};
            const bool pointer = args.TryGetPosition(state->tree, point);
            if (!node && !pointer) node = state->tree.SelectedNode();
            if (!node && state->tree.RootNodes().Size()) node = state->tree.RootNodes().GetAt(0);
            if (!node) return;
            state->tree.SelectedNode(node);
            MenuFlyout menu;
            auto add = [&](wchar_t const* label, auto handler) {
                MenuFlyoutItem item; item.Text(Localize(label));
                item.IsEnabled(!state->operationBusy && !state->preparingEdit && !state->editingNode);
                item.Click(handler); menu.Items().Append(item);
            };
            add(L"NewFile", [weak, node](auto const&, auto const&) { if (auto state = weak.lock()) BeginCreateAsync(state, false, node); });
            add(L"NewFolder", [weak, node](auto const&, auto const&) { if (auto state = weak.lock()) BeginCreateAsync(state, true, node); });
            menu.Items().Append(MenuFlyoutSeparator());
            add(L"Rename", [weak, node](auto const&, auto const&) { if (auto state = weak.lock()) BeginRename(state, node); });
            Primitives::FlyoutShowOptions options;
            if (pointer) options.Position(point);
            menu.ShowAt(state->tree, options);
            args.Handled(true);
        });
    }
}
