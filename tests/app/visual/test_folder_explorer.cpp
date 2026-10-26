#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "workspace/FolderExplorerController.h"
#include "visual_window.h"

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

        "folder explorer window exposes native root actions and lazy expansion"_test = [] { try {
            ExplorerWindow f;
            auto root = f.tree.RootNodes().GetAt(0);
            auto nested = f.Find(L"章节");
            expect(nested.HasUnrealizedChildren());
            expect(nested.Children().Size() == 0_u);
            f.explorer.Expand(nested);
            Pump([&] { return !nested.HasUnrealizedChildren(); });
            expect(nested.Children().Size() == 1_u);
            auto row = f.Row(root);
            auto refresh = row.Children().GetAt(1).as<Button>();
            auto collapse = row.Children().GetAt(2).as<Button>();
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
    };
}
