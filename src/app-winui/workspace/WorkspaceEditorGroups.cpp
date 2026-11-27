#include "pch.h"
import std;
#include "workspace/WorkspaceEditorGroups.h"
#include "workspace/WorkspaceDocuments.h"

namespace winrt::Folia {
    std::span<WorkspaceEditorView const> WorkspaceEditorGroups::Views(EditorGroup group) const { return State(group).views; }
    bool WorkspaceEditorGroups::IsSplit() const { return !State(EditorGroup::Right).views.empty(); }
    std::uint64_t WorkspaceEditorGroups::Selected(EditorGroup group) const { return State(group).selected; }

    std::optional<EditorGroup> WorkspaceEditorGroups::GroupOf(std::uint64_t view) const {
        for (auto group : {EditorGroup::Left, EditorGroup::Right}) {
            auto views = Views(group);
            if (std::ranges::find(views, view, &WorkspaceEditorView::id) != views.end()) return group;
        }
        return {};
    }

    WorkspaceEditorView const* WorkspaceEditorGroups::Find(std::uint64_t view) const {
        if (auto group = GroupOf(view)) return &*std::ranges::find(Views(*group), view, &WorkspaceEditorView::id);
        return nullptr;
    }

    WorkspaceEditorView* WorkspaceEditorGroups::Find(std::uint64_t view) {
        if (auto group = GroupOf(view)) return &*std::ranges::find(State(*group).views, view, &WorkspaceEditorView::id);
        return nullptr;
    }

    std::uint64_t WorkspaceEditorGroups::ViewFor(EditorGroup group, std::uint64_t document) const {
        auto views = Views(group);
        if (auto active = Find(Selected(group)); active && active->document == document) return active->id;
        auto found = std::ranges::find(views, document, &WorkspaceEditorView::document);
        return found == views.end() ? 0 : found->id;
    }

    std::size_t WorkspaceEditorGroups::ViewCount(std::uint64_t document) const {
        return std::ranges::count(Views(EditorGroup::Left), document, &WorkspaceEditorView::document)
            + std::ranges::count(Views(EditorGroup::Right), document, &WorkspaceEditorView::document);
    }

    std::uint64_t WorkspaceEditorGroups::Add(EditorGroup group, std::uint64_t document, std::size_t index) {
        if (auto existing = ViewFor(group, document)) return existing;
        auto id = ViewCount(document) ? AllocateWorkspaceItemId() : document;
        Insert(group, {id, document, {}, {}}, index);
        return id;
    }

    void WorkspaceEditorGroups::Insert(EditorGroup group, WorkspaceEditorView view, std::size_t index) {
        auto& target = State(group);
        target.views.insert(target.views.begin() + (std::min)(index, target.views.size()), view);
        if (!target.selected) target.selected = view.id;
    }

    std::uint64_t WorkspaceEditorGroups::Split() {
        if (IsSplit()) return 0;
        auto selected = Find(Selected(EditorGroup::Left));
        if (!selected) return 0;
        auto original = *selected;
        auto view = Add(EditorGroup::Right, original.document);
        auto& copy = *Find(view);
        copy.viewport = original.viewport;
        copy.selection = original.selection;
        return view;
    }

    void WorkspaceEditorGroups::Select(std::uint64_t view) {
        if (auto group = GroupOf(view)) {
            State(*group).selected = view;
            active_ = *group;
        }
    }

    std::uint64_t WorkspaceEditorGroups::Replacement(std::uint64_t view, int direction) const {
        auto group = GroupOf(view);
        if (!group) return 0;
        auto views = Views(*group);
        if (views.size() == 1) return Selected(OtherEditorGroup(*group));
        auto found = std::ranges::find(views, view, &WorkspaceEditorView::id);
        auto index = static_cast<std::size_t>(found - views.begin());
        return views[(index + (direction < 0 ? views.size() - 1 : 1)) % views.size()].id;
    }

    void WorkspaceEditorGroups::Remove(std::uint64_t view) {
        auto group = GroupOf(view);
        if (!group) return;
        auto& state = State(*group);
        auto found = std::ranges::find(state.views, view, &WorkspaceEditorView::id);
        auto index = static_cast<std::size_t>(found - state.views.begin());
        state.views.erase(found);
        if (state.selected == view)
            state.selected = state.views.empty() ? 0 : state.views[(std::min)(index, state.views.size() - 1)].id;
        Normalize();
    }

    void WorkspaceEditorGroups::ForgetDocument(std::uint64_t document) {
        for (auto& state : groups_) {
            std::erase_if(state.views, [document](auto const& view) { return view.document == document; });
            if (std::ranges::find(state.views, state.selected, &WorkspaceEditorView::id) == state.views.end())
                state.selected = state.views.empty() ? 0 : state.views.front().id;
        }
        Normalize();
    }

    std::uint64_t WorkspaceEditorGroups::Move(std::uint64_t view, EditorGroup target, std::size_t index) {
        auto group = GroupOf(view);
        if (!group || (target == EditorGroup::Right && !IsSplit())) return 0;
        auto& from = State(*group);
        auto found = std::ranges::find(from.views, view, &WorkspaceEditorView::id);
        auto entry = *found;
        auto previousIndex = static_cast<std::size_t>(found - from.views.begin());
        if (*group == target) {
            index = (std::min)(index, from.views.size());
            if (index > previousIndex) --index;
            if (index != previousIndex) {
                from.views.erase(found);
                from.views.insert(from.views.begin() + index, entry);
            }
            return view;
        }
        Insert(target, entry, index);
        // Remove the original by iterator so both groups briefly owning the
        // same view ID cannot make a lookup select the receiving group.
        from.views.erase(found);
        if (from.selected == view)
            from.selected = from.views.empty() ? 0 : from.views[(std::min)(previousIndex, from.views.size() - 1)].id;
        Normalize();
        return view;
    }

    bool WorkspaceEditorGroups::Reorder(EditorGroup group, std::span<std::uint64_t const> order) {
        auto& views = State(group).views;
        if (order.size() != views.size()) return false;
        std::vector<WorkspaceEditorView> sorted;
        sorted.reserve(order.size());
        for (auto id : order) {
            auto found = std::ranges::find(views, id, &WorkspaceEditorView::id);
            if (found == views.end() || std::ranges::find(sorted, id, &WorkspaceEditorView::id) != sorted.end()) return false;
            sorted.push_back(*found);
        }
        views.swap(sorted);
        return true;
    }

    void WorkspaceEditorGroups::Normalize() {
        if (State(EditorGroup::Left).views.empty() && IsSplit()) {
            State(EditorGroup::Left) = std::exchange(State(EditorGroup::Right), {});
        }
        if (!IsSplit()) active_ = EditorGroup::Left;
    }
}
