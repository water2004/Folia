#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "workspace/FolderExplorerController.h"
#include "visual_window.h"
#include "workspace/FolderExplorerView.h"
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;

    struct ExplorerWindow {
        Grid root{nullptr};
        Border host;
        Border header;
        TreeView tree;
        winrt::Folia::FolderExplorerController explorer;
        TestDirectory directory;
        std::filesystem::path folder = directory.path / L"Examples";
        std::wstring longName = std::wstring(50, L'长') + L"-notes.markdown";
        std::filesystem::path opened;
        bool kept = false;
        explicit ExplorerWindow() {
            root = visual_tests::Window().Content().as<Grid>();
            Pump([&] { return root.IsLoaded(); });
            std::filesystem::create_directory(folder);
            std::filesystem::create_directory(folder / L"章节");
            std::ofstream(folder / L"章节" / L"inside.md") << "inside";
            std::filesystem::create_directories(folder / L"章节" / L"小节" / L"更深");
            std::ofstream(folder / L"章节" / L"小节" / L"更深" / (std::wstring(24, L'深') + L".md")) << "leaf";
            std::ofstream(folder / L"正常.md") << "normal";
            std::ofstream(folder / L"空格 & emoji😀.md") << "unicode";
            std::ofstream(folder / longName) << "long";
            tree.Style(root.Resources().Lookup(winrt::box_value(L"ExplorerTreeStyle")).as<Style>());
            host.Width(248);
            host.Height(360);
            host.HorizontalAlignment(HorizontalAlignment::Left);
            host.VerticalAlignment(VerticalAlignment::Top);
            host.Background(root.Resources().Lookup(winrt::box_value(L"FoliaShellLayerBrush")).as<Media::Brush>());
            host.Child(tree);
            Grid::SetRow(host, 3);
            root.Children().Append(host);
            explorer.Attach(tree, [this](auto path, bool keep) { opened = path; kept = keep; }, {}, header);
            explorer.Open(folder);
            Pump([&] { return tree.RootNodes().Size() == 4; });
            root.UpdateLayout();
        }
        ~ExplorerWindow() {
            explorer.Detach();
            std::uint32_t index = 0;
            if (root.Children().IndexOf(host, index)) root.Children().RemoveAt(index);
        }
        TreeViewNode Find(std::wstring_view name) {
            for (auto node : tree.RootNodes())
                if (node.Content().as<winrt::Windows::Foundation::IStringable>().ToString() == name) return node;
            throw std::runtime_error("expected explorer node not found");
        }
        Input::ICommand HeaderCommand(wchar_t const* name) {
            auto content = header.DataContext();
            return content.as<Data::ICustomPropertyProvider>().GetCustomProperty(name).GetValue(content).as<Input::ICommand>();
        }
        Grid Row(TreeViewNode const& node) {
            Pump([&] { return tree.ContainerFromNode(node) != nullptr; });
            auto item = tree.ContainerFromNode(node).as<TreeViewItem>();
            item.ApplyTemplate();
            root.UpdateLayout();
            Pump([&] { return item.ContentTemplateRoot() != nullptr; });
            return item.ContentTemplateRoot().as<Grid>();
        }
    };

    suite explorer_window_tests = [] {
        "folder explorer window binds actual names and bounds long rows"_test = [] { try {
            ExplorerWindow f;
            expect(f.header.DataContext().as<winrt::Windows::Foundation::IStringable>().ToString() == L"Examples");
            expect(f.tree.RootNodes().Size() == 4_u);
            auto normal = f.Row(f.Find(L"正常.md")).FindName(L"EntryName").as<TextBlock>();
            expect(normal.Text() == L"正常.md");
            expect(f.Row(f.Find(L"空格 & emoji😀.md")).FindName(L"EntryName").as<TextBlock>().Text() == L"空格 & emoji😀.md");
            auto node = f.Find(f.longName);
            auto row = f.Row(node);
            auto label = row.FindName(L"EntryName").as<TextBlock>();
            expect(label.Text() == f.longName);
            expect(node.Depth() == 0_i);
            expect(label.TextTrimming() == TextTrimming::CharacterEllipsis);
            expect(label.TextWrapping() == TextWrapping::NoWrap);
            auto tooltip = ToolTipService::GetToolTip(row).as<winrt::Windows::Foundation::IPropertyValue>().GetString();
            expect(tooltip == winrt::hstring((f.folder / f.longName).c_str()));
            double previousWidth = 0;
            for (double width : {160., 248., 360.}) {
                f.host.Width(width);
                f.root.UpdateLayout();
                auto at = label.TransformToVisual(f.host).TransformPoint({0, 0});
                expect(label.ActualWidth() > previousWidth);
                expect(at.X + label.ActualWidth() <= width + 0.5);
                expect(label.IsTextTrimmed());
                expect(f.tree.ContainerFromNode(node).as<TreeViewItem>().ActualHeight() == 29.);
                previousWidth = label.ActualWidth();
            }
            f.host.Width(248);
            f.tree.SelectedNode(f.Find(L"正常.md"));
            f.root.UpdateLayout();
            auto selected = f.tree.ContainerFromNode(f.tree.SelectedNode()).as<TreeViewItem>();
            // TreeViewItem shares SelectorItem's native state names, unlike
            // the theme brush keys (BackgroundSelectedPointerOver, etc.).
            for (auto state : {L"Selected", L"PointerOverSelected", L"PressedSelected"})
                expect(VisualStateManager::GoToState(selected, state, false)) << winrt::to_string(state);
            VisualStateManager::GoToState(selected, L"Selected", false);
            Media::Imaging::RenderTargetBitmap bitmap;
            Await(bitmap.RenderAsync(f.host));
            auto pixels = Await(bitmap.GetPixelsAsync());
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-explorer";
            visual_tests::SavePng(artifacts / L"files-native.png", bitmap.PixelWidth(), bitmap.PixelHeight(), pixels);
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer window uses eight DIP levels and a compact chevron"_test = [] { try {
            ExplorerWindow f;
            auto nested = f.Find(L"章节");
            auto folderRow = f.Row(nested);
            auto folderName = folderRow.FindName(L"EntryName").as<TextBlock>();
            auto x = [&](FrameworkElement const& element) { return element.TransformToVisual(f.tree).TransformPoint({0, 0}).X; };
            const auto firstRow = x(folderRow);
            expect(firstRow <= 24.f);
            expect(std::abs(x(folderName) - firstRow - 20.f) < 0.5f) << "folder icon occupies its own slot";
            auto node = nested;
            for (int depth = 1; depth <= 3; ++depth) {
                f.Row(node);
                Automation::Peers::TreeViewItemAutomationPeer peer(f.tree.ContainerFromNode(node).as<TreeViewItem>());
                peer.as<Automation::Provider::IExpandCollapseProvider>().Expand();
                Pump([&] { return node.IsExpanded() && !node.HasUnrealizedChildren(); });
                f.root.UpdateLayout();
                expect(std::abs(x(f.Row(node)) - firstRow - 8.f * (depth - 1)) < 0.5f) << "indent follows depth" << depth;
                if (depth < 3) node = node.Children().GetAt(0);
            }
            auto leaf = node.Children().GetAt(0);
            auto leafName = f.Row(leaf).FindName(L"EntryName").as<TextBlock>();
            auto firstLevelFile = f.Row(f.Find(L"正常.md")).FindName(L"EntryName").as<TextBlock>();
            expect(x(firstLevelFile) <= 50.f);
            expect(std::abs(x(leafName) - x(firstLevelFile) - 24.f) < 0.5f);
            f.host.Width(160);
            f.root.UpdateLayout();
            expect(x(leafName) + leafName.ActualWidth() <= 160.5);
            expect(leafName.ActualWidth() > 60.);
            expect(leafName.IsTextTrimmed());
            f.host.Width(248);
            f.root.UpdateLayout();
            Media::Imaging::RenderTargetBitmap bitmap;
            Await(bitmap.RenderAsync(f.host));
            auto pixels = Await(bitmap.GetPixelsAsync());
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-explorer";
            visual_tests::SavePng(artifacts / L"compact-deep-native.png", bitmap.PixelWidth(), bitmap.PixelHeight(), pixels);
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer window exposes native root actions and lazy expansion"_test = [] { try {
            ExplorerWindow f;
            expect(f.tree.ItemContainerTransitions().Size() == 1_u);
            auto entrance = f.tree.ItemContainerTransitions().GetAt(0).as<Media::Animation::EntranceThemeTransition>();
            expect(entrance.FromHorizontalOffset() == 0.);
            expect(entrance.FromVerticalOffset() == 0.);
            auto nested = f.Find(L"章节");
            expect(nested.HasUnrealizedChildren());
            expect(nested.Children().Size() == 0_u);
            f.explorer.Expand(nested);
            Pump([&] { return !nested.HasUnrealizedChildren(); });
            expect(nested.Children().Size() == 2_u);
            auto refresh = f.HeaderCommand(L"Refresh");
            auto collapse = f.HeaderCommand(L"Collapse");
            expect(refresh.CanExecute(nullptr));
            expect(collapse.CanExecute(nullptr));
            collapse.Execute(nullptr);
            expect(!nested.IsExpanded());
            expect(f.tree.RootNodes().Size() == 4_u);
            f.explorer.Expand(nested);
            auto selected = f.Find(L"正常.md");
            f.tree.SelectedNode(selected);
            std::ofstream(f.folder / L"new.md") << "new";
            refresh.Execute(nullptr);
            Pump([&] { return f.tree.RootNodes().Size() == 5; });
            expect(nested.IsExpanded());
            expect(f.Find(L"章节") == nested);
            expect(f.tree.SelectedNode() == selected);
            expect(f.header.DataContext().as<winrt::Windows::Foundation::IStringable>().ToString() == L"Examples");
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer bounds native rows for large levels and releases collapsed children"_test = [] {
            ExplorerWindow f;
            auto folder = f.directory.path / L"Large";
            auto child = folder / L"Nested";
            std::filesystem::create_directories(child);
            constexpr std::uint32_t count = 4096;
            for (std::uint32_t i = 0; i < count; ++i) {
                std::ofstream file(folder / (std::to_wstring(i) + L".md"));
            }
            std::ofstream(child / L"before.md") << "before";
            f.explorer.Open(folder);
            auto rows = f.tree.RootNodes();
            auto isLink = [](TreeViewNode const& node) { return bool(node.Content().try_as<Input::ICommand>()); };
            Pump([&] { return rows.Size() > 0 && isLink(rows.GetAt(rows.Size() - 1)); });
            auto firstPageSize = rows.Size();
            expect(firstPageSize < count) << "the first page uses fewer native nodes than the directory";
            auto nested = f.Find(L"Nested");
            expect(rows.GetAt(0) == nested) << "folders remain before files";
            f.explorer.Expand(nested);
            Pump([&] { return nested.Children().Size() == 1; });
            nested.IsExpanded(false);
            expect(nested.Children().Size() == 0_u && nested.HasUnrealizedChildren()) << "collapse releases the child nodes";
            std::ofstream(child / L"after.md") << "after";
            f.explorer.Expand(nested);
            Pump([&] { return nested.Children().Size() == 2 && !nested.HasUnrealizedChildren(); });
            auto next = rows.GetAt(rows.Size() - 1);
            winrt::Folia::ScrollFolderExplorerNodeIntoView(f.tree, next);
            Button nextButton{nullptr};
            Pump([&] {
                if (auto item = f.tree.ContainerFromNode(next).try_as<TreeViewItem>())
                    nextButton = item.ContentTemplateRoot().try_as<Button>();
                return bool(nextButton);
            });
            Automation::Peers::ButtonAutomationPeer(nextButton).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return rows.Size() > 0 && isLink(rows.GetAt(0)); });
            expect(rows.Size() == firstPageSize + 1) << "a middle page adds one previous-page button";
            Pump([&] { return f.tree.ContainerFromNode(rows.GetAt(1)) != nullptr; });
            std::uint32_t oldIndex = 0;
            expect(!rows.IndexOf(nested, oldIndex)) << "the previous page is detached";
            rows.GetAt(0).Content().as<Input::ICommand>().Execute(nullptr);
            Pump([&] { return rows.Size() == firstPageSize && !isLink(rows.GetAt(0)); });
            expect(f.Find(L"Nested").HasUnrealizedChildren()) << "returning to a page reloads directory nodes lazily";

            rows.GetAt(rows.Size() - 1).Content().as<Input::ICommand>().Execute(nullptr);
            Pump([&] { return isLink(rows.GetAt(0)); });
            auto file = rows.GetAt(1);
            f.tree.SelectedNode(file);
            f.explorer.RenameSelected();
            expect(Await(f.explorer.CommitNameAsync(L"000-first.md"))) << "a file can be renamed across pages";
            Pump([&] { return !isLink(rows.GetAt(0)) && f.tree.SelectedNode() == file; });
            expect(f.Find(L"000-first.md") == file) << "the renamed node retains its identity and selection";
        };

        "folder explorer window edits names in place and preserves rows on failed creation"_test = [] { try {
            ExplorerWindow f;
            auto newFile = f.HeaderCommand(L"NewFile");
            newFile.Execute(nullptr);
            Pump([&] { return f.explorer.IsEditing(); });
            auto draft = f.tree.SelectedNode();
            auto editor = f.Row(draft).FindName(L"EntryEditor").as<TextBox>();
            expect(winrt::to_string(editor.Text()) == winrt::to_string(winrt::Folia::Localize(L"UntitledMarkdown")));
            expect(!newFile.CanExecute(nullptr));
            expect(!Await(f.explorer.CommitNameAsync(L"../bad.md")));
            expect(f.explorer.IsEditing());
            expect(editor.IsEnabled());
            expect(!Await(f.explorer.CommitNameAsync(L"正常.md")));
            expect(f.explorer.IsEditing());
            expect(std::filesystem::file_size(f.folder / L"正常.md") == 6_u);
            expect(Await(f.explorer.CommitNameAsync(L"created.md")));
            Pump([&] { return f.tree.SelectedNode() == draft; });
            expect(std::filesystem::exists(f.folder / L"created.md"));
            expect(f.opened == f.folder / L"created.md" && f.kept);
            expect(!f.explorer.IsEditing());
            expect(f.Find(L"created.md") == draft);
            f.tree.SelectedNode(draft);
            f.explorer.RenameSelected();
            expect(f.explorer.IsEditing());
            expect(f.Row(draft).FindName(L"EntryEditor").try_as<TextBox>() != nullptr);
            expect(Await(f.explorer.CommitNameAsync(L"renamed.md")));
            expect(!std::filesystem::exists(f.folder / L"created.md"));
            expect(f.Find(L"renamed.md") == draft);
            f.tree.SelectedNode(nullptr);
            f.explorer.NewFolder();
            Pump([&] { return f.explorer.IsEditing() && f.tree.SelectedNode() != nullptr; });
            auto folderDraft = f.tree.SelectedNode();
            expect(f.Row(folderDraft).FindName(L"EntryEditor").try_as<TextBox>() != nullptr);
            f.explorer.CancelNameEdit();
            expect(!f.explorer.IsEditing());
            f.explorer.NewFolder();
            Pump([&] { return f.explorer.IsEditing() && f.tree.SelectedNode() != nullptr; });
            expect(Await(f.explorer.CommitNameAsync(L"New directory")));
            expect(std::filesystem::is_directory(f.folder / L"New directory"));
            expect(f.tree.CanDragItems() && !f.tree.CanReorderItems() && f.tree.AllowDrop());
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer window moves and copies while retaining expanded subtree identity"_test = [] { try {
            ExplorerWindow f;
            expect(f.tree.ItemContainerTransitions().Size() == 1_u);
            auto entrance = f.tree.ItemContainerTransitions().GetAt(0).as<Media::Animation::EntranceThemeTransition>();
            expect(entrance.FromHorizontalOffset() == 0.);
            expect(entrance.FromVerticalOffset() == 0.);
            auto nested = f.Find(L"章节");
            f.explorer.Expand(nested);
            Pump([&] { return !nested.HasUnrealizedChildren(); });
            auto leaf = nested.Children().GetAt(1);
            auto target = f.folder / L"destination";
            std::filesystem::create_directory(target);
            f.explorer.Refresh();
            Pump([&] { return f.tree.RootNodes().Size() == 5; });
            auto destination = f.Find(L"destination");
            auto result = Await(f.explorer.TransferAsync(f.folder / L"章节", target, true, false));
            Pump([&] { return !destination.HasUnrealizedChildren(); });
            expect(std::filesystem::is_directory(target / L"章节"));
            expect(destination.Children().GetAt(0) == nested);
            expect(nested.IsExpanded());
            expect(nested.Children().GetAt(1) == leaf);
            expect(leaf.Content().as<winrt::Windows::Foundation::IStringable>().ToString() == L"inside.md");
            auto copied = Await(f.explorer.TransferAsync(f.folder / L"正常.md", f.folder, false, true));
            expect(std::filesystem::exists(std::filesystem::path(copied.c_str())));
            expect(std::filesystem::exists(f.folder / L"正常.md"));
            expect(std::filesystem::path(copied.c_str()) != f.folder / L"正常.md");
            bool rejected = false;
            try { Await(f.explorer.TransferAsync(target, target / L"章节", true, false)); }
            catch (winrt::hresult_error const&) { rejected = true; }
            expect(rejected);
            expect(!f.explorer.IsBusy());
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer window draws hover guides from realized tree depth"_test = [] { try {
            ExplorerWindow f;
            auto folder = f.Find(L"章节");
            f.explorer.Expand(folder);
            Pump([&] { return !folder.HasUnrealizedChildren(); });
            f.root.UpdateLayout();
            auto settle = std::chrono::steady_clock::now() + std::chrono::milliseconds(450);
            Pump([&] { return std::chrono::steady_clock::now() >= settle; });
            winrt::Folia::UpdateFolderExplorerGuides(f.tree, folder, true);
            int guides = 0, emphasized = 0;
            auto visit = [&](auto const& self, DependencyObject const& object) -> void {
                if (auto canvas = object.try_as<Canvas>(); canvas && canvas.Name() == L"IndentGuides") {
                    if (canvas.Visibility() == Visibility::Visible) {
                        ++guides;
                        for (auto line : canvas.Children()) if (std::abs(line.Opacity() - .4) < .001) ++emphasized;
                    }
                    return;
                }
                for (int n = 0; n < Media::VisualTreeHelper::GetChildrenCount(object); ++n) self(self, Media::VisualTreeHelper::GetChild(object, n));
            };
            visit(visit, f.tree);
            expect(guides == 2_i);
            expect(emphasized == 2_i) << "expanded folder highlights its child branch";
            auto collapsed = folder.Children().GetAt(0);
            expect(!collapsed.IsExpanded());
            winrt::Folia::UpdateFolderExplorerGuides(f.tree, collapsed, true);
            guides = emphasized = 0; visit(visit, f.tree);
            expect(emphasized == 2_i) << "collapsed folder uses parent branch";
            winrt::Folia::UpdateFolderExplorerGuides(f.tree, folder, false);
            guides = emphasized = 0; visit(visit, f.tree);
            expect(guides == 2_i && emphasized == 2_i) << "leaving hover retains only active branch";
            winrt::Folia::UpdateFolderExplorerGuides(f.tree, folder, true);
            Media::Imaging::RenderTargetBitmap bitmap;
            Await(bitmap.RenderAsync(f.host));
            auto pixels = Await(bitmap.GetPixelsAsync());
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-explorer";
            visual_tests::SavePng(artifacts / L"hover-guides-native.png", bitmap.PixelWidth(), bitmap.PixelHeight(), pixels);
            winrt::Folia::UpdateFolderExplorerGuides(f.tree, nullptr, false);
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer updates active guides as offscreen rows are realized"_test = [] {
            ExplorerWindow f;
            auto folder = f.Find(L"章节");
            for (unsigned i = 0; i < 100; ++i) {
                std::ofstream file(f.folder / L"章节" / (std::to_wstring(i) + L".md"));
            }
            f.explorer.Expand(folder);
            Pump([&] { return !folder.HasUnrealizedChildren(); });
            f.tree.SelectedNode(folder);
            auto child = folder.Children().GetAt(folder.Children().Size() - 1);
            expect(f.tree.ContainerFromNode(child) == nullptr);
            winrt::Folia::ScrollFolderExplorerNodeIntoView(f.tree, child);
            Pump([&] {
                auto row = f.tree.ContainerFromNode(child).try_as<TreeViewItem>();
                auto guides = row ? row.as<IControlProtected>().GetTemplateChild(L"IndentGuides").try_as<Canvas>() : nullptr;
                return guides && guides.Visibility() == Visibility::Visible && guides.Children().Size() == 1
                    && std::abs(guides.Children().GetAt(0).Opacity() - .4) < .001;
            });
        };
    };
}
