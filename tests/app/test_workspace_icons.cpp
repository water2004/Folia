#include "pch.h"
#include "support/folia_test.hpp"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;
    using namespace winrt::Microsoft::UI::Xaml;

    suite workspace_icon_tests = [] {
        "workspace icons preserve the complete SVG viewbox at every control size"_test = [] { try {
            auto xaml = Hosting::WindowsXamlManager::InitializeForCurrentThread();
            auto resources = Application::Current().Resources();
            auto key = winrt::box_value(L"TextFillColorPrimaryBrush");
            const bool inserted = !resources.HasKey(key);
            if (inserted) resources.Insert(key, Media::SolidColorBrush(winrt::Windows::UI::Color{255, 0, 0, 0}));
            struct Restore { ResourceDictionary resources; winrt::Windows::Foundation::IInspectable key; bool inserted;
                ~Restore() { if (inserted) resources.Remove(key); } } restore{resources, key, inserted};
            for (auto kind : {WorkspaceIcon::PanelLeft, WorkspaceIcon::PanelRight, WorkspaceIcon::Folder,
                WorkspaceIcon::Document, WorkspaceIcon::NewFile, WorkspaceIcon::NewFolder, WorkspaceIcon::Refresh, WorkspaceIcon::Collapse}) {
                for (float size : {14.f, 16.f, 20.f, 24.f, 32.f}) {
                    auto icon = MakeWorkspaceIcon(kind);
                    icon.Width(size);
                    icon.Height(size);
                    auto canvas = icon.Child().as<Controls::Canvas>();
                    expect(canvas.Width() == 20.0);
                    expect(canvas.Height() == 20.0);
                    auto path = canvas.Children().GetAt(0).as<Shapes::Path>();
                    expect(path.Width() == 20.0 && path.Height() == 20.0);
                    auto bounds = path.Data().Bounds();
                    expect(bounds.X >= 0.f && bounds.Y >= 0.f);
                    expect(bounds.X + bounds.Width <= 20.001f && bounds.Y + bounds.Height <= 20.001f);
                    expect(bounds.Width > 0.f && bounds.Height > 0.f);
                    icon.Measure({size, size});
                    icon.Arrange({0, 0, size, size});
                    auto visible = path.TransformToVisual(icon).TransformBounds(bounds);
                    expect(visible.X >= -0.01f && visible.Y >= -0.01f);
                    expect(visible.X + visible.Width <= size + 0.01f);
                    expect(visible.Y + visible.Height <= size + 0.01f);
                }
            }
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };
    };
}
