#include "pch.h"
#include "editor/interaction/EditorSidebarController.h"

namespace winrt::Folia
{
    namespace
    {
        struct OutlineUpdate
        {
            bool& flag;
            bool previous;
            explicit OutlineUpdate(bool& value) : flag(value), previous(std::exchange(value, true)) {}
            ~OutlineUpdate() { flag = previous; }
        };
    }

    void EditorSidebarController::Attach(
        EditorSession& session,
        EditorSurfaceRenderer& renderer,
        EditorTextInputController& textInput,
        winrt::Microsoft::UI::Xaml::Controls::ListView const& outline,
        Render render)
    {
        Detach();
        session_ = &session;
        renderer_ = &renderer;
        textInput_ = &textInput;
        outline_ = outline;
        render_ = std::move(render);
        using namespace winrt::Microsoft::UI::Xaml;
        using namespace winrt::Microsoft::UI::Xaml::Controls;
        outline_.ItemTemplate(Markup::XamlReader::Load(LR"(
            <DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
                <TextBlock Text="{Binding}" TextWrapping="Wrap" TextTrimming="None"/>
            </DataTemplate>)").as<DataTemplate>());
        outline_.ItemContainerStyle(Markup::XamlReader::Load(LR"(
            <Style xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" TargetType="ListViewItem">
                <Setter Property="HorizontalContentAlignment" Value="Stretch"/>
            </Style>)").as<Style>());
        ScrollViewer::SetHorizontalScrollMode(outline_, ScrollMode::Disabled);
        ScrollViewer::SetHorizontalScrollBarVisibility(outline_, ScrollBarVisibility::Disabled);
        outline_.IsItemClickEnabled(true);
        containerChanging_ = outline_.ContainerContentChanging(winrt::auto_revoke,
            [this](auto const&, ContainerContentChangingEventArgs const& args)
            {
                if (args.InRecycleQueue()) return;
                auto index = static_cast<std::size_t>(args.ItemIndex());
                if (index >= outlineLevels_.size()) return;
                auto container = args.ItemContainer();
                container.Padding(Thickness{8.0 + 12.0 * (outlineLevels_[index] - 1), 6, 8, 6});
            });
        Refresh();
    }

    void EditorSidebarController::Detach()
    {
        OutlineUpdate update(updatingOutline_);
        containerChanging_.revoke();
        if (outline_) outline_.ItemsSource(nullptr);
        session_ = nullptr;
        renderer_ = nullptr;
        textInput_ = nullptr;
        outline_ = nullptr;
        outlineItems_ = nullptr;
        render_ = {};
        outlinePositions_.clear();
        outlineLevels_.clear();
        outlineContentKey_.reset();
    }

    void EditorSidebarController::Refresh()
    {
        RefreshOutline();
    }

    void EditorSidebarController::SelectOutline(winrt::Windows::Foundation::IInspectable const& selectedItem)
    {
        if (updatingOutline_ || !session_ || !renderer_ || !outline_ || !selectedItem) return;
        std::uint32_t selectedIndex = 0;
        if (!outlineItems_ || !outlineItems_.IndexOf(selectedItem, selectedIndex)
            || selectedIndex >= outlinePositions_.size()) return;
        auto const position = outlinePositions_[selectedIndex];
        OutlineUpdate update(updatingOutline_);
        outline_.SelectedIndex(static_cast<int>(selectedIndex));
        session_->SetSelection(position, position);
        if (textInput_) textInput_->NotifySelectionChanged();
        renderer_->ScrollToPosition(position, EditorScrollAlignment::Start);
        if (render_) render_();
        // An offscreen nested heading first materializes its containing flow.
        // Its exact line geometry is available after that render.
        if (renderer_->ScrollToPosition(position, EditorScrollAlignment::Start) && render_) render_();
    }

    void EditorSidebarController::SyncViewport()
    {
        if (updatingOutline_ || !session_ || !renderer_ || !outline_) return;
        RefreshOutline();
        if (outlinePositions_.empty()) return;
        auto const& order = session_->RenderModel().editable_index;
        auto position = renderer_->ViewportPosition();
        auto rank = position ? order.find(position->container_id.v) : order.end();
        int index = -1;
        if (rank != order.end())
        {
            auto next = std::upper_bound(outlinePositions_.begin(), outlinePositions_.end(), rank->second,
                [&](std::size_t value, folia::TextPosition heading)
                {
                    auto found = order.find(heading.container_id.v);
                    return found != order.end() && value < found->second;
                });
            if (next != outlinePositions_.begin())
                index = static_cast<int>(next - outlinePositions_.begin() - 1);
        }
        // A final short section cannot reach the viewport's top edge.
        if (!outlinePositions_.empty() && renderer_->MaximumScrollOffset() > 0.0f
            && renderer_->ScrollOffset() >= renderer_->MaximumScrollOffset())
            index = static_cast<int>(outlinePositions_.size() - 1);
        if (outline_.SelectedIndex() == index) return;
        OutlineUpdate update(updatingOutline_);
        outline_.SelectedIndex(index);
        if (index >= 0)
            outline_.ScrollIntoView(outlineItems_.GetAt(index),
                winrt::Microsoft::UI::Xaml::Controls::ScrollIntoViewAlignment::Default);
    }

    void EditorSidebarController::RefreshOutline()
    {
        if (!session_ || !outline_) return;
        auto const contentKey = session_->RenderModel().outline.content_key;
        if (outlineContentKey_ == contentKey) return;
        auto const flatItems = session_->RenderModel().outline.flat_items();
        std::vector<winrt::Windows::Foundation::IInspectable> labels;
        std::vector<folia::TextPosition> positions;
        std::vector<std::uint8_t> levels;
        labels.reserve(flatItems.size());
        positions.reserve(flatItems.size());
        levels.reserve(flatItems.size());
        for (auto const* item : flatItems)
        {
            labels.push_back(winrt::box_value(winrt::to_hstring(item->title_plain_text)));
            positions.push_back(item->position);
            levels.push_back((std::clamp)(item->level, std::uint8_t{1}, std::uint8_t{6}));
        }
        OutlineUpdate update(updatingOutline_);
        outlinePositions_ = std::move(positions);
        outlineLevels_ = std::move(levels);
        outlineContentKey_ = contentKey;
        outlineItems_ = winrt::single_threaded_observable_vector(std::move(labels));
        outline_.ItemsSource(outlineItems_);
    }

}
