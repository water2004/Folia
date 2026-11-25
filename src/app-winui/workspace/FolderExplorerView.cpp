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
            DataTemplate markdown = Template(WorkspaceIcon::Markdown);
            DataTemplate image = Template(WorkspaceIcon::Image);
            DataTemplate editFile = EditorTemplate(WorkspaceIcon::Document);
            DataTemplate editMarkdown = EditorTemplate(WorkspaceIcon::Markdown);
            DataTemplate editImage = EditorTemplate(WorkspaceIcon::Image);
            DataTemplate editFolder = EditorTemplate(WorkspaceIcon::Folder);
            DataTemplate pageLink = Markup::XamlReader::Load(LR"(
                <DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
                    <Button Style="{ThemeResource SubtleButtonStyle}" Content="{Binding Content}"
                            Command="{Binding Content}" Padding="0" MinHeight="24" HorizontalAlignment="Stretch"
                            HorizontalContentAlignment="Left" FontSize="13"/>
                </DataTemplate>)").as<DataTemplate>();

            static DataTemplate Template(WorkspaceIcon icon) {
                auto xaml = std::wstring(LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
                    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><Grid ColumnDefinitions="16,*"
                    ColumnSpacing="4" MinHeight="24" HorizontalAlignment="Stretch"
                    ToolTipService.ToolTip="{Binding Content.FullPath}">)")
                    + WorkspaceIconXaml(icon)
                    + LR"(<TextBlock x:Name="EntryName" Grid.Column="1" Text="{Binding Content.Name}"
                          FontSize="13" TextWrapping="NoWrap" TextTrimming="CharacterEllipsis" VerticalAlignment="Center"/>
                          </Grid></DataTemplate>)";
                return Markup::XamlReader::Load(xaml).as<DataTemplate>();
            }

            static DataTemplate EditorTemplate(WorkspaceIcon icon) {
                auto xaml = std::wstring(LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
                    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><Grid ColumnDefinitions="16,*"
                    ColumnSpacing="4" Height="26" HorizontalAlignment="Stretch">)")
                    + WorkspaceIconXaml(icon) + LR"(<Grid Grid.Column="1">
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
                auto entry = explorer_detail::Entry(node);
                if (!entry) return pageLink;
                if (entry->directory) return entry->edit ? editFolder : folder;
                auto icon = WorkspaceFileIcon(ClassifyWorkspaceFile(entry->path));
                if (icon == WorkspaceIcon::Markdown) return entry->edit ? editMarkdown : markdown;
                if (icon == WorkspaceIcon::Image) return entry->edit ? editImage : image;
                return entry->edit ? editFile : file;
            }
            DataTemplate SelectTemplateCore(IInspectable const& item, DependencyObject const&) {
                return SelectTemplateCore(item);
            }
        };

        TreeViewNode GuideBranch(TreeViewNode const& selected) {
            if (auto entry = explorer_detail::Entry(selected); entry && entry->directory
                && selected.IsExpanded() && selected.Children().Size()) return selected;
            return selected ? selected.Parent() : nullptr;
        }

        void DrawRowGuides(TreeViewItem const& row, TreeViewNode const& node, TreeViewNode const& branch,
            Media::Brush const& foreground, bool visible) {
            const auto depth = node ? (std::max)(0, node.Depth()) : 0;
            bool inBranch = false;
            if (branch && branch.Depth() >= 0 && node) {
                for (auto parent = node.Parent(); parent && parent.Depth() >= branch.Depth(); parent = parent.Parent())
                    if (parent == branch) { inBranch = true; break; }
            }
            const auto visibility = depth && (visible || inBranch) ? Visibility::Visible : Visibility::Collapsed;
            auto control = row.as<IControlProtected>();
            auto guides = control.GetTemplateChild(L"IndentGuides").try_as<Canvas>();
            if (!guides && visibility == Visibility::Visible) {
                row.ApplyTemplate();
                guides = control.GetTemplateChild(L"IndentGuides").try_as<Canvas>();
            }
            if (!guides) return;
            if (guides.Visibility() != visibility) guides.Visibility(visibility);
            if (visibility == Visibility::Collapsed) return;

            auto lines = guides.Children();
            while (lines.Size() > static_cast<unsigned>(depth)) lines.RemoveAtEnd();
            while (lines.Size() < static_cast<unsigned>(depth)) {
                Border line;
                line.Width(1);
                Canvas::SetLeft(line, 9 + lines.Size() * 8.);
                lines.Append(line);
            }
            for (unsigned index = 0; index < lines.Size(); ++index) {
                auto line = lines.GetAt(index).as<Border>();
                if (line.Height() != row.Height()) line.Height(row.Height());
                if (line.Background() != foreground) line.Background(foreground);
                const double opacity = inBranch && static_cast<int>(index) == branch.Depth() ? .4 : visible ? .13 : 0.;
                if (std::abs(line.Opacity() - opacity) > .001) line.Opacity(opacity);
            }
        }
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

    ListView FolderExplorerList(TreeView const& tree) {
        return tree.as<IControlProtected>().GetTemplateChild(L"ListControl").try_as<ListView>();
    }

    void ScrollFolderExplorerNodeIntoView(TreeView const& tree, TreeViewNode const& node) {
        if (auto list = FolderExplorerList(tree))
            list.ScrollIntoView(node, ScrollIntoViewAlignment::Leading);
    }

    void UpdateFolderExplorerRowGuides(TreeView const& tree, TreeViewItem const& row, TreeViewNode const& node, bool visible) {
        DrawRowGuides(row, node, GuideBranch(tree.SelectedNode()), tree.Foreground(), visible);
    }

    void UpdateFolderExplorerGuides(TreeView const& tree, TreeViewNode const& activeNode, bool visible) {
        auto list = FolderExplorerList(tree);
        auto panel = list ? list.ItemsPanelRoot() : nullptr;
        if (!panel) return;
        const auto branch = GuideBranch(activeNode);
        const auto foreground = tree.Foreground();
        for (auto item : panel.Children())
            if (auto row = item.try_as<TreeViewItem>()) DrawRowGuides(row, tree.NodeFromContainer(row), branch, foreground, visible);
    }

    void ConfigureFolderExplorerView(Microsoft::UI::Xaml::Controls::TreeView const& tree) {
        ScrollViewer::SetHorizontalScrollBarVisibility(tree, ScrollBarVisibility::Disabled);
        ScrollViewer::SetHorizontalScrollMode(tree, ScrollMode::Disabled);
        tree.ItemTemplateSelector(winrt::make<EntryTemplateSelector>());
    }
}
