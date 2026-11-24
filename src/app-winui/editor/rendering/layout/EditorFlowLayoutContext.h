#pragma once

namespace winrt::Folia
{
    // Text-format selection and line spacing belonged to the complete semantic
    // flow before it acquired physical fragments. Keep this small context
    // shared, without retaining any fragment's display text or glyph objects.
    enum class EditorFlowTextFormat : std::uint8_t
    {
        Body, Code, Heading1, Heading2, Heading3,
    };

    struct EditorFlowLayoutContext
    {
        EditorFlowTextFormat format = EditorFlowTextFormat::Body;
        bool naturalLineSpacing = false;
        bool fragmented = false;
        bool operator==(EditorFlowLayoutContext const&) const = default;
    };

    struct EditorFlowLayoutSummary
    {
        std::array<std::size_t, 5> formatCounts{};
        std::size_t naturalLineSpacingCount = 0;

        void Add(EditorFlowLayoutContext context)
        {
            ++formatCounts[static_cast<std::size_t>(context.format)];
            if (context.naturalLineSpacing) ++naturalLineSpacingCount;
        }

        void Remove(EditorFlowLayoutContext context)
        {
            --formatCounts[static_cast<std::size_t>(context.format)];
            if (context.naturalLineSpacing) --naturalLineSpacingCount;
        }

        EditorFlowLayoutContext Context() const
        {
            EditorFlowLayoutContext result;
            result.fragmented = true;
            bool found = false;
            for (std::size_t index = 0; index < formatCounts.size(); ++index)
            {
                if (formatCounts[index] == 0) continue;
                if (found)
                {
                    result.format = EditorFlowTextFormat::Body;
                    break;
                }
                result.format = static_cast<EditorFlowTextFormat>(index);
                found = true;
            }
            result.naturalLineSpacing = naturalLineSpacingCount != 0;
            return result;
        }
    };
}
