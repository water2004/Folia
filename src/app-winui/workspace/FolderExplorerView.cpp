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

    void ScrollFolderExplorerNodeIntoView(TreeView const& tree, TreeViewNode const& node) {
        if (auto list = tree.as<IControlProtected>().GetTemplateChild(L"ListControl").try_as<ListView>())
            list.ScrollIntoView(node, ScrollIntoViewAlignment::Leading);
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
                    Border line; line.Width(1); line.Background(tree.Foreground());
                    Canvas::SetLeft(line, 9 + canvas.Children().Size() * 8.);
                    canvas.Children().Append(line);
                }
                for (unsigned index = 0; index < canvas.Children().Size(); ++index) {
                    auto line = canvas.Children().GetAt(index).as<Border>();
                    if (line.Height() != canvas.ActualHeight()) line.Height(canvas.ActualHeight());
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
