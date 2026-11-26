#include "pch.h"
#include "MainWindow.xaml.h"
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>

import std;

namespace winrt::Folia::implementation
{
    namespace
    {
        Windows::UI::Color UiColor(folia::Color color)
        {
            return Windows::UI::Color{ color.a, color.r, color.g, color.b };
        }

        void UpdateBrushResource(
            Microsoft::UI::Xaml::ResourceDictionary const& resources,
            wchar_t const* name,
            folia::Color color)
        {
            auto key = winrt::box_value(name);
            if (resources.HasKey(key))
            {
                if (auto brush = resources.Lookup(key)
                    .try_as<Microsoft::UI::Xaml::Media::SolidColorBrush>())
                {
                    brush.Color(UiColor(color));
                    return;
                }
            }
            resources.Insert(key, Microsoft::UI::Xaml::Media::SolidColorBrush(UiColor(color)));
        }

        Windows::Foundation::IReference<Windows::UI::Color> ColorReference(folia::Color color)
        {
            return winrt::box_value(UiColor(color))
                .as<Windows::Foundation::IReference<Windows::UI::Color>>();
        }

        Microsoft::UI::Xaml::Media::FontFamily Font(std::string const& family)
        {
            return Microsoft::UI::Xaml::Media::FontFamily(winrt::to_hstring(family));
        }
    }

    folia::Theme MainWindow::CurrentThemeVariant()
    {
        if (Windows::UI::ViewManagement::AccessibilitySettings().HighContrast())
            return folia::Theme::HighContrast;
        auto background = Windows::UI::ViewManagement::UISettings().GetColorValue(
            Windows::UI::ViewManagement::UIColorType::Background);
        auto luminance = 0.2126 * background.R + 0.7152 * background.G + 0.0722 * background.B;
        return luminance < 128.0 ? folia::Theme::Dark : folia::Theme::Light;
    }

    void MainWindow::ApplyShellTheme()
    {
        auto const& colors = themeProfile.colors;
        auto resources = Root().Resources();
        // ThemeResource bindings retain the object returned by the dictionary. Mutating
        // stable brushes updates every existing binding; replacing them leaves controls
        // holding colors from the previous theme until another resource pass occurs.
        UpdateBrushResource(resources, L"FoliaShellBackgroundBrush", colors.shell_bg);
        UpdateBrushResource(resources, L"FoliaShellLayerBrush", colors.shell_layer_bg);
        UpdateBrushResource(resources, L"FoliaShellBorderBrush", colors.shell_border);
        UpdateBrushResource(resources, L"FoliaShellForegroundBrush", colors.shell_fg);
        UpdateBrushResource(resources, L"FoliaShellMutedForegroundBrush", colors.shell_muted_fg);
        UpdateBrushResource(resources, L"FoliaShellAccentBrush", colors.shell_accent);
        resources.Insert(winrt::box_value(L"FoliaUiFontFamily"), Font(themeProfile.typography.ui.family));
        resources.Insert(winrt::box_value(L"FoliaUiFontSize"), winrt::box_value(static_cast<double>(themeProfile.typography.ui.size)));
        resources.Insert(winrt::box_value(L"FoliaMonoFontFamily"), Font(themeProfile.typography.ui_monospace.family));

        ApplyWorkspaceMaterial();

        auto titleBar = AppWindow().TitleBar();
        titleBar.BackgroundColor(Windows::UI::Color{});
        titleBar.InactiveBackgroundColor(Windows::UI::Color{});
        titleBar.ForegroundColor(ColorReference(colors.shell_fg));
        titleBar.ButtonBackgroundColor(Windows::UI::Color{});
        titleBar.ButtonForegroundColor(ColorReference(colors.shell_fg));
        titleBar.ButtonHoverBackgroundColor(ColorReference(colors.shell_layer_bg));
        titleBar.ButtonHoverForegroundColor(ColorReference(colors.shell_fg));
        titleBar.ButtonPressedBackgroundColor(ColorReference(colors.shell_border));
        titleBar.ButtonPressedForegroundColor(ColorReference(colors.shell_fg));
        titleBar.ButtonInactiveBackgroundColor(Windows::UI::Color{});
        titleBar.ButtonInactiveForegroundColor(ColorReference(colors.shell_muted_fg));

        UpdateWorkspaceLayout();
        EditorScrollBar().Width(themeProfile.layout.scrollbar_width);
        EditorScrollBarColumn().Width(Microsoft::UI::Xaml::GridLengthHelper::FromPixels(themeProfile.layout.scrollbar_width));
        scrollController.SetWidth(themeProfile.layout.scrollbar_width);
        DocumentToolbar().MinHeight((std::max)(30.f, themeProfile.layout.status_bar_min_height));
    }

    void MainWindow::ApplyWorkspaceMaterial()
    {
        using namespace Microsoft::UI::Composition::SystemBackdrops;
        namespace media = Microsoft::UI::Xaml::Media;
        bool highContrast = CurrentThemeVariant() == folia::Theme::HighContrast || themeProfile.variant == folia::Theme::HighContrast;
        auto supported = [highContrast](WindowMaterial value) {
            if (highContrast || value == WindowMaterial::Solid) return WindowMaterial::Solid;
            auto available = value == WindowMaterial::Acrylic ? DesktopAcrylicController::IsSupported() : MicaController::IsSupported();
            return available ? value : WindowMaterial::Solid;
        };
        auto backdrop = [](WindowMaterial value) -> media::SystemBackdrop {
            if (value == WindowMaterial::Solid) return nullptr;
            if (value == WindowMaterial::Acrylic) return media::DesktopAcrylicBackdrop{};
            media::MicaBackdrop mica;
            mica.Kind(value == WindowMaterial::Mica ? MicaKind::Base : MicaKind::BaseAlt);
            return mica;
        };
        auto const& appearance = appSettings.appearance;
        auto material = supported(appearance.material);
        auto reading = supported(appearance.readingMaterial.value_or(appearance.material));
        const bool sharesBase = reading == material;
        const bool changedLayer = (appliedReadingMaterial == appliedMaterial) != sharesBase;
        if (appliedMaterial != material) {
            SystemBackdrop(backdrop(material));
            appliedMaterial = material;
        }
        if (appliedReadingMaterial != reading || changedLayer) {
            auto paperBackdrop = sharesBase ? nullptr : backdrop(reading);
            ReadingBackdrop().SystemBackdrop(paperBackdrop);
            SelectedTabBackdrop().SystemBackdrop(paperBackdrop);
            ReadingBackdrop().Visibility(paperBackdrop ? Microsoft::UI::Xaml::Visibility::Visible : Microsoft::UI::Xaml::Visibility::Collapsed);
            appliedReadingMaterial = reading;
            UpdateSelectedTabBackdrop();
        }

        auto const& colors = themeProfile.colors;
        auto resources = Root().Resources();
        auto metric = [&](wchar_t const* key) { return unbox_value<double>(resources.Lookup(box_value(key))); };
        const auto amount = metric(L"FoliaSolidBaseTintBlend");
        auto blend = [amount](auto from, auto to) {
            return static_cast<std::uint8_t>(std::lround(std::lerp(static_cast<double>(from), static_cast<double>(to), amount)));
        };
        auto shellTone = highContrast ? colors.shell_layer_bg : folia::Color{
            blend(colors.shell_bg.r, colors.shell_fg.r), blend(colors.shell_bg.g, colors.shell_fg.g),
            blend(colors.shell_bg.b, colors.shell_fg.b)};
        UpdateBrushResource(resources, L"FoliaWorkspaceBaseBrush", shellTone);
        media::Brush shellBackground{nullptr};
        if (material == WindowMaterial::Solid || material == WindowMaterial::Acrylic) {
            auto key = material == WindowMaterial::Acrylic ? L"FoliaAcrylicBaseShadeBrush" : L"FoliaWorkspaceBaseBrush";
            shellBackground = resources.Lookup(box_value(key)).as<media::Brush>();
        }
        Root().Background(shellBackground);

        UpdateBrushResource(resources, L"FoliaReadingSurfaceBrush", colors.bg);
        auto paper = resources.Lookup(box_value(L"FoliaReadingSurfaceBrush")).as<media::SolidColorBrush>();
        paper.Opacity(reading == WindowMaterial::Solid ? 1. : metric(reading == WindowMaterial::Acrylic
            ? L"FoliaAcrylicReadingOpacity" : sharesBase ? L"FoliaSharedReadingMaterialOpacity" : L"FoliaReadingMaterialOpacity"));
        ReadingTint().Background(paper);
    }

    void MainWindow::UpdateTheme()
    {
        if (updatingTheme) return;
        updatingTheme = true;
        struct ResetUpdating
        {
            bool& value;
            ~ResetUpdating() { value = false; }
        } resetUpdating{ updatingTheme };

        themeCatalog->Refresh();
        auto systemVariant = CurrentThemeVariant();
        auto loaded = themeCatalog->Resolve(appSettings.themeId, systemVariant);
        themeProfile = std::move(loaded.profile);
        ApplyShellTheme();
        using Microsoft::UI::Xaml::ElementTheme;
        if (appSettings.themeId == "system" || themeProfile.variant == folia::Theme::HighContrast)
            Root().RequestedTheme(ElementTheme::Default);
        else
            Root().RequestedTheme(themeProfile.variant == folia::Theme::Dark
                ? ElementTheme::Dark
                : ElementTheme::Light);
        ++workspaceThemeRevision;
        if (auto const& editor = workspaceDocuments.Active()->editor) {
            editor->session.SetTheme(themeProfile);
            editor->themeRevision = workspaceThemeRevision;
        }
        editorRenderer.SetTheme(themeProfile);
        if (settingsView)
        {
            settingsView->SetSystemVariant(systemVariant);
        }
        if (!loaded.diagnostic.empty()) ShowError(loaded.diagnostic);
        RenderEditorSurface();
    }

    void MainWindow::UpdateWorkspaceAppearance(WorkspaceAppearance const& settings)
    {
        if (appSettings.appearance == settings) return;
        appSettings.appearance = settings;
        ApplyBarLayout();
        ApplyWorkspaceMaterial();
        if (settingsView) settingsView->UpdateWorkspaceAppearance(settings);
    }

    void MainWindow::UpdateSelectedTabBackdrop()
    {
        using namespace Microsoft::UI::Xaml;
        using Microsoft::UI::Xaml::Controls::IControlProtected;
        if (!ReadingBackdrop().SystemBackdrop()) {
            SelectedTabBackdropFrame().Visibility(Visibility::Collapsed);
            return;
        }
        if (selectedTabBackdropUpdatePending || windowClosed) return;
        selectedTabBackdropUpdatePending = DispatcherQueue().TryEnqueue([weak = get_weak()] {
            auto window = weak.get();
            if (!window) return;
            window->selectedTabBackdropUpdatePending = false;
            if (window->windowClosed || !window->ReadingBackdrop().SystemBackdrop()) return;
            auto frame = window->SelectedTabBackdropFrame();
            auto tabs = window->DocumentTabs();
            auto tab = tabs.ContainerFromItem(tabs.SelectedItem()).try_as<FrameworkElement>();
            auto viewer = window->TabStripScroller();
            auto viewport = viewer ? viewer.as<IControlProtected>().GetTemplateChild(L"ScrollContentPresenter").try_as<FrameworkElement>() : nullptr;
            if (!tab || !tab.IsLoaded() || !viewport) {
                frame.Visibility(Visibility::Collapsed);
                return;
            }
            auto origin = window->TabBackdropLayer();
            auto bounds = [&](FrameworkElement const& element) {
                return element.TransformToVisual(origin).TransformBounds({0, 0,
                    static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())});
            };
            auto item = bounds(tab);
            auto visible = bounds(viewport);
            // Keep the native backdrop inside the portion the tab scroller displays.
            auto left = (std::max)(item.X, visible.X);
            auto right = (std::min)(item.X + item.Width, visible.X + visible.Width);
            auto top = (std::max)(0.f, item.Y);
            auto bottom = (std::min)(item.Y + item.Height, static_cast<float>(origin.ActualHeight()));
            if (right <= left || bottom <= top) {
                frame.Visibility(Visibility::Collapsed);
                return;
            }
            frame.Margin({left, top, 0, 0});
            frame.Width(right - left);
            frame.Height(bottom - top);
            frame.Visibility(Visibility::Visible);
        });
    }
}
