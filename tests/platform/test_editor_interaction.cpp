#include <d2d1.h>
#include <dwrite.h>
#include <wrl.h>
#include "support/folia_test.hpp"

import folia.markdown.ids;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.platform.dwrite_factory;
import folia.platform.editor_display_mapping;
import folia.platform.editor_interaction;

using namespace boost::ut;
using namespace folia;
using namespace folia::platform;
using namespace folia::platform::editor;

namespace
{
    EditorDisplayMapping IdentityMapping(NodeId owner, std::size_t length)
    {
        EditorDisplayMapping mapping;
        for (std::size_t offset = 0; offset <= length; ++offset)
        {
            mapping.push_back({
                owner,
                offset,
                TextAffinity::Downstream,
                EditorDisplayPositionKind::Source});
        }
        return mapping;
    }

    Microsoft::WRL::ComPtr<IDWriteTextLayout> MakeLayout(
        std::wstring_view text,
        float width = 400.0f)
    {
        auto factory = create_dwrite_factory();
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
        auto formatResult = factory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            18.0f,
            L"en-US",
            &format);
        expect(SUCCEEDED(formatResult));

        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        auto layoutResult = factory->CreateTextLayout(
            text.data(),
            static_cast<UINT32>(text.size()),
            format.Get(),
            width,
            200.0f,
            &layout);
        expect(SUCCEEDED(layoutResult));
        return layout;
    }

    EditorVisualTable MakeEditableTable(NodeId owner)
    {
        EditorVisualTable table;
        table.rect = D2D1_RECT_F{0.0f, 0.0f, 200.0f, 100.0f};
        table.sourceSpans = {{owner, {0, 40}}};
        table.rowCount = 2;
        table.columnCount = 2;
        table.rowBoundaries = {0.0f, 50.0f, 100.0f};
        table.columnBoundaries = {0.0f, 100.0f, 200.0f};
        for (std::size_t row = 0; row < table.rowCount; ++row)
        {
            for (std::size_t column = 0; column < table.columnCount; ++column)
            {
                auto offset = (row * table.columnCount + column) * 10;
                table.cells.push_back(EditorVisualTableCell{
                    .sourceSpan = {owner, {offset, offset + 10}},
                    .row = row,
                    .column = column,
                });
            }
        }
        return table;
    }
}

suite editor_interaction_tests = [] {

"wrapped generated suggestions keep the caret at their original insertion boundary"_test = [] {
    for (auto prefix : {std::wstring{L"abc"}, std::wstring{L""}, std::wstring{L"a\n"}})
    for (auto affinity : {TextAffinity::Upstream, TextAffinity::Downstream})
    for (bool table : {false, true}) {
        auto owner = NodeId{709};
        const std::wstring ghost = L" a very long suggestion\nwith multiple lines that wrap";
        auto layout = MakeLayout(prefix + ghost + L"z", 90);
        auto mapping = IdentityMapping(owner, prefix.size());
        mapping.pop_back();
        for (std::size_t i = 0; i < ghost.size(); ++i)
            mapping.push_back({owner, prefix.size(), affinity, EditorDisplayPositionKind::GeneratedPreview});
        mapping.push_back({owner, prefix.size(), TextAffinity::Downstream});
        mapping.push_back({owner, prefix.size() + 1, TextAffinity::Downstream});
        EditorInteractionMap interaction;
        interaction.blocks.push_back(EditorVisualBlock{
            .rect = {0, 0, 90, 300}, .textOrigin = {0, 0}, .textWidth = 90,
            .sourceSpan = {owner, {0, prefix.size() + 1}},
            .displayToSource = mapping, .layout = layout});
        if (table) {
            EditorVisualTable cells;
            EditorVisualTableCell cell;
            cell.sourceSpan = {owner, {0, prefix.size() + 1}};
            cell.rect = {0, 0, 90, 300}; cell.displayToSource = mapping; cell.layout = layout;
            cells.cells.push_back(std::move(cell));
            interaction.tables.push_back(std::move(cells));
            interaction.AddTableCellLines(0, 0, 0);
        } else interaction.AddBlockLines(0);
        auto caret = interaction.CaretBounds({owner, prefix.size(), affinity}, 24);
        expect(fatal(caret.has_value()));
        auto baseline = MakeLayout(prefix + L"z", 90);
        float x{}, y{}; DWRITE_HIT_TEST_METRICS hit{};
        expect(SUCCEEDED(baseline->HitTestTextPosition(static_cast<UINT32>(prefix.size()), FALSE, &x, &y, &hit)));
        expect(std::abs(caret->top - y) < 0.1f);
        expect(std::abs(caret->left - x) < 0.1f);
    }
};

"generated control rows do not become editable caret lines"_test = [] {
    auto owner = NodeId{510};
    EditorInteractionMap interaction;
    interaction.blocks.push_back(EditorVisualBlock{
        .rect = D2D1_RECT_F{0, 0, 400, 100}, .textOrigin = D2D1_POINT_2F{0, 0},
        .textWidth = 400, .sourceSpan = {owner, {10, 10}}, .displayToSource = {{owner, 10, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
            {owner, 10, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
            {owner, 10, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
            {owner, 10, TextAffinity::Downstream, EditorDisplayPositionKind::Source}},
        .layout = MakeLayout(L"\u200B\n\u200B"),
    });
    interaction.AddBlockLines(0); expect(interaction.lines.size() == 1_u);
    for (auto affinity : {TextAffinity::Upstream, TextAffinity::Downstream}) {
        auto caret = interaction.CaretBounds({owner, 10, affinity}, 24);
        expect(caret.has_value()); if (caret) expect(caret->top > 0.f);
    }
};

"decorative padding cannot extend an empty source row into later lines"_test = [] {
    auto owner = NodeId{511};
    EditorInteractionMap interaction;
    interaction.blocks.push_back(EditorVisualBlock{
        .rect = D2D1_RECT_F{0, 0, 400, 140}, .textOrigin = D2D1_POINT_2F{0, 0},
        .textWidth = 400, .sourceSpan = {owner, {0, 6}}, .displayToSource = {{owner, 0, TextAffinity::Downstream},
            {owner, 2, TextAffinity::Downstream}, {owner, 3, TextAffinity::Downstream},
            {owner, 4, TextAffinity::Downstream},
            {owner, 3, TextAffinity::Upstream, EditorDisplayPositionKind::BoundaryDecoration},
            {owner, 3, TextAffinity::Upstream, EditorDisplayPositionKind::BoundaryDecoration},
            {owner, 3, TextAffinity::Upstream, EditorDisplayPositionKind::BoundaryDecoration}},
        .layout = MakeLayout(L"$\n\n$\n\uFFFC"),
    });
    interaction.AddBlockLines(0);
    expect(fatal(interaction.lines.size() == 3_u));
    expect(std::ranges::equal(interaction.lines[1].SourceSpans(), std::vector<TextSpan>{{owner, {3, 3}}}));
    expect(std::ranges::equal(interaction.lines[2].SourceSpans(), std::vector<TextSpan>{{owner, {4, 4}}}));
    for (auto affinity : {TextAffinity::Upstream, TextAffinity::Downstream}) {
        auto caret = interaction.CaretBounds({owner, 3, affinity}, 24);
        expect(fatal(caret.has_value()));
        expect(std::abs(caret->top - interaction.lines[1].rect.top) < .5f);
    }
};

"an empty source boundary after indentation remains editable"_test = [] {
    auto owner = NodeId{512};
    EditorInteractionMap interaction;
    interaction.blocks.push_back(EditorVisualBlock{
        .rect = D2D1_RECT_F{0, 0, 400, 40}, .textOrigin = D2D1_POINT_2F{0, 0},
        .textWidth = 400, .sourceSpan = {NodeId{513}, {0, 0}}, .displayToSource = {{owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
            {owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
            {owner, 0, TextAffinity::Downstream}},
        .layout = MakeLayout(L"  "),
    });
    interaction.AddBlockLines(0);
    expect(fatal(interaction.lines.size() == 1_u));
    expect(std::ranges::equal(interaction.lines.front().SourceSpans(), std::vector<TextSpan>{{owner, {0, 0}}}));
    expect(interaction.CaretBounds({owner, 0, TextAffinity::Downstream}, 24).has_value());
};

"suffix decoration retains the final caret boundary of preceding text"_test = [] {
    auto owner = NodeId{514};
    EditorInteractionMap interaction;
    interaction.blocks.push_back(EditorVisualBlock{
        .rect = D2D1_RECT_F{0, 0, 400, 80}, .textOrigin = D2D1_POINT_2F{0, 0},
        .textWidth = 400, .sourceSpan = {owner, {10, 11}}, .displayToSource = {{owner, 10, TextAffinity::Downstream},
            {owner, 11, TextAffinity::Upstream, EditorDisplayPositionKind::BoundaryDecoration},
            {owner, 11, TextAffinity::Upstream, EditorDisplayPositionKind::BoundaryDecoration},
            {owner, 11, TextAffinity::Upstream, EditorDisplayPositionKind::BoundaryDecoration}},
        .layout = MakeLayout(L"a\n\uFFFC"),
    });
    interaction.AddBlockLines(0);
    expect(fatal(interaction.lines.size() == 1_u));
    expect(std::ranges::equal(interaction.lines.front().SourceSpans(), std::vector<TextSpan>{{owner, {10, 11}}}));
    expect(interaction.CaretBounds({owner, 11, TextAffinity::Downstream}, 24).has_value());
};

"directwrite block geometry keeps source coordinates authoritative"_test = [] {
    constexpr auto length = std::size_t{10};
    const auto owner = NodeId{501};
    EditorInteractionMap interaction;
    interaction.blocks.push_back(EditorVisualBlock{
        .rect = D2D1_RECT_F{10.0f, 20.0f, 410.0f, 80.0f},
        .textOrigin = D2D1_POINT_2F{10.0f, 20.0f},
        .textWidth = 400.0f,
        .sourceSpan = {owner, {0, length}},
        .documentY = 20.0f,
        .displayToSource = IdentityMapping(owner, length),
        .layout = MakeLayout(L"alpha beta"),
    });

    interaction.AddBlockLines(0);
    expect(interaction.lines.size() == 1_u);
    expect(interaction.lines.front().SourceSpans().size() == 1_u);
    expect(interaction.lines.front().SourceSpans().front() == TextSpan{owner, {0, length}});

    auto start = interaction.CaretBounds({owner, 0, TextAffinity::Downstream}, 24.0f);
    auto middle = interaction.CaretBounds({owner, 5, TextAffinity::Downstream}, 24.0f);
    auto end = interaction.CaretBounds({owner, length, TextAffinity::Upstream}, 24.0f);
    expect(start.has_value());
    expect(middle.has_value());
    expect(end.has_value());
    expect(start->left < middle->left);
    expect(middle->left < end->left);

    auto hit = interaction.HitTest(middle->left, (middle->top + middle->bottom) * 0.5f);
    expect(hit.has_value());
    expect(hit->container_id == owner);
    expect(hit->source_offset == 5_u);

    expect(interaction.VisualLineStart({owner, 5, TextAffinity::Downstream})
        == TextPosition{owner, 0, TextAffinity::Downstream});
    expect(interaction.VisualLineEnd({owner, 5, TextAffinity::Downstream})
        == TextPosition{owner, length, TextAffinity::Upstream});
};

"directwrite visual movement crosses explicit lines without serialized offsets"_test = [] {
    constexpr auto length = std::size_t{10};
    const auto owner = NodeId{502};
    EditorInteractionMap interaction;
    interaction.blocks.push_back(EditorVisualBlock{
        .rect = D2D1_RECT_F{30.0f, 40.0f, 330.0f, 120.0f},
        .textOrigin = D2D1_POINT_2F{30.0f, 40.0f},
        .textWidth = 300.0f,
        .sourceSpan = {owner, {0, length}},
        .documentY = 40.0f,
        .displayToSource = IdentityMapping(owner, length),
        .layout = MakeLayout(L"alpha\nbeta", 300.0f),
    });

    interaction.AddBlockLines(0);
    expect(interaction.lines.size() == 2_u);

    float goalX = -1.0f;
    auto moved = interaction.MoveCaretVertically(
        {owner, 2, TextAffinity::Downstream},
        true,
        goalX,
        24.0f);
    expect(moved.has_value());
    expect(moved->container_id == owner);
    expect(moved->source_offset >= 6_u);
    expect(moved->source_offset <= length);
    expect(goalX >= 30.0f);
};

"table text range geometry preserves partial source selections"_test = [] {
    constexpr auto length = std::size_t{10};
    const auto owner = NodeId{505};
    EditorInteractionMap interaction;
    interaction.blocks.push_back(EditorVisualBlock{
        .rect = D2D1_RECT_F{20.0f, 30.0f, 420.0f, 90.0f},
        .sourceSpan = {owner, {0, length}},
    });

    EditorVisualTable table;
    table.rect = D2D1_RECT_F{20.0f, 30.0f, 420.0f, 90.0f};
    table.sourceSpans = {{owner, {0, length}}};
    table.rowCount = 1;
    table.columnCount = 1;
    table.rowBoundaries = {30.0f, 90.0f};
    table.columnBoundaries = {20.0f, 420.0f};
    table.cells.push_back(EditorVisualTableCell{
        .rect = table.rect,
        .textOrigin = D2D1_POINT_2F{30.0f, 40.0f},
        .textWidth = 380.0f,
        .sourceSpan = {owner, {0, length}},
        .displayToSource = IdentityMapping(owner, length),
        .layout = MakeLayout(L"alpha beta", 380.0f),
    });
    interaction.tables.push_back(std::move(table));
    interaction.AddTableCellLines(0, 0, 0);

    auto const whole = interaction.TextRangeBounds({owner, {0, length}});
    auto const partial = interaction.TextRangeBounds({owner, {2, 5}});
    expect(fatal(!whole.empty()));
    expect(fatal(!partial.empty()));
    if (whole.empty() || partial.empty()) return;
    expect(partial.front().left > whole.front().left);
    expect(partial.front().right < whole.front().right);
    expect(partial.front().top == whole.front().top);
    expect(partial.front().bottom == whole.front().bottom);
};

"table hit policy returns semantic actions without renderer ownership"_test = [] {
    const auto owner = NodeId{503};
    EditorInteractionMap interaction;
    interaction.tables.push_back(MakeEditableTable(owner));

    auto insertRow = interaction.TableActionAt(50.0f, 0.0f);
    expect(insertRow.has_value());
    expect(insertRow->kind == EditorTableActionKind::InsertRow);
    expect(insertRow->index == 0_u);
    expect(insertRow->sourcePosition == TextPosition{owner, 0, TextAffinity::Downstream});

    auto insertColumn = interaction.TableActionAt(0.0f, 25.0f);
    expect(insertColumn.has_value());
    expect(insertColumn->kind == EditorTableActionKind::InsertColumn);
    expect(insertColumn->index == 0_u);

    auto dragRow = interaction.TableActionAt(-40.0f, 25.0f);
    expect(dragRow.has_value());
    expect(dragRow->kind == EditorTableActionKind::DragRow);
    expect(dragRow->index == 0_u);

    auto deleteRow = interaction.TableActionAt(-15.0f, 75.0f);
    expect(deleteRow.has_value());
    expect(deleteRow->kind == EditorTableActionKind::DeleteRow);
    expect(deleteRow->index == 1_u);

    auto dragColumn = interaction.TableActionAt(40.0f, -18.0f);
    expect(dragColumn.has_value());
    expect(dragColumn->kind == EditorTableActionKind::DragColumn);
    expect(dragColumn->index == 0_u);

    auto deleteColumn = interaction.TableActionAt(60.0f, -18.0f);
    expect(deleteColumn.has_value());
    expect(deleteColumn->kind == EditorTableActionKind::DeleteColumn);
    expect(deleteColumn->index == 0_u);

    // Cell interiors always belong to text hit-testing. Structural actions
    // are available only in the explicit margins and boundary handles.
    expect(!interaction.TableActionAt(50.0f, 25.0f).has_value());
    expect(!interaction.TableActionAt(150.0f, 75.0f).has_value());
};

"table drop policy stays within the source table and tolerates incomplete geometry"_test = [] {
    const auto owner = NodeId{504};
    EditorInteractionMap interaction;
    interaction.tables.push_back(MakeEditableTable(owner));

    EditorTableAction dragged{
        EditorTableActionKind::DragRow,
        TextPosition{owner, 0, TextAffinity::Downstream},
        0,
    };
    expect(interaction.TableDropIndexAt(dragged, 50.0f, 80.0f, true) == std::optional<std::size_t>{2});
    expect(interaction.TableDropIndexAt(dragged, 130.0f, 25.0f, false) == std::optional<std::size_t>{1});

    auto foreign = dragged;
    foreign.sourcePosition.container_id = NodeId{999};
    expect(!interaction.TableDropIndexAt(foreign, 50.0f, 80.0f, true).has_value());

    EditorVisualTable incomplete;
    incomplete.editable = true;
    incomplete.rowCount = 1;
    incomplete.columnCount = 1;
    interaction.tables = {std::move(incomplete)};
    expect(!interaction.TableActionAt(0.0f, 0.0f).has_value());
    expect(!interaction.TableDropIndexAt(std::nullopt, 0.0f, 0.0f, true).has_value());
};

"raw block controls are resolved by semantic owner and cleared per frame"_test = [] {
    const auto codeOwner = NodeId{601};
    const auto mathOwner = NodeId{602};
    EditorInteractionMap interaction;
    interaction.rawBlocks.push_back(EditorVisualRawBlock{
        .rect = D2D1_RECT_F{10.0f, 20.0f, 410.0f, 140.0f},
        .headerRect = D2D1_RECT_F{10.0f, 20.0f, 410.0f, 48.0f},
        .contentStart = {codeOwner, 7, TextAffinity::Downstream},
        .kind = EditorRawBlockKind::Code,
        .language = std::string{"cpp"},
        .languageEditable = true,
    });
    interaction.rawBlocks.push_back(EditorVisualRawBlock{
        .rect = D2D1_RECT_F{10.0f, 160.0f, 410.0f, 260.0f},
        .headerRect = D2D1_RECT_F{10.0f, 160.0f, 410.0f, 160.0f},
        .contentStart = {mathOwner, 3, TextAffinity::Downstream},
        .kind = EditorRawBlockKind::Math,
    });

    auto code = interaction.RawBlockFor(codeOwner);
    auto math = interaction.RawBlockFor(mathOwner);
    expect(fatal(code.has_value()));
    expect(fatal(math.has_value()));
    expect(code->kind == EditorRawBlockKind::Code);
    expect(code->language == std::optional<std::string>{"cpp"});
    expect(code->languageEditable);
    expect(code->contentStart.source_offset == 7_u);
    expect(math->kind == EditorRawBlockKind::Math);
    expect(!interaction.RawBlockFor(NodeId{603}).has_value());

    interaction.Clear();
    expect(!interaction.RawBlockFor(codeOwner).has_value());
    expect(!interaction.RawBlockFor(mathOwner).has_value());
};


"cached line allocation agrees with native glyph baselines far into a paragraph"_test = [] {
    struct Baselines final : Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextRenderer> {
        std::vector<std::pair<UINT32, float>> values;
        HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override {
            *disabled = TRUE; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override {
            *transform = {1, 0, 0, 1, 0, 0}; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixels) override { *pixels = 1; return S_OK; }
        HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT, FLOAT baseline, DWRITE_MEASURING_MODE,
            DWRITE_GLYPH_RUN const*, DWRITE_GLYPH_RUN_DESCRIPTION const* description, IUnknown*) override {
            if (description) values.emplace_back(description->textPosition, baseline); return S_OK;
        }
        HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, DWRITE_UNDERLINE const*, IUnknown*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, DWRITE_STRIKETHROUGH const*, IUnknown*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override { return E_NOTIMPL; }
    };
    std::wstring text;
    for (int i = 0; i < 10000; ++i) text += L"sample line\n";
    auto layout = MakeLayout(text);
    expect(fatal(SUCCEEDED(layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 23.4f, 18.1f))));
    auto mapping = IdentityMapping(NodeId{701}, text.size());
    auto lines = BuildEditorTextLines(layout.Get(), mapping, {NodeId{701}, {0, text.size()}}, 400);
    expect(fatal(lines && lines->size() == 10001));
    std::vector<DWRITE_LINE_METRICS> metrics(lines->size());
    UINT32 count = 0;
    expect(fatal(SUCCEEDED(layout->GetLineMetrics(metrics.data(), static_cast<UINT32>(metrics.size()), &count))));
    auto renderer = Microsoft::WRL::Make<Baselines>();
    expect(fatal(SUCCEEDED(layout->Draw(nullptr, renderer.Get(), 0, 0))));
    expect(fatal(!renderer->values.empty()));
    for (auto index : {std::size_t{1}, std::size_t{5000}, std::size_t{9999}}) {
        auto const& line = (*lines)[index];
        auto native = std::ranges::find(renderer->values, line.displayStart,
            &std::pair<UINT32, float>::first);
        expect(fatal(native != renderer->values.end()));
        expect(std::abs(line.rect.top + metrics[index].baseline - native->second) < 0.01f)
            << index << " cached baseline " << line.rect.top + metrics[index].baseline
            << " native " << native->second;
        expect(std::abs(line.rect.bottom - line.rect.top - metrics[index].height) < 0.01f);
    }
};

}; // suite editor_interaction_tests
