#pragma once

#include "media/MathJaxRenderer.h"
#include "editor/session/EditorRenderFrame.h"
#include "editor/rendering/EditorPreparedDocument.h"

namespace winrt::Folia
{
    struct EditorMathPreparationContext
    {
        folia::ThemeProfile theme;
        std::uint64_t themeRevision = 0;
        float surfaceWidth = 1.0f;
        float surfaceHeight = 1.0f;
        bool ready = false;
        bool svgSupported = true;
    };

    // Completed math results and compact measured geometry leave background
    // preparation. No text layouts, paint plans or GPU resources cross threads.
    struct EditorMathPreparation
    {
        EditorMathPreparationContext context;
        MathJaxRenderer math;
        std::size_t formulaCount = 0;
        std::unique_ptr<EditorPreparedDocument> document;
    };
}
