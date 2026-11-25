#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "MainWindow.xaml.h"
#include "App.xaml.h"
#include "visual_window.h"
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
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
            window = winrt::Folia::implementation::App::Current()->CreateMainWindow();
            window.Title(L"Folia workspace visual test");
            expect(!!::AreDpiAwarenessContextsEqual(::GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
            HWND handle{};
            winrt::check_hresult(window.as<::IWindowNative>()->get_WindowHandle(&handle));
            const auto dpi = double(::GetDpiForWindow(handle)) / 96.;
            window.AppWindow().Resize({static_cast<int>(1120 * dpi), static_cast<int>(740 * dpi)});
            window.Activate();
            root = window.Content().as<Grid>();
            Pump([&] { return root.IsLoaded() && root.ActualWidth() > 100; });
            root.UpdateLayout();
            expect(std::abs(root.XamlRoot().RasterizationScale() - double(::GetDpiForWindow(handle)) / 96.) < 0.01);
            const auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
            Pump([&] { return std::chrono::steady_clock::now() >= settled; });
        }
        ~ChromeWindow() { window.Close(); }
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
        bool Ink(winrt::Windows::Foundation::Point point, winrt::Windows::Foundation::Point empty) const {
            int contrast = 0;
            auto background = pixels.data() + (int(empty.Y * scale) * bitmap.PixelWidth() + int(empty.X * scale)) * 4;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                int x = static_cast<int>(point.X * scale) + dx, y = static_cast<int>(point.Y * scale) + dy;
                if (x < 0 || y < 0 || x >= bitmap.PixelWidth() || y >= bitmap.PixelHeight()) continue;
                auto p = pixels.data() + (y * bitmap.PixelWidth() + x) * 4;
                for (int c = 0; c < 4; ++c) contrast += std::abs(int(p[c]) - background[c]);
            }
            // Integrate coverage: a one-pixel SVG stroke can straddle pixels
            // at fractional scales, without any individual pixel being opaque.
            return contrast > 180;
        }
    };

    suite workspace_chrome_tests = [] {
        "workspace tab highlights cover native selected hover and pressed states immediately"_test = [] {
            ChromeWindow f;
            auto tabs = f.Find<TabView>(L"DocumentTabs");
            auto firstItem = tabs.SelectedItem();
            Invoke(f.Find<Button>(L"NewTabButton"));
            Pump([&] { return tabs.SelectedItem() != firstItem; });
            auto secondItem = tabs.SelectedItem();
            f.root.UpdateLayout();
            auto first = tabs.ContainerFromItem(firstItem).as<TabViewItem>();
            auto second = tabs.ContainerFromItem(secondItem).as<TabViewItem>();
            for (auto item : {firstItem, secondItem, firstItem, secondItem}) {
                tabs.SelectedItem(item);
                auto selected = tabs.ContainerFromItem(item).as<TabViewItem>();
                auto other = selected == first ? second : first;
                expect(selected.IsSelected());
                expect(!other.IsSelected());
                expect(Descendant(selected, L"SelectedBackgroundPath").Visibility() == Visibility::Visible) << "native selection highlights synchronously";
                expect(Descendant(other, L"SelectedBackgroundPath").Visibility() == Visibility::Collapsed) << "old highlight is removed synchronously";
                // These are the exact CommonStates in the installed WinUI
                // TabViewItem template. No input simulation, timer, layout or
                // dispatcher turn may be needed to apply the indicator setter.
                for (auto state : {L"Selected", L"PointerOverSelected", L"PressedSelected"}) {
                    expect(VisualStateManager::GoToState(selected, state, false)) << winrt::to_string(state);
                    expect(Descendant(selected, L"SelectedBackgroundPath").Visibility() == Visibility::Visible) << winrt::to_string(state);
                }
                for (auto state : {L"Normal", L"PointerOver", L"Pressed"}) {
                    expect(VisualStateManager::GoToState(other, state, false));
                    expect(Descendant(other, L"SelectedBackgroundPath").Visibility() == Visibility::Collapsed);
                }
            }
            // Leave the fixture's original tab as the only document.
            tabs.SelectedItem(secondItem);
            Invoke(Descendant(second, L"CloseButton").as<Button>());
            Pump([&] { return tabs.ContainerFromItem(secondItem) == nullptr; });
            expect(tabs.SelectedItem() == firstItem);
        };
        "workspace chrome separates background tabs and preserves their width on hover"_test = [] { try {
            ChromeWindow f;
            auto tabs = f.Find<TabView>(L"DocumentTabs");
            auto original = tabs.SelectedItem();
            auto add = f.Find<Button>(L"NewTabButton");
            for (int i = 0; i < 2; ++i) {
                auto previous = tabs.SelectedItem();
                Invoke(add);
                Pump([&] { return previous != tabs.SelectedItem(); });
            }
            tabs.SelectedItem(original);
            f.root.UpdateLayout();
            auto selected = tabs.ContainerFromIndex(0).as<TabViewItem>();
            auto next = tabs.ContainerFromIndex(1).as<TabViewItem>();
            expect(Descendant(selected, L"TabSeparator").Opacity() == 0.);
            expect(Descendant(next, L"TabSeparator").Opacity() == 1.);
            auto width = next.ActualWidth();
            expect(VisualStateManager::GoToState(next, L"PointerOver", false));
            expect(Descendant(next, L"TabContainer").as<Grid>().Background() != nullptr);
            expect(VisualStateManager::GoToState(next, L"CloseButtonVisible", false));
            f.root.UpdateLayout();
            expect(next.ActualWidth() == width);
            auto list = Descendant(tabs, L"TabListView").as<ListViewBase>();
            expect(list.ItemContainerTransitions().GetAt(0).try_as<Media::Animation::RepositionThemeTransition>() != nullptr);
            auto slot = selected.TransformToVisual(f.root).TransformPoint({0, 0});
            winrt::Folia::TabDragPresentation feedback;
            auto converter = f.root.XamlRoot().CoordinateConverter();
            auto destination = converter.ConvertLocalToScreen(winrt::Windows::Foundation::Point{slot.X + 20.f, slot.Y + 10.f});
            feedback.Start(tabs, original, 8.f, winrt::Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(f.window.AppWindow().Id()));
            feedback.MoveTo(destination);
            auto artwork = Descendant(selected, L"LayoutRoot");
            expect(std::abs(artwork.Translation().x - (converter.ConvertScreenToLocal(destination).X - slot.X - 8.f)) < .01);
            expect(selected.TransformToVisual(f.root).TransformPoint({0, 0}).X == slot.X);
            feedback.Stop();
            expect(artwork.Translation().x == 0.f);
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };
        "workspace chrome distinguishes the reading tab from the title strip by its surface"_test = [] { try {
            ChromeWindow f;
            auto tabs = f.Find<TabView>(L"DocumentTabs");
            expect(tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size() == 1_u);
            auto tab = tabs.ContainerFromIndex(0).as<TabViewItem>();
            auto bounds = tab.TransformToVisual(f.root).TransformBounds({0, 0, static_cast<float>(tab.ActualWidth()), static_cast<float>(tab.ActualHeight())});
            auto chrome = f.Find<Grid>(L"TopChrome");
            expect(bounds.Y > 0.f);
            expect(std::abs(bounds.Y + bounds.Height - chrome.ActualHeight()) <= 1.);
            expect(tab.IsSelected());
            expect(f.root.FindName(L"CloseOutlineButton") == nullptr);
            expect(f.Find<Grid>(L"WorkspaceSidebar").Visibility() == Visibility::Collapsed);
            expect(f.root.FindName(L"ExplorerHeadingText") == nullptr);
            expect(f.root.FindName(L"FolderActionsButton") == nullptr);
            expect(f.Find<Grid>(L"ExplorerPane").RowDefinitions().Size() == 2_u);
            expect(Grid::GetRow(f.Find<TreeView>(L"FileTree")) == 1_i);
            auto fill = Descendant(tab, L"SelectedBackgroundPath").as<Shapes::Path>().Fill();
            expect(fill == f.Find<Border>(L"ReadingTint").Background());
            expect(f.Find<Grid>(L"DocumentToolbar").Background().as<Media::SolidColorBrush>().Color().A == 0);
            for (float scale : {1.f, 1.25f, 1.5f, 2.f}) {
                Media::Imaging::RenderTargetBitmap bitmap;
                Await(bitmap.RenderAsync(f.root, static_cast<int>(f.root.ActualWidth() * scale / f.root.XamlRoot().RasterizationScale()), static_cast<int>(f.root.ActualHeight() * scale / f.root.XamlRoot().RasterizationScale())));
                auto pixels = Await(bitmap.GetPixelsAsync());
                expect(pixels.Length() == static_cast<std::uint32_t>(bitmap.PixelWidth() * bitmap.PixelHeight() * 4));
                auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-chrome";
                SavePng(artifacts / (L"light-" + std::to_wstring(int(scale * 100)) + L".png"), bitmap.PixelWidth(), bitmap.PixelHeight(), pixels);
            }
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "workspace chrome uses a downward main menu and keeps insertion in the editor context menu"_test = [] {
            ChromeWindow f;
            auto main = f.Find<Button>(L"DocumentActionsButton").Flyout().as<MenuFlyout>();
            expect(main.Placement() == Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
            for (auto item : main.Items()) if (auto sub = item.try_as<MenuFlyoutSubItem>()) expect(sub.Name() != L"InsertButton");
            auto context = f.Find<SwapChainPanel>(L"EditorSurface").ContextFlyout().as<MenuFlyout>();
            MenuFlyoutSubItem insertion{nullptr};
            for (auto item : context.Items()) if (auto sub = item.try_as<MenuFlyoutSubItem>(); sub && sub.Name() == L"InsertButton") insertion = sub;
            expect(insertion != nullptr);
            expect(insertion.Items().Size() >= 7_u);
            expect(!f.root.FindName(L"ShellDivider"));
            // Activate the deployed desktop picker factory against the real
            // owning window without displaying a modal system dialog.
            winrt::Microsoft::Windows::Storage::Pickers::FolderPicker picker{f.window.AppWindow().Id()};
            TestDirectory folder;
            picker.SuggestedFolder(folder.path.native());
            expect(std::filesystem::path(picker.SuggestedFolder().c_str()) == folder.path);
        };

        "workspace chrome keeps text at native scale and separates title and document actions"_test = [] {
            ChromeWindow f;
            auto tabs = f.Find<TabView>(L"DocumentTabs");
            auto selected = tabs.ContainerFromItem(tabs.SelectedItem()).as<TabViewItem>();
            auto header = selected.Header().as<Panel>();
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
            expect(std::abs(origin.X * raster - std::round(origin.X * raster)) < 0.01) << std::format("text X in pixels {}", origin.X * raster);
            expect(std::abs(origin.Y * raster - std::round(origin.Y * raster)) < 0.01) << std::format("text Y in pixels {}", origin.Y * raster);
            auto left = f.Find<Primitives::ToggleButton>(L"SidebarButton");
            auto right = f.Find<Primitives::ToggleButton>(L"SourceModeButton");
            auto leftAt = left.TransformToVisual(f.root).TransformPoint({0, 0});
            auto rightAt = right.TransformToVisual(f.root).TransformPoint({0, 0});
            auto top = f.Find<Grid>(L"TopChrome");
            auto card = f.Find<Grid>(L"ReadingCard");
            expect(leftAt.Y < top.ActualHeight());
            expect(rightAt.X > leftAt.X && rightAt.Y >= top.ActualHeight());
            expect(card.CornerRadius().TopLeft > 0. && card.Margin().Right > 0.);
            expect(!f.root.FindName(L"StatusBar"));
            Capture native(f.root, raster);
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-chrome";
            SavePng(artifacts / L"native-scale.png", native.bitmap.PixelWidth(), native.bitmap.PixelHeight(), native.pixels);
            SavePng(artifacts / L"native-titlebar.png", static_cast<std::uint32_t>(360 * native.scale), static_cast<std::uint32_t>(48 * native.scale), native.pixels, native.bitmap.PixelWidth());
        };

        "workspace chrome retains native tabs and pane controls in a narrow window"_test = [] { try {
            ChromeWindow f;
            auto tabs = f.Find<TabView>(L"DocumentTabs");
            auto add = f.Find<Button>(L"NewTabButton");
            for (int i = 0; i < 6; ++i) {
                const auto previous = tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size();
                Invoke(add);
                Pump([&] { return tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size() == previous + 1; });
            }
            f.root.UpdateLayout();
            auto selected = tabs.ContainerFromItem(tabs.SelectedItem()).as<TabViewItem>();
            Pump([&] { return Descendant(selected, L"CloseButton") != nullptr; });
            VisualStateManager::GoToState(selected, L"CloseButtonVisible", false);
            auto close = Descendant(selected, L"CloseButton").as<Button>();
            Invoke(close);
            Pump([&] { return tabs.TabItemsSource().as<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>>().Size() == 6; });
            const auto dpi = f.root.XamlRoot().RasterizationScale();
            f.window.AppWindow().Resize({static_cast<int>(680 * dpi), static_cast<int>(600 * dpi)});
            Pump([&] { return f.root.ActualWidth() < 700; });
            auto button = f.Find<Primitives::ToggleButton>(L"SidebarButton");
            auto pane = f.Find<Grid>(L"WorkspaceSidebar");
            Automation::Peers::ToggleButtonAutomationPeer peer(button);
            auto toggle = peer.GetPattern(Automation::Peers::PatternInterface::Toggle).as<Automation::Provider::IToggleProvider>();
            toggle.Toggle();
            Pump([&] { return pane.Visibility() == Visibility::Visible; });
            toggle.Toggle();
            Pump([&] { return pane.Visibility() == Visibility::Collapsed; });
            auto settle = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
            Pump([&] { return std::chrono::steady_clock::now() >= settle; });
            auto active = tabs.ContainerFromItem(tabs.SelectedItem()).as<TabViewItem>();
            expect(active.IsSelected());
            for (auto name : {L"ScrollDecreaseButton", L"ScrollIncreaseButton"}) {
                auto arrow = Descendant(tabs, name).as<Primitives::RepeatButton>();
                expect(arrow.Content().try_as<Viewbox>() != nullptr) << "native overflow uses a vector chevron";
                auto at = arrow.TransformToVisual(f.root).TransformPoint({0, 0});
                expect(arrow.ActualWidth() == 28. && arrow.ActualHeight() == 28.) << "overflow hit target is 28 DIP";
                auto center = active.TransformToVisual(f.root).TransformPoint({0, static_cast<float>(active.ActualHeight() / 2)}).Y;
                expect(std::abs(at.Y + arrow.ActualHeight() / 2 - center) <= 1. / dpi) << "overflow center aligns to the tabs";
            }
            auto activeBounds = active.TransformToVisual(f.root).TransformBounds({0, 0, static_cast<float>(active.ActualWidth()), static_cast<float>(active.ActualHeight())});
            expect(activeBounds.X >= 0.f);
            expect(activeBounds.X + activeBounds.Width <= f.Find<Border>(L"TitleDragArea").TransformToVisual(f.root).TransformPoint({0, 0}).X);
            auto indicator = Descendant(active, L"SelectedBackgroundPath");
            expect(indicator.Visibility() == Visibility::Visible);
            auto cardTop = f.Find<Grid>(L"ReadingCard").TransformToVisual(f.root).TransformPoint({0, 0}).Y;
            expect(activeBounds.Y + activeBounds.Height <= cardTop);
            Capture narrow(f.root, f.root.XamlRoot().RasterizationScale());
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-chrome";
            SavePng(artifacts / L"narrow-tabs.png", narrow.bitmap.PixelWidth(), narrow.bitmap.PixelHeight(), narrow.pixels);
            SavePng(artifacts / L"overflow-controls-native.png", narrow.bitmap.PixelWidth(), static_cast<int>(50 * narrow.scale), narrow.pixels, narrow.bitmap.PixelWidth());
            auto viewer = Descendant(tabs, L"ScrollViewer").as<ScrollViewer>();
            tabs.SelectedIndex(0);
            active = tabs.ContainerFromItem(tabs.SelectedItem()).as<TabViewItem>();
            Pump([&] { return viewer.HorizontalOffset() == 0.; });
            const auto originalOffset = viewer.HorizontalOffset();
            expect(viewer.ScrollableWidth() > 0.);
            auto forward = Descendant(tabs, L"ScrollIncreaseButton").as<Primitives::RepeatButton>();
            Pump([&] { return forward.IsEnabled(); });
            bool settled = false;
            auto viewChanged = viewer.ViewChanged(winrt::auto_revoke, [&](auto const&, auto const& args) {
                settled = !args.IsIntermediate();
            });
            Automation::Peers::RepeatButtonAutomationPeer forwardPeer(forward);
            forwardPeer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return settled && viewer.HorizontalOffset() > originalOffset; });
            const auto previousOffset = viewer.HorizontalOffset();
            auto backward = Descendant(tabs, L"ScrollDecreaseButton").as<Primitives::RepeatButton>();
            Pump([&] { return backward.IsEnabled(); });
            Automation::Peers::RepeatButtonAutomationPeer backwardPeer(backward);
            backwardPeer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return viewer.HorizontalOffset() < previousOffset; });
            expect(static_cast<bool>(tabs.ContainerFromItem(tabs.SelectedItem()) == active));
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "workspace chrome sidebar and source icons have distinct complete silhouettes"_test = [] {
            ChromeWindow f;
            auto artifacts = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / L"build/tests/workspace-chrome";
            for (auto name : {L"SidebarButton", L"SourceModeButton"}) {
                auto button = f.Find<Primitives::ToggleButton>(name);
                auto icon = button.Content().as<Viewbox>();
                Media::Imaging::RenderTargetBitmap bitmap;
                Await(bitmap.RenderAsync(icon, 80, 80));
                auto pixels = Await(bitmap.GetPixelsAsync());
                int ink = 0, clipped = 0;
                for (int y = 0; y < bitmap.PixelHeight(); ++y)
                    for (int x = 0; x < bitmap.PixelWidth(); ++x)
                        if (pixels.data()[(y * bitmap.PixelWidth() + x) * 4 + 3] > 20) {
                            ++ink;
                            if (!x || !y || x + 1 == bitmap.PixelWidth() || y + 1 == bitmap.PixelHeight()) ++clipped;
                        }
                expect(ink > 50);
                expect(clipped == 0);
                SavePng(artifacts / (std::wstring(name) + L".png"), bitmap.PixelWidth(), bitmap.PixelHeight(), pixels);
            }
        };

        "workspace chrome icons retain the right and bottom SVG strokes in actual pixels"_test = [] { try {
            ChromeWindow f;
            {
                auto name = L"SidebarButton";
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
                    auto empty = transform.TransformPoint({-2.f, 10.f});
                    expect(capture.Ink(right, empty)) << "right stroke at raster scale" << scale;
                    expect(capture.Ink(bottom, empty)) << "bottom stroke at raster scale" << scale;
                }
            }
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };
    };
}
