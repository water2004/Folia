/*************************************************************
 * Native mathematical services from MathJax 4.1.3 output/common/Wrapper.ts.
 * See src/math/LICENSE-MathJax.txt.
 *************************************************************/
module folia.math.output.common;
import :wrapper;
import :wrapper_factory;
import :output_jax;
import :mo;
import :mrow;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.util.js_numeric_input;
import folia.math.util.js_number;
import folia.math.util.string;
import folia.math.util.utf16;
import std;

namespace folia::math::output::common {
namespace {
using mml::AttributeNull;
using mml::AttributeValue;

bool truthy(AttributeValue const *value) {
    if (!value || std::holds_alternative<AttributeNull>(*value))
        return false;
    if (const auto *boolean = std::get_if<bool>(value))
        return *boolean;
    if (const auto *number = std::get_if<double>(value))
        return *number != 0 && !std::isnan(*number);
    return !std::get<std::u16string>(*value).empty();
}

std::u16string_view text(AttributeValue const *value) {
    if (value) {
        if (const auto *string = std::get_if<std::u16string>(value))
            return *string;
    }
    return {};
}

double number(AttributeValue const *value, double fallback = 0) {
    if (!value)
        return fallback;
    if (std::holds_alternative<AttributeNull>(*value))
        return 0;
    if (const auto *numeric = std::get_if<double>(value))
        return *numeric;
    if (const auto *boolean = std::get_if<bool>(value))
        return *boolean ? 1 : 0;
    return js_input::number(std::get<std::u16string>(*value));
}

std::string utf8(std::u16string_view s) {
    return utf16::encode(s);
}

std::u16string decodeUtf8(std::string_view value) {
    return utf16::decode(value).text;
}

std::string valueString(AttributeValue const *value, std::string fallback = {}) {
    if (!value || std::holds_alternative<AttributeNull>(*value))
        return fallback;
    if (const auto *string = std::get_if<std::u16string>(value))
        return utf8(*string);
    if (const auto *boolean = std::get_if<bool>(value))
        return *boolean ? "true" : "false";
    return js_number::toString(std::get<double>(*value));
}

LengthInput input(std::u16string_view value) {
    return std::u16string(value);
}

LengthInput input(AttributeValue const *value, std::u16string_view fallback = {}) {
    if (!value || std::holds_alternative<AttributeNull>(*value))
        return std::u16string(fallback);
    if (const auto *string = std::get_if<std::u16string>(value))
        return *string;
    if (const auto *numeric = std::get_if<double>(value))
        return *numeric;
    return std::u16string(std::get<bool>(*value) ? u"true" : u"false");
}

std::string sideKey(std::string_view prefix, std::string_view side, std::string_view suffix = {}) {
    std::string result(prefix);
    result.append(side);
    result.append(suffix);
    return result;
}

bool isZeroIndent(std::u16string_view value) {
    while (!value.empty() && js_input::isWhitespace(value.front()))
        value.remove_prefix(1);
    while (!value.empty() && js_input::isWhitespace(value.back()))
        value.remove_suffix(1);
    if (value.empty() || value.front() != u'0')
        return false;
    return std::all_of(value.begin() + 1, value.end(),
                       [](char16_t c) { return c >= u'a' && c <= u'z'; });
}

std::string attributeName(std::u16string_view value) {
    return utf8(value);
}

scene::SemanticMetadata sceneMetadata(mml::MmlNode const &node) {
    scene::SemanticMetadata result;
    result.source_tag = std::string(node.document().kindName(node.kind()));
    return result;
}

scene::PaintColor cssPaint(std::string value) {
    if (value.empty())
        return scene::InheritedPaint{};
    if (value == "none")
        return scene::NoPaint{};
    if (value == "currentColor")
        return scene::CurrentColorPaint{};
    return scene::CssColor{std::move(value)};
}

scene::Transform translated(double x, double y) {
    return scene::Transform(std::vector<scene::TransformOperation>{scene::Translation{x, y}});
}

std::string hexCodepoint(std::uint32_t value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    do {
        result.push_back(digits[value & 0xF]);
        value >>= 4;
    } while (value);
    std::reverse(result.begin(), result.end());
    return result;
}

void addPolygon(scene::Builder &builder, scene::NodeId parent,
                std::array<scene::Point, 4> const &points, scene::Paint paint,
                scene::ElementRole role, std::optional<std::size_t> insertionIndex) {
    scene::Path path;
    path.element = scene::PathElement::polygon;
    path.role = role;
    path.paint = std::move(paint);
    path.geometry.segments = {scene::MoveTo{points[0]}, scene::LineTo{points[1]},
                              scene::LineTo{points[2]}, scene::LineTo{points[3]},
                              scene::ClosePath{}};
    if (insertionIndex)
        builder.insert_primitive(parent, *insertionIndex, std::move(path));
    else
        builder.add_primitive(parent, std::move(path));
}

constexpr double smallMathSpace = 2.0 / 18.0;
constexpr double operatorMathSpace = 5.0 / 18.0;

double mathMax(double a, double b) {
    if (std::isnan(a) || std::isnan(b))
        return std::numeric_limits<double>::quiet_NaN();
    return std::max(a, b);
}

double mathMin(double a, double b) {
    if (std::isnan(a) || std::isnan(b))
        return std::numeric_limits<double>::quiet_NaN();
    return std::min(a, b);
}
} // namespace

CommonWrapper::CommonWrapper(CommonWrapperFactory &factoryValue, mml::MmlNode &source,
                             CommonWrapper *parentValue)
    : tree::AbstractWrapper<CommonWrapper, mml::MmlNode>(factoryValue, source),
      parent(parentValue) {
}

CommonWrapperFactory &CommonWrapper::factory() const {
    return dynamic_cast<CommonWrapperFactory &>(factory_);
}

CommonOutputJax &CommonWrapper::jax() const {
    return factory().jax();
}

ExtendedMetrics const &CommonWrapper::metrics() const {
    return jax().currentMetrics();
}

double CommonWrapper::containerWidth() const {
    return parent ? parent->containerWidth() : jax().containerWidth();
}

Linebreaks &CommonWrapper::linebreaks() const {
    return jax().linebreaks();
}

LinebreakOptions const &CommonWrapper::linebreakOptions() const {
    return jax().currentLinebreakOptions();
}

void CommonWrapper::initializeSource() {
    bbox = BBox::zero();
    bboxComputed = false;
    _breakCount = -1;
    getStyles();
    getStyleData();
    getVariant();
    getScale();
    getSpace();
    for (auto *child : node->children()) {
        auto &wrapped = wrapNode(*child);
        childNodes.push_back(&wrapped);
        if (!wrapped.bbox.pwidth.empty() &&
            (node->notParent() || node->kind() == mml::NodeKind::math))
            bbox.pwidth = std::string(BBox::fullWidth);
    }
}

void CommonWrapper::getStyles() {
    // Source reads only an explicit style attribute and returns before parsing
    // an empty string.
    auto const *raw = node->attributes().own(u"style");
    if (!raw || !std::holds_alternative<std::u16string>(*raw) ||
        std::get<std::u16string>(*raw).empty())
        return;
    styles.emplace(std::get<std::u16string>(*raw));
    const auto &removals = factory().classData(node->kind()).removeStyles;
    for (const auto &name : removals) {
        const auto unicodeName = decodeUtf8(name);
        const auto removed = styles->get(unicodeName);
        if (removed.empty())
            continue;
        removedStyles.insert_or_assign(name, utf8(removed));
        styles->set(unicodeName, styles::StyleValue(u""));
    }
}

void CommonWrapper::getStyleData() {
    if (!styles)
        return;
    StyleData data;
    bool hasPadding = false;
    bool hasBorder = false;
    bool hasMargin = false;
    for (const auto &side : BBox::boxSides) {
        const auto index = static_cast<std::size_t>(side.index);
        const auto suffix = side.name;
        const auto borderWidth = styles->get(decodeUtf8(sideKey("border", suffix, "Width")));
        if (!borderWidth.empty()) {
            hasBorder = true;
            if (!data.border)
                data.border.emplace();
            data.border->width[index] = mathMax(0.0, length2em(input(borderWidth)));
            const auto borderStyle = styles->get(decodeUtf8(sideKey("border", suffix, "Style")));
            const auto borderColor = styles->get(decodeUtf8(sideKey("border", suffix, "Color")));
            data.border->style[index] = borderStyle.empty() ? "solid" : utf8(borderStyle);
            data.border->color[index] = utf8(borderColor);
        }
        const auto padding = styles->get(decodeUtf8(sideKey("padding", suffix)));
        if (!padding.empty()) {
            hasPadding = true;
            data.padding[index] = mathMax(0.0, length2em(input(padding)));
        }
        const auto margin = styles->get(decodeUtf8(sideKey("margin", suffix)));
        if (!margin.empty()) {
            hasMargin = true;
            data.margin[index] = length2em(input(margin));
        }
    }
    if (hasPadding || hasBorder || hasMargin)
        styleData = std::move(data);
}

void CommonWrapper::getVariant() {
    if (!node->isToken())
        return;
    const auto &attributes = node->attributes();
    if (attributes.own(u"mathvariant")) {
        auto selected = valueString(attributes.lookup(u"mathvariant"), "null");
        if (!jax().face().variants.contains(selected)) {
            jax().recordUnknownMathVariant(node->kind(), selected);
            variant = "normal";
        } else {
            variant = std::move(selected);
        }
        return;
    }
    std::string family;
    std::string weight;
    std::string style;
    if (auto found = removedStyles.find("fontFamily"); found != removedStyles.end())
        family = found->second;
    if (auto found = removedStyles.find("fontWeight"); found != removedStyles.end())
        weight = found->second;
    if (auto found = removedStyles.find("fontStyle"); found != removedStyles.end())
        style = found->second;
    if (const auto *attribute = attributes.lookup(u"fontfamily"); truthy(attribute))
        family = valueString(attribute);
    if (const auto *attribute = attributes.lookup(u"fontweight"); truthy(attribute))
        weight = valueString(attribute);
    if (const auto *attribute = attributes.lookup(u"fontstyle"); truthy(attribute))
        style = valueString(attribute);

    if (!family.empty() && family != "undefined") {
        const bool numericWeight =
            !weight.empty() && std::all_of(weight.begin(), weight.end(),
                                           [](unsigned char c) { return c >= '0' && c <= '9'; });
        if (numericWeight) {
            const auto numeric = js_input::parseInt(decodeUtf8(weight));
            weight = numeric > 600 ? "bold" : "normal";
        }
        variant = explicitVariant(family, weight, style);
        return;
    }
    variant = valueString(attributes.lookup(u"mathvariant"), "italic");
    // MmlNode properties are the native, already-resolved counterpart of
    // Source getProperty("variantForm"); this is a property lookup, not an
    // additional wrapper-side state.
    if (node->properties().tex.variant_form.value_or(false))
        variant = "-tex-variant";

    const auto &classData = factory().classData(node->kind());
    if (!weight.empty()) {
        if (const auto outer = classData.boldVariants.find(weight);
            outer != classData.boldVariants.end()) {
            if (const auto inner = outer->second.find(variant); inner != outer->second.end())
                variant = inner->second;
        }
    }
    if (!style.empty()) {
        if (const auto outer = classData.italicVariants.find(style);
            outer != classData.italicVariants.end()) {
            if (const auto inner = outer->second.find(variant); inner != outer->second.end())
                variant = inner->second;
        }
    }
}

std::string CommonWrapper::explicitVariant(std::string_view family, std::string_view weight,
                                           std::string_view style) {
    if (!styles)
        styles.emplace();
    styles->set(u"fontFamily", styles::StyleValue(decodeUtf8(family)));
    if (!weight.empty())
        styles->set(u"fontWeight", styles::StyleValue(decodeUtf8(weight)));
    if (!style.empty())
        styles->set(u"fontStyle", styles::StyleValue(decodeUtf8(style)));
    return "-explicitFont";
}

void CommonWrapper::getScale() {
    const auto &attributes = node->attributes();
    const double parentScale = parent ? parent->bbox.scale : 1;
    const double scriptLevel = mathMin(number(attributes.lookup(u"scriptlevel")), 2.0);
    double scale =
        scriptLevel == 0
            ? 1
            : std::pow(number(attributes.lookup(u"scriptsizemultiplier"), .71), scriptLevel);
    const auto *fontSize = attributes.lookup(u"fontsize");
    const auto *mathSize = (node->isToken() || node->kind() == mml::NodeKind::mstyle)
                               ? attributes.lookup(u"mathsize")
                               : attributes.effectiveInherited(u"mathsize");
    std::u16string removedSize;
    if (!truthy(fontSize)) {
        if (auto found = removedStyles.find("fontSize"); found != removedStyles.end())
            removedSize = decodeUtf8(found->second);
    }
    if (!removedSize.empty() && !attributes.own(u"mathsize"))
        mathSize = nullptr;
    if ((truthy(fontSize) || !removedSize.empty()) && !attributes.own(u"mathsize")) {
        const LengthInput size = !removedSize.empty() ? LengthInput(removedSize) : input(fontSize);
        scale *= length2em(size, 1, 1);
    } else if (mathSize && (!std::holds_alternative<std::u16string>(*mathSize) ||
                            std::get<std::u16string>(*mathSize) != u"1")) {
        scale *= length2em(input(mathSize), 1, 1);
    }
    if (scriptLevel != 0) {
        const double minimum =
            length2em(input(attributes.lookup(u"scriptminsize"), u".4em"), .4, 1);
        if (scale < minimum)
            scale = minimum;
    }
    bbox.scale = scale;
    bbox.rscale = scale / parentScale;
}

bool CommonWrapper::isTopEmbellished() const {
    if (!node->isEmbellished())
        return false;
    auto *sourceParent = node->parent();
    return !sourceParent || !sourceParent->isEmbellished();
}

void CommonWrapper::getSpace() {
    const bool top = isTopEmbellished();
    const bool hasSpacing = node->hasSpacingAttributes();
    if (jax().options().mathmlSpacing || hasSpacing) {
        if (top)
            getMathMLSpacing();
    } else {
        getTeXSpacing(top, hasSpacing);
    }
}

void CommonWrapper::getTeXSpacing(bool top, bool hasSpacing) {
    if (!hasSpacing) {
        const auto spacing = node->texSpacing();
        if (!spacing.empty())
            bbox.L = length2em(LengthInput(decodeUtf8(spacing)));
    }
    if (top || hasSpacing) {
        auto *mo = node->coreMO();
        if (!mo)
            return;
        const auto &attributes = mo->attributes();
        if (attributes.isSet(u"lspace"))
            bbox.L = mathMax(0.0, length2em(input(attributes.lookup(u"lspace"))));
        if (attributes.isSet(u"rspace"))
            bbox.R = mathMax(0.0, length2em(input(attributes.lookup(u"rspace"))));
    }
}

void CommonWrapper::getMathMLSpacing() {
    auto *mo = dynamic_cast<mml::MmlMo *>(node->coreMO());
    if (!mo)
        return;

    auto *child = mo->coreParent();
    if (!child)
        return;
    auto *row = child->parent();
    if (!row || (row->kind() != mml::NodeKind::mrow && row->kind() != mml::NodeKind::inferred_mrow))
        return;

    const auto index = child->childPosition();
    if (!index || row->children().size() == 1)
        return;

    const bool script = number(mo->attributes().lookup(u"scriptlevel")) > 0;
    const bool noDictionary = mo->properties().tex.no_dictionary_definition.value_or(false);
    const auto defaultSpace = [=](double size) {
        if (noDictionary)
            return script ? smallMathSpace : operatorMathSpace;
        return script ? (size < smallMathSpace ? 0.0 : smallMathSpace) : size;
    };
    const auto &attributes = mo->attributes();
    bbox.L = attributes.isSet(u"lspace")
                 ? mathMax(0.0, length2em(input(attributes.lookup(u"lspace"))))
                 : defaultSpace(mo->lspace.value_or(operatorMathSpace));
    bbox.R = attributes.isSet(u"rspace")
                 ? mathMax(0.0, length2em(input(attributes.lookup(u"rspace"))))
                 : defaultSpace(mo->rspace.value_or(operatorMathSpace));

    if (*index == 0)
        return;
    auto *previous = row->children()[*index - 1];
    if (!previous->isEmbellished())
        return;
    if (auto *previousWrapper = jax().wrapperFor(*previous);
        previousWrapper && previousWrapper->getBBox().R) {
        bbox.L = mathMax(0.0, bbox.L - previousWrapper->getBBox().R);
    }
}

double CommonWrapper::length2em(LengthInput const &value, double size,
                                std::optional<double> scale) const {
    const auto &face = jax().face();
    lengths::LengthContext context;
    context.size_em = size;
    context.scale = scale.value_or(bbox.scale);
    context.em_pixels = jax().pxPerEm();
    context.mu_em = 1.0 / 18.0;
    if (const auto *number = std::get_if<double>(&value))
        return lengths::length2em(*number, context);

    const auto string = utf8(std::get<std::u16string>(value));
    if (string == "medium")
        return face.parameters.required("rule_thickness");
    if (string == "thin")
        return face.parameters.required("rule_thickness") * 2.0 / 3.0;
    if (string == "thick")
        return face.parameters.required("rule_thickness") * 5.0 / 3.0;
    return lengths::length2em(string, context);
}

std::string CommonWrapper::percent(double value) const {
    return lengths::percent(value);
}

std::string CommonWrapper::em(double value) const {
    return lengths::em(value);
}

std::string CommonWrapper::px(double value, double minimum) const {
    return lengths::px(value, minimum, metrics().em);
}

double CommonWrapper::getAlignX(double width, BBox const &box, std::string_view align) const {
    if (align == "right")
        return width - (box.w + box.R) * box.rscale;
    if (align == "left")
        return box.L * box.rscale;
    return (width - box.w * box.rscale) / 2;
}

double CommonWrapper::getAlignY(double parentHeight, double parentDepth, double childHeight,
                                double childDepth, std::string_view align) const {
    if (align == "top")
        return parentHeight - childHeight;
    if (align == "bottom")
        return childDepth - parentDepth;
    if (align == "center")
        return (parentHeight - childHeight - (parentDepth - childDepth)) / 2;
    return 0;
}

bool CommonWrapper::fixesPWidth() const {
    return !node->notParent() && !node->isToken();
}

std::size_t CommonWrapper::breakCount() {
    if (_breakCount < 0) {
        if (node->isEmbellished()) {
            auto *mo = dynamic_cast<CommonMo *>(coreMO());
            _breakCount = mo ? static_cast<std::int64_t>(mo->embellishedBreakCount()) : 0;
        } else if (node->arity() < 0 && !node->linebreakContainer() && !childNodes.empty()) {
            auto *row = dynamic_cast<CommonMrow *>(childNodes.front());
            _breakCount = row && row->isStack ? static_cast<std::int64_t>(row->breakCount()) : 0;
        } else {
            _breakCount = 0;
        }
    }
    return static_cast<std::size_t>(_breakCount);
}

CommonWrapper *CommonWrapper::breakTop(CommonWrapper *mrow, CommonWrapper *) {
    return node->linebreakContainer() || !parent ? mrow : parent->breakTop(mrow, this);
}

CommonWrapper &CommonWrapper::wrapNode(mml::MmlNode &source, CommonWrapper *parentValue) {
    auto &result = factory().wrap(source, parentValue ? parentValue : this);
    if (parentValue)
        parentValue->childNodes.push_back(&result);
    jax().registerWrapper(source, result);
    return result;
}

BBox &CommonWrapper::getBBox() {
    if (!bboxComputed)
        (void)getBBox(true);
    return bbox;
}

BBox CommonWrapper::getBBox(bool save) {
    if (bboxComputed)
        return bbox;
    if (save) {
        computeBBox(bbox, false);
        bboxComputed = true;
        return bbox;
    }
    // Source seeds a non-saving computation from BBox.zero(), rather than
    // copying wrapper state (scale, side spacing, or other cached metadata).
    BBox result = BBox::zero();
    computeBBox(result, false);
    return result;
}

void CommonWrapper::computeBBox(BBox &target, bool recompute) {
    target.empty();
    for (auto *child : childNodes)
        target.append(child->getOuterBBox());
    target.clean();
    if (fixesPWidth() && setChildPWidths(recompute) && !recompute)
        computeBBox(target, true);
}

std::shared_ptr<LineBBox> CommonWrapper::computeLineBBox(std::size_t index) {
    if (childNodes.empty())
        throw std::logic_error("line box requires a child wrapper");
    return getChildLineBBox(*childNodes.front(), index);
}

std::shared_ptr<LineBBox> CommonWrapper::getLineBBox(std::size_t index) {
    if (index >= lineBBox.size())
        lineBBox.resize(index + 1);
    if (lineBBox[index])
        return lineBBox[index];

    const auto count = breakCount();
    if (count) {
        auto line = embellishedBBox(index);
        if (!line)
            line = computeLineBBox(index);
        lineBBox[index] = line;
        if (index == 0) {
            if (node->kind() != mml::NodeKind::mo && node->isEmbellished())
                line->originalL = getBBox().L;
            else
                line->L = getBBox().L;
        }
        if (index == count)
            line->R = getBBox().R;
    } else {
        lineBBox[index] = std::make_shared<LineBBox>(
            LineBBox::from(getOuterBBox(), jax().currentLinebreakOptions().lineleading));
    }
    return lineBBox[index];
}

std::shared_ptr<LineBBox> CommonWrapper::embellishedBBox(std::size_t index) {
    if (!node->isEmbellished() || node->kind() == mml::NodeKind::mo)
        return {};
    auto *mo = dynamic_cast<CommonMo *>(coreMO());
    if (!mo)
        return {};
    auto box = getOuterBBox();
    return std::make_shared<LineBBox>(mo->moLineBBox(index, mo->embellishedBreakStyle(), &box));
}

std::shared_ptr<LineBBox> CommonWrapper::getChildLineBBox(CommonWrapper &child, std::size_t index) {
    const auto count = breakCount();
    auto result = child.getLineBBox(index);
    if (styleData || bbox.L || bbox.R)
        result = std::make_shared<LineBBox>(result->copy());
    addMiddleBorders(*result);
    if (index == 0) {
        result->L += bbox.L;
        addLeftBorders(*result);
    } else if (index == count) {
        result->R += bbox.R;
        addRightBorders(*result);
    }
    return result;
}

std::array<CommonWrapper *, 2> CommonWrapper::getBreakNode(LineBBox &line) {
    if (!line.start)
        return {this, nullptr};
    const auto childIndex = static_cast<std::size_t>((*line.start)[0]);
    const auto lineIndex = static_cast<std::size_t>((*line.start)[1]);
    if (node->isEmbellished())
        return {this, coreMO()};

    auto *source = this;
    if (!childNodes.empty() &&
        (childNodes.front()->node->isInferred() || node->kind() == mml::NodeKind::semantics))
        source = childNodes.front();
    if (node->isToken() || childIndex >= source->childNodes.size())
        return {this, nullptr};
    auto *child = source->childNodes[childIndex];
    return child->getBreakNode(*child->getLineBBox(lineIndex));
}

double CommonWrapper::sideStyleSize(std::size_t side) const {
    if (!styleData || side >= 4)
        return 0;
    const auto border = styleData->border ? styleData->border->width[side] : 0;
    return border + styleData->padding[side] + styleData->margin[side];
}

void CommonWrapper::addLeftBorders(BBox &target) const {
    if (styleData)
        target.w += sideStyleSize(3);
}

void CommonWrapper::addMiddleBorders(BBox &target) const {
    if (styleData) {
        target.h += sideStyleSize(0);
        target.d += sideStyleSize(2);
    }
}

void CommonWrapper::addRightBorders(BBox &target) const {
    if (styleData)
        target.w += sideStyleSize(1);
}

void CommonWrapper::copySkewIC(BBox &target) const {
    if (childNodes.empty())
        return;
    const auto &first = childNodes.front()->bbox;
    const auto &last = childNodes.back()->bbox;
    if (first.sk)
        target.sk = first.sk;
    if (first.dx)
        target.dx = first.dx;
    if (last.ic) {
        target.ic = last.ic;
        target.w += target.ic;
    }
}

BBox CommonWrapper::getOuterBBox(bool save) {
    BBox outer = getBBox(save);
    if (!styleData)
        return outer;
    const auto border = styleData->border ? styleData->border->width : std::array<double, 4>{};
    for (const auto &side : BBox::boxSides) {
        const auto index = static_cast<std::size_t>(side.index);
        const auto amount = styleData->margin[index] + styleData->padding[index] + border[index];
        if (side.dimension == "w")
            outer.w += amount;
        else if (side.dimension == "h")
            outer.h += amount;
        else
            outer.d += amount;
    }
    return outer;
}

std::array<double, 2> CommonWrapper::getUnbrokenHD() {
    double height = 0;
    double depth = 0;
    for (std::size_t i = 0, count = breakCount() + 1; i < count; ++i) {
        auto line = getLineBBox(i);
        height = mathMax(height, line->h);
        depth = mathMax(depth, line->d);
    }
    return {height, depth};
}

void CommonWrapper::breakToWidth(double) {
    // The pinned Source base wrapper deliberately leaves this to linebreaking
    // subclasses; retain the existing base behavior here.
}

bool CommonWrapper::setChildPWidths(bool recompute, std::optional<double> width, bool clear) {
    if (recompute)
        return false;
    if (clear)
        bbox.pwidth.clear();
    bool changed = false;
    for (auto *child : childNodes) {
        if (!child->bbox.pwidth.empty() &&
            child->setChildPWidths(false, width.value_or(child->bbox.w), clear))
            changed = true;
    }
    return changed;
}

void CommonWrapper::invalidateBBox(bool bubble) {
    if (bboxComputed || _breakCount >= 0) {
        bboxComputed = false;
        lineBBox.clear();
        _breakCount = -1;
        if (parent && bubble)
            parent->invalidateBBox();
    }
}

CommonWrapper *CommonWrapper::core() {
    auto *source = node->core();
    return source ? jax().wrapperFor(*source) : nullptr;
}

CommonWrapper *CommonWrapper::coreMO() {
    auto *source = node->coreMO();
    return source ? jax().wrapperFor(*source) : nullptr;
}

double CommonWrapper::coreRScale() const {
    double result = bbox.rscale;
    CommonWrapper *current = const_cast<CommonWrapper *>(this)->coreMO();
    while (current && current != this) {
        result *= current->bbox.rscale;
        current = current->parent;
    }
    return result;
}

double CommonWrapper::getRScale() const {
    double result = 1;
    for (auto *current = const_cast<CommonWrapper *>(this); current; current = current->parent)
        result *= current->bbox.rscale;
    return result;
}

std::u16string CommonWrapper::getText() const {
    if (!node->isToken())
        return {};
    std::u16string result;
    for (auto *child : node->children()) {
        if (const auto *textNode = dynamic_cast<mml::TextNode const *>(child))
            result += textNode->getText();
    }
    return result;
}

std::vector<std::uint32_t> CommonWrapper::unicodeChars(std::u16string_view s,
                                                       std::string_view selected) const {
    const auto points = utf16::codePoints(s);
    std::vector<std::uint32_t> out;
    out.reserve(points.size());
    for (const auto point : points)
        out.push_back(static_cast<std::uint32_t>(point));
    for (auto &cp : out) {
        if (auto remap = jax().fonts().remap(jax().face().name, selected, cp))
            cp = remap->codepoint;
    }
    return out;
}

std::vector<std::uint32_t> CommonWrapper::remapChars(std::span<std::uint32_t const> chars) const {
    return {chars.begin(), chars.end()};
}

mml::TextNode &CommonWrapper::mmlText(std::u16string value) {
    auto &result = node->document().factory().make(mml::NodeKind::text);
    auto *textNode = dynamic_cast<mml::TextNode *>(&result);
    if (!textNode)
        throw std::logic_error("text creator returned a non-TextNode");
    return textNode->setText(std::move(value));
}

mml::MmlNode &CommonWrapper::mmlNode(mml::NodeKind kind, mml::AttributeSet attributes,
                                     std::vector<mml::MmlNode *> children) {
    return node->document().factory().make(kind, std::move(attributes), children);
}

CommonMo &CommonWrapper::createMo(std::u16string_view value) {
    auto &textNode = mmlText(std::u16string(value));
    mml::AttributeSet attributes;
    attributes.set(u"stretchy", true);
    std::array<mml::MmlNode *, 1> children{&textNode};
    auto &moNode =
        node->document().factory().make(mml::NodeKind::mo, std::move(attributes), children);
    moNode.inheritAttributesFrom(*node);
    if (auto *parentNode = node->parent())
        node->document().attachAuxiliary(*parentNode, moNode);
    auto &wrapper = wrapNode(moNode);
    auto *mo = dynamic_cast<CommonMo *>(&wrapper);
    if (!mo)
        throw std::logic_error("mo creator returned incompatible wrapper");
    return *mo;
}

double CommonWrapper::getWrapWidth(std::size_t index) {
    return childNodes.at(index)->getBBox().w;
}

std::optional<ResolvedFontGlyph> CommonWrapper::getVariantChar(std::string_view selected,
                                                               std::uint32_t codepoint) {
    return jax().fonts().resolveGlyph(jax().face().name, selected, codepoint);
}

double CommonWrapper::sceneDx() {
    if (!styleData)
        return 0;
    return styleData->padding[3] + (styleData->border ? styleData->border->width[3] : 0);
}

SceneContainers CommonWrapper::createSceneNodes(scene::Builder &builder,
                                                std::span<scene::NodeId const> parents) {
    SceneContainers containers;
    containers.nodes.reserve(parents.size());
    auto metadata = sceneMetadata(*node);
    std::optional<scene::Link> link;
    if (const auto *href = node->attributes().lookup(u"href"); truthy(href)) {
        // MathJax 4.1.3 svg.ts commonStyles gives SVG links blue fill and stroke.
        // Freeze that paint on the link shell, beneath the child's authored paint.
        link = scene::Link{valueString(href), {}, {},
                           {scene::CssColor{"blue"}, scene::CssColor{"blue"}}};
    }
    for (std::size_t i = 0; i < parents.size(); ++i) {
        const auto id = builder.add_group(parents[i], {}, {}, link, {}, metadata,
                                          scene::ElementRole::math_wrapper);
        containers.nodes.push_back(id);
        containers.content.push_back(id);
        if (link) {
            const bool embellished = node->isEmbellished() && node->kind() != mml::NodeKind::mo;
            const auto box = embellished ? getOuterBBox() : getLineBBox(i)->copy();
            scene::Paint invisible;
            invisible.fill = scene::NoPaint{};
            invisible.stroke = scene::NoPaint{};
            builder.add_primitive(id, scene::Rule{{i == 0 || embellished ? -sceneDx() : 0, -box.d,
                                                   box.w, box.h + box.d},
                                                  invisible,
                                                  {},
                                                  scene::ElementRole::link_hitbox});
        }
    }
    return containers;
}

SceneContainers CommonWrapper::standardSceneNodes(scene::Builder &builder,
                                                  std::span<scene::NodeId const> parents) {
    auto containers = createSceneNodes(builder, parents);
    handleSceneStyles(builder, containers.nodes);
    handleSceneScale(builder, containers.nodes);
    handleSceneBorder(builder, containers.nodes);
    handleSceneColor(builder, containers.nodes);
    handleSceneAttributes(builder, containers.nodes);
    return containers;
}

void CommonWrapper::handleSceneStyles(scene::Builder &builder, std::span<SceneNode const> nodes) {
    if (!styles)
        return;
    for (auto slot : nodes) {
        if (!slot)
            continue;
        const auto id = *slot;
        auto metadata = builder.metadata(id);
        const auto css = utf8(styles->cssText());
        if (!css.empty())
            metadata.ordered_attributes.push_back({"style", css});
        const double margin = styleData ? styleData->margin[3] : 0;
        if (margin != 0) {
            builder.set_transform(id, translated(margin, 0));
            metadata.initial_transform = true;
        }
        builder.set_metadata(id, std::move(metadata));
    }
}

void CommonWrapper::handleSceneScale(scene::Builder &builder, std::span<SceneNode const> nodes) {
    if (bbox.rscale == 1)
        return;
    for (auto slot : nodes) {
        if (!slot)
            continue;
        const auto id = *slot;
        builder.set_transform(id, scene::Transform(std::vector<scene::TransformOperation>{
                                      scene::Scaling{bbox.rscale, bbox.rscale}}));
        auto metadata = builder.metadata(id);
        metadata.initial_transform = true;
        builder.set_metadata(id, std::move(metadata));
    }
}

void CommonWrapper::handleSceneBorder(scene::Builder &builder, std::span<SceneNode const> nodes) {
    if (!styleData || !styleData->border || nodes.empty())
        return;
    constexpr double fuzz = 0.005;
    const auto &border = *styleData->border;
    const auto margin = styleData->margin;
    const auto n = nodes.size() - 1;
    const bool embellished = node->isEmbellished() && node->kind() != mml::NodeKind::mo;
    for (std::size_t k = 0; k < nodes.size(); ++k) {
        if (!nodes[k])
            continue;
        std::optional<std::size_t> solidAnchor;
        if (builder.entry_count(*nodes[k]) != 0)
            solidAnchor = 0;
        const double left = (!n || k == 0) ? 1 : 0;
        const double right = (!n || k == n) ? 1 : 0;
        const auto box = embellished ? getOuterBBox() : getLineBBox(k)->copy();
        const double h = box.h - margin[0] + fuzz;
        const double d = box.d - margin[2] + fuzz;
        const double w = box.w - margin[1] - margin[3] + fuzz;
        const scene::Point outerLT{-fuzz, h}, outerRT{w, h}, outerRB{w, -d}, outerLB{-fuzz, -d};
        std::array<scene::Point, 4> insets[4] = {
            std::array<scene::Point, 4>{outerLT,
                                        outerRT,
                                        {w - right * border.width[1], h - border.width[0]},
                                        {-fuzz + left * border.width[3], h - border.width[0]}},
            std::array<scene::Point, 4>{outerRB,
                                        outerRT,
                                        {w - right * border.width[1], h - border.width[0]},
                                        {w - right * border.width[1], -d + border.width[2]}},
            std::array<scene::Point, 4>{outerLB,
                                        outerRB,
                                        {w - right * border.width[1], -d + border.width[2]},
                                        {-fuzz + left * border.width[3], -d + border.width[2]}},
            std::array<scene::Point, 4>{outerLB,
                                        outerLT,
                                        {-fuzz + left * border.width[3], h - border.width[0]},
                                        {-fuzz + left * border.width[3], -d + border.width[2]}}};
        const double dx = left * sceneDx();
        for (std::size_t side = 0; side < 4; ++side) {
            if (!border.width[side] || (side == 3 && !left) || (side == 1 && !right))
                continue;
            auto points = insets[side];
            for (auto &p : points)
                p.x -= dx;
            scene::Paint paint;
            paint.fill = cssPaint(border.color[side]);
            paint.stroke = scene::NoPaint{};
            if (border.style[side] == "dashed" || border.style[side] == "dotted") {
                // The SVG source uses a side-specific dash cadence; retain the
                // completed path and stroke semantics as Scene data.
                const auto &a = points[0];
                const auto &b = points[1];
                scene::Path line;
                line.element = scene::PathElement::line;
                line.role = scene::ElementRole::background_box;
                line.paint.stroke = scene::CssColor{border.color[side]};
                line.paint.stroke_width = border.width[side];
                line.paint.line_cap = border.style[side] == "dotted" ? scene::StrokeCap::round
                                                                     : scene::StrokeCap::square;
                const double t = border.width[side];
                const double half = t / 2;
                constexpr std::array<std::array<double, 4>, 4> offsets{
                    {{{1, -1, -1, -1}}, {{-1, 1, -1, -1}}, {{1, 1, -1, 1}}, {{1, 1, 1, -1}}}};
                const double x1 = a.x + offsets[side][0] * half;
                const double y1 = a.y + offsets[side][1] * half;
                const double x2 = b.x + offsets[side][2] * half;
                const double y2 = b.y + offsets[side][3] * half;
                const double length = (side % 2) ? std::abs(y2 - y1) : std::abs(x2 - x1);
                const double count = border.style[side] == "dotted"
                                         ? std::ceil(length / (2 * t))
                                         : std::ceil((length - t) / (4 * t));
                const double m = length / (4 * count + 1);
                line.paint.dash_array = border.style[side] == "dotted"
                                            ? std::vector<double>{0.001, length / count - 0.002}
                                            : std::vector<double>{m, 3 * m};
                line.geometry.segments = {scene::MoveTo{{x1, y1}}, scene::LineTo{{x2, y2}}};
                builder.insert_primitive(*nodes[k], 0, std::move(line));
                if (solidAnchor)
                    ++*solidAnchor;
            } else {
                addPolygon(builder, *nodes[k], points, std::move(paint),
                           scene::ElementRole::background_box, solidAnchor);
                if (solidAnchor)
                    ++*solidAnchor;
            }
        }
    }
}

void CommonWrapper::handleSceneColor(scene::Builder &builder, std::span<SceneNode const> nodes) {
    const auto *color = node->attributes().own(u"mathcolor");
    if (!truthy(color))
        color = node->attributes().own(u"color");
    const auto *background = node->attributes().own(u"mathbackground");
    if (!truthy(background))
        background = node->attributes().own(u"background");
    if (!truthy(background) && styles) {
        const auto value = styles->get(u"background-color");
        if (!value.empty()) {
            for (std::size_t i = 0; i < nodes.size(); ++i) {
                if (!nodes[i])
                    continue;
                const bool embellished = node->isEmbellished() && node->kind() != mml::NodeKind::mo;
                const auto box = embellished ? getOuterBBox() : getLineBBox(i)->copy();
                scene::Paint paint;
                paint.fill = cssPaint(utf8(value));
                scene::SemanticMetadata meta;
                meta.ordered_attributes.push_back({"data-bgcolor", "true"});
                builder.insert_primitive(*nodes[i], 0,
                                         scene::Rule{{i == 0 || embellished ? -sceneDx() : 0,
                                                      -box.d, box.w, box.h + box.d},
                                                     std::move(paint),
                                                     std::move(meta),
                                                     scene::ElementRole::background_box});
            }
        }
    }
    for (auto slot : nodes) {
        if (!slot)
            continue;
        const auto id = *slot;
        if (truthy(color)) {
            auto paint = builder.paint(id);
            paint.fill = cssPaint(valueString(color));
            paint.stroke = cssPaint(valueString(color));
            builder.set_paint(id, std::move(paint));
        }
    }
    if (truthy(background)) {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (!nodes[i])
                continue;
            const bool embellished = node->isEmbellished() && node->kind() != mml::NodeKind::mo;
            const auto box = embellished ? getOuterBBox() : getLineBBox(i)->copy();
            scene::Paint paint;
            paint.fill = cssPaint(valueString(background));
            scene::SemanticMetadata meta;
            meta.ordered_attributes.push_back({"data-bgcolor", "true"});
            builder.insert_primitive(
                *nodes[i], 0,
                scene::Rule{{i == 0 || embellished ? -sceneDx() : 0, -box.d, box.w, box.h + box.d},
                            std::move(paint),
                            std::move(meta),
                            scene::ElementRole::background_box});
        }
    }
}

void CommonWrapper::handleSceneAttributes(scene::Builder &builder,
                                          std::span<SceneNode const> nodes) {
    const auto &skip = factory().classData(node->kind()).skipAttributes;
    for (const auto &[name, value] : node->attributes().ownEntries()) {
        const auto key = attributeName(name);
        if (key == "class" || std::holds_alternative<AttributeNull>(value))
            continue;
        const auto skipIt = skip.find(key);
        const bool forced = skipIt != skip.end() && !skipIt->second;
        if (skipIt != skip.end() && skipIt->second)
            continue;
        bool exists = false;
        if (!nodes.empty() && nodes.front()) {
            if (key == "data-mml-node" && !builder.metadata(*nodes.front()).source_tag.empty())
                exists = true;
            for (const auto &attribute : builder.metadata(*nodes.front()).ordered_attributes)
                if (attribute.name == key) {
                    exists = true;
                    break;
                }
        }
        if (!forced && (node->attributes().hasDefault(name) || exists))
            continue;
        for (auto slot : nodes) {
            if (!slot)
                continue;
            const auto id = *slot;
            auto metadata = builder.metadata(id);
            metadata.ordered_attributes.push_back({key, valueString(&value)});
            builder.set_metadata(id, std::move(metadata));
        }
    }
    const auto *classValue = node->attributes().lookup(u"class");
    if (!truthy(classValue))
        return;
    std::vector<std::string> classes;
    auto addClasses = [&](std::string_view source) {
        std::size_t i = 0;
        while (i < source.size()) {
            while (i < source.size() && std::isspace(static_cast<unsigned char>(source[i])))
                ++i;
            const auto begin = i;
            while (i < source.size() && !std::isspace(static_cast<unsigned char>(source[i])))
                ++i;
            if (begin == i)
                continue;
            std::string value(source.substr(begin, i - begin));
            if (std::find(classes.begin(), classes.end(), value) == classes.end())
                classes.push_back(std::move(value));
        }
    };
    for (auto slot : nodes) {
        if (!slot)
            continue;
        const auto id = *slot;
        auto metadata = builder.metadata(id);
        classes.clear();
        for (const auto &attribute : metadata.ordered_attributes)
            if (attribute.name == "class")
                addClasses(attribute.value);
        addClasses(valueString(classValue));
        std::erase_if(metadata.ordered_attributes,
                      [](auto const &attribute) { return attribute.name == "class"; });
        std::string combined;
        for (const auto &name : classes) {
            if (!combined.empty())
                combined.push_back(' ');
            combined += name;
        }
        metadata.ordered_attributes.push_back({"class", std::move(combined)});
        builder.set_metadata(id, std::move(metadata));
    }
}

double CommonWrapper::handleSceneId(scene::Builder &builder, std::span<SceneNode const> nodes,
                                    double y) {
    if (nodes.empty() || !nodes.front() || !truthy(node->attributes().lookup(u"id")))
        return y;
    const auto h = getBBox().h;
    const auto scale = getBBox().rscale;
    scene::TextRun alignment;
    alignment.role = scene::ElementRole::id_alignment_marker;
    scene::SemanticMetadata metadata;
    metadata.ordered_attributes.push_back({"data-idbox", "true"});
    builder.wrap_contents(*nodes.front(), translated(0, -h), std::move(metadata),
                          scene::ElementRole::id_alignment_box);
    builder.insert_primitive(*nodes.front(), 0, std::move(alignment));
    return y + h * scale;
}

void CommonWrapper::placeScene(scene::Builder &builder, std::span<SceneNode const> nodes, double x,
                               double y, std::optional<scene::NodeId> target) {
    scene::NodeId id;
    if (target) {
        id = *target;
    } else {
        if (nodes.empty() || !nodes.front())
            return;
        id = *nodes.front();
    }
    if (!target) {
        x += sceneDx() * bbox.rscale;
        if (x == 0 && y == 0)
            return;
        y = handleSceneId(builder, nodes, y);
    } else if (x == 0 && y == 0)
        return;
    auto operations = std::vector<scene::TransformOperation>{
        scene::Translation{x, y, scene::TranslationSeparator::comma}};
    auto existing = builder.transform(id);
    operations.insert(operations.end(), existing.operations().begin(), existing.operations().end());
    builder.set_transform(id, scene::Transform(std::move(operations)));
}

void CommonWrapper::addSceneChildren(scene::Builder &builder,
                                     std::span<scene::NodeId const> parents) {
    double x = 0;
    for (auto *child : childNodes) {
        if (!child)
            throw std::logic_error("CommonWrapper contains a null child wrapper");
        auto emitted = child->emitScene(builder, parents);
        const auto box = child->getOuterBBox();
        if (!emitted.empty())
            child->placeScene(builder, emitted, x + box.L * box.rscale, 0);
        x += (box.L + box.w + box.R) * box.rscale;
    }
}

std::optional<SceneNodes>
CommonWrapper::toEmbellishedScene(scene::Builder &builder, std::span<scene::NodeId const> parents) {
    if (parents.size() <= 1 || !node->isEmbellished() || !node->parent() ||
        node->parent()->isEmbellished())
        return {};
    auto *mo = dynamic_cast<CommonMo *>(coreMO());
    if (!mo)
        throw std::logic_error("embellished scene requires CommonMo core");
    const auto style = mo->embellishedBreakStyle();
    SceneNodes result;
    result.reserve(2);
    for (std::size_t i = 0; i < 2; ++i) {
        if ((i == 0 && style != "before") || (i == 1 && style != "after")) {
            std::array<scene::NodeId, 1> one{parents[i]};
            auto emitted = emitScene(builder, one);
            result.push_back(emitted.at(0));
            placeScene(builder, emitted, 0, 0);
        } else {
            std::array<scene::NodeId, 1> one{parents[i]};
            auto emitted = createSceneNodes(builder, one).nodes;
            result.push_back(emitted.at(0));
        }
    }
    return result;
}

SceneNodes CommonWrapper::emitScene(scene::Builder &builder,
                                    std::span<scene::NodeId const> parents) {
    if (auto embellished = toEmbellishedScene(builder, parents))
        return *embellished;
    switch (node->kind()) {
    case mml::NodeKind::mi:
    case mml::NodeKind::mn:
    case mml::NodeKind::ms:
    case mml::NodeKind::mspace:
    case mml::NodeKind::mstyle:
    case mml::NodeKind::math:
    case mml::NodeKind::mtext:
    case mml::NodeKind::mo:
    case mml::NodeKind::tex_atom:
    case mml::NodeKind::annotation_xml:
    case mml::NodeKind::none:
        break;
    default:
        throw std::logic_error("CommonWrapper scene emission for MathML kind '" +
                               std::string(node->document().kindName(node->kind())) +
                               "' requires a specialized wrapper");
    }
    auto containers = standardSceneNodes(builder, parents);
    addSceneChildren(builder, containers.content);
    return containers.nodes;
}

double CommonWrapper::emitUnknownText(scene::Builder &builder, scene::NodeId parent, double x,
                                      double y, std::u16string &unknown, std::string_view selected,
                                      CssFontData font) {
    if (unknown.empty())
        return 0;
    if (font.family.empty() && selected != "-explicitFont")
        font = jax().face().getCssFont(selected);
    const auto measured = selected == "-explicitFont" ? jax().measureText(unknown, selected, font)
                                                      : jax().measureText(unknown, selected);
    scene::TextRun run;
    run.utf8 = utf8(unknown);
    run.origin = {x, y};
    run.metrics = {measured.w, measured.h, measured.d};
    run.paint.fill = scene::InheritedPaint{};
    const auto size = jax().face().parameters.required("x_height") / metrics().ex * metrics().em;
    const auto points = utf16::codePoints(unknown);
    if (points.size() == 1 && points.front() >= 0x1D400 && points.front() <= 0x1D7FF)
        font = CssFontData{};
    run.font = scene::TextFont{font.family, size, font.italic, font.bold, std::string(selected)};
    run.role = scene::ElementRole::unknown_text;
    builder.add_primitive(parent, std::move(run));
    unknown.clear();
    return measured.w;
}

double CommonWrapper::emitChar(scene::Builder &builder, scene::NodeId parent,
                               std::uint32_t codepoint, double x, double y,
                               std::string_view selected, std::u16string &unknown, bool buffer,
                               scene::Transform transform) {
    auto resolved = getVariantChar(selected, codepoint);
    if (!resolved) {
        if (!transform.operations().empty())
            throw std::logic_error("transformed unknown glyph output is unsupported");
        const std::array<double, 1> point{static_cast<double>(codepoint)};
        unknown += string_util::unicodeString(point);
        return buffer ? 0
                      : emitUnknownText(builder, parent, x, y, unknown, selected,
                                        jax().face().getCssFont(selected));
    }
    const auto font = jax().face().getCssFont(selected);
    const auto dx = emitUnknownText(builder, parent, x, y, unknown, selected, font);
    const auto &glyph = *resolved->data;
    if (!glyph.composition.empty()) {
        scene::SemanticMetadata metadata;
        metadata.ordered_attributes.push_back({"data-c", hexCodepoint(codepoint)});
        std::vector<scene::TransformOperation> operations;
        if (x + dx != 0 || y != 0)
            operations.emplace_back(
                scene::Translation{x + dx, y, scene::TranslationSeparator::comma});
        operations.insert(operations.end(), transform.operations().begin(),
                          transform.operations().end());
        const auto group =
            builder.add_group(parent, scene::Transform(std::move(operations)), {}, {}, {},
                              std::move(metadata), scene::ElementRole::glyph_composition);
        double advance = 0;
        for (auto cp : glyph.composition)
            advance += emitChar(builder, group, cp, advance, 0, selected, unknown, false);
        return advance + dx;
    }
    if (glyph.resource) {
        scene::Glyph item{glyph.resource, resolved->key, {x + dx, y}, glyph.metrics.w, {}};
        item.paint.stroke_width = jax().options().glyphStrokeWidth;
        item.transform = std::move(transform);
        scene::GlyphRun run;
        run.glyphs.push_back(std::move(item));
        builder.add_primitive(parent, std::move(run));
    }
    return glyph.metrics.w + dx;
}

bool CommonWrapper::canStretch(StretchDirection direction) {
    stretch = NoStretch{};
    if (direction == StretchDirection::none)
        return false;
    if (node->isEmbellished()) {
        if (auto *inner = core(); inner && inner != this && inner->canStretch(direction))
            stretch = inner->stretch;
    }
    return std::holds_alternative<ActiveStretch>(stretch);
}

std::pair<std::string, double> CommonWrapper::getAlignShift() {
    const auto &attributes = node->attributes();
    auto align = valueString(attributes.lookup(u"indentalign"));
    auto shift = valueString(attributes.lookup(u"indentshift"));
    const auto firstAlign = valueString(attributes.lookup(u"indentalignfirst"), "indentalign");
    const auto firstShift = valueString(attributes.lookup(u"indentshiftfirst"), "indentshift");
    if (firstAlign != "indentalign")
        align = firstAlign;
    if (firstShift != "indentshift")
        shift = firstShift;
    return processIndent(align, shift);
}

std::pair<std::string, double> CommonWrapper::processIndent(std::string_view alignment,
                                                            std::string_view shift,
                                                            std::string_view defaultAlign,
                                                            std::string_view defaultShift,
                                                            std::optional<double> width) {
    if (!metrics().display)
        return {"left", 0};

    const auto *root = jax().document().root();
    const bool inlineMarked = root && root->properties().layout.inline_marked.value_or(false);
    std::string align(alignment);
    std::string indent(shift);
    std::string baseAlign(defaultAlign);
    std::string baseShift(defaultShift);
    if (baseAlign.empty() || baseAlign == "auto")
        baseAlign = inlineMarked ? "left" : jax().options().displayAlign;
    if (baseShift.empty() || baseShift == "auto")
        baseShift = inlineMarked ? "0" : utf8(jax().options().displayIndent);
    if (align == "auto")
        align = baseAlign;
    if (indent == "auto") {
        indent = baseShift;
        if (align == "right" && !isZeroIndent(decodeUtf8(indent))) {
            auto trimmed = decodeUtf8(indent);
            while (!trimmed.empty() && js_input::isWhitespace(trimmed.front()))
                trimmed.erase(trimmed.begin());
            while (!trimmed.empty() && js_input::isWhitespace(trimmed.back()))
                trimmed.pop_back();
            indent = "-" + utf8(trimmed);
            if (indent.starts_with("--"))
                indent.erase(0, 2);
        }
    }
    if (!width && indent.contains('%')) jax().requireExactContainerWidth();
    return {align,
            length2em(LengthInput(decodeUtf8(indent)), width.value_or(metrics().containerWidth))};
}

std::string CommonWrapper::getChildAlign(std::size_t) const {
    return "left";
}
} // namespace folia::math::output::common
