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
            DataTemplate editFile = EditorTemplate(true), editFolder = EditorTemplate(false);

            static DataTemplate Template(WorkspaceIcon icon, bool root = false) {
                const bool showIcon = icon == WorkspaceIcon::Document;
                auto xaml = std::wstring(LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
                    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><Grid ColumnDefinitions=")")
                    + (root ? L"*,Auto,Auto,Auto,Auto" : showIcon ? L"16,*" : L"*")
                    + LR"(" ColumnSpacing="2" Height="24" HorizontalAlignment="Stretch"
                            ToolTipService.ToolTip="{Binding Content.FullPath}">)";
                if (showIcon) xaml += WorkspaceIconXaml(icon);
                xaml += std::wstring(LR"(<TextBlock x:Name="EntryName" Grid.Column=")") + (showIcon ? L"1" : L"0")
                    + LR"(" Text="{Binding Content.Name}" FontSize="13" TextWrapping="NoWrap"
                           TextTrimming="CharacterEllipsis" VerticalAlignment="Center")";
                xaml += root ? LR"( FontWeight="SemiBold"/>)" : L"/>";
                if (root) {
                    xaml += LR"(<Button Grid.Column="1" Width="24" Height="24" MinWidth="24" MinHeight="24"
                            Padding="0" Style="{StaticResource SubtleButtonStyle}" Command="{Binding Content.NewFile}"
                            AutomationProperties.Name="{Binding Content.NewFileName}" ToolTipService.ToolTip="{Binding Content.NewFileName}">)"
                        + WorkspaceIconXaml(WorkspaceIcon::NewFile) + L"</Button>";
                    xaml += LR"(<Button Grid.Column="2" Width="24" Height="24" MinWidth="24" MinHeight="24"
                            Padding="0" Style="{StaticResource SubtleButtonStyle}" Command="{Binding Content.NewFolder}"
                            AutomationProperties.Name="{Binding Content.NewFolderName}" ToolTipService.ToolTip="{Binding Content.NewFolderName}">)"
                        + WorkspaceIconXaml(WorkspaceIcon::NewFolder) + L"</Button>";
                    xaml += LR"(<Button Grid.Column="3" Width="24" Height="24" MinWidth="24" MinHeight="24"
                            Padding="0" Style="{StaticResource SubtleButtonStyle}" Command="{Binding Content.Refresh}"
                            AutomationProperties.Name="{Binding Content.RefreshName}" ToolTipService.ToolTip="{Binding Content.RefreshName}">)"
                        + WorkspaceIconXaml(WorkspaceIcon::Refresh) + L"</Button>";
                    xaml += LR"(<Button Grid.Column="4" Width="24" Height="24" MinWidth="24" MinHeight="24"
                            Padding="0" Style="{StaticResource SubtleButtonStyle}" Command="{Binding Content.Collapse}"
                            AutomationProperties.Name="{Binding Content.CollapseName}" ToolTipService.ToolTip="{Binding Content.CollapseName}">)"
                        + WorkspaceIconXaml(WorkspaceIcon::Collapse) + L"</Button>";
                }
                xaml += L"</Grid></DataTemplate>";
                return Markup::XamlReader::Load(xaml).as<DataTemplate>();
            }

            static DataTemplate EditorTemplate(bool file) {
                auto xaml = std::wstring(LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
                    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><Grid ColumnDefinitions=")")
                    + (file ? L"16,*" : L"*") + LR"(" ColumnSpacing="6" Height="24" HorizontalAlignment="Stretch">)";
                if (file) xaml += WorkspaceIconXaml(WorkspaceIcon::Document);
                xaml += std::wstring(LR"(<Grid Grid.Column=")") + (file ? L"1" : L"0") + LR"(">
                    <TextBox x:Name="EntryEditor" Height="22" MinHeight="22" MinWidth="0" Padding="4,0"
                        FontSize="13" MaxLength="255" IsSpellCheckEnabled="False" IsTextPredictionEnabled="False"
                        Text="{Binding Content.EditName, Mode=TwoWay, UpdateSourceTrigger=PropertyChanged}"
                        IsEnabled="{Binding Content.EditEnabled}" ToolTipService.ToolTip="{Binding Content.EditError}"
                        AutomationProperties.HelpText="{Binding Content.EditError}"/>
                    <Border BorderBrush="{ThemeResource SystemFillColorCriticalBrush}" BorderThickness="1" CornerRadius="2"
                        Visibility="{Binding Content.ErrorVisibility}" IsHitTestVisible="False"/>
                    </Grid></Grid></DataTemplate>)";
                return Markup::XamlReader::Load(xaml).as<DataTemplate>();
            }

            DataTemplate SelectTemplateCore(IInspectable const& item) {
                auto node = item.try_as<TreeViewNode>();
                if (!node) return file;
                auto entry = winrt::get_self<explorer_detail::FolderEntry>(node.Content().as<Windows::Foundation::IStringable>());
                if (entry->edit) return entry->directory ? editFolder : editFile;
                return entry->root ? root : entry->directory ? folder : file;
            }
            DataTemplate SelectTemplateCore(IInspectable const& item, DependencyObject const&) {
                return SelectTemplateCore(item);
            }
        };
    }

    Microsoft::UI::Xaml::Controls::TreeViewNode ExplorerNodeFromElement(
        Microsoft::UI::Xaml::Controls::TreeView const& tree, Microsoft::UI::Xaml::DependencyObject element) {
        while (element && element != tree) {
            if (auto container = element.try_as<Microsoft::UI::Xaml::Controls::TreeViewItem>())
                return tree.NodeFromContainer(container);
            element = Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(element);
        }
        return nullptr;
    }

    Microsoft::UI::Xaml::Controls::TextBox ExplorerNameEditorFromElement(Microsoft::UI::Xaml::DependencyObject element) {
        while (element) {
            if (auto text = element.try_as<Microsoft::UI::Xaml::Controls::TextBox>(); text && text.Name() == L"EntryEditor") return text;
            if (element.try_as<Microsoft::UI::Xaml::Controls::TreeView>()) break;
            element = Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(element);
        }
        return nullptr;
    }

    void UpdateFolderExplorerRow(Microsoft::UI::Xaml::Controls::TreeView const& tree,
        Microsoft::UI::Xaml::Controls::TreeViewNode const& node) {
        if (auto container = tree.ContainerFromNode(node).try_as<Microsoft::UI::Xaml::Controls::TreeViewItem>())
            container.ContentTemplate(tree.ItemTemplateSelector().SelectTemplate(node, container));
    }

    void UpdateFolderExplorerGuides(TreeView const& tree, TreeViewNode const& activeNode, bool visible) {
        // Match VS Code AbstractTree's active-branch rule: an expanded folder
        // with children owns its guides; files/collapsed/empty nodes use parent.
        auto activeBranch = activeNode;
        if (auto entry = explorer_detail::Entry(activeBranch); entry &&
            (!entry->directory || !activeBranch.IsExpanded() || activeBranch.Children().Size() == 0)) activeBranch = activeBranch.Parent();
        // Walk only native realized controls, never the directory model. Each
        // virtualized row owns at most depth lines; no document layout changes.
        auto visit = [&](auto const& self, DependencyObject const& element, TreeViewNode node) -> void {
            if (auto item = element.try_as<TreeViewItem>()) node = tree.NodeFromContainer(item);
            if (auto canvas = element.try_as<Canvas>(); canvas && canvas.Name() == L"IndentGuides") {
                const auto depth = node ? (std::max)(0, node.Depth()) : 0;
                bool inBranch = false;
                for (auto parent = node ? node.Parent() : nullptr; parent && parent.Depth() >= 0; parent = parent.Parent())
                    if (parent == activeBranch) { inBranch = true; break; }
                auto visibility = depth && (visible || inBranch) ? Visibility::Visible : Visibility::Collapsed;
                if (canvas.Visibility() != visibility) canvas.Visibility(visibility);
                if (visibility == Visibility::Collapsed) return;
                while (canvas.Children().Size() > static_cast<unsigned>(depth)) canvas.Children().RemoveAtEnd();
                while (canvas.Children().Size() < static_cast<unsigned>(depth)) {
                    Border line; line.Width(1); line.Height(24); line.Background(tree.Foreground());
                    Canvas::SetLeft(line, 9 + canvas.Children().Size() * 8.);
                    canvas.Children().Append(line);
                }
                for (unsigned index = 0; index < canvas.Children().Size(); ++index) {
                    auto line = canvas.Children().GetAt(index).as<Border>();
                    if (line.Background() != tree.Foreground()) line.Background(tree.Foreground());
                    const double opacity = inBranch && activeBranch && static_cast<int>(index) == activeBranch.Depth() ? .4 : visible ? .13 : 0.;
                    if (std::abs(line.Opacity() - opacity) > .001) line.Opacity(opacity);
                }
                return;
            }
            for (int index = 0, count = Media::VisualTreeHelper::GetChildrenCount(element); index < count; ++index)
                self(self, Media::VisualTreeHelper::GetChild(element, index), node);
        };
        visit(visit, tree, nullptr);
    }

    void ConfigureFolderExplorerView(Microsoft::UI::Xaml::Controls::TreeView const& tree) {
        ScrollViewer::SetHorizontalScrollBarVisibility(tree, ScrollBarVisibility::Disabled);
        ScrollViewer::SetHorizontalScrollMode(tree, ScrollMode::Disabled);
        tree.ItemTemplateSelector(winrt::make<EntryTemplateSelector>());
    }
}
