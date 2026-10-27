#pragma once

#include "media/MathJaxRenderer.h"
#include "media/MermaidRenderer.h"

import folia.markdown.text_edit;

namespace winrt::Folia
{
    // Keep SVG intrinsic geometry intact; scale belongs to its presentation.
    using EditorDrawMath = std::function<bool(MathJaxSvgFragment const&, D2D1_POINT_2F, D2D1_COLOR_F, float scale)>;
    using EditorDrawMathFallback = std::function<void(folia::TextSpan, D2D1_POINT_2F)>;
    using EditorDrawMermaid = std::function<bool(
        MermaidSvg const&,
        float,
        float,
        D2D1_POINT_2F)>;
}
