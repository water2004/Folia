#include "pch.h"
#include "agent/ChatView.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "localization/Localization.h"

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    void MainWindow::RegisterAgentChats() {
        ChatSidebarTitle().Text(Localize(L"ChatSessions"));
        NewChatButton().Click(WindowEvent([this](auto const&, auto const&) { OpenAgentChat(); }));
        ChatList().ItemClick(WindowEvent([this](auto const&, ItemClickEventArgs const& args) {
            std::uint32_t index{};
            if (ChatList().Items().IndexOf(args.ClickedItem(), index) && index < chatListIds.size())
                OpenAgentChat(chatListIds[index]);
        }));
        ChatList().ContainerContentChanging(WindowEvent([this](auto const&, ContainerContentChangingEventArgs const& args) {
            auto row = args.ItemContainer();
            const auto index = args.ItemIndex();
            if (args.InRecycleQueue() || index < 0 || !std::cmp_less(index, chatListIds.size())) {
                row.ContextFlyout(nullptr);
                return;
            }
            auto const& id = chatListIds[index];
            MenuFlyout menu;
            menu.MenuFlyoutPresenterStyle(DocumentActionsButton().Flyout().as<MenuFlyout>().MenuFlyoutPresenterStyle());
            for (auto const& [action, remove] : {std::pair{L"ChatRename", false}, std::pair{L"ChatDelete", true}}) {
                MenuFlyoutItem item;
                item.Text(Localize(action));
                item.Click(WindowEvent([this, id, remove](auto const&, auto const&) { ManageAgentChat(id, remove); }));
                menu.Items().Append(item);
            }
            row.ContextFlyout(menu);
        }));
    }

    void MainWindow::RefreshChatList() {
        try {
            auto active = workspaceDocuments.Active();
            auto selected = ChatList().SelectedIndex();
            auto id = active->kind == WorkspaceTabKind::Chat ? active->chatId :
                selected >= 0 && std::cmp_less(selected, chatListIds.size()) ? chatListIds[selected] : std::string{};
            auto chats = chat::Conversations();
            auto items = ChatList().Items();
            items.Clear();
            chatListIds.clear();
            int next = -1;
            for (auto const& conversation : chats) {
                chatListIds.push_back(conversation.id);
                items.Append(box_value(to_hstring(conversation.title)));
                if (conversation.id == id) next = static_cast<int>(items.Size()) - 1;
            }
            ChatList().SelectedIndex(next);
        } catch (std::exception const& e) { ShowError(to_hstring(e.what())); }
        catch (hresult_error const& e) { ShowError(e.message()); }
    }

    void MainWindow::OpenAgentChat(std::string id) {
        if (windowClosed || closePending || closingDocument) return;
        try {
            chat::Conversation conversation;
            const bool created = id.empty();
            if (created) conversation = chat::NewConversation(to_string(Localize(L"ChatNew")));
            else {
                if (auto existing = App::Current()->FindChatWindow(id); existing && existing != this) {
                    existing->ActivateDocument(existing->workspaceDocuments.FindChat(id)->id);
                    existing->Activate();
                    return;
                }
                conversation = chat::LoadConversation(id);
            }
            auto tab = workspaceDocuments.OpenChat(conversation.id, conversation.title);
            ActivateDocument(tab->id);
            RefreshDocumentTabs();
            if (created) RefreshChatList();
            else {
                auto found = std::ranges::find(chatListIds, conversation.id);
                if (found != chatListIds.end()) ChatList().SelectedIndex(static_cast<int>(found - chatListIds.begin()));
            }
        } catch (std::exception const& e) { ShowError(to_hstring(e.what())); }
        catch (hresult_error const& e) { ShowError(e.message()); }
    }

    fire_and_forget MainWindow::ManageAgentChat(std::string id, bool remove) {
        auto owner = get_strong();
        try {
            auto conversation = chat::LoadConversation(id);
            ContentDialog dialog;
            dialog.XamlRoot(Root().XamlRoot());
            dialog.Title(box_value(Localize(remove ? L"ChatDelete" : L"ChatRename")));
            dialog.PrimaryButtonText(Localize(remove ? L"ChatDelete" : L"Save"));
            dialog.CloseButtonText(Localize(L"Cancel"));
            TextBox name;
            if (remove) dialog.Content(box_value(Localize(L"ChatDeleteConfirm")));
            else {
                name.Text(to_hstring(conversation.title));
                name.MaxLength(90);
                dialog.Content(name);
            }
            if (co_await dialog.ShowAsync() != ContentDialogResult::Primary || windowClosed) co_return;
            if (remove) {
                chat::DeleteConversation(id);
                if (auto existing = App::Current()->FindChatWindow(id))
                    existing->CloseDocumentAsync(existing->workspaceDocuments.FindChat(id)->id);
            } else if (!name.Text().empty()) {
                conversation = chat::RenameConversation(id, to_string(name.Text()));
                if (auto existing = App::Current()->FindChatWindow(id)) {
                    auto tab = existing->workspaceDocuments.FindChat(id);
                    tab->chatTitle = conversation.title;
                    existing->UpdateDocumentTab(tab->id);
                    if (existing->IsActiveDocument(tab->id)) existing->UpdateDocumentInfo();
                }
            }
            RefreshChatList();
        } catch (std::exception const& error) { if (!windowClosed) ShowError(to_hstring(error.what())); }
        catch (hresult_error const& error) { if (!windowClosed) ShowError(error.message()); }
    }

    void MainWindow::UpdateAgentChatPages() {
        auto assign = [](ContentControl const& host, std::shared_ptr<chat::ChatView> const& view) {
            auto desired = view ? view->Element().as<Windows::Foundation::IInspectable>() : nullptr;
            if (host.Content() != desired) host.Content(desired);
            host.Visibility(view ? Visibility::Visible : Visibility::Collapsed);
        };
        if (agentChatViews.empty() && workspaceDocuments.Active()->kind != WorkspaceTabKind::Chat) return;
        auto active = workspaceDocuments.Active();
        auto passiveView = editorGroups.Find(editorGroups.Selected(OtherEditorGroup(editorGroups.ActiveGroup())));
        auto passive = passiveView ? workspaceDocuments.Find(passiveView->document) : nullptr;
        auto getView = [&](std::shared_ptr<WorkspaceDocument> const& document) -> std::shared_ptr<chat::ChatView> {
            if (!document || document->kind != WorkspaceTabKind::Chat) return {};
            if (auto found = agentChatViews.find(document->id); found != agentChatViews.end()) return found->second;
            auto window = get_weak();
            chat::ViewHost host;
            host.window = WindowHandle();
            host.tabs = [window] {
                std::vector<chat::OpenTab> snapshot;
                if (auto main = window.get()) {
                    for (auto const& tab : main->workspaceDocuments.Items()) {
                        if (!tab->editor) continue;
                        chat::OpenTab data;
                        data.documentId = tab->id;
                        data.title = to_string(tab->path.empty() ? Localize(L"UntitledMarkdown") : hstring(tab->path.filename().native()));
                        if (tab->editor->loaded) {
                            auto revision = tab->editor->session.ContentState();
                            auto serial = tab->editor->openSerial;
                            data.read = [window, queue = main->DispatcherQueue(), id = tab->id, revision, serial](std::stop_token stop) {
                                auto task = std::make_shared<std::packaged_task<chat::TabSnapshot()>>([=] {
                                    auto main = window.get();
                                    auto doc = main ? main->workspaceDocuments.Find(id) : nullptr;
                                    if (stop.stop_requested() || !doc || !doc->editor || !doc->editor->loaded ||
                                        doc->editor->openSerial != serial || doc->editor->session.ContentState() != revision)
                                        throw std::runtime_error("Tab changed or closed; start a new turn to read its current contents");
                                    auto& session = doc->editor->session;
                                    chat::TabSnapshot result{session.TextUtf8()};
                                    for (auto item : session.RenderModel().outline.flat_items())
                                        result.outline.emplace_back(item->level, item->title_plain_text);
                                    return result;
                                });
                                auto result = task->get_future();
                                if (!queue.TryEnqueue([task] { (*task)(); })) throw std::runtime_error("Window closed");
                                while (result.wait_for(std::chrono::milliseconds(30)) != std::future_status::ready)
                                    if (stop.stop_requested()) throw std::runtime_error("Cancelled");
                                return result.get();
                            };
                        }
                        snapshot.push_back(std::move(data));
                    }
                }
                std::unordered_map<std::string, unsigned> counts;
                for (auto const& tab : snapshot) ++counts[tab.title];
                for (auto& tab : snapshot) {
                    if (counts.at(tab.title) > 1) tab.title += " (#" + std::to_string(tab.documentId) + ")";
                }
                return snapshot;
            };
            host.currentFolder = [window] {
                if (auto main = window.get()) return main->folderExplorer.RootPath();
                return std::filesystem::path{};
            };
            host.settings = [window] {
                if (auto main = window.get()) {
                    main->OpenSettingsTab();
                    if (main->settingsView) main->settingsView->Navigate(SettingsView::Page::Providers);
                }
            };
            host.changed = [window, id = document->id](chat::Conversation const& conversation) {
                if (auto main = window.get()) {
                    if (auto tab = main->workspaceDocuments.Find(id)) {
                        tab->chatTitle = conversation.title;
                        main->UpdateDocumentTab(id);
                        if (main->IsActiveDocument(id)) main->UpdateDocumentInfo();
                        if (main->workspacePane == WorkspacePane::Chats) main->RefreshChatList();
                    }
                }
            };
            auto result = chat::ChatView::Create(chat::LoadConversation(document->chatId), std::move(host), themeProfile);
            agentChatViews.emplace(document->id, result);
            return result;
        };
        try {
            auto activePage = getView(active);
            auto passivePage = getView(passive);
            auto front = AgentChatHost();
            auto back = OtherAgentChatHost();
            const bool entering = activePage && front.Content() != activePage->Element();
            if (passivePage && front.Content() == passivePage->Element()) front.Content(nullptr);
            if (activePage && back.Content() == activePage->Element()) back.Content(nullptr);
            assign(front, activePage);
            assign(back, passivePage);
            if (entering) {
                auto found = std::ranges::find(chatListIds, active->chatId);
                if (found != chatListIds.end()) ChatList().SelectedIndex(static_cast<int>(found - chatListIds.begin()));
                DispatcherQueue().TryEnqueue([page = std::weak_ptr(activePage), window = get_weak(), id = active->id] {
                    if (auto main = window.get(); main && main->IsActiveDocument(id))
                        if (auto view = page.lock()) view->Activate();
                });
            }
        } catch (std::exception const& e) { ShowError(to_hstring(e.what())); }
        catch (hresult_error const& e) { ShowError(e.message()); }
    }

    void MainWindow::UpdateAgentChatTheme() {
        for (auto const& [_, chat] : agentChatViews) chat->SetTheme(themeProfile);
    }

    void MainWindow::ReleaseClosedAgentChats() {
        std::erase_if(agentChatViews, [&](auto const& item) { return !workspaceDocuments.Find(item.first); });
        if (agentChatViews.empty() && workspaceDocuments.Active()->kind != WorkspaceTabKind::Chat) {
            AgentChatHost().Content(nullptr);
            AgentChatHost().Visibility(Visibility::Collapsed);
            OtherAgentChatHost().Content(nullptr);
            OtherAgentChatHost().Visibility(Visibility::Collapsed);
        } else UpdateAgentChatPages();
    }
}
