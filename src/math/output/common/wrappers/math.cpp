module folia.math.output.common;
import :math;
import :output_jax;
import folia.math.output.scene.types;
import std;

namespace folia::math::output::common {
double CommonMath::getWrapWidth(std::size_t) {
    if (parent)
        return getBBox().w;
    jax().requireExactContainerWidth();
    return metrics().containerWidth / jax().pxPerEm();
}

bool CommonMath::setChildPWidths(bool recompute, std::optional<double> width, bool) {
    // The numerical part of Source SvgMath.setChildPWidths belongs to the
    // shared layout: both output backends consume the same resolved widths.
    if (!parent && !recompute && std::ranges::any_of(childNodes, [](auto* child) { return !child->bbox.pwidth.empty(); }))
        jax().requireExactContainerWidth();
    return CommonWrapper::setChildPWidths(
        recompute, parent ? width : metrics().containerWidth / jax().pxPerEm(), false);
}

void CommonMath::computeBBox(BBox &box, bool recompute) {
    CommonWrapper::computeBBox(box, recompute);
    const auto *overflowValue = node->attributes().lookup(u"overflow");
    const auto *overflow = overflowValue ? std::get_if<std::u16string>(overflowValue) : nullptr;
    if (parent || !metrics().display || !overflow || *overflow != u"linebreak")
        return;

    const double width = containerWidth();
    if (box.w > width) {
        jax().requireExactContainerWidth();
        if (childNodes.empty())
            throw std::logic_error("linebreak math wrapper has no children");
        childNodes.front()->breakToWidth(width);
    } else if (!jax().usesContainerLinebreakWidth()) {
        jax().requireExactContainerWidth();
    } else {
        jax().requireMinimumContainerWidth(box.w * jax().pxPerEm());
    }
    if (childNodes.empty())
        throw std::logic_error("linebreak math wrapper has no children");
    box.updateFrom(childNodes.front()->getBBox());
}

SceneNodes CommonMath::emitScene(scene::Builder &builder, std::span<scene::NodeId const> parents) {
    auto result = CommonWrapper::emitScene(builder, parents);
    const auto *displayValue = node->attributes().lookup(u"display");
    const auto *displayText = displayValue ? std::get_if<std::u16string>(displayValue) : nullptr;
    const bool display = displayText && *displayText == u"block";
    if (display) {
        const auto [alignment, shift] = getAlignShift();
        const bool fullWidth = bbox.pwidth == BBox::fullWidth;
        double minimumWidth = 0;
        if (fullWidth) {
            if (auto *table = jax().table()) {
                auto box = table->getOuterBBox();
                double left = box.L, right = box.R;
                double width = box.w;
                if (alignment == "right")
                    right = std::max(right, -shift);
                else if (alignment == "left")
                    left = std::max(left, shift);
                else if (alignment == "center")
                    width += 2 * std::abs(shift);
                minimumWidth = std::max(0.0, left + width + right);
            }
        }
        builder.set_display_layout(scene::DisplayLayout{.display = true,
                                                        .alignment = alignment,
                                                        .shift = fullWidth ? 0.0 : shift,
                                                        .minimum_width = minimumWidth,
                                                        .full_width = fullWidth});
    }
    return result;
}
} // namespace folia::math::output::common
