/*************************************************************
 * MathJax 4.1.3 output/common/Wrappers/mtable.ts.
 *************************************************************/
module folia.math.output.common;
import :mtable;
import :mtd;
import :mo;
import :output_jax;
import folia.math.core.mml_tree.attributes;
import folia.math.core.mml_tree.mml_nodes.mtable;
import folia.math.util.js_numeric_input;
import folia.math.util.js_number;
import folia.math.util.string;
import folia.math.util.utf16;
import std;

namespace folia::math::output::common {
namespace {
using mml::AttributeNull;
using mml::AttributeValue;

double sourceMax(double left, double right) {
    return std::isnan(left) || std::isnan(right) ? std::numeric_limits<double>::quiet_NaN()
                                                 : std::max(left, right);
}

std::string tableLineClass(std::string_view style) {
    return "mjx-" + std::string(style);
}

scene::Paint tableLinePaint(double thickness, std::string_view style) {
    scene::Paint paint;
    paint.fill = scene::NoPaint{};
    paint.stroke = scene::InheritedPaint{};
    paint.stroke_width = thickness;
    if (style == "dashed")
        paint.dash_array = std::vector<double>{2 * thickness};
    else if (style == "dotted") {
        paint.dash_array = std::vector<double>{0, 2 * thickness};
        paint.line_cap = scene::StrokeCap::round;
    }
    return paint;
}

scene::SemanticMetadata tableLineMetadata(std::string_view marker, std::string_view value,
                                          std::string_view style) {
    scene::SemanticMetadata metadata;
    metadata.ordered_attributes.push_back({std::string(marker), std::string(value)});
    metadata.ordered_attributes.push_back({"class", tableLineClass(style)});
    return metadata;
}

scene::ViewportAspectRatio tableAspect(scene::ViewportAlignment horizontal) {
    return {horizontal, scene::ViewportAlignment::mid, scene::ViewportFit::meet};
}

const AttributeValue *attr(CommonMtable const &table, std::u16string_view name) {
    return table.node->attributes().lookup(name);
}

bool truthy(const AttributeValue *value) {
    if (!value || std::holds_alternative<AttributeNull>(*value))
        return false;
    if (auto p = std::get_if<bool>(value))
        return *p;
    if (auto p = std::get_if<double>(value))
        return *p != 0 && !std::isnan(*p);
    return !std::get<std::u16string>(*value).empty();
}

std::u16string text16(const AttributeValue *value, std::u16string_view fallback = {}) {
    if (!value || std::holds_alternative<AttributeNull>(*value))
        return std::u16string(fallback);
    if (auto p = std::get_if<std::u16string>(value))
        return *p;
    if (auto p = std::get_if<bool>(value))
        return *p ? u"true" : u"false";
    const auto number = js_number::toString(std::get<double>(*value));
    std::u16string result;
    result.reserve(number.size());
    for (const auto c : number)
        result.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
    return result;
}

std::string text8(const AttributeValue *value, std::u16string_view fallback = {}) {
    return utf16::encode(text16(value, fallback));
}

LengthInput lengthInput(const AttributeValue *value, std::u16string_view fallback = u"medium") {
    if (!value || std::holds_alternative<AttributeNull>(*value))
        return std::u16string(fallback);
    if (auto p = std::get_if<double>(value))
        return *p;
    if (auto p = std::get_if<std::u16string>(value))
        return *p;
    return std::u16string(std::get<bool>(*value) ? u"1" : u"0");
}

std::vector<std::string> words(std::u16string_view value) {
    std::vector<std::string> result;
    for (const auto &part : string_util::split(value))
        result.push_back(utf16::encode(part));
    return result;
}

bool percentText(std::string_view value) {
    return !value.empty() && value.back() == '%';
}

double sum(std::span<const double> values) {
    return std::accumulate(values.begin(), values.end(), 0.0);
}

double widthValue(ColumnWidth const &width, double automatic = 0) {
    if (const auto value = std::get_if<double>(&width))
        return *value;
    return automatic;
}

LengthInput lengthInput(std::string_view text) {
    return utf16::decode(text).text;
}
} // namespace

CommonMtable::CommonMtable(CommonWrapperFactory &factory, mml::MmlNode &source,
                           CommonWrapper *parentValue)
    : CommonWrapper(factory, source, parentValue) {}

std::vector<CommonMtr *> CommonMtable::tableRows() const {
    std::vector<CommonMtr *> rows;
    rows.reserve(childNodes.size());
    for (auto *child : childNodes) {
        auto *row = dynamic_cast<CommonMtr *>(child);
        if (!row)
            throw std::invalid_argument("mtable child wrapper is not an mtr");
        rows.push_back(row);
    }
    return rows;
}

std::array<double, 2> CommonMtable::getRowHD(bool equal, double HD, double H, double D) const {
    return equal ? std::array<double, 2>{(HD + H - D) / 2, (HD - H + D) / 2}
                 : std::array<double, 2>{H, D};
}

std::vector<CommonMtable::EmittedRow>
CommonMtable::placeSceneRows(scene::Builder &builder, scene::NodeId tableNode,
                             std::optional<scene::NodeId> labelParent) {
    const auto rows = tableRows();
    const auto &table = getTableData();
    const bool equalRows = truthy(attr(*this, u"equalrows"));
    const double equalHeight = getEqualRowHeight();
    const auto rowSpace = getRowHalfSpacing();
    std::vector<double> rowLines{fLine};
    rowLines.insert(rowLines.end(), rLines.begin(), rLines.end());
    rowLines.push_back(fLine);
    if (rowSpace.size() < numRows + 1 || rowLines.size() < numRows + 1 ||
        table.H.size() < numRows || table.D.size() < numRows)
        throw std::logic_error("mtable layout arrays do not cover every source row");

    std::vector<EmittedRow> emittedRows;
    emittedRows.reserve(rows.size());
    const std::array<scene::NodeId, 1> rowParents{tableNode};
    double rowY = getBBox().h - rowLines[0];
    for (std::size_t i = 0; i < numRows; ++i) {
        auto *row = rows.at(i);
        const auto [h, d] = getRowHD(equalRows, equalHeight, table.H[i], table.D[i]);
        const TableRowGeometry geometry{
            h, d, rowSpace[i], rowSpace[i + 1], rowLines[i], rowLines[i + 1]};
        auto rowScene = row->emitTableScene(builder, rowParents, geometry, labelParent);
        row->placeScene(builder, rowScene.nodes, 0, rowY - rowSpace[i] - h);
        emittedRows.push_back({row, geometry, std::move(rowScene)});
        rowY -= rowSpace[i] + h + d + rowSpace[i + 1] + rowLines[i + 1];
    }
    return emittedRows;
}

void CommonMtable::handleSceneColumnLines(scene::Builder &builder, scene::NodeId tableNode) {
    const auto widths = getComputedWidths();
    const auto columnSpace = getColumnHalfSpacing();
    if (auto lines = getColumnAttributes("columnlines")) {
        double x = fLine;
        for (std::size_t i = 0; i < lines->size(); ++i) {
            x += columnSpace.at(i) + widths.at(i) + columnSpace.at(i + 1);
            const auto thickness = cLines.at(i);
            if ((*lines)[i] != "none") {
                const auto style = (*lines)[i];
                const double dt = style == "dotted" ? thickness / 2 : 0;
                const double lineX = x + thickness / 2;
                scene::Path path;
                path.element = scene::PathElement::line;
                path.role = scene::ElementRole::table_vertical_line;
                path.paint = tableLinePaint(thickness, style);
                path.metadata = tableLineMetadata("data-line", "v", style);
                path.geometry.segments = {scene::MoveTo{{lineX, dt - getBBox().d}},
                                          scene::LineTo{{lineX, getBBox().h - dt}}};
                builder.add_primitive(tableNode, std::move(path));
            }
            x += thickness;
        }
    }
}

void CommonMtable::handleSceneRowLines(scene::Builder &builder, scene::NodeId tableNode) {
    if (auto lines = getRowAttributes("rowlines")) {
        const auto &table = getTableData();
        const bool equalRows = truthy(attr(*this, u"equalrows"));
        const double equalHeight = getEqualRowHeight();
        const auto rowSpace = getRowHalfSpacing();
        double y = getBBox().h - fLine;
        for (std::size_t i = 0; i < lines->size(); ++i) {
            const auto [h, d] = getRowHD(equalRows, equalHeight, table.H.at(i), table.D.at(i));
            y -= rowSpace.at(i) + h + d + rowSpace.at(i + 1);
            const auto thickness = rLines.at(i);
            if ((*lines)[i] != "none") {
                const auto style = (*lines)[i];
                const double dt = style == "dotted" ? thickness / 2 : 0;
                const double lineY = y - thickness / 2;
                scene::Path path;
                path.element = scene::PathElement::line;
                path.role = scene::ElementRole::table_horizontal_line;
                path.paint = tableLinePaint(thickness, style);
                path.metadata = tableLineMetadata("data-line", "h", style);
                path.geometry.segments = {scene::MoveTo{{dt, lineY}},
                                          scene::LineTo{{getBBox().w - dt, lineY}}};
                builder.add_primitive(tableNode, std::move(path));
            }
            y -= thickness;
        }
    }
}

void CommonMtable::handleSceneFrame(scene::Builder &builder, scene::NodeId tableNode) {
    if (frame && fLine) {
        const auto box = getBBox();
        scene::Rule rule;
        rule.bounds = {fLine / 2, fLine / 2 - box.d, box.w - fLine, box.h + box.d - fLine};
        const auto style = text8(attr(*this, u"frame"));
        rule.paint = tableLinePaint(fLine, style);
        rule.metadata = tableLineMetadata("data-frame", "true", style);
        rule.role = scene::ElementRole::table_frame;
        builder.add_primitive(tableNode, std::move(rule));
    }
}

double CommonMtable::handleScenePWidth(scene::Builder &builder, scene::NodeId tableNode) {
    if (!pWidth)
        return 0;
    const auto box = getBBox();
    if (!container || !containerI)
        throw std::logic_error("percentage-width mtable requires its Source container");
    const double widthWithLabels = box.L + pWidth + box.R;
    const auto alignment = getAlignShift().first;
    const double maximum =
        sourceMax(isTop ? widthWithLabels : 0, container->getWrapWidth(*containerI));
    const double contentWidth = maximum - box.L - box.R;
    const double difference = box.w - (pWidth > contentWidth ? contentWidth : pWidth);
    const double shift = alignment == "left"    ? 0
                         : alignment == "right" ? difference
                                                : difference / 2;
    if (!std::isnan(shift) && shift != 0) {
        const auto inner = builder.wrap_contents(tableNode);
        const std::array<SceneNode, 1> emittedInner{SceneNode{inner}};
        placeScene(builder, emittedInner, shift, 0, inner);
    }
    return shift;
}

void CommonMtable::spaceSceneLabels(scene::Builder &builder,
                                    std::span<EmittedRow const> emittedRows) {
    double y = getBBox().h - fLine;
    const double labelWidth = getTableData().L;
    const auto rowSpace = getRowHalfSpacing();
    for (std::size_t i = 0; i < emittedRows.size(); ++i) {
        const auto &entry = emittedRows[i];
        const auto &geometry = entry.geometry;
        if (entry.wrapper->labeled()) {
            y -= rowSpace[i] + geometry.h;
            if (entry.wrapper->childNodes.empty())
                throw std::logic_error("mlabeledtr has no leading label cell");
            auto *cell = dynamic_cast<CommonMtd *>(entry.wrapper->childNodes.front());
            if (!cell)
                throw std::logic_error("mlabeledtr leading child is not an mtd wrapper");
            const TableCellPlacement placement{0, y, 0, labelWidth, 0, 0, 0};
            (void)entry.wrapper->placeSceneCell(builder, *cell, entry.scene.label_nodes, geometry,
                                                placement);
            if (i + 1 < emittedRows.size())
                y -= geometry.d + rowSpace[i + 1] + rLines.at(i);
        } else if (i + 1 < emittedRows.size()) {
            y -= rowSpace[i] + geometry.h + geometry.d + rowSpace[i + 1] + rLines[i];
        }
    }
}

void CommonMtable::topSceneTable(scene::Builder &builder, scene::NodeId tableNode,
                                 scene::NodeId labelsGroup) {
    const auto side = text8(attr(*this, u"side"), u"right");
    const auto box = getBBox();
    const double labelWidth = getTableData().L;
    const double width = box.L + (pWidth ? pWidth : box.w) + box.R;
    const auto [pad, alignment, shift] = getPadAlignShift(side);
    (void)pad;
    const double dx = shift +
                      (alignment == "right"    ? -width
                       : alignment == "center" ? -width / 2
                                               : 0) +
                      box.L;
    const double scale = jax().face().parameters.required("x_height") * 1000 / metrics().ex;

    scene::SemanticMetadata matrixMetadata;
    matrixMetadata.initial_transform = true;
    const auto contentTransform =
        builder.wrap_contents(tableNode, scene::Transform(scene::Affine{1, 0, 0, -1, 0, 0}),
                              std::move(matrixMetadata), scene::ElementRole::anonymous_group);

    scene::SemanticMetadata transformMetadata;
    transformMetadata.initial_transform = true;
    const scene::Transform transform(std::vector<scene::TransformOperation>{
        scene::Translation{0, box.h, scene::TranslationSeparator::space},
        scene::Affine{1, 0, 0, -1, 0, 0}, scene::Scaling{scale, scale, false, 2}});
    const auto transformGroup =
        builder.add_group(tableNode, transform, {}, {}, {}, std::move(transformMetadata),
                          scene::ElementRole::anonymous_group);

    scene::Viewport tableViewport;
    tableViewport.bounds = {};
    tableViewport.view_box = {-dx, -box.h, .001, box.h + box.d};
    const auto horizontal = alignment == "left"    ? scene::ViewportAlignment::min
                            : alignment == "right" ? scene::ViewportAlignment::max
                                                   : scene::ViewportAlignment::mid;
    tableViewport.aspect_ratio = tableAspect(horizontal);
    tableViewport.clip_content = false;
    scene::SemanticMetadata tableMetadata;
    tableMetadata.ordered_attributes.push_back({"data-table", "true"});
    const auto tableViewportGroup =
        builder.add_group(transformGroup, {}, {}, {}, {}, std::move(tableMetadata),
                          scene::ElementRole::table_content_viewport, {}, tableViewport);
    builder.reparent_group(contentTransform, tableViewportGroup);

    scene::Viewport labelViewport;
    labelViewport.bounds = {};
    labelViewport.view_box = {side == "left" ? 0.0 : labelWidth, -box.h, .001, box.h + box.d};
    labelViewport.aspect_ratio =
        tableAspect(side == "left" ? scene::ViewportAlignment::min : scene::ViewportAlignment::max);
    labelViewport.clip_content = false;
    scene::SemanticMetadata labelMetadata;
    labelMetadata.ordered_attributes.push_back({"data-labels", "true"});
    const auto labelViewportGroup =
        builder.add_group(transformGroup, {}, {}, {}, {}, std::move(labelMetadata),
                          scene::ElementRole::table_label_frame, {}, labelViewport);
    builder.reparent_group(labelsGroup, labelViewportGroup);

    const std::array<SceneNode, 1> emitted{SceneNode{tableNode}};
    placeScene(builder, emitted, -box.L, 0, tableNode);
}

void CommonMtable::subSceneTable(scene::Builder &builder, scene::NodeId tableNode,
                                 scene::NodeId labelsGroup, double pWidthShift) {
    if (!container || !containerI)
        throw std::logic_error("subtable labels require their Source container");
    const auto side = text8(attr(*this, u"side"), u"right");
    const auto box = getBBox();
    const double labelWidth = getTableData().L;
    const double width = box.L + (pWidth ? pWidth : box.w) + box.R;
    const auto alignment = getAlignShift().first;
    const double containerWidth = container->getWrapWidth(*containerI);
    const double commonWidth = sourceMax(width, containerWidth);
    const double x = side == "left"
                         ? (alignment == "left"    ? 0
                            : alignment == "right" ? width - commonWidth + pWidthShift
                                                   : (width - commonWidth) / 2 + pWidthShift) -
                               box.L
                         : (alignment == "left"    ? commonWidth
                            : alignment == "right" ? width + pWidthShift
                                                   : (commonWidth + width) / 2 + pWidthShift) -
                               box.L - labelWidth;
    const std::array<SceneNode, 1> emittedLabels{SceneNode{labelsGroup}};
    placeScene(builder, emittedLabels, x, 0, labelsGroup);
    builder.reparent_group(labelsGroup, tableNode);
}

SceneNodes CommonMtable::emitScene(scene::Builder &builder,
                                   std::span<scene::NodeId const> parents) {
    auto containers = standardSceneNodes(builder, parents);
    auto &nodes = containers.nodes;
    if (nodes.empty() || !nodes.front())
        throw std::logic_error("mtable Scene emission requires its first output segment");
    const auto tableNode = *nodes.front();
    (void)builder.set_background_width(tableNode, getWidth());

    std::optional<scene::NodeId> labelsGroup;
    if (hasLabels) {
        scene::SemanticMetadata metadata;
        metadata.ordered_attributes.push_back({"data-labels", "true"});
        scene::Transform transform;
        if (isTop)
            transform = scene::Transform(scene::Affine{1, 0, 0, -1, 0, 0});
        labelsGroup = builder.add_group(0, std::move(transform), {}, {}, {}, std::move(metadata),
                                        scene::ElementRole::anonymous_group);
    }

    auto emittedRows = placeSceneRows(builder, tableNode, labelsGroup);
    handleSceneColumnLines(builder, tableNode);
    handleSceneRowLines(builder, tableNode);
    handleSceneFrame(builder, tableNode);
    const double pWidthShift = handleScenePWidth(builder, tableNode);
    if (hasLabels) {
        if (!labelsGroup)
            throw std::logic_error("mtable labels group was not created");
        spaceSceneLabels(builder, emittedRows);
        if (isTop)
            topSceneTable(builder, tableNode, *labelsGroup);
        else
            subSceneTable(builder, tableNode, *labelsGroup, pWidthShift);
    }
    return std::move(nodes);
}

CommonWrapper *CommonMtable::findContainer() {
    auto *current = static_cast<CommonWrapper *>(this);
    auto *outer = parent;
    while (outer && (outer->node->notParent() || outer->node->kind() == mml::NodeKind::mrow)) {
        current = outer;
        outer = outer->parent;
    }
    container = outer;
    containerI = current->node->childPosition();
    if (!container || (container->node->kind() == mml::NodeKind::math && !container->parent))
        jax().setTable(*this);
    return container;
}

double CommonMtable::getPercentageWidth() {
    if (hasLabels) {
        bbox.pwidth = std::string(BBox::fullWidth);
        return 1;
    }
    const auto width = text8(attr(*this, u"width"));
    if (percentText(width)) {
        bbox.pwidth = width;
        return 1;
    }
    return 0;
}

std::vector<std::string> CommonMtable::getAttributeArray(std::string_view name) const {
    std::u16string key;
    for (const auto c : name)
        key.push_back(static_cast<unsigned char>(c));
    const auto *value = attr(*this, key);
    if (!truthy(value))
        return {utf16::encode(node->attributes().defaultValue(key)
                                  ? text16(node->attributes().defaultValue(key))
                                  : std::u16string{})};
    return words(text16(value));
}

std::optional<std::vector<std::string>> CommonMtable::getColumnAttributes(std::string_view name,
                                                                          std::size_t first) const {
    const auto n = static_cast<std::ptrdiff_t>(numCols) - static_cast<std::ptrdiff_t>(first);
    auto values = getAttributeArray(name);
    if (values.empty())
        return std::nullopt;
    while (n > 0 && values.size() < static_cast<std::size_t>(n))
        values.push_back(values.back());
    if (static_cast<std::ptrdiff_t>(values.size()) > n) {
        const auto end =
            n < 0 ? std::max<std::ptrdiff_t>(0, static_cast<std::ptrdiff_t>(values.size()) + n) : n;
        values.resize(static_cast<std::size_t>(end));
    }
    return values;
}

std::optional<std::vector<std::string>> CommonMtable::getRowAttributes(std::string_view name,
                                                                       std::size_t first) const {
    const auto n = static_cast<std::ptrdiff_t>(numRows) - static_cast<std::ptrdiff_t>(first);
    auto values = getAttributeArray(name);
    if (values.empty())
        return std::nullopt;
    while (n > 0 && values.size() < static_cast<std::size_t>(n))
        values.push_back(values.back());
    if (static_cast<std::ptrdiff_t>(values.size()) > n) {
        const auto end =
            n < 0 ? std::max<std::ptrdiff_t>(0, static_cast<std::ptrdiff_t>(values.size()) + n) : n;
        values.resize(static_cast<std::size_t>(end));
    }
    return values;
}

std::optional<std::vector<double>>
CommonMtable::convertLengths(const std::vector<std::string> &values) const {
    std::vector<double> result;
    result.reserve(values.size());
    for (const auto &value : values)
        result.push_back(length2em(lengthInput(value)));
    return result;
}

std::vector<std::string> CommonMtable::getEmHalfSpacing(const std::array<double, 4> &frame,
                                                        const std::vector<double> &spaces,
                                                        double scale) const {
    std::vector<std::string> result;
    result.reserve(spaces.size() + 2);
    for (const auto value : spaces)
        result.push_back(em(value * scale / 2));
    result.insert(result.begin(), em(frame[0] * scale));
    result.push_back(em(frame[1] * scale));
    return result;
}

std::optional<std::vector<std::string>> CommonMtable::addEm(const std::vector<double> &values,
                                                            double n) const {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto value : values)
        result.push_back(em(value / n));
    return result;
}

std::array<double, 4> CommonMtable::getFrameSpacing() {
    std::array<double, 4> result{};
    if (fframe) {
        const auto values = convertLengths(getAttributeArray("framespacing"));
        if (values) {
            if (!values->empty())
                result[0] = (*values)[0];
            if (values->size() > 1)
                result[1] = (*values)[1];
        }
    }
    result[2] = result[0];
    if (const auto *padding = attr(*this, u"data-array-padding"); truthy(padding)) {
        const auto values = convertLengths(words(text16(padding)));
        if (values) {
            if (!values->empty())
                result[0] = (*values)[0];
            if (values->size() > 1)
                result[2] = (*values)[1];
        }
    }
    return result;
}

void CommonMtable::initializeSource() {
    CommonWrapper::initializeSource();
    const auto rows = tableRows();
    numRows = rows.size();
    for (auto *row : rows) {
        numCols = std::max(numCols, row->numCells());
        hasLabels |= row->labeled();
    }
    findContainer();
    isTop = !container || (container->node->kind() == mml::NodeKind::math && !container->parent);
    getPercentageWidth();
    frame = text8(attr(*this, u"frame"), u"none") != "none";
    fframe = frame || node->attributes().isSet(u"data-frame-styles");
    fLine = frame ? .07 : 0;
    fSpace = getFrameSpacing();
    if (auto values = getColumnAttributes("columnspacing"))
        if (auto converted = convertLengths(*values))
            cSpace = *converted;
    if (auto values = getRowAttributes("rowspacing"))
        if (auto converted = convertLengths(*values))
            rSpace = *converted;
    if (auto values = getColumnAttributes("columnlines"))
        for (const auto &v : *values)
            cLines.push_back(v == "none" ? 0 : .07);
    if (auto values = getRowAttributes("rowlines"))
        for (const auto &v : *values)
            rLines.push_back(v == "none" ? 0 : .07);
    cWidths = getColumnWidths();
    adjustWideTable();
    stretchColumns();
    stretchRows();
}

TableData &CommonMtable::getTableData() {
    if (data)
        return *data;
    TableData result;
    result.H.assign(numRows, 0);
    result.D.assign(numRows, 0);
    result.W.assign(numCols, 0);
    result.NH.resize(numRows);
    result.ND.resize(numRows);
    std::vector<double> labelWidth(1, 0);
    const auto rows = tableRows();
    for (std::size_t j = 0; j < rows.size(); ++j) {
        auto *row = rows[j];
        const auto rowAlign = text8(row->node->attributes().lookup(u"rowalign"));
        for (std::size_t i = 0; i < row->numCells(); ++i) {
            auto *cell = row->getChild(i);
            if (!cell)
                throw std::logic_error("table row has a missing cell wrapper");
            updateHDW(*cell, i, j, rowAlign, result, &result.W);
            recordPWidthCell(*cell, i);
        }
        result.NH[j] = result.H[j];
        result.ND[j] = result.D[j];
        if (row->labeled() && !row->childNodes.empty())
            updateHDW(*row->childNodes.front(), 0, j, rowAlign, result, &labelWidth);
        row->bbox.h = result.H[j];
        row->bbox.d = result.D[j];
    }
    result.L = labelWidth.front();
    data = std::move(result);
    return *data;
}

void CommonMtable::updateHDW(CommonWrapper &cell, std::size_t i, std::size_t j,
                             std::string_view rowAlign, TableData &table,
                             std::vector<double> *widths) {
    const auto box = cell.getBBox();
    double h = box.h, d = box.d, w = box.w;
    const double scale = cell.parent ? cell.parent->bbox.rscale : 1;
    h *= scale;
    d *= scale;
    w *= scale;
    if (node->properties().table.use_height.value_or(false)) {
        h = std::max(h, .75);
        d = std::max(d, .25);
    }
    const auto *cellAlignValue = cell.node->attributes().lookup(u"rowalign");
    const auto align = truthy(cellAlignValue) ? text8(cellAlignValue) : std::string(rowAlign);
    if (j >= table.H.size())
        throw std::out_of_range("row height index outside TableData");
    if (align == "top") {
        if (h > table.H[j]) {
            table.D[j] -= h - table.H[j];
            table.H[j] = h;
        }
        if (h + d > table.H[j] + table.D[j])
            table.D[j] = h + d - table.H[j];
    } else if (align == "bottom") {
        if (d > table.D[j]) {
            table.H[j] -= d - table.D[j];
            table.D[j] = d;
        }
        if (h + d > table.H[j] + table.D[j])
            table.H[j] = h + d - table.D[j];
    } else if (align == "center") {
        if (h + d > table.H[j] + table.D[j])
            table.H[j] = table.D[j] = (h + d) / 2;
    } else {
        table.H[j] = std::max(table.H[j], h);
        table.D[j] = std::max(table.D[j], d);
    }
    if (widths && i < widths->size())
        (*widths)[i] = std::max((*widths)[i], w);
}

void CommonMtable::recordPWidthCell(CommonWrapper &cell, std::size_t i) {
    if (cell.childNodes.empty())
        return;
    if (!cell.childNodes.front()->getBBox().pwidth.empty())
        pwidthCells.emplace_back(&cell, i);
}

void CommonMtable::setColumnPWidths() {
    for (auto [cell, i] : pwidthCells)
        if (cell && i < cWidths.size()) {
            const double width = widthValue(cWidths[i]);
            if (cell->setChildPWidths(false, width)) {
                cell->invalidateBBox();
                (void)cell->getBBox();
            }
        }
}

std::array<double, 2> CommonMtable::getBBoxHD(double height) const {
    const auto [align, row] = getAlignmentRow();
    if (!row) {
        const double half = height / 2, axis = jax().face().parameters.required("axis_height");
        if (align == "top")
            return {0, height};
        if (align == "bottom")
            return {height, 0};
        if (align == "axis")
            return {half + axis, half - axis};
        return {half, half};
    }
    const double y = const_cast<CommonMtable *>(this)->getVerticalPosition(*row, align);
    return {y, height - y};
}

std::pair<std::string, std::optional<double>> CommonMtable::getAlignmentRow() const {
    auto values = words(text16(attr(*this, u"align"), u"axis"));
    const std::string align = values.empty() ? "axis" : values.front();
    if (values.size() < 2)
        return {align, {}};
    std::u16string rowText;
    for (char c : values[1])
        rowText.push_back(static_cast<unsigned char>(c));
    double row = js_input::parseInt(rowText);
    if (row < 0)
        row += static_cast<double>(numRows) + 1;
    if (row < 1 || row > static_cast<double>(numRows))
        return {align, {}};
    return {align, row - 1};
}

double CommonMtable::getVerticalPosition(double i, std::string_view align) {
    auto table = getTableData();
    double y = fLine;
    const auto space = getRowHalfSpacing();
    const bool equal = truthy(attr(*this, u"equalrows"));
    const double common = equal ? getEqualRowHeight() : 0;
    for (std::size_t j = 0; static_cast<double>(j) < i; ++j)
        y += space[j] + (equal ? common : table.H[j] + table.D[j]) + space[j + 1] +
             (j < rLines.size() ? rLines[j] : 0);
    if (!std::isfinite(i) || i < 0 || std::floor(i) != i || i >= table.H.size())
        return std::numeric_limits<double>::quiet_NaN();
    const auto index = static_cast<std::size_t>(i);
    const double h = equal ? (common + table.H[index] - table.D[index]) / 2 : table.H[index];
    const double d = equal ? (common - table.H[index] + table.D[index]) / 2 : table.D[index];
    if (align == "center")
        return y + space[index] + (h + d) / 2;
    if (align == "bottom")
        return y + space[index] + h + d + space[index + 1];
    if (align == "baseline")
        return y + space[index] + h;
    if (align == "axis")
        return y + space[index] + h - .25;
    return y;
}

std::vector<double> CommonMtable::getRowHalfSpacing() const {
    auto v = rSpace;
    for (auto &x : v)
        x /= 2;
    v.insert(v.begin(), fSpace[1]);
    v.push_back(fSpace[1]);
    return v;
}

std::vector<double> CommonMtable::getColumnHalfSpacing() const {
    auto v = cSpace;
    for (auto &x : v)
        x /= 2;
    v.insert(v.begin(), fSpace[0]);
    v.push_back(fSpace[2]);
    return v;
}

double CommonMtable::getEqualRowHeight() {
    auto t = getTableData();
    double h = 0;
    for (std::size_t i = 0; i < t.H.size(); ++i)
        h = std::max(h, t.H[i] + t.D[i]);
    return h;
}

std::vector<double> CommonMtable::getComputedWidths() {
    auto t = getTableData();
    for (std::size_t i = 0; i < t.W.size() && i < cWidths.size(); ++i)
        t.W[i] = widthValue(cWidths[i], t.W[i]);
    if (truthy(attr(*this, u"equalcolumns")) && !t.W.empty())
        std::fill(t.W.begin(), t.W.end(), *std::max_element(t.W.begin(), t.W.end()));
    return t.W;
}

double CommonMtable::naturalWidth() {
    auto w = getComputedWidths();
    w.insert(w.end(), cLines.begin(), cLines.end());
    w.insert(w.end(), cSpace.begin(), cSpace.end());
    return sum(w) + 2 * fLine + fSpace[0] + fSpace[2];
}

std::array<double, 2> CommonMtable::getBBoxLR() {
    if (!hasLabels)
        return {bbox.L, 0};
    const auto side = text8(attr(*this, u"side"), u"right");
    const auto [pad, align, shift] = getPadAlignShift(side);
    (void)shift;
    const bool labels = truthy(attr(*this, u"data-width-includes-label"));
    const double adjusted = labels && frame && fSpace[0] ? pad - fSpace[0] : pad;
    if (align == "center" && !labels)
        return {adjusted, adjusted};
    return side == "left" ? std::array<double, 2>{adjusted, 0} : std::array<double, 2>{0, adjusted};
}

std::tuple<double, std::string, double> CommonMtable::getPadAlignShift(std::string_view side) {
    double pad = getTableData().L + length2em(lengthInput(attr(*this, u"minlabelspacing")));
    if (styles) {
        const auto l = styles->get(u"padding-left"), r = styles->get(u"padding-right");
        if (!l.empty() || !r.empty())
            pad = std::max({pad, length2em(std::u16string(l.empty() ? u"0" : l)),
                            length2em(std::u16string(r.empty() ? u"0" : r))});
    }
    auto [align, shift] = getAlignShift();
    if (align == side) {
        if (side == "left")
            shift = std::max(pad, shift) - pad;
        else
            shift = std::min(-pad, shift) + pad;
    }
    return {pad, align, shift};
}

double CommonMtable::getWidth() {
    return pWidth ? pWidth : getBBox().w;
}

void CommonMtable::adjustWideTable() {
    if (text8(attr(*this, u"width"), u"auto") != "auto")
        return;
    if (std::isfinite(containerWidth())) jax().requireExactContainerWidth();
    const auto [pad, align, shift] = getPadAlignShift(text8(attr(*this, u"side"), u"right"));
    (void)shift;
    const double available =
        std::max(containerWidth() / 10, containerWidth() - pad - (align == "center" ? pad : 0));
    if (naturalWidth() > available)
        adjustColumnWidths(available);
}

void CommonMtable::computeBBox(BBox &box, bool) {
    auto t = getTableData();
    const bool equal = truthy(attr(*this, u"equalrows"));
    double h;
    if (equal) {
        std::vector<double> parts = rLines;
        parts.insert(parts.end(), rSpace.begin(), rSpace.end());
        h = sum(parts) + getEqualRowHeight() * numRows;
    } else {
        std::vector<double> parts = t.H;
        parts.insert(parts.end(), t.D.begin(), t.D.end());
        parts.insert(parts.end(), rLines.begin(), rLines.end());
        parts.insert(parts.end(), rSpace.begin(), rSpace.end());
        h = sum(parts);
    }
    h += 2 * (fLine + fSpace[1]);
    double w = naturalWidth();
    const auto width = lengthInput(attr(*this, u"width"), u"auto");
    if (text8(attr(*this, u"width"), u"auto") != "auto")
        w = std::max(length2em(width, 0) + 2 * fLine, w);
    const auto hd = getBBoxHD(h), lr = getBBoxLR();
    box.h = hd[0];
    box.d = hd[1];
    box.w = w;
    box.L = lr[0];
    box.R = lr[1];
    if (!percentText(text8(attr(*this, u"width"))))
        setColumnPWidths();
}

void CommonMtable::getStyles() {
    CommonWrapper::getStyles();
    const auto *value = attr(*this, u"data-frame-styles");
    if (!truthy(value))
        return;
    if (!styles)
        styles.emplace(u"");
    const auto parts = string_util::split(text16(value));
    constexpr std::array<std::u16string_view, 4> names{u"top", u"right", u"bottom", u"left"};
    for (std::size_t i = 0; i < std::min(parts.size(), names.size()); ++i)
        if (parts[i] != u"none")
            styles->set(std::u16string(u"border-") + std::u16string(names[i]),
                        std::u16string(u".07em ") + parts[i]);
}

std::string CommonMtable::getChildAlign(std::size_t) const {
    return container ? container->getChildAlign(containerI.value()) : "center";
}

double CommonMtable::getWrapWidth(std::size_t) {
    return getWidth();
}

bool CommonMtable::setChildPWidths(bool, std::optional<double> cwidth, bool) {
    if (!percentText(text8(attr(*this, u"width"))))
        return false;
    jax().requireExactContainerWidth();
    if (!hasLabels) {
        bbox.pwidth.clear();
        if (!container)
            throw std::logic_error("percent-width table has no container");
        container->bbox.pwidth.clear();
    }
    const auto w = bbox.w;
    const bool label = truthy(attr(*this, u"data-width-includes-label"));
    const double requested =
        std::max(w, length2em(lengthInput(attr(*this, u"width")),
                              std::max(cwidth.value_or(containerWidth()), bbox.L + w + bbox.R)));
    const double W = requested - (label ? bbox.L + bbox.R : 0);
    auto cols =
        truthy(attr(*this, u"equalcolumns"))
            ? std::vector<std::string>(numCols, percent(1.0 / std::max<std::size_t>(1, numCols)))
            : getColumnAttributes("columnwidth", 0).value_or(std::vector<std::string>{});
    cWidths = getColumnWidthsFixed(cols, W);
    pWidth = naturalWidth();
    if (isTop)
        bbox.w = pWidth;
    setColumnPWidths();
    if (pWidth != w && parent)
        parent->invalidateBBox();
    return pWidth != w;
}

std::pair<std::string, double> CommonMtable::getAlignShift() {
    return isTop ? CommonWrapper::getAlignShift()
                 : std::pair{container ? container->getChildAlign(containerI.value()) : "center",
                             0.0};
}

ColumnWidths CommonMtable::getColumnWidths() {
    const auto width = text8(attr(*this, u"width"), u"auto");
    auto attrs = getColumnAttributes("columnwidth", 0).value_or(std::vector<std::string>{});
    if (truthy(attr(*this, u"equalcolumns")))
        return getEqualColumns(width);
    if (width == "auto")
        return getColumnWidthsAuto(attrs);
    if (percentText(width))
        return getColumnWidthsPercent(attrs);
    return getColumnWidthsFixed(attrs, length2em(lengthInput(width)));
}

ColumnWidths CommonMtable::getEqualColumns(std::string_view width) {
    const auto count = std::max<std::size_t>(1, numCols);
    if (percentText(width))
        return ColumnWidths(numCols, ColumnWidth(percent(1.0 / count)));
    double value = 0;
    if (width == "auto") {
        for (auto w : getTableData().W)
            value = std::max(value, w);
    } else
        value = std::max(0.0, length2em(lengthInput(width)) -
                                  (sum(cLines) + sum(cSpace) + fSpace[0] + fSpace[2])) /
                count;
    return ColumnWidths(numCols, ColumnWidth(value));
}

ColumnWidths CommonMtable::getColumnWidthsAuto(const std::vector<std::string> &values) {
    ColumnWidths out;
    for (const auto &v : values)
        if (v == "auto" || v == "fit")
            out.emplace_back(AutomaticColumnWidth{});
        else if (percentText(v))
            out.emplace_back(v);
        else
            out.emplace_back(length2em(lengthInput(v)));
    return out;
}

ColumnWidths CommonMtable::getColumnWidthsPercent(const std::vector<std::string> &values) {
    const bool hasFit = std::find(values.begin(), values.end(), "fit") != values.end();
    const auto data = hasFit ? std::optional<TableData>(getTableData()) : std::nullopt;
    ColumnWidths out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto &v = values[i];
        if (v == "fit")
            out.emplace_back(AutomaticColumnWidth{});
        else if (v == "auto")
            out.emplace_back(data && i < data->W.size() ? ColumnWidth(data->W[i])
                                                        : ColumnWidth(AutomaticColumnWidth{}));
        else if (percentText(v))
            out.emplace_back(v);
        else
            out.emplace_back(length2em(lengthInput(v)));
    }
    return out;
}

ColumnWidths CommonMtable::getColumnWidthsFixed(const std::vector<std::string> &values,
                                                double width) {
    std::vector<std::size_t> fit, automatic;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i] == "fit")
            fit.push_back(i);
        else if (values[i] == "auto")
            automatic.push_back(i);
    }
    const auto n = fit.empty() ? automatic.size() : fit.size();
    const auto data = n ? std::optional<TableData>(getTableData()) : std::nullopt;
    const double cw = width - sum(cLines) - sum(cSpace) - fSpace[0] - fSpace[2];
    double remaining = cw;
    for (std::size_t i = 0; i < values.size(); ++i)
        remaining -= values[i] == "fit" || values[i] == "auto"
                         ? (data && i < data->W.size() ? data->W[i] : 0)
                         : length2em(lengthInput(values[i]), cw);
    const double each = n && remaining > 0 ? remaining / n : 0;
    ColumnWidths out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto &v = values[i];
        if (v == "fit")
            out.emplace_back((data && i < data->W.size() ? data->W[i] : 0) + each);
        else if (v == "auto")
            out.emplace_back((data && i < data->W.size() ? data->W[i] : 0) +
                             (fit.empty() ? each : 0));
        else
            out.emplace_back(length2em(lengthInput(v), cw));
    }
    return out;
}

void CommonMtable::adjustColumnWidths(double width) {
    const auto table = getTableData();
    const auto attrs = getColumnAttributes("columnwidth", 0).value_or(std::vector<std::string>{});
    std::vector<std::size_t> order;
    for (const auto kind : {0, 1, 2, 3}) {
        std::vector<std::size_t> part;
        for (std::size_t i = 0; i < attrs.size(); ++i)
            if ((kind == 0 && attrs[i] == "fit") || (kind == 1 && attrs[i] == "auto") ||
                (kind == 2 && percentText(attrs[i])) ||
                (kind == 3 && attrs[i] != "fit" && attrs[i] != "auto" && !percentText(attrs[i])))
                part.push_back(i);
        std::sort(part.begin(), part.end(),
                  [&](auto a, auto b) { return table.W[a] > table.W[b]; });
        order.insert(order.end(), part.begin(), part.end());
    }
    if (order.empty())
        return;
    for (std::size_t i = 0; i < cWidths.size(); ++i)
        cWidths[i] = widthValue(cWidths[i], table.W[i]);
    const double cw = width - sum(cLines) - sum(cSpace) - fSpace[0] - fSpace[2];
    double excess = 0;
    for (const auto &v : cWidths)
        excess += widthValue(v);
    excess -= cw;
    double total = 0;
    std::size_t n = 0;
    while (n < order.size()) {
        total += table.W[order[n++]];
        if (total && excess / total < .333)
            break;
    }
    if (!total)
        return;
    const double factor = 1 - excess / total;
    for (std::size_t j = 0; j < n; ++j)
        cWidths[order[j]] = widthValue(cWidths[order[j]]) * factor;
}

void CommonMtable::stretchRows() {
    const bool equal = truthy(attr(*this, u"equalrows"));
    const double common = equal ? getEqualRowHeight() : 0;
    const auto table = equal ? std::optional<TableData>(getTableData()) : std::nullopt;
    for (std::size_t i = 0; i < tableRows().size(); ++i) {
        auto *row = tableRows()[i];
        if (equal) {
            const auto h = (common + table->H[i] - table->D[i]) / 2,
                       d = (common - table->H[i] + table->D[i]) / 2;
            row->stretchChildren(std::array<double, 2>{h, d});
        } else
            row->stretchChildren();
    }
}

void CommonMtable::stretchColumns() {
    const auto attrs = getColumnAttributes("columnwidth", 0).value_or(std::vector<std::string>{});
    for (std::size_t i = 0; i < numCols; ++i) {
        std::optional<double> width;
        if (i < cWidths.size())
            if (auto p = std::get_if<double>(&cWidths[i]))
                width = *p;
        stretchColumn(i, width);
        if (width)
            breakColumn(i, *width, i < attrs.size() ? attrs[i] : "");
    }
}

void CommonMtable::stretchColumn(std::size_t i, std::optional<double> width) {
    std::vector<CommonWrapper *> stretchy;
    const auto rows = tableRows();
    for (auto *row : rows)
        if (auto *cell = row->getChild(i); cell) {
            if (cell->childNodes.empty())
                throw std::logic_error("table cell has no first child wrapper for stretch lookup");
            auto *child = cell->childNodes.front();
            if (std::holds_alternative<NoStretch>(child->stretch) &&
                child->canStretch(StretchDirection::horizontal))
                stretchy.push_back(child);
        }
    if (stretchy.empty())
        return;
    if (!width) {
        width = 0;
        const bool all = stretchy.size() == rows.size();
        for (auto *row : rows)
            if (auto *cell = row->getChild(i); cell) {
                if (cell->childNodes.empty())
                    throw std::logic_error(
                        "table cell has no first child wrapper for width lookup");
                auto *child = cell->childNodes.front();
                const bool none = std::holds_alternative<NoStretch>(child->stretch);
                if (all || none)
                    width = std::max(*width, child->getBBox(none).w);
            }
    }
    auto &widths = getTableData().W;
    for (auto *child : stretchy) {
        const auto before = child->getBBox().w;
        auto *mo = dynamic_cast<CommonMo *>(child->coreMO());
        if (!mo)
            throw std::logic_error("stretchable table cell has no operator core");
        mo->getStretchedVariant(
            std::array<double, 1>{std::max(*width, before) / child->coreRScale()});
        widths[i] = std::max(widths[i], child->getBBox().w);
    }
}

void CommonMtable::breakColumn(std::size_t i, double width, std::string_view type) {
    auto *root = jax().document().root();
    if (!root || text8(root->attributes().lookup(u"overflow")) != "linebreak" ||
        !jax().currentMetrics().display)
        return;
    auto &table = getTableData();
    std::size_t j = 0;
    double maxWidth = 0;
    for (auto *row : tableRows()) {
        auto *cell = row->getChild(i);
        if (cell) {
            const double scale = row->getBBox().rscale;
            auto box = cell->getBBox();
            if (box.w * scale > width) {
                if (cell->childNodes.empty())
                    throw std::logic_error("overwide table cell has no child to break");
                cell->childNodes.front()->breakToWidth(width);
                updateHDW(*cell, i, j, text8(row->node->attributes().lookup(u"rowalign")), table);
                box = cell->getBBox();
            }
            maxWidth = std::max(maxWidth, box.w * scale);
        }
        row->bbox.h = table.H[j];
        row->bbox.d = table.D[j];
        ++j;
    }
    if (i < cWidths.size() &&
        (type == "fit" || type == "auto" || percentText(type) || maxWidth > widthValue(cWidths[i])))
        cWidths[i] = maxWidth;
}
} // namespace folia::math::output::common
