#include "pch.h"
#include "agent/ChatStore.h"
#include "settings/storage/ModelCatalog.h"
#include "storage/JsonRecords.h"
#include "agent/ChatTools.h"
#include "agent/ChatComposerExtras.h"
#include "agent/ChatView.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.System.h>
#include "support/folia_test.hpp"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "visual_window.h"
#include "../support/CompletionHttpServer.h"
#include "../support/DocumentControllerFixture.h"
#include "localization/Localization.h"
#include <wil/resource.h>
#include <microsoft.ui.xaml.window.h>
#include <dwmapi.h>
#pragma comment(lib, "gdi32.lib")
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>

namespace winrt::Folia::implementation {
    struct AgentChatWindowTestAccess {
        static auto Impl(Microsoft::UI::Xaml::Window const& window) {
            return get_self<MainWindow>(window.as<winrt::Folia::MainWindow>());
        }
        static void ChatTheme(MainWindow* shell, folia::ThemeProfile const& theme) {
            shell->agentChatViews.at(Active(shell)->id)->SetTheme(theme);
        }
        static auto const& Theme(MainWindow* shell) { return shell->themeProfile; }
        static auto Host(MainWindow* shell) { return shell->AgentChatHost(); }
        static auto Sidebar(MainWindow* shell) { return shell->ChatList(); }
        static void RefreshSidebar(MainWindow* shell) { shell->RefreshChatList(); }
        static auto Active(MainWindow* shell) { return shell->workspaceDocuments.Active(); }
        static void Open(MainWindow* shell, std::string id, WorkspaceOpenMode mode = WorkspaceOpenMode::Preview) {
            shell->OpenAgentChat(std::move(id), mode);
        }
        static auto TabCount(MainWindow* shell) { return shell->editorGroups.Views(shell->editorGroups.ActiveGroup()).size(); }
        static void KeepActive(MainWindow* shell) { shell->workspaceDocuments.Keep(Active(shell)->id); }
        static void Dark(MainWindow* shell) {
            shell->appSettings.themeId = "folia.dark";
            shell->UpdateTheme();
        }
        static void Close(MainWindow* shell) { shell->CloseDocumentAsync(Active(shell)->id); }
        static void CloseRightView(MainWindow* shell) {
            shell->CloseEditorViewAsync(shell->editorGroups.Selected(EditorGroup::Right));
        }
        static bool MoveActive(MainWindow* source, MainWindow* target) {
            return target->TransferTab(source->tabItems.at(source->SelectedEditorView()), 0);
        }
        static bool Split(MainWindow* shell) { return shell->editorGroups.IsSplit(); }
        static std::uint64_t Companion(MainWindow* shell) {
            auto& groups = shell->editorGroups;
            auto view = groups.Find(groups.Selected(OtherEditorGroup(groups.ActiveGroup())));
            return view ? view->document : 0;
        }
        static void OpenReference(MainWindow* shell, std::uint64_t source, std::string_view key, std::filesystem::path const& path, std::size_t line) {
            shell->OpenChatDocument(source, key, path, {EditorSourceLocation::Kind::Line, line});
        }
        static void CloseId(MainWindow* shell, std::uint64_t id) { shell->CloseDocumentAsync(id); }
        static void ShowSidebar(MainWindow* shell) { shell->SelectWorkspacePane(MainWindow::WorkspacePane::Chats); }
    };
}

namespace {
    using namespace boost::ut;
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;
    using namespace winrt::Folia;
    using document_tests::Pump;
    using Access = winrt::Folia::implementation::AgentChatWindowTestAccess;
    using folia::ai::Json;

    template<class Control, class Predicate>
    Control Find(DependencyObject const& root, Predicate const& matches) {
        if (!root) return nullptr;
        if (auto item = root.try_as<Control>())
            if (matches(item)) return item;
        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(root); ++i)
            if (auto item = Find<Control>(Media::VisualTreeHelper::GetChild(root, i), matches)) return item;
        return nullptr;
    }

    void Invoke(Button const& button) {
        Automation::Peers::ButtonAutomationPeer peer(button);
        peer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
    }

    void Capture(Window const& window, std::filesystem::path const& name) {
        // Composite the actual window for visual inspection, including the Direct2D reply.
        HWND hwnd{};
        window.as<IWindowNative>()->get_WindowHandle(&hwnd);
        RECT bounds{};
        GetWindowRect(hwnd, &bounds);
        int w = bounds.right - bounds.left, h = bounds.bottom - bounds.top;
        auto screen = GetDC(nullptr);
        auto dc = CreateCompatibleDC(screen);
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
        void* bits{};
        auto dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        auto old = SelectObject(dc, dib);
        window.Content().as<FrameworkElement>().UpdateLayout();
        // Wait for popup transitions and the D2D composition commit before sampling the HWND.
        auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(450);
        Pump([&] { return std::chrono::steady_clock::now() >= until; });
        DwmFlush();
        if (PrintWindow(hwnd, dc, 2) && bits) {
            winrt::Windows::Storage::Streams::Buffer pixels(w*h*4);
            pixels.Length(w*h*4);
            std::memcpy(pixels.data(), bits, pixels.Length());
            for (std::uint32_t i = 3; i < pixels.Length(); i += 4) pixels.data()[i] = 255;
            visual_tests::SavePng(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                / "build/tests/agent-chat" / name, w, h, pixels);
        }
        SelectObject(dc, old); DeleteObject(dib); DeleteDC(dc); ReleaseDC(nullptr, screen);
    }

    suite agent_chat_tests = [] {
        "agent workspace tools page only the approved root and reject absolute and parent paths"_test = [] {
            auto folder = std::filesystem::temp_directory_path() / ("folia-agent-tool-" + records::NewId());
            std::filesystem::create_directory(folder);
            auto file = folder / "test.md";
            auto cleanup = wil::scope_exit([&] { DeleteFileW(file.c_str()); RemoveDirectoryW(folder.c_str()); });
            std::ofstream(file, std::ios::binary) << std::string(2999, 'a') << "你好";
            chat::ChatTranscript::Documents docs;
            auto tools = chat::MakeTools(std::nullopt, folder, [&](auto const& ref) { docs.push_back(ref); });
            expect(tools.size() == 4_u);
            auto directory = tools[0].invoke(Json::object(), {});
            expect(directory["entries"][0]["name"] == "test.md");
            auto page = tools[1].invoke(Json{{"path", "test.md"}}, {});
            expect(page["next_offset"].get<std::size_t>() == 2999_u);
            auto next = tools[1].invoke(Json{{"path", "test.md"}, {"offset", page["next_offset"]}}, {});
            expect(next["text"] == "你好");
            auto hits = tools[2].invoke(Json{{"path", "test.md"}, {"query", "你好"}}, {});
            expect(hits["matches"].at(0)["offset"].get<std::size_t>() == 2999_u);
            auto uri = page.at("link").get<std::string>();
            auto target = chat::ResolveDocumentLink(hits.at("matches")[0].at("link").get<std::string>(), docs);
            expect(docs.size() == 1_u && target.has_value());
            if (target) {
                expect(target->location.kind == EditorSourceLocation::Kind::Utf8Byte && target->location.value == 2999_u);
                expect(target->document.path == winrt::to_string(winrt::hstring(file.native())));
            }
            auto linked = tools[3].invoke(Json{{"path", "test.md"}, {"line", 3}}, {});
            expect(linked.at("link") == uri + "#L3");
            auto line = chat::ResolveDocumentLink(linked.at("link").get<std::string>(), docs);
            expect(line.has_value() && line->location.value == 3_u);
            expect(!chat::ResolveDocumentLink(uri + "#L0", docs) && !chat::ResolveDocumentLink(uri + "#B-4", docs));
            expect(!chat::ResolveDocumentLink(uri, {}) && !chat::ResolveDocumentLink("file:///D:/notes.md", docs));
            auto matches = chat::FindWorkspaceMentions(folder, "Te", {});
            expect(matches.size() == 1_u && matches.front() == "test.md");
            constexpr std::size_t searchPage = 8 * 1024 * 1024;
            {
                std::ofstream output(file, std::ios::binary);
                std::string block(64 * 1024, 'x');
                for (std::size_t bytes = 0; bytes < searchPage; bytes += block.size()) output << block;
                output << "needle at the end";
            }
            auto first = tools[2].invoke(Json{{"path", "test.md"}, {"query", "needle"}}, {});
            expect(first["matches"].empty() && first["next_offset"] == searchPage);
            auto tail = tools[2].invoke(Json{{"path", "test.md"}, {"query", "needle"}, {"offset", first["next_offset"]}}, {});
            expect(tail["matches"].at(0)["offset"] == searchPage);
            expect(tools[1].invoke(Json{{"path", "test.md"}, {"offset", searchPage}}, {})["text"] == "needle at the end");
            expect(throws([&] { (void)tools[1].invoke(Json{{"path", "../test.md"}}, {}); }));
            expect(throws([&] { (void)tools[1].invoke(Json{{"path", winrt::to_string(winrt::hstring(file.native()))}}, {}); }));
        };

        "agent large open documents support lazy reads, outlines, and searches across chunk boundaries"_test = [] {
            int snapshots = 0;
            std::vector<chat::OpenTab> tabs{{"Notes", 42, [&](std::stop_token) {
                ++snapshots;
                return chat::TabSnapshot{std::string(131070, 'a') + "中文 needle at the end", {{2, "Results"}}};
            }}};
            auto tools = chat::MakeTools(tabs, {});
            expect(snapshots == 0_i);
            expect(tools[0].invoke(Json::object(), {})["tabs"].size() == 1_u);
            auto hits = tools[2].invoke(Json{{"id", 1}, {"query", "中文 needle"}}, {});
            auto offset = hits["matches"].at(0).at("offset");
            expect(offset.get<std::size_t>() == 131070_u);
            expect(tools[1].invoke(Json{{"id", 1}, {"offset", offset}}, {})["text"] == "中文 needle at the end");
            expect(tools[3].invoke(Json{{"id", 1}}, {})["headings"].at(0)["heading"] == "Results");
            expect(snapshots == 1_i);
        };

        "agent streams native Markdown through unstable tails and preserves selection"_test = [] {
            EditorSession session;
            auto stream = session.BeginMarkdownStream();
            auto append = [&](std::string_view part) { session.AppendMarkdown(stream, part); };
            append("中文 **bold**\r\n\r\n");
            auto selection = session.Selection().active;
            selection.source_offset = 3;
            auto end = selection; end.source_offset = 11;
            session.SetSelection(selection, end);
            auto original = session.SelectedDisplayText();
            expect(!original.empty());
            std::string expected = "中文 **bold**\r\n\r\n";
            for (std::string_view part : {"## Ke", "ep\n\n", "| Name |", " Value |\n|---|---|", "\n| a | 2 |\n\n",
                    "$\nx^", "2\n$\n\n", "```cpp\na", "uto a=1;\n```\n"}) {
                append(part);
                expected += part;
                expect(session.SelectedDisplayText() == original);
            }
            expect(stream.accepted == stream.source.size());
            expect(session.TextUtf8() == expected);
            EditorSession reference;
            reference.SetText(winrt::to_hstring(expected));
            expect(session.RenderModel().blocks.size() == reference.RenderModel().blocks.size());
            session.JumpToSourceLocation({EditorSourceLocation::Kind::Line, 3});
            expect(session.Selection().active.container_id != selection.container_id);
            session.EnterSourceMode();
            session.JumpToSourceLocation({EditorSourceLocation::Kind::Utf8Byte, 6});
            expect(session.CaptureReloadViewState().active == 2_u);
        };

        "agent composer pastes a real bitmap and grants only selected mentions"_test = [] {
            try {
                using namespace winrt::Windows::ApplicationModel::DataTransfer;
                using winrt::Windows::System::VirtualKey;
                using winrt::Windows::Storage::Streams::RandomAccessStreamReference;
                auto shell = Access::Impl(visual_tests::Window());
                auto fixture = chat::NewConversation("Composer fixture");
                Access::Open(shell, fixture.id);
                auto host = Access::Host(shell);
                auto previous = host.Content();
                auto restore = wil::scope_exit([&] { host.Content(previous); Access::Close(shell); chat::DeleteConversation(fixture.id); });
                StackPanel composer;
                TextBox input;
                composer.Children().Append(input);
                host.Content(composer);
                Pump([&] { return input.IsLoaded(); });
                winrt::hstring notice;
                chat::OpenTab tab{"Project notes.md", 7, [](auto) { return chat::TabSnapshot{}; }};
                auto extras = chat::ChatComposerExtras::Create(input, [&] { return std::vector{tab}; },
                    [] { return std::filesystem::path{}; }, [&](auto const& message) { notice = message; });
                composer.Children().Append(extras->Element());
                std::string command;
                extras->command = [&](std::string const& value) { command = value; };
                input.Text(L"/mod"); input.Select(4, 0);
                expect(extras->HandleKey(VirtualKey::Enter)) << "slash accepts prefix";
                expect(command == "/model" && input.Text().empty()) << "slash clears input and chooses model";
                extras->BeginMention();
                expect(extras->HandleKey(VirtualKey::Tab)) << "mention accepts via Tab";
                expect(std::wstring_view(input.Text()).starts_with(L"@\"Project notes.md\"")) << winrt::to_string(input.Text());
                expect(extras->Mentions(tab)) << "explicit tab mention grants access";
                input.Text(L"@\"Project notes.md\"other");
                expect(!extras->Mentions(tab));
                auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                    / "src/app-winui/Assets/Square44x44Logo.scale-200.png";
                auto file = document_tests::Await(winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path.make_preferred().native()));
                DataPackage data;
                data.SetBitmap(RandomAccessStreamReference::CreateFromFile(file));
                extras->PasteImages(data.GetView());
                expect(notice == Localize(L"ChatNeedsVision"));
                extras->SetVision(true);
                extras->PasteImages(data.GetView());
                Pump([&] { return !extras->Pasting(); });
                expect(fatal(extras->Images().size() == 1_u));
                auto conversation = chat::NewConversation("Attachment test");
                auto cleanup = wil::scope_exit([&] { chat::DeleteConversation(conversation.id); });
                auto id = chat::StoreImage(conversation.id, extras->Images()[0].png);
                chat::Entry message{folia::ai::MessageRole::User, "describe"};
                message.images = {id};
                chat::AppendEntry(conversation.id, message);
                auto history = chat::LoadHistory(conversation.id);
                folia::ai::agent::Options options{"fixture"};
                options.loadImage = [&](auto const& stored) { return chat::LoadImage(conversation.id, stored); };
                bool imageSent = false;
                auto request = [&](folia::ai::ModelRequest const& wire, auto, auto const&) {
                    auto const& parts = wire.messages.back().content;
                    imageSent = parts.size() == 2 && parts.back().kind == folia::ai::ContentKind::Image;
                    folia::ai::ModelResult result;
                    result.message.content.push_back({folia::ai::ContentKind::Text, "Test"});
                    return result;
                };
                folia::ai::agent::Run(history, options, {}, request, {}, {});
                expect(imageSent);
                extras->Clear();
                expect(extras->Images().empty());
            } catch (winrt::hresult_error const& e) {
                throw std::runtime_error(winrt::to_string(e.message()));
            }
        };

        "agent journal pages older turns without gaps and restores the latest context checkpoint"_test = [] {
            auto conversation = chat::NewConversation("Journal test");
            auto cleanup = wil::scope_exit([&] { chat::DeleteConversation(conversation.id); });
            auto const& id = conversation.id;
            for (int t = 0; t < 11; ++t) {
                chat::AppendEntry(id, {folia::ai::MessageRole::User, std::to_string(t)});
                chat::Entry entry{folia::ai::MessageRole::Assistant, std::string(20000, 'a') + std::to_string(t)};
                entry.reasoning = "saved summary";
                entry.activities.push_back({"search_open_tab", "Notes", false, "match"});
                entry.steps.push_back({entry.markdown.size(), entry.reasoning.size(), 1});
                entry.usage.inputTokens = 201;
                if (t == 9) {
                    auto checkpoint = chat::LoadHistory(id);
                    checkpoint.summary = "Earlier turns";
                    checkpoint.summarized = 16;
                    chat::AppendEntry(id, entry, &checkpoint);
                } else chat::AppendEntry(id, entry);
            }
            auto recent = chat::ReadHistoryPage(id);
            expect(recent.entries.size() == 8_u && recent.entries.front().markdown == "7");
            auto older = chat::ReadHistoryPage(id, recent.before);
            expect(older.entries.size() == 8_u && older.entries.front().markdown == "3");
            auto first = chat::ReadHistoryPage(id, older.before);
            expect(first.entries.size() == 6_u && first.entries.front().markdown == "0" && first.before == 0);
            auto context = chat::LoadHistory(id);
            expect(context.entries.size() == 6_u && context.retired == 16_u);
            expect(context.summary == "Earlier turns" && context.entries.front().markdown == "8");
            expect(recent.entries.back().activities.front().detail == "match");
            expect(recent.entries.back().usage.inputTokens.value_or(0) == 201_u);
            context.summary = "All eleven turns";
            context.summarized = context.entries.size();
            chat::AppendCompaction(id, context);
            auto reopened = chat::LoadHistory(id);
            expect(reopened.entries.empty() && reopened.retired == 22_u && reopened.summary == "All eleven turns");
            auto visible = chat::ReadHistoryPage(id);
            expect(visible.entries.size() == 8_u && visible.compactions == std::vector<std::size_t>{8});
            chat::AppendEntry(id, {folia::ai::MessageRole::User, "Next"});
            expect(chat::LoadHistory(id).entries.front().markdown == "Next");
        };

        "agent transcript pages on demand and preserves the reading position during output"_test = [] {
            auto conversation = chat::NewConversation("Paging fixture");
            auto cleanup = wil::scope_exit([&] { chat::DeleteConversation(conversation.id); });
            std::string markdown;
            for (int paragraph = 0; paragraph < 24; ++paragraph) markdown += "A paragraph of the long reply.\n\n";
            for (int i = 0; i < 9; ++i) {
                chat::AppendEntry(conversation.id, {folia::ai::MessageRole::User, "Question " + std::to_string(i)});
                chat::AppendEntry(conversation.id, {folia::ai::MessageRole::Assistant, markdown});
            }
            auto shell = Access::Impl(visual_tests::Window());
            Access::Open(shell, conversation.id);
            auto host = Access::Host(shell);
            auto close = wil::scope_exit([&] { Access::Close(shell); });
            Pump([&] { return host.Content() != nullptr; });
            auto root = host.Content().as<Grid>();
            auto findText = [&](auto title) {
                return Find<TextBlock>(root, [&](auto const& item) { return item.Text() == title; });
            };
            auto action = [&](auto key) {
                return Find<Button>(root, [&](auto const& item) {
                    return Automation::AutomationProperties::GetName(item) == Localize(key);
                });
            };
            auto scroll = Find<ScrollViewer>(root, [](auto const& item) { return item.Name() == L"ConversationScroll"; });
            Pump([&] { return root.IsLoaded() && scroll.ScrollableHeight() > 150 && scroll.VerticalOffset() > 60; });
            expect(findText(L"Question 8") && !findText(L"Question 4"));
            Invoke(action(L"ChatOlderMessages"));
            Pump([&] { return findText(L"Question 4") && !findText(L"Question 8"); });
            Pump([&] { return scroll.VerticalOffset() < 1; });
            Capture(visual_tests::Window(), "history.png");
            expect(scroll.VerticalOffset() < 1.);
            auto firstQuestion = findText(L"Question 1");
            auto firstReply = Find<Canvas>(root, [](auto const& canvas) {
                return std::wstring_view(Automation::AutomationProperties::GetName(canvas)).starts_with(L"A paragraph");
            });
            if (!expect(firstQuestion && firstReply)) return;
            auto visible = [&](FrameworkElement const& item) {
                auto origin = item.TransformToVisual(scroll).TransformPoint({0,0});
                return origin.X >= 0 && origin.X + item.ActualWidth() <= scroll.ActualWidth() + 1 &&
                       origin.Y >= 0 && origin.Y < scroll.ActualHeight();
            };
            auto centered = [&] {
                auto left = firstReply.TransformToVisual(scroll).TransformPoint({0,0}).X;
                auto right = scroll.ActualWidth() - firstReply.ActualWidth() - left;
                return std::abs(left - right) < 3;
            };
            if (!visible(firstQuestion) || !visible(firstReply) || !centered()) {
                auto q = firstQuestion.TransformToVisual(scroll).TransformPoint({0,0});
                auto r = firstReply.TransformToVisual(scroll).TransformPoint({0,0});
                throw std::runtime_error(std::format("Page layout: viewport {}x{}, question ({},{}), reply ({},{}) width {}",
                    scroll.ActualWidth(), scroll.ActualHeight(), q.X, q.Y, r.X, r.Y, firstReply.ActualWidth()));
            }
            auto window = visual_tests::Window();
            auto originalSize = window.AppWindow().Size();
            window.AppWindow().Resize({1120, 850});
            Pump([&] { return scroll.ActualWidth() < 700 && centered() && visible(firstQuestion); });
            window.AppWindow().Resize(originalSize);
            Pump([&] { return scroll.ActualWidth() > 800 && centered(); });
            Invoke(action(L"ChatOlderMessages"));
            Pump([&] { return findText(L"Question 0") && !action(L"ChatOlderMessages").IsEnabled(); });
            Invoke(action(L"ChatLatest"));
            Pump([&] { return findText(L"Question 8") && scroll.VerticalOffset() > 60; });
            Pump([&] { root.UpdateLayout(); return scroll.ScrollableHeight() - scroll.VerticalOffset() < 2; });
            // Inject streaming events into a transcript mounted in the same window.
            auto transcript = chat::ChatTranscript::Create(conversation.id, Access::Theme(shell));
            host.Content(transcript->Element());
            root = transcript->Element();
            scroll = Find<ScrollViewer>(root, [](auto const& item) { return item.Name() == L"ConversationScroll"; });
            Pump([&] { return root.IsLoaded() && scroll.VerticalOffset() > 60; });
            scroll.ChangeView(nullptr, 0., nullptr, true);
            Pump([&] { return scroll.VerticalOffset() < 1 && action(L"ChatLatest").Visibility() == Visibility::Visible; });
            transcript->Begin();
            transcript->Progress(folia::ai::agent::ProgressKind::StepStarted, {});
            transcript->Progress(folia::ai::agent::ProgressKind::TextSnapshot, L"More output");
            root.UpdateLayout();
            expect(scroll.VerticalOffset() < 1.);
            transcript->Finish();
        };

        "agent activity stays between its text passages, live and after reopening"_test = [] {
            using enum folia::ai::agent::ProgressKind;
            auto window = visual_tests::Window();
            auto shell = Access::Impl(window);
            auto conversation = chat::NewConversation("Activity sequence");
            Access::Open(shell, conversation.id);
            auto host = Access::Host(shell);
            auto old = host.Content();
            auto cleanup = wil::scope_exit([&] { host.Content(old); Access::Close(shell); chat::DeleteConversation(conversation.id); });
            auto view = chat::ChatTranscript::Create(conversation.id, Access::Theme(shell));
            host.Content(view->Element());
            view->Begin();
            view->Progress(StepStarted, {});
            view->Progress(TextDelta, L"先查看笔记里对收敛性的说明。");
            view->Progress(Reading, L"read_workspace_file notes.md");
            auto root = Find<StackPanel>(host, [](auto const& e) { return e.Name() == L"ChatTurn"; });
            auto caption = [](DependencyObject const& element) {
                return Find<Button>(element, [](auto const& b) { return b.Name() == L"ChatActivitySummary"; });
            };
            Pump([&] {
                root = Find<StackPanel>(host, [](auto const& e) { return e.Name() == L"ChatTurn"; });
                return root && root.Children().Size() == 2 && caption(root);
            });
            expect(std::wstring_view(Automation::AutomationProperties::GetName(caption(root))).contains(L"notes.md"));
            Capture(window, "activity-running.png");
            view->Progress(ToolResult, L"{\"action\":\"read_workspace_file notes.md\",\"detail\":\"3. Convergence\"}");
            view->Progress(StepStarted, {});
            view->Progress(ReasoningSnapshot, L"核对笔记中的边界条件");
            view->Progress(TextDelta, L"## 结论\n\n这里需要有界性。");
            view->Progress(Reading, L"search_open_tab theorem.md · 有界性");
            view->Progress(ToolResult, L"{\"action\":\"search_open_tab theorem.md · 有界性\",\"detail\":\"line 17\"}");
            view->Progress(StepStarted, {});
            view->Progress(TextDelta, L"见文档第十七行。");
            chat::Entry reply{folia::ai::MessageRole::Assistant};
            auto first = winrt::to_string(L"先查看笔记里对收敛性的说明。");
            auto second = first + "\n\n" + winrt::to_string(L"## 结论\n\n这里需要有界性。");
            reply.markdown = second + "\n\n" + winrt::to_string(L"见文档第十七行。");
            reply.reasoning = winrt::to_string(L"核对笔记中的边界条件");
            reply.activities = {{"read_workspace_file", "notes.md", false, "3. Convergence"},
                {"search_open_tab", winrt::to_string(L"theorem.md · 有界性"), false, "line 17"}};
            auto n = reply.reasoning.size();
            reply.steps = {{first.size(),0,0},{first.size(),0,1},{first.size(),n,1},{second.size(),n,1},
                {second.size(),n,2},{reply.markdown.size(),n,2}};
            chat::AppendEntry(conversation.id, reply);
            view->Finish();
            auto check = [&](StackPanel const& turn) {
                auto children = turn.Children();
                if (!expect(children.Size() == 5_u)) return;
                expect(children.GetAt(0).try_as<Canvas>() && children.GetAt(2).try_as<Canvas>() && children.GetAt(4).try_as<Canvas>());
                auto firstTrace = children.GetAt(1);
                auto summary = caption(firstTrace);
                expect(std::wstring_view(Automation::AutomationProperties::GetName(summary)).contains(L"notes.md"));
                Invoke(summary);
                expect(Find<Button>(firstTrace, [](auto const& b) { return b.Name() == L"ChatActivityItem"; }) != nullptr);
                auto thought = Find<Button>(firstTrace, [](auto const& b) {
                    return b.Name() == L"ChatActivityItem" && Automation::AutomationProperties::GetName(b) == Localize(L"ChatThought");
                });
                if (expect(thought != nullptr)) {
                    Invoke(thought);
                    expect(Find<TextBlock>(firstTrace, [](auto const& t) {
                        return t.Name() == L"ChatActivityDetail" && t.Text() == L"核对笔记中的边界条件";
                    }) != nullptr);
                }
                auto lastTrace = children.GetAt(3);
                Invoke(caption(lastTrace));
                expect(Find<Button>(lastTrace, [](auto const& b) { return b.Name() == L"ChatActivityItem"; }) == nullptr);
                expect(Find<TextBlock>(lastTrace, [](auto const& t) { return t.Text() == L"line 17"; }) != nullptr);
            };
            host.UpdateLayout();
            check(root);
            auto reopened = chat::ChatTranscript::Create(conversation.id, Access::Theme(shell));
            host.Content(reopened->Element());
            Pump([&] { root = Find<StackPanel>(host, [](auto const& e) { return e.Name() == L"ChatTurn"; }); return root && root.IsLoaded(); });
            check(root);
            Capture(window, "activity-timeline.png");
        };

        "agent sessions share the preview tab until kept or edited"_test = [] {
            auto window = visual_tests::Window();
            auto shell = Access::Impl(window);
            Access::KeepActive(shell);
            auto first = chat::NewConversation("Preview");
            auto second = chat::NewConversation("Preview");
            auto cleanup = wil::scope_exit([&] {
                chat::DeleteConversation(first.id);
                chat::DeleteConversation(second.id);
            });
            Access::RefreshSidebar(shell);
            auto items = Access::Sidebar(shell).Items();
            unsigned matches = 0;
            for (std::uint32_t i = 0; i < items.Size(); ++i) {
                auto item = items.GetAt(i);
                if (item.as<winrt::Windows::Foundation::IStringable>().ToString() != L"Preview") continue;
                std::uint32_t index{};
                expect(items.IndexOf(item, index) && index == i);
                ++matches;
            }
            expect(matches >= 2_u);
            auto originalCount = Access::TabCount(shell);
            Access::Open(shell, first.id);
            auto host = Access::Host(shell);
            Pump([&] { host.UpdateLayout(); return host.IsLoaded() && host.Content() && host.ActualWidth() > 0; });
            expect(!Access::Active(shell)->kept);
            Access::Open(shell, second.id);
            auto secondTab = Access::Active(shell);
            expect(secondTab->chatId == second.id && !secondTab->kept);
            expect(Access::TabCount(shell) == originalCount + 1);
            Access::Open(shell, second.id, WorkspaceOpenMode::Keep);
            expect(Access::Active(shell) == secondTab && secondTab->kept);
            Access::Open(shell, first.id);
            auto firstTab = Access::Active(shell);
            expect(!firstTab->kept && Access::TabCount(shell) == originalCount + 2);
            TextBox input{nullptr};
            Pump([&] {
                host.UpdateLayout();
                input = Find<TextBox>(host, [](auto const& box) { return box.PlaceholderText() == Localize(L"ChatPrompt"); });
                return input && input.IsLoaded();
            });
            input.Text(L"An unsent draft");
            Pump([&] { return firstTab->kept; });
            Access::Open(shell, second.id);
            Access::Open(shell, first.id);
            expect(Access::Active(shell) == firstTab && input.Text() == L"An unsent draft");
            Access::CloseId(shell, firstTab->id);
            Access::CloseId(shell, secondTab->id);
        };

        "agent split toolbar opens and revisits the cited line beside the chat"_test = [] {
            auto window = visual_tests::Window();
            auto shell = Access::Impl(window);
            auto conversation = chat::NewConversation("Split reference");
            Access::Open(shell, conversation.id);
            auto chatTab = Access::Active(shell)->id;
            auto fixture = std::filesystem::temp_directory_path() / ("folia-citation-" + records::NewId() + ".md");
            { std::ofstream file(fixture); file << "First\n\nSecond\n\nThird"; }
            auto cleanup = wil::scope_exit([&] {
                Access::CloseId(shell, chatTab);
                chat::DeleteConversation(conversation.id);
                DeleteFileW(fixture.c_str());
            });
            auto chatHost = Access::Host(shell);
            Pump([&] { chatHost.UpdateLayout(); return chatHost.IsLoaded() && chatHost.Content() && chatHost.ActualWidth() > 0; });
            auto split = Find<Button>(chatHost, [](auto const& button) {
                return button.Visibility() == Visibility::Visible && Automation::AutomationProperties::GetName(button) == Localize(L"SplitEditor");
            });
            if (!expect(split != nullptr)) return;
            Invoke(split);
            Pump([&] { return Access::Split(shell) && Access::Companion(shell) == chatTab; });
            Access::OpenReference(shell, chatTab, {}, fixture, 3);
            try {
                Pump([&] {
                    auto doc = Access::Active(shell);
                    std::error_code ec;
                    return doc->editor.get() != nullptr && std::filesystem::equivalent(doc->path, fixture, ec) &&
                        doc->editor->loaded && !doc->editor->pendingLocation;
                });
            } catch (std::exception const&) {
                auto doc = Access::Active(shell);
                throw std::runtime_error(std::format("Citation load: expected {}, actual {}, loaded {}, opening {}, pending {}",
                    fixture.string(), doc->path.string(), static_cast<bool>(doc->editor) && doc->editor->loaded,
                    static_cast<bool>(doc->editor) && doc->editor->opening, static_cast<bool>(doc->editor) && doc->editor->pendingLocation.has_value()));
            }
            auto doc = Access::Active(shell);
            auto& session = doc->editor->session;
            expect(!doc->referenceKey.empty()) << "Newly opened editors can be cited immediately";
            expect(session.EditableSource(session.Selection().active.container_id) == U"Second") << "Open at line three";
            expect(Access::Companion(shell) == chatTab);
            auto id = doc->id;
            Access::OpenReference(shell, chatTab, doc->referenceKey, {}, 5);
            expect(Access::Active(shell)->id == id && Access::Companion(shell) == chatTab);
            expect(session.EditableSource(session.Selection().active.container_id) == U"Third") << "Revisit the displayed editor at line five";
            Capture(window, "split-reference.png");
            Access::CloseId(shell, id);
            if (Access::Split(shell)) Access::CloseRightView(shell);
            Pump([&] { return !Access::Split(shell); });
        };

        "agent keeps a dropped reply inline, streams the retry and preserves a manual title"_test = [] {
            std::string part = R"(data: {"choices":[{"index":0,"delta":{"content":"### First streamed heading\n\n"},"finish_reason":null}]})" "\n\n";
            std::string more = R"(data: {"choices":[{"index":0,"delta":{"content":"**Second part**"},"finish_reason":null}]})" "\n\n";
            std::string done = R"(data: {"choices":[{"index":0,"delta":{},"finish_reason":"stop"}]})" "\n\ndata: [DONE]\n\n";
            completion_tests::Server server(part, 200, false, "text/event-stream", std::chrono::milliseconds(250), part.size(),
                {"", part + more + done}, {503, 200});
            models::ModelProfile draft{{}, "Streaming test", server.baseUrl};
            draft.protocol = folia::ai::ApiProtocol::ChatCompletions;
            draft.models.push_back({{}, "Test stream", {}, "streaming-model"});
            draft.models.front().maxOutputTokens = 62000;
            auto profile = models::SaveProfile(std::move(draft), std::nullopt);
            auto conversation = chat::NewConversation("New");
            conversation.model = profile.models.front().id;
            chat::SaveConversationOptions(conversation);
            conversation = chat::RenameConversation(conversation.id, "Pinned title");
            auto window = visual_tests::Window();
            auto shell = Access::Impl(window);
            Access::Open(shell, conversation.id);
            auto cleanup = wil::scope_exit([&] { Access::Close(shell); chat::DeleteConversation(conversation.id); models::DeleteProfile(profile.id); });
            auto host = Access::Host(shell);
            Pump([&] { host.UpdateLayout(); return host.IsLoaded() && host.Content() && host.ActualWidth() > 0; });
            auto input = Find<TextBox>(host, [](auto const& e) { return e.PlaceholderText() == Localize(L"ChatPrompt"); });
            auto action = [&](auto name) { return Find<Button>(host, [&](auto const& e) {
                return e.Visibility() == Visibility::Visible && Automation::AutomationProperties::GetName(e) == Localize(name);
            }); };
            if (!expect(input != nullptr && action(L"ChatSend") != nullptr)) return;
            input.Text(L"Stream a heading");
            Invoke(action(L"ChatSend"));
            Pump([&] { return action(L"ChatRetry") != nullptr; });
            auto failed = chat::LoadHistory(conversation.id);
            expect(failed.entries.size() == 2_u);
            expect(!failed.entries.back().error.empty() && failed.entries.back().markdown.find("heading") != std::string::npos);
            auto note = Find<TextBlock>(host, [&](auto const& e) { return winrt::to_string(e.Text()) == failed.entries.back().error; });
            expect(note != nullptr);
            Capture(window, "interrupted.png");
            auto other = winrt::Folia::implementation::App::Current()->CreateMainWindow();
            other.Activate();
            auto receiver = Access::Impl(other);
            auto closeOther = wil::scope_exit([&] { other.Close(); });
            Pump([&] { return other.Content().as<FrameworkElement>().IsLoaded(); });
            window.Activate();
            Invoke(action(L"ChatRetry"));
            Pump([&] { return server.receivedRequests == 3 && action(L"ChatStop") != nullptr; });
            auto live = [&] { return Find<Canvas>(host, [](auto const& c) {
                return c.Name() == L"ChatMarkdownReply" && Automation::AutomationProperties::GetName(c).empty() && c.ActualHeight() > 10;
            }); };
            Pump([&] { return live() != nullptr && chat::LoadHistory(conversation.id).entries.size() == 2; });
            auto original = live();
            input.Text(L"Unsent follow-up");
            expect(Access::MoveActive(shell, receiver));
            host = Access::Host(receiver);
            other.Activate();
            Pump([&] { return input.IsLoaded() && input.XamlRoot() == host.XamlRoot(); });
            expect(input.Text() == L"Unsent follow-up");
            Capture(other, "streaming.png");
            Pump([&] { return action(L"ChatStop") == nullptr && chat::LoadHistory(conversation.id).entries.size() == 3; });
            expect(std::wstring_view(Automation::AutomationProperties::GetName(original)).contains(L"Second part"));
            const auto result = chat::LoadHistory(conversation.id);
            expect(result.entries.back().error.empty() && result.entries.back().markdown.find("Second part") != std::string::npos);
            expect(chat::LoadConversation(conversation.id).title == "Pinned title");
            expect(server.receivedRequests.load() == 3);
            expect(server.requests.back().find("62000") != std::string::npos);
            expect(Access::MoveActive(receiver, shell));
        };

        "agent chat opens a tab, calls the configured model, and reopens the saved conversation"_test = [] {
            completion_tests::Server server(R"({"id":"first","status":"completed","output":[{"type":"function_call",
                "call_id":"read-1","name":"list_open_tabs","arguments":"{}"}]})",
                200, false, "application/json", {}, 4096, {R"({"id":"answer","status":"completed",
                "usage":{"input_tokens":1864,"output_tokens":103},"output":[{"type":"reasoning","id":"r1",
                "summary":[{"type":"summary_text","text":"先查看打开的文档，再核对两种推导。"}]},
                {"type":"message","role":"assistant","content":[{"type":"output_text",
                "text":"### 等差数列的和\n\n把首尾两项配对，每一组都等于 **101**。一共有 50 组。\n\n$$\nS = 50 \\times 101 = 5050\n$$\n\n同样的思路也适用于任意等差数列。"}]}]})", R"({"id":"title","status":"completed","output":[{"type":"message","role":"assistant","content":[{"type":"output_text","text":"首尾配对求和"}]}]})", R"({"id":"summary","status":"completed","output":[{"type":"message","role":"assistant","content":[{"type":"output_text","text":"已解释首尾配对求和。"}]}]})"});
            models::ModelProfile draft{{}, "Loopback agent test", server.baseUrl + "/custom/complete"};
            draft.fullUrl = true;
            draft.allowWebSearch = true;
            draft.models.push_back({{}, "Notes", {}, "fixture", "low", true, {"low", "medium", "high", "xhigh", "max"}});
            draft.models.front().maxInputTokens = 128000;
            draft.models.push_back({{}, "Loopback plain model", {}, "fixture-plain"});
            auto profile = models::SaveProfile(std::move(draft), std::nullopt);
            auto const& model = profile.models.front();
            auto conversation = chat::NewConversation("Temporary conversation");
            auto cleanup = wil::scope_exit([&] {
                chat::DeleteConversation(conversation.id); models::DeleteProfile(profile.id);
            });
            conversation.model = model.id;
            conversation.effort = model.effort;
            conversation.search = true;
            conversation.openTabs = true;
            chat::SaveConversationOptions(conversation);

            auto window = visual_tests::Window();
            window.AppWindow().Resize({1960, 1200});
            auto shell = Access::Impl(window);
            Access::ShowSidebar(shell);
            Access::Open(shell, conversation.id);
            auto host = Access::Host(shell);
            Pump([&] {
                host.UpdateLayout();
                return host.IsLoaded() && static_cast<bool>(host.Content());
            });
            expect(Access::Active(shell)->kind == WorkspaceTabKind::Chat);
            auto sidebar = Access::Sidebar(shell);
            expect(sidebar.Items().Size() > 0_u);
            auto addChat = Find<Button>(window.Content(), [](auto const& button) { return button.Name() == L"NewChatButton"; });
            expect(addChat && Grid::GetColumn(addChat) == 1);
            Pump([&] { return sidebar.SelectedIndex() >= 0 && sidebar.ContainerFromIndex(sidebar.SelectedIndex()) != nullptr; });
            auto row = sidebar.ContainerFromIndex(sidebar.SelectedIndex()).as<ListViewItem>();
            Pump([&] { return Find<TextBlock>(row, [](auto const& text) { return text.Text() == L"Temporary conversation"; }) != nullptr; });
            auto menu = row.ContextFlyout().try_as<MenuFlyout>();
            if (expect(menu != nullptr) && expect(menu.Items().Size() == 2_u)) {
                expect(winrt::to_string(menu.Items().GetAt(0).as<MenuFlyoutItem>().Text()) == winrt::to_string(Localize(L"ChatRename")));
                expect(winrt::to_string(menu.Items().GetAt(1).as<MenuFlyoutItem>().Text()) == winrt::to_string(Localize(L"ChatDelete")));
            }
            auto input = Find<TextBox>(host, [](auto const& field) { return field.PlaceholderText() == Localize(L"ChatPrompt"); });
            auto send = Find<Button>(host, [](auto const& button) {
                return Automation::AutomationProperties::GetName(button) == Localize(L"ChatSend");
            });
            if (!expect(static_cast<bool>(input)) || !expect(static_cast<bool>(send))) return;
            Pump([&] {
                window.Content().as<FrameworkElement>().UpdateLayout();
                return host.ActualWidth() > 300 && input.ActualWidth() > 200 && send.ActualWidth() > 0;
            });
            winrt::Microsoft::UI::Xaml::Media::Imaging::RenderTargetBitmap image;
            auto chatPage = host.Content().as<Grid>();
            // RenderTargetBitmap does not include the Windows backdrop behind this page.
            auto oldBackground = chatPage.Background();
            chatPage.Background(Media::SolidColorBrush({255, 247, 248, 250}));
            document_tests::Await(image.RenderAsync(chatPage));
            chatPage.Background(oldBackground);
            auto pixels = document_tests::Await(image.GetPixelsAsync());
            visual_tests::SavePng(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                / "build/tests/agent-chat/composer.png", image.PixelWidth(), image.PixelHeight(), pixels);

            auto picker = Find<Button>(host, [](auto const& button) {
                return std::wstring_view(Automation::AutomationProperties::GetName(button)).starts_with(L"Notes");
            });
            if (!expect(picker != nullptr)) return;
            auto flyout = picker.Flyout().as<Flyout>();
            auto panel = flyout.Content().as<Grid>();
            auto reasoning = panel.FindName(L"ReasoningPanel").as<StackPanel>();
            auto modelsPanel = panel.FindName(L"ModelsPanel").as<StackPanel>();
            auto efforts = panel.FindName(L"Efforts").as<Slider>();
            flyout.ShowAt(picker);
            Pump([&] { return efforts.IsLoaded() && reasoning.Visibility() == Visibility::Visible; });
            expect(efforts.Maximum() == 4. && efforts.Value() == 0.);
            auto fill = panel.FindName(L"Fill").as<Border>();
            auto originalTheme = Access::Theme(shell);
            for (auto const& [accent, filename] : std::array{std::pair{folia::Color{174,64,74}, "accent-rose.png"},
                    std::pair{folia::Color{32,142,115}, "accent-jade.png"}}) {
                auto themed = originalTheme;
                themed.colors.shell_accent = accent;
                Access::ChatTheme(shell, themed);
                efforts.Value(2);
                panel.UpdateLayout();
                auto actual = fill.Background().as<Media::SolidColorBrush>().Color();
                expect(actual.R == accent.r && actual.G == accent.g && actual.B == accent.b);
                expect(send.Background().as<Media::SolidColorBrush>().Color() == actual);
                Capture(window, filename);
            }
            Access::ChatTheme(shell, originalTheme);
            auto knob = panel.FindName(L"Knob").as<Border>();
            auto thumbLayer = panel.FindName(L"ThumbLayer").as<Canvas>();
            for (auto const& [level, filename] : std::array{std::pair{0., "effort-first.png"},
                    std::pair{2., "effort-middle.png"}, std::pair{4., "effort-last.png"}}) {
                efforts.Value(level);
                panel.UpdateLayout();
                auto top = knob.TransformToVisual(thumbLayer).TransformPoint({0,0});
                auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(220);
                Pump([&] { return std::chrono::steady_clock::now() >= settled; });
                expect(top.X >= 0 && top.Y >= 1 && top.X + knob.ActualWidth() <= thumbLayer.ActualWidth() &&
                       top.Y + knob.ActualHeight() < thumbLayer.ActualHeight()) << "entire thumb fits inside its own layer at every stop";
                Media::Imaging::RenderTargetBitmap snapshot;
                document_tests::Await(snapshot.RenderAsync(panel));
                auto sliderPixels = document_tests::Await(snapshot.GetPixelsAsync());
                const auto point = fill.TransformToVisual(panel).TransformPoint({static_cast<float>(fill.ActualWidth() * .28), 5});
                const auto x = static_cast<int>(point.X * snapshot.PixelWidth() / panel.ActualWidth());
                const auto y = static_cast<int>(point.Y * snapshot.PixelHeight() / panel.ActualHeight());
                auto pixel = sliderPixels.data() + 4 * (y * snapshot.PixelWidth() + x);
                auto accent = fill.Background().as<Media::SolidColorBrush>().Color();
                const bool filled = std::abs(pixel[2]-int(accent.R)) + std::abs(pixel[1]-int(accent.G)) + std::abs(pixel[0]-int(accent.B)) < 9;
                expect(filled == (level > 0)) << "slider paints the selected fraction after settling at " << level;
                visual_tests::SavePng(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                    / "build/tests/agent-chat" / filename, snapshot.PixelWidth(), snapshot.PixelHeight(), sliderPixels);
            }
            bool closed = false;
            auto closedEvent = flyout.Closed(winrt::auto_revoke, [&](auto const&, auto const&) { closed = true; });
            efforts.Value(2);
            Pump([&] { return chat::LoadConversation(conversation.id).effort == "high" && flyout.IsOpen(); });
            expect(!closed);
            Invoke(panel.FindName(L"ChooseModel").as<Button>());
            Pump([&] { return modelsPanel.Visibility() == Visibility::Visible; });
            auto modelList = panel.FindName(L"ModelList").as<ListView>();
            auto selected = modelList.SelectedIndex();
            auto plainIndex = -1;
            for (std::uint32_t index = 0; index < modelList.Items().Size(); ++index) {
                auto item = modelList.Items().GetAt(index).as<ListViewItem>();
                if (Automation::AutomationProperties::GetName(item) == L"Loopback plain model")
                    plainIndex = static_cast<int>(index);
            }
            auto access = Find<Button>(host, [](auto const& button) {
                return Automation::AutomationProperties::GetName(button) == Localize(L"ChatAccess");
            });
            if (!expect(access != nullptr)) return;
            expect(static_cast<bool>(access.Content().try_as<TextBlock>())) << "permissions chip displays just its text label";
            auto accessMenu = access.Flyout().as<MenuFlyout>();
            auto allowSearch = accessMenu.Items().GetAt(2).as<ToggleMenuFlyoutItem>();
            if (!expect(plainIndex >= 0 && allowSearch)) return;
            modelList.SelectedIndex(plainIndex);
            expect(!allowSearch.IsEnabled() && !allowSearch.IsChecked());
            expect(chat::LoadConversation(conversation.id).search);
            modelList.SelectedIndex(selected);
            expect(allowSearch.IsEnabled() && allowSearch.IsChecked());
            efforts.Value(2);
            Capture(window, "model-picker.png");
            expect(reasoning.Visibility() == Visibility::Visible);
            closed = false;
            flyout.Hide();
            Pump([&] { return closed; });
            bool accessClosed = false;
            auto accessEvent = accessMenu.Closed(winrt::auto_revoke, [&](auto const&, auto const&) { accessClosed = true; });
            accessMenu.ShowAt(access);
            Pump([&] { return accessMenu.IsOpen() && allowSearch.IsLoaded() && allowSearch.ActualWidth() > 0; });
            auto captureMenu = [&](DependencyObject node, wchar_t const* filename) {
                while (node && !node.try_as<MenuFlyoutPresenter>()) node = Media::VisualTreeHelper::GetParent(node);
                Media::Imaging::RenderTargetBitmap bitmap;
                document_tests::Await(bitmap.RenderAsync(node.as<MenuFlyoutPresenter>()));
                auto pixels = document_tests::Await(bitmap.GetPixelsAsync());
                visual_tests::SavePng(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                    / "build/tests/agent-chat" / filename, bitmap.PixelWidth(), bitmap.PixelHeight(), pixels);
            };
            captureMenu(allowSearch, L"access.png");
            accessMenu.Hide();
            Pump([&] { return accessClosed; });
            auto mention = Find<Button>(host, [](auto const& item) {
                return Automation::AutomationProperties::GetName(item) == Localize(L"ChatAddContent");
            });
            auto attachments = mention.Flyout().as<MenuFlyout>();
            expect(attachments.Items().Size() == 2_u);
            auto reference = attachments.Items().GetAt(0).as<MenuFlyoutItem>();
            auto addImage = attachments.Items().GetAt(1).as<MenuFlyoutItem>();
            attachments.ShowAt(mention);
            Pump([&] { return reference.IsLoaded() && reference.ActualWidth() > 0; });
            expect(reference.KeyboardAcceleratorTextOverride() == L"@" && !std::wstring_view(reference.Text()).contains(L'@'));
            expect(!addImage.IsEnabled());
            captureMenu(reference, L"add-content.png");
            attachments.Hide();
            Pump([&] { return !attachments.IsOpen(); });
            input.Text(L"@"); input.Select(1, 0);
            auto suggestions = Find<ListView>(host, [](auto const& item) { return item.Name() == L"ChatMentionList"; });
            Pump([&] { return suggestions && suggestions.Visibility() == Visibility::Visible && suggestions.Items().Size(); });
            Capture(window, "mentions.png");
            input.Text({});
            Pump([&] { return suggestions.Visibility() == Visibility::Collapsed; });
            input.Text(L"请用首尾配对解释从 1 加到 100 的结果。");
            input.Select(input.Text().size(), 0);
            input.Focus(FocusState::Programmatic);
            auto focus = GetFocus();
            if (!focus) throw std::runtime_error("No keyboard focus in test window");
            PostMessageW(focus, WM_KEYDOWN, VK_RETURN, 0x001C0001);
            PostMessageW(focus, WM_KEYUP, VK_RETURN, 0xC01C0001);
            try {
                Pump([&] { return chat::LoadHistory(conversation.id).entries.size() == 2; });
            } catch (std::exception const&) {
                throw std::runtime_error(std::format("Enter: requests {}, messages {}, text length {}, new line {}, keyboard focus {}, suggestions {}, shift {}",
                    server.receivedRequests.load(), chat::LoadHistory(conversation.id).entries.size(), input.Text().size(),
                    std::wstring_view(input.Text()).find_first_of(L"\r\n") != std::wstring_view::npos,
                    GetFocus() == focus, suggestions.Visibility() == Visibility::Visible, GetKeyState(VK_SHIFT) < 0));
            }
            auto history = chat::LoadHistory(conversation.id);
            expect(history.entries.back().markdown.find("**101**") != std::string::npos);
            expect(history.entries.back().reasoning.find("核对") != std::string::npos);
            expect(history.entries.back().activities.size() == 1_u);
            expect(history.entries.back().usage.inputTokens.value_or(0) == 1864_u);
            auto bubble = Find<Border>(host, [](auto const& item) {
                auto style = Application::Current().Resources().Lookup(winrt::box_value(L"ChatUserBubbleStyle"));
                return item.Style() == style;
            });
            expect(bubble != nullptr);
            auto scroll = Find<ScrollViewer>(host, [](auto const& item) { return item.Name() == L"ConversationScroll"; });
            expect(scroll != nullptr);
            auto surface = [&] { return Find<Canvas>(host, [](auto const& canvas) {
                return std::wstring_view(Automation::AutomationProperties::GetName(canvas)).contains(L"101");
            }); };
            Pump([&] { host.UpdateLayout(); return surface() && surface().Height() > 40; });
            auto thinking = Find<Button>(host, [](auto const& button) {
                return button.Name() == L"ChatActivitySummary";
            });
            if (expect(thinking != nullptr)) {
                Invoke(thinking);
                Capture(window, "activity-list.png");
                auto nested = Find<Button>(host, [&](auto const& button) { return button.Name() == L"ChatActivityItem" &&
                    std::wstring_view(Automation::AutomationProperties::GetName(button)).contains(L"核对"); });
                if (expect(nested != nullptr)) Invoke(nested);
                auto details = Find<TextBlock>(host, [](auto const& text) { return std::wstring_view(text.Text()).contains(L"先查看"); });
                Pump([&] { return details && details.Visibility() == Visibility::Visible; });
            }
            Capture(window, "conversation.png");
            Access::Dark(shell);
            window.AppWindow().Resize({1420, 1080});
            Capture(window, "conversation-dark.png");
            input.Text(L"第一行");
            input.Select(input.Text().size(), 0);
            input.Focus(FocusState::Programmatic);
            focus = GetFocus();
            {
                BYTE original[256]{}, shifted[256]{};
                winrt::check_bool(GetKeyboardState(original));
                std::ranges::copy(original, std::begin(shifted));
                shifted[VK_SHIFT] |= 0x80;
                winrt::check_bool(SetKeyboardState(shifted));
                auto restoreKeys = wil::scope_exit([&] { SetKeyboardState(original); });
                PostMessageW(focus, WM_KEYDOWN, VK_RETURN, 0x001C0001);
                PostMessageW(focus, WM_KEYUP, VK_RETURN, 0xC01C0001);
                Pump([&] { return std::wstring_view(input.Text()).find_first_of(L"\r\n") != std::wstring_view::npos; });
                expect(chat::LoadHistory(conversation.id).entries.size() == 2_u);
            }
            Pump([&] { return chat::LoadConversation(conversation.id).title == "首尾配对求和"; });
            expect(server.receivedRequests.load() == 3_i);
            expect(server.request.starts_with("POST /custom/complete HTTP/1.1"));
            expect(server.requests.at(0).find("\"type\":\"web_search\"") != std::string::npos);
            input.Text(L"/compact");
            input.Select(input.Text().size(), 0);
            input.Focus(FocusState::Programmatic);
            PostMessageW(GetFocus(), WM_KEYDOWN, VK_RETURN, 0x001C0001);
            PostMessageW(GetFocus(), WM_KEYUP, VK_RETURN, 0xC01C0001);
            Pump([&] { return chat::LoadHistory(conversation.id).summary == "已解释首尾配对求和。"; });
            auto compacted = chat::ReadHistoryPage(conversation.id);
            expect(compacted.entries.size() == 2_u && compacted.compactions == std::vector<std::size_t>{2});
            expect(server.receivedRequests.load() == 4_i);
            Access::Close(shell);
            Pump([&] { return Access::Active(shell)->kind == WorkspaceTabKind::Editor; });
            Access::Open(shell, conversation.id);
            Pump([&] { return host.Content() != nullptr; });
            expect(Access::Active(shell)->chatId == conversation.id);
            expect(chat::LoadConversation(conversation.id).title == "首尾配对求和");
            Access::Close(shell);
        };
    };
}
