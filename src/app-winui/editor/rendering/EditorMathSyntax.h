#pragma once
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorRenderResources.h"

namespace winrt::Folia {
    void PaintMathBrackets(EditorRenderResources& resources, EditorStyleSheet const& sheet,
        IDWriteTextLayout* layout, D2D1_POINT_2F origin, DisplayInlineText const& display,
        EditorDisplayMapping const& mapping);
}
