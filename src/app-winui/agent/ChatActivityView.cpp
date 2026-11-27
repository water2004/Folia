#include "pch.h"
#include "agent/ChatMotion.h"
#include "agent/ChatActivityView.h"
#include "agent/ChatControls.h"
#include "workspace/WorkspaceIcons.h"

namespace winrt::Folia::chat {
    namespace {
        struct Action {
            std::string_view name;
            wchar_t const* group;
            WorkspaceIcon icon;
        };
        constexpr Action actions[]{
            {"list_open_tabs", L"Tabs", WorkspaceIcon::Document}, {"read_open_tab", L"Read", WorkspaceIcon::Document},
            {"outline_open_tab", L"Outline", WorkspaceIcon::Outline}, {"search_open_tab", L"Find", WorkspaceIcon::Outline},
            {"list_workspace", L"Folder", WorkspaceIcon::Folder}, {"read_workspace_file", L"Read", WorkspaceIcon::Document},
            {"link_workspace_file", L"Locate", WorkspaceIcon::Document}, {"web_search", L"Web", WorkspaceIcon::Network},
            {"web_fetch", L"Page", WorkspaceIcon::Network}, {"search_workspace_file", L"Find", WorkspaceIcon::Outline}};
        Action Identify(std::string_view name) {
            auto it = std::ranges::find(actions, name, &Action::name);
            return it == std::end(actions) ? Action{name, L"Tool", WorkspaceIcon::Document} : *it;
        }
        hstring ActionLabel(Action action, wchar_t const* tense) {
            return Localize((std::wstring(L"ChatAction") + action.group + tense).c_str());
        }
        hstring WithTarget(hstring const& label, std::string_view target) {
            return target.empty() ? label : label + L"  " + to_hstring(target);
        }
        hstring Brief(hstring const& text) {
            std::wstring_view view(text);
            auto end = view.find_last_not_of(L" \t\r\n");
            if (end == view.npos) return {};
            view = view.substr(0, end + 1);
            auto start = view.find_last_of(L"\r\n");
            if (start != view.npos) view.remove_prefix(start + 1);
            start = view.find_first_not_of(L" #*>\t");
            if (start != view.npos) view.remove_prefix(start);
            bool trimmed = view.size() > 96;
            if (trimmed) {
                view = view.substr(0, 96);
                if (view.back() >= 0xD800 && view.back() <= 0xDBFF) view.remove_suffix(1);
            }
            return trimmed ? hstring(view) + L"…" : hstring(view);
        }

        TextBlock Detail(hstring const& text) {
            TextBlock body;
            body.Style(settings_ui::ResourceStyle(L"ChatModelPickerSecondaryLabelStyle"));
            body.Name(L"ChatActivityDetail");
            body.FontSize(12);
            body.TextWrapping(TextWrapping::Wrap);
            body.TextTrimming(TextTrimming::None);
            body.IsTextSelectionEnabled(true);
            body.Margin({20, 3, 6, 8});
            body.Text(text);
            return body;
        }
    }
    std::shared_ptr<ChatActivityView> ChatActivityView::Create() {
        auto view = std::make_shared<ChatActivityView>();
        view->details_.Margin({12,0,0,0});
        view->Refresh();
        return view;
    }

    Button ChatActivityView::Header(std::optional<std::size_t> index, hstring const& text, bool expanded) {
        Button button;
        button.Name(index ? L"ChatActivityItem" : L"ChatActivitySummary");
        button.Style(settings_ui::ResourceStyle(L"ChatActionStyle"));
        button.HorizontalAlignment(HorizontalAlignment::Left);
        button.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        button.Height(26);
        button.Padding({4,0,6,0});
        Grid line;
        Columns(line, {false,true,false});
        auto const& item = items_[index.value_or(items_.size() - 1)];
        auto glyphItem = &item;
        if (!index && complete_) {
            auto first = std::ranges::find_if(items_, [](auto const& row) { return row.kind != Kind::Reasoning; });
            if (first != items_.end()) glyphItem = &*first;
        }
        auto glyph = glyphItem->kind == Kind::Reasoning ? WorkspaceIcon::Reasoning :
            glyphItem->kind == Kind::Tool ? Identify(glyphItem->action).icon : WorkspaceIcon::Refresh;
        auto icon = MakeWorkspaceIcon(glyph, L"FoliaShellMutedForegroundBrush");
        icon.Width(12); icon.Height(12);
        Put(line, icon, 0);
        line.ColumnSpacing(6);
        TextBlock title;
        title.Style(settings_ui::ResourceStyle(L"ChatModelPickerSecondaryLabelStyle"));
        title.FontSize(12);
        title.TextWrapping(TextWrapping::NoWrap);
        title.TextTrimming(TextTrimming::CharacterEllipsis);
        title.Text(text);
        if (index) items_[*index].caption = title;
        else { summary_ = title; summaryButton_ = button; }
        Put(line, title, 0, 1);
        auto arrow = MakeWorkspaceIcon(expanded ? WorkspaceIcon::Chevron : WorkspaceIcon::ChevronRight, L"FoliaShellMutedForegroundBrush");
        arrow.Width(11);
        arrow.Height(11);
        bool disclose = (!index && items_.size() > 1) || !item.detail.empty();
        if (disclose) Put(line, arrow, 0, 2);
        button.IsTabStop(disclose);
        button.IsHitTestVisible(disclose);
        button.Content(line);
        Automation::AutomationProperties::SetName(button, text);
        button.Click([weak = weak_from_this(), index](auto const&, auto const&) { if (auto self = weak.lock()) self->Toggle(index); });
        return button;
    }
    void ChatActivityView::Toggle(std::optional<std::size_t> index) {
        if (index) items_.at(*index).expanded = !items_.at(*index).expanded;
        else expanded_ = !expanded_;
        Refresh();
        if (expanded_ || (index && items_.at(*index).expanded)) Reveal(details_, 0, -3);
    }
    hstring ChatActivityView::Caption() const {
        if (!complete_) return items_.back().title;
        if (items_.size() == 1) return items_.front().finished;
        struct Summary { std::wstring key; hstring label; unsigned count; };
        std::vector<Summary> groups;
        for (auto const& item : items_) {
            if (item.kind == Kind::Reasoning) continue;
            auto key = item.kind == Kind::Status ? std::wstring(item.finished) : std::wstring(Identify(item.action).group);
            if (item.failed) key += L"!";
            auto group = std::ranges::find(groups, key, &Summary::key);
            if (group == groups.end()) groups.push_back({std::move(key), item.finished, 1});
            else {
                ++group->count;
                if (item.kind == Kind::Tool) group->label = ActionLabel(Identify(item.action), L"Done");
                if (item.failed) group->label = Localize(L"ChatToolFailed");
            }
        }
        if (groups.empty()) return items_.back().finished;
        hstring text;
        for (auto const& group : groups) {
            if (!text.empty()) text = text + L" · ";
            text = text + group.label;
            if (group.count > 1) text = text + L" ×" + to_hstring(group.count);
        }
        return text;
    }
    void ChatActivityView::Refresh() {
        root_.Children().Clear();
        details_.Children().Clear();
        for (auto& item : items_) { item.caption = nullptr; item.body = nullptr; }
        root_.Visibility(items_.empty() ? Visibility::Collapsed : Visibility::Visible);
        if (items_.empty()) return;
        root_.Children().Append(Header({}, Caption(), expanded_));
        if (!expanded_) return;
        for (std::size_t i = 0; i < items_.size(); ++i) {
            auto& item = items_[i];
            if (items_.size() > 1) details_.Children().Append(Header(i, complete_ ? item.finished : item.title, item.expanded));
            if ((items_.size() == 1 || item.expanded) && !item.detail.empty()) {
                item.body = Detail(item.detail);
                details_.Children().Append(item.body);
            }
        }
        root_.Children().Append(details_);
    }
    void ChatActivityView::Reason(hstring const& value) {
        if (value.empty()) return;
        if (!currentReason_) {
            currentReason_ = items_.size();
            items_.push_back({Localize(L"ChatThinkingNow"), Localize(L"ChatThought"), value, Kind::Reasoning});
            Refresh();
            return;
        }
        auto& item = items_[*currentReason_];
        item.detail = value;
        if (item.body) item.body.Text(value);
    }
    void ChatActivityView::Summary(hstring const& value) {
        if (!currentReason_ || value.empty()) return;
        auto& item = items_[*currentReason_];
        item.title = item.finished = Brief(value);
        if (item.caption) item.caption.Text(item.title);
        if (summary_) {
            auto caption = Caption();
            summary_.Text(caption);
            Automation::AutomationProperties::SetName(summaryButton_, caption);
        }
    }
    std::size_t ChatActivityView::StartTool(std::string const& description) {
        currentTool_ = items_.size();
        auto space = description.find(' ');
        auto name = description.substr(0, space);
        auto target = space == description.npos ? std::string_view{} : std::string_view(description).substr(space + 1);
        auto title = WithTarget(ActionLabel(Identify(name), L"Now"), target);
        items_.push_back({title, Localize(L"ChatUnfinished") + L" · " + title, {}, Kind::Tool, std::move(name)});
        Refresh();
        return *currentTool_;
    }
    void ChatActivityView::EndTool(std::string const& description, bool failed, hstring const& value) {
        if (!currentTool_) StartTool(description);
        UpdateTool(*currentTool_, description, true, failed, value);
    }
    void ChatActivityView::UpdateTool(std::size_t index, std::string const& description, bool done, bool failed, hstring const& value) {
        auto& item = items_.at(index);
        auto space = description.find(' ');
        auto target = space == description.npos ? std::string_view{} : std::string_view(description).substr(space + 1);
        item.action = description.substr(0, space);
        if (!done) {
            item.title = WithTarget(ActionLabel(Identify(item.action), L"Now"), target);
            item.finished = Localize(L"ChatUnfinished") + L" · " + item.title;
            Refresh();
            return;
        }
        item.finished = WithTarget(ActionLabel(Identify(item.action), failed ? L"Now" : L"Done"), target);
        if (failed) item.finished = Localize(L"ChatToolFailed") + L" · " + item.finished;
        item.title = item.finished;
        item.detail = value;
        item.failed = failed;
        if (currentTool_ == index) currentTool_.reset();
        Refresh();
    }
    void ChatActivityView::Complete() {
        complete_ = true;
        Refresh();
    }
    void ChatActivityView::Status(hstring const& live, hstring const& finished) {
        items_.push_back({live, finished, {}, Kind::Status});
        Refresh();
    }
}
