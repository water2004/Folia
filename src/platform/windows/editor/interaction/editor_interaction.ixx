// folia.platform.editor_interaction — DirectWrite-backed visual geometry and hit testing.
module;
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

export module folia.platform.editor_interaction;
import std;
import folia.markdown.text_edit;
import folia.markdown.table_metadata;
export import folia.platform.editor_display_mapping;
export import folia.platform.editor_text_paint;

export namespace folia::platform::editor
{
    struct EditorVisualLine
    {
        std::size_t blockIndex = 0;
        std::size_t tableIndex = (std::numeric_limits<std::size_t>::max)();
        std::size_t cellIndex = (std::numeric_limits<std::size_t>::max)();
        std::shared_ptr<const std::vector<folia::TextSpan>> sourceSpans;
        std::span<const folia::TextSpan> SourceSpans() const {
            return sourceSpans ? std::span<const folia::TextSpan>{*sourceSpans} : std::span<const folia::TextSpan>{};
        }
        std::uint32_t displayStart = 0;
        std::uint32_t displayEnd = 0;
        bool wrapContinuation = false;
        D2D1_RECT_F rect{};
    };

    using EditorLayoutLines = std::shared_ptr<const std::vector<EditorVisualLine>>;
    EditorLayoutLines BuildEditorTextLines(IDWriteTextLayout*, EditorDisplayMapping const&, folia::TextSpan, float width);

    struct EditorVisualBlock
    {
        D2D1_RECT_F rect{};
        D2D1_POINT_2F textOrigin{};
        float textWidth = 0.0f;
        folia::TextSpan sourceSpan;
        float documentY = 0.0f;
        EditorDisplayMapping displayToSource;
        ::Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        EditorLayoutLines layoutLines;
        std::shared_ptr<const EditorTextPaintPlan> paintPlan;
        bool thematicBreak = false;
    };

    struct EditorVisualTableCell
    {
        D2D1_RECT_F rect{};
        D2D1_POINT_2F textOrigin{};
        float textWidth = 0.0f;
        float textHeight = 0.0f;
        folia::TextSpan sourceSpan;
        std::size_t row = 0;
        std::size_t column = 0;
        std::size_t rowSpan = 1;
        std::size_t columnSpan = 1;
        bool covered = false;
        std::optional<bool> checkbox;
        D2D1_RECT_F checkboxRect{};
        EditorDisplayMapping displayToSource;
        ::Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        EditorLayoutLines layoutLines;
        std::shared_ptr<const EditorTextPaintPlan> paintPlan;
    };

    class EditorTableAxis
    {
    public:
        EditorTableAxis& operator=(std::vector<float> edges)
        {
            local = std::make_shared<const std::vector<float>>(std::move(edges));
            origin = 0;
            return *this;
        }
        std::span<const float> Local() const { return local ? std::span<const float>{*local} : std::span<const float>{}; }
        std::size_t size() const { return Local().size(); }
        float operator[](std::size_t index) const { return Local()[index] + origin; }
        float back() const { return Local().back() + origin; }
        void Translate(float offset) { origin += offset; }
        float ToLocal(float point) const { return point - origin; }
        std::size_t Interval(float point) const;
        std::size_t Nearest(float point) const;

    private:
        std::shared_ptr<const std::vector<float>> local;
        float origin = 0;
    };

    struct EditorVisualTable
    {
        D2D1_RECT_F rect{};
        folia::NodeId id{};
        std::shared_ptr<const std::vector<folia::TextSpan>> sourceSpans;
        std::size_t rowCount = 0;
        std::size_t columnCount = 0;
        bool editable = true;
        EditorTableAxis rowBoundaries;
        EditorTableAxis columnBoundaries;
        std::vector<EditorVisualTableCell> cells;
        std::optional<folia::TableRegion> selection;
        std::optional<D2D1_POINT_2F> selectionHandle;
        std::size_t hiddenRows = 0;
        D2D1_RECT_F foldBar{};
        float horizontalOffset = 0;
        float maximumHorizontalOffset = 0;
        float viewportWidth = 0;
        float frozenWidth = 0;
        std::size_t headerRows = 0;
        float stickyOffset = 0;
        std::size_t dataEnd = 0;

        std::span<const folia::TextSpan> SourceSpans() const
        {
            return sourceSpans ? std::span<const folia::TextSpan>{*sourceSpans} : std::span<const folia::TextSpan>{};
        }
    };

    struct EditorVisualMathHit
    {
        D2D1_RECT_F rect{};
        folia::TextSpan sourceSpan;
        float progressStart = 0.0f;
        float progressEnd = 1.0f;
    };

    struct EditorVisualTaskCheckboxHit
    {
        D2D1_RECT_F rect{};
        folia::TextPosition sourcePosition;
    };

    struct EditorVisualFootnoteHit
    {
        using Kind = EditorFootnoteControlKind;

        D2D1_RECT_F rect{};
        folia::TextSpan sourceSpan;
        std::string label;
        Kind kind = Kind::Reference;
    };

    enum class EditorRawBlockKind
    {
        Code,
        Math,
    };

    struct EditorVisualRawBlock
    {
        D2D1_RECT_F rect{};
        D2D1_RECT_F headerRect{};
        folia::TextPosition contentStart;
        EditorRawBlockKind kind = EditorRawBlockKind::Code;
        std::optional<std::string> language;
        bool languageEditable = false;
    };

    enum class EditorTableActionKind
    {
        InsertRow,
        InsertColumn,
        DragRow,
        DragColumn,
        ResizeColumn,
        Menu,
        Fill,
        Unfold,
        Checkbox,
    };

    struct EditorTableAction
    {
        EditorTableActionKind kind = EditorTableActionKind::Menu;
        folia::TextPosition sourcePosition;
        std::size_t index = 0;
        float width = 0;
        folia::NodeId tableId{};
        bool draggable = true;

        bool operator==(EditorTableAction const&) const = default;
    };

    struct EditorTableControl
    {
        EditorTableAction action;
        D2D1_RECT_F bounds;
        bool hot = false;

        bool operator==(EditorTableControl const& other) const
        {
            return action == other.action && hot == other.hot
                && bounds.left == other.bounds.left && bounds.right == other.bounds.right
                && bounds.top == other.bounds.top && bounds.bottom == other.bounds.bottom;
        }
    };

    struct EditorTableControls
    {
        std::array<EditorTableControl, 10> items{};
        std::size_t count = 0;
        std::span<const EditorTableControl> Visible() const { return {items.data(), count}; }
        const EditorTableControl* Hit() const;
    };

    EditorTableControls TableControlsAt(EditorVisualTable const& table, float x, float y);

    class EditorTableGesture
    {
    public:
        enum class Phase { Pressed, Dragging };
        EditorTableGesture(std::uint32_t pointer, float x, float y) : pointer(pointer), x(x), y(y) {}
        bool Owns(std::uint32_t id) const { return id == pointer; }
        Phase Update(float xNow, float yNow)
        {
            const auto dx = xNow - x;
            const auto dy = yNow - y;
            if (dx * dx + dy * dy >= 25) phase = Phase::Dragging;
            return phase;
        }
        float DeltaX(float value) const { return value - x; }

    private:
        std::uint32_t pointer;
        float x, y;
        Phase phase = Phase::Pressed;
    };

    struct EditorInteractionMap
    {
        void Clear();
        void AddBlockLines(std::size_t blockIndex);
        void AddTableCellLines(std::size_t blockIndex, std::size_t tableIndex, std::size_t cellIndex);
        std::optional<folia::TextPosition> HitTest(float x, float y, bool characterStart = false,
            folia::NodeId tableTextOwner = {}) const;
        std::optional<folia::TextPosition> TaskCheckboxAt(float x, float y) const;
        std::optional<EditorVisualFootnoteHit> FootnoteAt(float x, float y) const;
        std::optional<EditorVisualRawBlock> RawBlockFor(folia::NodeId owner) const;
        std::vector<D2D1_RECT_F> TextRangeBounds(folia::TextSpan span) const;
        EditorTableControls TableControlsAt(float x, float y) const;
        std::optional<std::pair<std::size_t, std::size_t>> TableCellAt(float x, float y) const;
        std::optional<folia::TableCoordinate> TableFillTargetAt(folia::NodeId table, float x, float y) const;
        std::optional<std::size_t> TableDropIndexAt(
            std::optional<EditorTableAction> const& draggedAction,
            float x,
            float y,
            bool rows) const;
        std::optional<D2D1_RECT_F> CaretBounds(folia::TextPosition position, float bodyLineHeight) const;
        std::optional<folia::TextPosition> MoveCaretVertically(folia::TextPosition position, bool down, float& goalX, float bodyLineHeight) const;
        std::optional<folia::TextPosition> VisualLineStart(folia::TextPosition position) const;
        std::optional<folia::TextPosition> VisualLineEnd(folia::TextPosition position) const;

        std::vector<EditorVisualBlock> blocks;
        std::vector<EditorVisualLine> lines;
        std::vector<EditorVisualTable> tables;
        std::vector<EditorVisualMathHit> mathHits;
        std::vector<EditorVisualTaskCheckboxHit> taskCheckboxHits;
        std::vector<EditorVisualFootnoteHit> footnoteHits;
        std::vector<EditorVisualRawBlock> rawBlocks;

    private:
        std::optional<std::size_t> LineIndexFor(folia::TextPosition position) const;
        std::optional<D2D1_RECT_F> CaretRectOnLine(EditorVisualLine const& line, folia::TextPosition position, float bodyLineHeight) const;
    };
}
