#pragma once

#include "windows/media/math/MathJaxRenderer.h"
#include "media/MediaDisplaySettings.h"
#include "editor/session/EditorRenderFrame.h"
#include "editor/rendering/layout/EditorPreparedDocument.h"

import folia.app.text_layout_options;

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
        MediaDisplaySettings media;
        std::optional<float> imageMaxHeight;
        TextLayoutOptions textLayout;
    };

    // Completed math results and compact measured geometry leave background
    // preparation. No text layouts, paint plans or GPU resources cross threads.
    struct EditorMathPreparation
    {
        EditorMathPreparationContext context;
        folia::platform::MathJaxRenderer math;
        std::size_t formulaCount = 0;
        std::unique_ptr<EditorPreparedDocument> document;
    };
}
