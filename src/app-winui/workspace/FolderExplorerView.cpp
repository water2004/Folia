#include "pch.h"
import std;
#include "workspace/FolderExplorerView.h"
#include "workspace/FolderEntry.h"
#include "workspace/WorkspaceIcons.h"

namespace winrt::Folia {
    namespace {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        using Windows::Foundation::IInspectable;

        struct EntryTemplateSelector : DataTemplateSelectorT<EntryTemplateSelector> {
            DataTemplate folder = Template(WorkspaceIcon::Folder);
            DataTemplate file = Template(WorkspaceIcon::Document);
            DataTemplate root = Template(WorkspaceIcon::Folder, true);

            static DataTemplate Template(WorkspaceIcon icon, bool root = false) {
                auto xaml = std::wstring(LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
                    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><Grid ColumnDefinitions=")")
                    + (root ? L"*,Auto,Auto" : L"16,*")
                    + LR"(" ColumnSpacing="6" Height="24" HorizontalAlignment="Stretch"
                            ToolTipService.ToolTip="{Binding Content.FullPath}">)";
                if (!root) xaml += WorkspaceIconXaml(icon);
                xaml += std::wstring(LR"(<TextBlock x:Name="EntryName" Grid.Column=")") + (root ? L"0" : L"1")
                    + LR"(" Text="{Binding Content.Name}" FontSize="13" TextWrapping="NoWrap"
                           TextTrimming="CharacterEllipsis" VerticalAlignment="Center")";
                xaml += root ? LR"( FontWeight="SemiBold"/>)" : L"/>";
                if (root) {
                    xaml += LR"(<Button Grid.Column="1" Width="24" Height="24" MinWidth="24" MinHeight="24"
                            Padding="0" Style="{StaticResource SubtleButtonStyle}" Command="{Binding Content.Refresh}"
                            AutomationProperties.Name="{Binding Content.RefreshName}" ToolTipService.ToolTip="{Binding Content.RefreshName}">)"
                        + WorkspaceIconXaml(WorkspaceIcon::Refresh) + L"</Button>";
                    xaml += LR"(<Button Grid.Column="2" Width="24" Height="24" MinWidth="24" MinHeight="24"
                            Padding="0" Style="{StaticResource SubtleButtonStyle}" Command="{Binding Content.Collapse}"
                            AutomationProperties.Name="{Binding Content.CollapseName}" ToolTipService.ToolTip="{Binding Content.CollapseName}">)"
                        + WorkspaceIconXaml(WorkspaceIcon::Collapse) + L"</Button>";
                }
                xaml += L"</Grid></DataTemplate>";
                return Markup::XamlReader::Load(xaml).as<DataTemplate>();
            }

            DataTemplate SelectTemplateCore(IInspectable const& item) {
                auto node = item.try_as<TreeViewNode>();
                if (!node) return file;
                auto entry = winrt::get_self<explorer_detail::FolderEntry>(node.Content().as<Windows::Foundation::IStringable>());
                return entry->root ? root : entry->directory ? folder : file;
            }
            DataTemplate SelectTemplateCore(IInspectable const& item, DependencyObject const&) {
                return SelectTemplateCore(item);
            }
        };
    }

    void ConfigureFolderExplorerView(Microsoft::UI::Xaml::Controls::TreeView const& tree) {
        ScrollViewer::SetHorizontalScrollBarVisibility(tree, ScrollBarVisibility::Disabled);
        ScrollViewer::SetHorizontalScrollMode(tree, ScrollMode::Disabled);
        tree.ItemTemplateSelector(winrt::make<EntryTemplateSelector>());
    }
}
