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
            explorer.Attach(tree, [this](auto path, bool keep) { opened = path; kept = keep; }, {});
            explorer.Open(folder);
            Pump([&] { return !tree.RootNodes().GetAt(0).HasUnrealizedChildren(); });
            root.UpdateLayout();
        }
        ~ExplorerWindow() {
            explorer.Detach();
            std::uint32_t index = 0;
            if (root.Children().IndexOf(host, index)) root.Children().RemoveAt(index);
        }
        TreeViewNode Find(std::wstring_view name) {
            for (auto node : tree.RootNodes().GetAt(0).Children())
                if (node.Content().as<winrt::Windows::Foundation::IStringable>().ToString() == name) return node;
            throw std::runtime_error("expected explorer node not found");
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
            auto rootNode = f.tree.RootNodes().GetAt(0);
            auto rootRow = f.Row(rootNode);
            auto rootName = rootRow.FindName(L"EntryName").as<TextBlock>();
            expect(rootName.Text() == L"Examples");
            auto normal = f.Row(f.Find(L"正常.md")).FindName(L"EntryName").as<TextBlock>();
            expect(normal.Text() == L"正常.md");
            expect(f.Row(f.Find(L"空格 & emoji😀.md")).FindName(L"EntryName").as<TextBlock>().Text() == L"空格 & emoji😀.md");
            auto node = f.Find(f.longName);
            auto row = f.Row(node);
            auto label = row.FindName(L"EntryName").as<TextBlock>();
            expect(label.Text() == f.longName);
            expect(f.tree.RootNodes().GetAt(0).IsExpanded());
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
                expect(f.tree.ContainerFromNode(node).as<TreeViewItem>().ActualHeight() == 24.);
                previousWidth = label.ActualWidth();
            }
            f.host.Width(248);
            f.tree.SelectedNode(f.Find(L"正常.md"));
            f.root.UpdateLayout();
            Media::Imaging::RenderTargetBitmap bitmap;
            Await(bitmap.RenderAsync(f.host));
            auto pixels = Await(bitmap.GetPixelsAsync());
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-explorer";
            visual_tests::SavePng(artifacts / L"files-native.png", bitmap.PixelWidth(), bitmap.PixelHeight(), pixels);
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer window uses eight DIP levels and a compact chevron"_test = [] { try {
            ExplorerWindow f;
            auto rootNode = f.tree.RootNodes().GetAt(0);
            auto rootName = f.Row(rootNode).FindName(L"EntryName").as<TextBlock>();
            auto nested = f.Find(L"章节");
            auto folderName = f.Row(nested).FindName(L"EntryName").as<TextBlock>();
            auto x = [&](TextBlock const& text) { return text.TransformToVisual(f.tree).TransformPoint({0, 0}).X; };
            expect(x(rootName) <= 24.f);
            expect(std::abs(x(folderName) - x(rootName) - 8.f) < 0.5f);
            auto node = nested;
            for (int depth = 1; depth <= 3; ++depth) {
                f.Row(node);
                Automation::Peers::TreeViewItemAutomationPeer peer(f.tree.ContainerFromNode(node).as<TreeViewItem>());
                peer.as<Automation::Provider::IExpandCollapseProvider>().Expand();
                Pump([&] { return node.IsExpanded() && !node.HasUnrealizedChildren(); });
                f.root.UpdateLayout();
                auto label = f.Row(node).FindName(L"EntryName").as<TextBlock>();
                expect(std::abs(x(label) - x(rootName) - 8.f * depth) < 0.5f);
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
            auto root = f.tree.RootNodes().GetAt(0);
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
            auto row = f.Row(root);
            auto refresh = row.Children().GetAt(3).as<Button>();
            auto collapse = row.Children().GetAt(4).as<Button>();
            expect(refresh.Command() != nullptr);
            expect(collapse.Command() != nullptr);
            expect(refresh.Content().try_as<Viewbox>() != nullptr);
            expect(collapse.Content().try_as<Viewbox>() != nullptr);
            for (auto const& button : {refresh, collapse}) {
                auto at = button.TransformToVisual(f.host).TransformPoint({0, 0});
                expect(at.X >= 0.f && at.X + button.ActualWidth() <= f.host.ActualWidth());
                expect(button.ActualWidth() == 24.);
            }
            collapse.Command().Execute(nullptr);
            expect(!root.IsExpanded());
            expect(!nested.IsExpanded());
            f.explorer.Expand(root);
            f.explorer.Expand(nested);
            auto selected = f.Find(L"正常.md");
            f.tree.SelectedNode(selected);
            std::ofstream(f.folder / L"new.md") << "new";
            refresh.Command().Execute(nullptr);
            Pump([&] { return root.Children().Size() == 5; });
            expect(f.tree.RootNodes().GetAt(0) == root);
            expect(root.IsExpanded());
            expect(nested.IsExpanded());
            expect(f.Find(L"章节") == nested);
            expect(f.tree.SelectedNode() == selected);
            expect(f.Row(f.tree.RootNodes().GetAt(0)).FindName(L"EntryName").as<TextBlock>().Text() == L"Examples");
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer window edits names in place and preserves rows on failed creation"_test = [] { try {
            ExplorerWindow f;
            auto root = f.tree.RootNodes().GetAt(0);
            auto actions = f.Row(root);
            expect(actions.Children().Size() == 5_u);
            auto newFile = actions.Children().GetAt(1).as<Button>();
            expect(newFile.Content().try_as<Viewbox>() != nullptr);
            newFile.Command().Execute(nullptr);
            Pump([&] { return f.explorer.IsEditing(); });
            auto draft = f.tree.SelectedNode();
            auto editor = f.Row(draft).FindName(L"EntryEditor").as<TextBox>();
            expect(winrt::to_string(editor.Text()) == winrt::to_string(winrt::Folia::Localize(L"UntitledMarkdown")));
            expect(!newFile.Command().CanExecute(nullptr));
            expect(!Await(f.explorer.CommitNameAsync(L"../bad.md")));
            expect(f.explorer.IsEditing());
            expect(editor.IsEnabled());
            expect(!Await(f.explorer.CommitNameAsync(L"正常.md")));
            expect(f.explorer.IsEditing());
            expect(std::filesystem::file_size(f.folder / L"正常.md") == 6_u);
            expect(Await(f.explorer.CommitNameAsync(L"created.md")));
            Pump([&] { return !root.HasUnrealizedChildren() && f.tree.SelectedNode() == draft; });
            expect(std::filesystem::exists(f.folder / L"created.md"));
            expect(f.opened == f.folder / L"created.md" && f.kept);
            expect(!f.explorer.IsEditing());
            expect(f.Find(L"created.md") == draft);
            f.tree.SelectedNode(draft);
            f.explorer.RenameSelected();
            expect(f.explorer.IsEditing());
            expect(f.Row(draft).FindName(L"EntryEditor").try_as<TextBox>() != nullptr);
            expect(Await(f.explorer.CommitNameAsync(L"renamed.md")));
            Pump([&] { return !root.HasUnrealizedChildren(); });
            expect(!std::filesystem::exists(f.folder / L"created.md"));
            expect(f.Find(L"renamed.md") == draft);
            f.tree.SelectedNode(root);
            f.explorer.NewFolder();
            Pump([&] { return f.explorer.IsEditing() && f.tree.SelectedNode() != root; });
            auto folderDraft = f.tree.SelectedNode();
            expect(f.Row(folderDraft).FindName(L"EntryEditor").try_as<TextBox>() != nullptr);
            f.explorer.CancelNameEdit();
            expect(!f.explorer.IsEditing());
            f.explorer.NewFolder();
            Pump([&] { return f.explorer.IsEditing() && f.tree.SelectedNode() != root; });
            expect(Await(f.explorer.CommitNameAsync(L"New directory")));
            Pump([&] { return !root.HasUnrealizedChildren(); });
            expect(std::filesystem::is_directory(f.folder / L"New directory"));
            expect(f.tree.CanDragItems() && !f.tree.CanReorderItems() && f.tree.AllowDrop());
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer window moves and copies while retaining expanded subtree identity"_test = [] { try {
            ExplorerWindow f;
            auto root = f.tree.RootNodes().GetAt(0);
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
            Pump([&] { return root.Children().Size() == 5; });
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
            expect(guides >= 6_i);
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
    };
}
