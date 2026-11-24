#pragma once

#include <string_view>

#include "windows/media/math/MathJaxRenderer.h"

namespace folia::platform
{
    MathJaxSvg ParseMathJaxSvgOutput(
        std::string_view output,
        bool display,
        float em,
        float containerWidth);
}
