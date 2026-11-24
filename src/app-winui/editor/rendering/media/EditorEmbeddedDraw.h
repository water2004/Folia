#pragma once

#include "windows/media/math/MathJaxRenderer.h"
#include "windows/media/mermaid/MermaidRenderer.h"

import folia.markdown.text_edit;

namespace winrt::Folia
{
    // Keep SVG intrinsic geometry intact; scale belongs to its presentation.
    using EditorDrawMath = std::function<bool(folia::platform::MathJaxSvgFragment const&, D2D1_POINT_2F, D2D1_COLOR_F, float scale)>;
    using EditorDrawMathFallback = std::function<void(folia::TextSpan, D2D1_POINT_2F)>;
    using EditorDrawMermaid = std::function<bool(
        folia::platform::MermaidSvg const&,
        float,
        float,
        D2D1_POINT_2F)>;
}
