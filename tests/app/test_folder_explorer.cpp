#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"
#include "workspace/FolderExplorerController.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Folia;
    using namespace winrt::Microsoft::UI::Xaml::Controls;

    suite folder_explorer_tests = [] {
        "folder explorer enumerates one directory lazily with native templates"_test = [] { try {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            TestDirectory directory;
            std::filesystem::create_directory(directory.path / L"nested");
            std::ofstream(directory.path / L"nested" / L"inside.md") << "inside";
            std::ofstream(directory.path / L"b.md") << "b";
            std::ofstream(directory.path / L"A.markdown") << "a";
            std::ofstream(directory.path / L"ignored.png") << "image";
            TreeView tree;
            FolderExplorerController explorer;
            std::wstring error;
            explorer.Attach(tree, {}, [&](auto text) { error = text; });
            explorer.Open(directory.path);
            Pump([&] { return !tree.RootNodes().GetAt(0).HasUnrealizedChildren(); });
            auto root = tree.RootNodes().GetAt(0);
            expect(error.empty());
            expect(root.IsExpanded());
            auto properties = root.Content().as<winrt::Microsoft::UI::Xaml::Data::ICustomPropertyProvider>();
            auto name = properties.GetCustomProperty(L"Name");
            expect(name != nullptr && name.CanRead() && !name.CanWrite());
            expect(winrt::unbox_value<winrt::hstring>(name.GetValue(root.Content())) == directory.path.filename().c_str());
            expect(winrt::unbox_value<winrt::hstring>(properties.GetCustomProperty(L"FullPath").GetValue(root.Content())) == directory.path.c_str());
            expect(root.Children().Size() == 4_u);
            auto folder = root.Children().GetAt(0);
            expect(folder.HasUnrealizedChildren());
            expect(folder.Children().Size() == 0_u);
            expect(root.Children().GetAt(1).Content().as<winrt::Windows::Foundation::IStringable>().ToString() == L"A.markdown");
            expect(tree.ItemTemplateSelector().SelectTemplate(folder) != tree.ItemTemplateSelector().SelectTemplate(root.Children().GetAt(1)));
            explorer.Expand(folder);
            Pump([&] { return !folder.HasUnrealizedChildren(); });
            expect(folder.Children().Size() == 1_u);
            expect(folder.IsExpanded());
            explorer.Collapse();
            expect(!root.IsExpanded());
            expect(!folder.IsExpanded());
            explorer.Detach();
            expect(tree.RootNodes().Size() == 0_u);
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };

        "folder explorer replacement cancellation failure and large directory remain bounded"_test = [] { try {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            TestDirectory directory;
            auto large = directory.path / L"large";
            auto smallDirectory = directory.path / L"smallDirectory";
            std::filesystem::create_directory(large);
            std::filesystem::create_directory(smallDirectory);
            for (int i = 0; i < 600; ++i) std::ofstream(large / (std::to_wstring(i) + L".md")) << i;
            std::ofstream(smallDirectory / L"keep.md") << "keep";
            TreeView tree;
            FolderExplorerController explorer;
            int errors = 0;
            explorer.Attach(tree, {}, [&](auto) { ++errors; });
            explorer.Open(large);
            explorer.Open(smallDirectory);
            Pump([&] { return !tree.RootNodes().GetAt(0).HasUnrealizedChildren(); });
            expect(explorer.RootPath() == smallDirectory);
            expect(tree.RootNodes().GetAt(0).Children().Size() == 1_u);
            expect(errors == 0_i);
            explorer.Open(directory.path / L"missing");
            Pump([&] { return errors != 0; });
            expect(errors == 1_i);
            explorer.Open(large);
            Pump([&] { return !tree.RootNodes().GetAt(0).HasUnrealizedChildren(); });
            expect(tree.RootNodes().GetAt(0).Children().Size() == 600_u);
            explorer.Refresh();
            explorer.Detach();
            expect(tree.RootNodes().Size() == 0_u);
        } catch (winrt::hresult_error const& error) { throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value)); } };
    };
}
