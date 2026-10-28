#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "MainWindow.xaml.h"
#include "visual_window.h"
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>

namespace {
    using namespace boost::ut;
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;
    using namespace document_tests;

    struct ChromeWindow {
        winrt::Microsoft::UI::Xaml::Window window{nullptr};
        Grid root{nullptr};
        ChromeWindow() {
            window = visual_tests::Window();
            window.Title(L"Folia workspace visual test");
            expect(!!::AreDpiAwarenessContextsEqual(::GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
            HWND handle{};
            winrt::check_hresult(window.as<::IWindowNative>()->get_WindowHandle(&handle));
            const auto dpi = double(::GetDpiForWindow(handle)) / 96.;
            window.AppWindow().Resize({static_cast<int>(1120 * dpi), static_cast<int>(740 * dpi)});
            root = window.Content().as<Grid>();
            Pump([&] { return root.IsLoaded() && root.ActualWidth() > 100; });
            root.UpdateLayout();
            expect(std::abs(root.XamlRoot().RasterizationScale() - double(::GetDpiForWindow(handle)) / 96.) < 0.01);
            const auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
            Pump([&] { return std::chrono::steady_clock::now() >= settled; });
        }
        template<class T> T Find(wchar_t const* name) { return root.FindName(name).as<T>(); }
    };

    FrameworkElement Descendant(DependencyObject const& parent, std::wstring_view name) {
        if (auto element = parent.try_as<FrameworkElement>(); element && element.Name() == name) return element;
        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
            if (auto found = Descendant(Media::VisualTreeHelper::GetChild(parent, i), name)) return found;
        return nullptr;
    }

    void Invoke(Button const& button) {
        Automation::Peers::ButtonAutomationPeer peer(button);
        peer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
    }

    using visual_tests::SavePng;

    struct Capture {
        Media::Imaging::RenderTargetBitmap bitmap;
        winrt::Windows::Storage::Streams::IBuffer pixels{nullptr};
        double scale = 1;
        explicit Capture(Grid const& root, double requestedScale = 1) {
            Await(bitmap.RenderAsync(root, static_cast<int>(root.ActualWidth() * requestedScale / root.XamlRoot().RasterizationScale()), static_cast<int>(root.ActualHeight() * requestedScale / root.XamlRoot().RasterizationScale())));
            pixels = Await(bitmap.GetPixelsAsync());
            scale = bitmap.PixelWidth() / root.ActualWidth();
        }
        bool Ink(winrt::Windows::Foundation::Point point, winrt::Windows::UI::Color background) const {
            int contrast = 0;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                int x = static_cast<int>(point.X * scale) + dx, y = static_cast<int>(point.Y * scale) + dy;
                if (x < 0 || y < 0 || x >= bitmap.PixelWidth() || y >= bitmap.PixelHeight()) continue;
                auto p = pixels.data() + (y * bitmap.PixelWidth() + x) * 4;
                contrast += std::abs(int(p[0]) - background.B) + std::abs(int(p[1]) - background.G) + std::abs(int(p[2]) - background.R);
            }
            // Integrate coverage: a one-pixel SVG stroke can straddle pixels
            // at fractional scales, without any individual pixel being opaque.
            return contrast > 180;
        }
    };

    suite workspace_chrome_tests = [] {
        "workspace chrome renders the real window with one complete divider and one outline toggle"_test = [] { try {
            ChromeWindow f;
            auto tabs = f.Find<TabView>(L"DocumentTabs");
            expect(tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size() == 1_u);
            auto tab = tabs.ContainerFromIndex(0).as<TabViewItem>();
            auto bounds = tab.TransformToVisual(f.root).TransformBounds({0, 0, static_cast<float>(tab.ActualWidth()), static_cast<float>(tab.ActualHeight())});
            expect(bounds.Y >= 0.f);
            expect(bounds.Y + bounds.Height <= 48.f);
            expect(tab.IsSelected());
            expect(f.root.FindName(L"CloseOutlineButton") == nullptr);
            expect(!f.Find<SplitView>(L"DocumentWorkspace").IsPaneOpen());
            expect(f.root.FindName(L"ExplorerHeadingText") == nullptr);
            expect(f.root.FindName(L"FolderActionsButton") == nullptr);
            expect(f.Find<Grid>(L"ExplorerPane").RowDefinitions().Size() == 2_u);
            expect(Grid::GetRow(f.Find<TreeView>(L"FileTree")) == 1_i);
            for (float scale : {1.f, 1.25f, 1.5f, 2.f}) {
                Media::Imaging::RenderTargetBitmap bitmap;
                Await(bitmap.RenderAsync(f.root, static_cast<int>(f.root.ActualWidth() * scale / f.root.XamlRoot().RasterizationScale()), static_cast<int>(f.root.ActualHeight() * scale / f.root.XamlRoot().RasterizationScale())));
                auto pixels = Await(bitmap.GetPixelsAsync());
                expect(pixels.Length() == static_cast<std::uint32_t>(bitmap.PixelWidth() * bitmap.PixelHeight() * 4));
                const double raster = bitmap.PixelWidth() / f.root.ActualWidth();
                auto divider = f.Find<Border>(L"ShellDivider");
                auto dividerBounds = divider.TransformToVisual(f.root).TransformBounds({0, 0, static_cast<float>(divider.ActualWidth()), static_cast<float>(divider.ActualHeight())});
                auto y = static_cast<int>((dividerBounds.Y + dividerBounds.Height * 0.6) * raster);
                auto reference = pixels.data() + (y * bitmap.PixelWidth() + 8) * 4;
                for (int x = 8; x < bitmap.PixelWidth() - 8; x += 32) {
                    auto offset = (y * bitmap.PixelWidth() + x) * 4;
                    auto data = pixels.data() + offset;
                    expect(std::abs(int(data[0]) - int(reference[0])) < 3);
                    expect(std::abs(int(data[1]) - int(reference[1])) < 3);
                    expect(std::abs(int(data[2]) - int(reference[2])) < 3);
                }
                auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-chrome";
                SavePng(artifacts / (L"light-" + std::to_wstring(int(scale * 100)) + L".png"), bitmap.PixelWidth(), bitmap.PixelHeight(), pixels);
            }
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "workspace chrome uses a downward main menu and keeps insertion in the editor context menu"_test = [] {
            ChromeWindow f;
            auto main = f.Find<Button>(L"MainMenuButton").Flyout().as<MenuFlyout>();
            expect(main.Placement() == Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
            for (auto item : main.Items()) if (auto sub = item.try_as<MenuFlyoutSubItem>()) expect(sub.Name() != L"InsertButton");
            auto context = f.Find<SwapChainPanel>(L"EditorSurface").ContextFlyout().as<MenuFlyout>();
            MenuFlyoutSubItem insertion{nullptr};
            for (auto item : context.Items()) if (auto sub = item.try_as<MenuFlyoutSubItem>(); sub && sub.Name() == L"InsertButton") insertion = sub;
            expect(insertion != nullptr);
            expect(insertion.Items().Size() >= 7_u);
            auto divider = f.Find<Border>(L"ShellDivider");
            auto rect = divider.TransformToVisual(f.root).TransformBounds({0, 0, static_cast<float>(divider.ActualWidth()), static_cast<float>(divider.ActualHeight())});
            expect(Grid::GetRow(divider) == 1_i);
            expect(rect.Y >= 48.f);
            expect(rect.Height == 1.f);
            expect(rect.Width == f.root.ActualWidth());
            // Activate the deployed desktop picker factory against the real
            // owning window without displaying a modal system dialog.
            winrt::Microsoft::Windows::Storage::Pickers::FolderPicker picker{f.window.AppWindow().Id()};
            TestDirectory folder;
            picker.SuggestedFolder(folder.path.native());
            expect(std::filesystem::path(picker.SuggestedFolder().c_str()) == folder.path);
        };

        "workspace chrome keeps text at native scale and layout buttons beside the captions"_test = [] {
            ChromeWindow f;
            auto tabs = f.Find<TabView>(L"DocumentTabs");
            auto selected = tabs.ContainerFromItem(tabs.SelectedItem()).as<TabViewItem>();
            auto header = selected.Header().as<StackPanel>();
            auto title = header.Children().GetAt(1).as<TextBlock>();
            expect(title.FontSize() >= 14.);
            expect(title.Opacity() == 1.);
            auto transform = title.TransformToVisual(f.root);
            auto origin = transform.TransformPoint({0, 0});
            auto x = transform.TransformPoint({1, 0}), y = transform.TransformPoint({0, 1});
            expect(std::abs(x.X - origin.X - 1.f) < 0.001f);
            expect(std::abs(x.Y - origin.Y) < 0.001f);
            expect(std::abs(y.Y - origin.Y - 1.f) < 0.001f);
            expect(std::abs(y.X - origin.X) < 0.001f);
            const double raster = f.root.XamlRoot().RasterizationScale();
            expect(std::abs(origin.X * raster - std::round(origin.X * raster)) < 0.01);
            expect(std::abs(origin.Y * raster - std::round(origin.Y * raster)) < 0.01);
            auto left = f.Find<Primitives::ToggleButton>(L"ExplorerPaneButton");
            auto right = f.Find<Primitives::ToggleButton>(L"OutlinePaneButton");
            auto leftAt = left.TransformToVisual(f.root).TransformPoint({0, 0});
            auto rightAt = right.TransformToVisual(f.root).TransformPoint({0, 0});
            expect(rightAt.X - leftAt.X == 30.f);
            expect(rightAt.Y == leftAt.Y);
            auto inset = f.Find<Border>(L"CaptionInset");
            expect(std::abs(rightAt.X + right.ActualWidth() + 8 - (f.root.ActualWidth() - inset.ActualWidth())) < 1.);
            Capture native(f.root, raster);
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-chrome";
            SavePng(artifacts / L"native-scale.png", native.bitmap.PixelWidth(), native.bitmap.PixelHeight(), native.pixels);
            SavePng(artifacts / L"native-titlebar.png", static_cast<std::uint32_t>(360 * native.scale), static_cast<std::uint32_t>(48 * native.scale), native.pixels, native.bitmap.PixelWidth());
        };

        "workspace chrome retains native tabs and pane controls in a narrow window"_test = [] { try {
            ChromeWindow f;
            auto tabs = f.Find<TabView>(L"DocumentTabs");
            auto add = Descendant(tabs, L"AddButton").as<Button>();
            for (int i = 0; i < 6; ++i) {
                const auto previous = tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size();
                Invoke(add);
                Pump([&] { return tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size() == previous + 1; });
            }
            f.root.UpdateLayout();
            auto selected = tabs.ContainerFromItem(tabs.SelectedItem()).as<TabViewItem>();
            Pump([&] { return Descendant(selected, L"CloseButton") != nullptr; });
            auto close = Descendant(selected, L"CloseButton").as<Button>();
            Invoke(close);
            Pump([&] { return tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size() == 6; });
            const auto dpi = f.root.XamlRoot().RasterizationScale();
            f.window.AppWindow().Resize({static_cast<int>(680 * dpi), static_cast<int>(600 * dpi)});
            Pump([&] { return f.root.ActualWidth() < 700; });
            for (auto name : {L"ExplorerPaneButton", L"OutlinePaneButton"}) {
                auto button = f.Find<Primitives::ToggleButton>(name);
                Automation::Peers::ToggleButtonAutomationPeer peer(button);
                auto provider = peer.GetPattern(Automation::Peers::PatternInterface::Toggle).as<Automation::Provider::IToggleProvider>();
                provider.Toggle();
                Pump([&] { return button.IsChecked().GetBoolean(); });
                auto pane = f.Find<SplitView>(name == std::wstring_view(L"ExplorerPaneButton") ? L"DocumentWorkspace" : L"OutlineSplitView");
                expect(pane.IsPaneOpen());
                expect(pane.DisplayMode() == SplitViewDisplayMode::Overlay);
                provider.Toggle();
                Pump([&] { return !button.IsChecked().GetBoolean(); });
                expect(!pane.IsPaneOpen());
            }
            for (auto name : {L"ExplorerPaneButton", L"OutlinePaneButton"}) {
                auto button = f.Find<Primitives::ToggleButton>(name);
                auto at = button.TransformToVisual(f.root).TransformPoint({0, 0});
                expect(at.X >= 0.f);
                expect(at.X + button.ActualWidth() <= f.root.ActualWidth());
                expect(at.Y >= 0.f && at.Y + button.ActualHeight() <= 48.f);
            }
            auto settle = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
            Pump([&] { return std::chrono::steady_clock::now() >= settle; });
            auto active = tabs.ContainerFromItem(tabs.SelectedItem()).as<TabViewItem>();
            expect(active.IsSelected());
            for (auto name : {L"ScrollDecreaseButton", L"ScrollIncreaseButton"}) {
                auto arrow = Descendant(tabs, name).as<Primitives::RepeatButton>();
                expect(arrow.Content().try_as<Viewbox>() != nullptr) << "native overflow uses a vector chevron";
                auto at = arrow.TransformToVisual(f.root).TransformPoint({0, 0});
                expect(arrow.ActualWidth() == 28. && arrow.ActualHeight() == 28.) << "overflow hit target is 28 DIP";
                expect(std::abs(at.Y + arrow.ActualHeight() / 2 - 24.) < .5) << "overflow center aligns to title strip";
            }
            auto activeBounds = active.TransformToVisual(f.root).TransformBounds({0, 0, static_cast<float>(active.ActualWidth()), static_cast<float>(active.ActualHeight())});
            expect(activeBounds.X >= 0.f);
            expect(activeBounds.X + activeBounds.Width <= f.Find<Primitives::ToggleButton>(L"ExplorerPaneButton").TransformToVisual(f.root).TransformPoint({0, 0}).X);
            auto indicator = Descendant(active, L"SelectionIndicator");
            expect(indicator.Opacity() == 1.);
            Capture narrow(f.root, f.root.XamlRoot().RasterizationScale());
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-chrome";
            SavePng(artifacts / L"narrow-tabs.png", narrow.bitmap.PixelWidth(), narrow.bitmap.PixelHeight(), narrow.pixels);
            SavePng(artifacts / L"overflow-controls-native.png", narrow.bitmap.PixelWidth(), static_cast<int>(50 * narrow.scale), narrow.pixels, narrow.bitmap.PixelWidth());
            auto viewer = Descendant(tabs, L"ScrollViewer").as<ScrollViewer>();
            const auto originalOffset = viewer.HorizontalOffset();
            expect(viewer.ScrollableWidth() > 0.);
            auto forward = Descendant(tabs, L"ScrollIncreaseButton").as<Primitives::RepeatButton>();
            Automation::Peers::RepeatButtonAutomationPeer forwardPeer(forward);
            forwardPeer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return viewer.HorizontalOffset() > originalOffset; });
            const auto previousOffset = viewer.HorizontalOffset();
            auto backward = Descendant(tabs, L"ScrollDecreaseButton").as<Primitives::RepeatButton>();
            Automation::Peers::RepeatButtonAutomationPeer backwardPeer(backward);
            backwardPeer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return viewer.HorizontalOffset() < previousOffset; });
            expect(static_cast<bool>(tabs.ContainerFromItem(tabs.SelectedItem()) == active));
            while (tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size() > 1) {
                auto previous = tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size();
                f.root.UpdateLayout();
                auto current = tabs.ContainerFromItem(tabs.SelectedItem()).as<TabViewItem>();
                Pump([&] { return Descendant(current, L"CloseButton") != nullptr; });
                Invoke(Descendant(current, L"CloseButton").as<Button>());
                Pump([&] { return tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size() < previous; });
            }
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "workspace chrome pane icons have aligned complete mirrored contours"_test = [] {
            ChromeWindow f;
            auto left = f.Find<Primitives::ToggleButton>(L"ExplorerPaneButton");
            auto right = f.Find<Primitives::ToggleButton>(L"OutlinePaneButton");
            auto leftIcon = left.Content().as<Viewbox>(), rightIcon = right.Content().as<Viewbox>();
            Media::Imaging::RenderTargetBitmap a, b;
            Await(a.RenderAsync(leftIcon));
            Await(b.RenderAsync(rightIcon));
            auto ap = Await(a.GetPixelsAsync()), bp = Await(b.GetPixelsAsync());
            expect(a.PixelWidth() == b.PixelWidth());
            expect(a.PixelHeight() == b.PixelHeight());
            int delta = 0, topA = a.PixelHeight(), topB = b.PixelHeight(), bottomA = -1, bottomB = -1;
            for (int y = 0; y < a.PixelHeight(); ++y) for (int x = 0; x < a.PixelWidth(); ++x) {
                int av = ap.data()[(y * a.PixelWidth() + x) * 4 + 3];
                int bv = bp.data()[(y * b.PixelWidth() + b.PixelWidth() - 1 - x) * 4 + 3];
                delta += std::abs(av - bv);
                if (av > 8) { topA = (std::min)(topA, y); bottomA = (std::max)(bottomA, y); }
                if (bv > 8) { topB = (std::min)(topB, y); bottomB = (std::max)(bottomB, y); }
            }
            // Compare the entire alpha mask, including all four corners. The
            // former upstream pair differs in height and fails this comparison.
            expect(delta == 0);
            expect(topA == topB);
            expect(bottomA == bottomB);
            expect(topA > 0 && bottomA < a.PixelHeight() - 1);
            auto atA = leftIcon.TransformToVisual(f.root).TransformPoint({0, 0});
            auto atB = rightIcon.TransformToVisual(f.root).TransformPoint({0, 0});
            expect(atA.Y == atB.Y);
            Capture native(f.root, f.root.XamlRoot().RasterizationScale());
            auto at = left.TransformToVisual(f.root).TransformPoint({0, 0});
            auto first = static_cast<std::uint32_t>(at.Y * native.scale) * native.bitmap.PixelWidth()
                + static_cast<std::uint32_t>(at.X * native.scale);
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-chrome";
            SavePng(artifacts / L"pane-pair-native.png", static_cast<std::uint32_t>(58 * native.scale),
                static_cast<std::uint32_t>(28 * native.scale), native.pixels, native.bitmap.PixelWidth(), first);
        };

        "workspace chrome icons retain the right and bottom SVG strokes in actual pixels"_test = [] { try {
            ChromeWindow f;
            auto background = f.root.Resources().Lookup(winrt::box_value(L"FoliaShellLayerBrush")).as<Media::SolidColorBrush>().Color();
            for (auto name : {L"ExplorerPaneButton", L"OutlinePaneButton"}) {
                auto icon = f.Find<Primitives::ToggleButton>(name).Content().as<Viewbox>();
                auto canvas = icon.Child().as<Canvas>();
                auto path = canvas.Children().GetAt(0).as<Shapes::Path>();
                Media::Imaging::RenderTargetBitmap iconBitmap;
                Await(iconBitmap.RenderAsync(icon, 80, 80));
                auto iconPixels = Await(iconBitmap.GetPixelsAsync());
                auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-chrome";
                SavePng(artifacts / (std::wstring(name) + L".png"), iconBitmap.PixelWidth(), iconBitmap.PixelHeight(), iconPixels);
                auto transform = path.TransformToVisual(f.root);
                for (double scale : {1., 1.25, 1.5, 2.}) {
                    Capture capture(f.root, scale);
                    auto right = transform.TransformPoint({17.5f, 10.f}), bottom = transform.TransformPoint({10.f, 15.5f});
                    expect(capture.Ink(right, background));
                    expect(capture.Ink(bottom, background));
                }
                // Demonstrate why geometry-bounds-only tests missed the old
                // defect: the inner Path's automatic layout box clips strokes.
                path.ClearValue(FrameworkElement::WidthProperty());
                path.ClearValue(FrameworkElement::HeightProperty());
                f.root.UpdateLayout();
                Capture cropped(f.root, 2.);
                transform = path.TransformToVisual(f.root);
                expect(!cropped.Ink(transform.TransformPoint({17.5f, 10.f}), background));
                expect(!cropped.Ink(transform.TransformPoint({10.f, 15.5f}), background));
                path.Width(20);
                path.Height(20);
                f.root.UpdateLayout();
            }
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };
    };
}
