#pragma once

import folia.markdown.text_edit;

namespace winrt::Folia {
    enum class EditorGroup : std::size_t { Left, Right };
    constexpr EditorGroup OtherEditorGroup(EditorGroup group) {
        return group == EditorGroup::Left ? EditorGroup::Right : EditorGroup::Left;
    }

    struct WorkspaceViewport {
        std::optional<folia::TextPosition> position;
        float offset = 0;
        float inset = 0;
    };

    struct WorkspaceEditorView {
        std::uint64_t id;
        std::uint64_t document;
        WorkspaceViewport viewport;
        std::optional<folia::TextSelection> selection;
    };

    // The document store owns each editing session exactly once. Groups own
    // only view identity, order, selection and the viewport for a shared view.
    class WorkspaceEditorGroups {
    public:
        std::span<WorkspaceEditorView const> Views(EditorGroup group) const;
        bool IsSplit() const;
        EditorGroup ActiveGroup() const { return active_; }
        std::uint64_t Selected(EditorGroup group) const;
        std::optional<EditorGroup> GroupOf(std::uint64_t view) const;
        WorkspaceEditorView* Find(std::uint64_t view);
        WorkspaceEditorView const* Find(std::uint64_t view) const;
        std::uint64_t ViewFor(EditorGroup group, std::uint64_t document) const;
        std::size_t ViewCount(std::uint64_t document) const;
        std::uint64_t Add(EditorGroup group, std::uint64_t document, std::size_t index = std::string::npos);
        void Insert(EditorGroup group, WorkspaceEditorView view, std::size_t index);
        std::uint64_t Split();
        void Select(std::uint64_t view);
        void Remove(std::uint64_t view);
        void ForgetDocument(std::uint64_t document);
        std::uint64_t Move(std::uint64_t view, EditorGroup group, std::size_t index);
        bool Reorder(EditorGroup group, std::span<std::uint64_t const> order);
        std::uint64_t Replacement(std::uint64_t view, int direction = 1) const;

    private:
        struct Group {
            std::vector<WorkspaceEditorView> views;
            std::uint64_t selected = 0;
        };
        Group& State(EditorGroup group) { return groups_[static_cast<std::size_t>(group)]; }
        Group const& State(EditorGroup group) const { return groups_[static_cast<std::size_t>(group)]; }
        void Normalize();

        std::array<Group, 2> groups_;
        EditorGroup active_ = EditorGroup::Left;
    };
}
