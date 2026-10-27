#include "pch.h"
#include "media/MathJaxSvgParser.h"

namespace
{
    float ParseLength(std::string_view value, float em, float percentageBasis = 0.0f)
    {
        std::size_t consumed = 0;
        float number = 0.0f;
        try
        {
            number = std::stof(std::string(value), &consumed);
        }
        catch (...)
        {
            return 0.0f;
        }

        auto unit = value.substr(consumed);
        if (unit.starts_with("%")) return number * percentageBasis / 100.0f;
        if (unit.starts_with("ex")) return number * em * 0.5f;
        if (unit.starts_with("em")) return number * em;
        if (unit.starts_with("pt")) return number * (96.0f / 72.0f);
        return number;
    }

    float AttributeLength(std::string_view svg, std::string_view attribute, float em,
                          float percentageBasis = 0.0f)
    {
        auto marker = std::string(attribute) + "=\"";
        auto start = svg.find(marker);
        if (start == std::string_view::npos) return 0.0f;
        start += marker.size();
        auto end = svg.find('"', start);
        if (end == std::string_view::npos) return 0.0f;
        return ParseLength(svg.substr(start, end - start), em, percentageBasis);
    }

    std::optional<std::string> AttributeText(std::string_view markup, std::string_view attribute)
    {
        auto marker = std::string(attribute) + "=\"";
        auto start = markup.find(marker);
        if (start == std::string_view::npos) return std::nullopt;
        start += marker.size();
        auto end = markup.find('"', start);
        if (end == std::string_view::npos) return std::nullopt;
        return std::string(markup.substr(start, end - start));
    }

    float StyleLength(std::string_view header, std::string_view property, float em,
                      float percentageBasis = 0.0f)
    {
        auto style = AttributeText(header, "style");
        if (!style) return 0.0f;
        auto remaining = std::string_view(*style);
        while (!remaining.empty())
        {
            auto end = remaining.find(';');
            auto declaration = remaining.substr(0, end);
            auto colon = declaration.find(':');
            if (colon != std::string_view::npos)
            {
                auto key = declaration.substr(0, colon);
                auto first = key.find_first_not_of(" \t\r\n");
                auto last = key.find_last_not_of(" \t\r\n");
                if (first != std::string_view::npos && key.substr(first, last - first + 1) == property)
                    return ParseLength(declaration.substr(colon + 1), em, percentageBasis);
            }
            if (end == std::string_view::npos) break;
            remaining.remove_prefix(end + 1);
        }
        return 0.0f;
    }

    float VerticalAlignment(std::string_view svg, float em)
    {
        constexpr auto marker = std::string_view("vertical-align:");
        auto start = svg.find(marker);
        if (start == std::string_view::npos) return 0.0f;
        start += marker.size();
        while (start < svg.size() && svg[start] == ' ') ++start;
        auto end = svg.find(';', start);
        if (end == std::string_view::npos) end = svg.find('"', start);
        if (end == std::string_view::npos) end = svg.size();
        return ParseLength(svg.substr(start, end - start), em);
    }

    float BreakSpacing(std::string_view markup, float em)
    {
        auto sizeStart = markup.find("size=\"");
        if (sizeStart != std::string_view::npos)
        {
            sizeStart += std::string_view("size=\"").size();
            auto sizeEnd = markup.find('"', sizeStart);
            if (sizeEnd != std::string_view::npos)
            {
                auto value = markup.substr(sizeStart, sizeEnd - sizeStart);
                constexpr std::array<float, 6> spaces{ 0.0f, 2.0f / 18.0f, 3.0f / 18.0f, 4.0f / 18.0f, 5.0f / 18.0f, 6.0f / 18.0f };
                if (value.size() == 1 && value.front() >= '0' && value.front() <= '5')
                {
                    return spaces[static_cast<std::size_t>(value.front() - '0')] * em;
                }
            }
        }

        constexpr auto marker = std::string_view("letter-spacing:");
        auto spacingStart = markup.find(marker);
        if (spacingStart == std::string_view::npos) return 0.0f;
        spacingStart += marker.size();
        while (spacingStart < markup.size() && markup[spacingStart] == ' ') ++spacingStart;
        auto spacingEnd = markup.find_first_of(";\"", spacingStart);
        if (spacingEnd == std::string_view::npos) spacingEnd = markup.size();
        return (std::max)(0.0f, ParseLength(markup.substr(spacingStart, spacingEnd - spacingStart), em) + em);
    }

    std::optional<std::size_t> SvgElementEnd(std::string_view markup, std::size_t start)
    {
        std::size_t depth = 0;
        auto cursor = start;
        while (cursor < markup.size())
        {
            auto open = markup.find("<svg", cursor);
            auto close = markup.find("</svg>", cursor);
            if (close == std::string_view::npos) return std::nullopt;
            if (open != std::string_view::npos && open < close)
            {
                auto tagEnd = markup.find('>', open + 4);
                if (tagEnd == std::string_view::npos) return std::nullopt;
                if (tagEnd == open || markup[tagEnd - 1] != '/') ++depth;
                cursor = tagEnd + 1;
                continue;
            }
            if (depth == 0) return std::nullopt;
            --depth;
            cursor = close + std::string_view("</svg>").size();
            if (depth == 0) return cursor;
        }
        return std::nullopt;
    }
}

namespace winrt::Folia
{
    MathJaxSvg ParseMathJaxSvgOutput(std::string_view output, bool display, float em,
                                   float containerWidth)
    {
        MathJaxSvg rendered;
        rendered.display = display;
        if (auto diagnostic = AttributeText(output, "data-mjx-error"))
        {
            rendered.error = std::move(*diagnostic);
            rendered.errorKind = MathJaxErrorKind::Formula;
        }

        std::size_t cursor = 0;
        std::size_t previousEnd = 0;
        float ascent = 0.0f;
        float descent = 0.0f;
        while (true)
        {
            auto svgStart = output.find("<svg", cursor);
            if (svgStart == std::string_view::npos) break;
            auto svgEnd = SvgElementEnd(output, svgStart);
            if (!svgEnd) break;

            MathJaxSvgFragment fragment;
            auto svg = std::string(output.substr(svgStart, *svgEnd - svgStart));
            auto header = std::string_view(svg).substr(0, svg.find('>') + 1);
            fragment.width = (std::max)(AttributeLength(header, "width", em, containerWidth),
                                       StyleLength(header, "min-width", em, containerWidth));
            fragment.height = AttributeLength(header, "height", em);
            fragment.verticalAlign = VerticalAlignment(header, em);
            // A standalone SVG has no browser layout container. Resolve its
            // outer viewport once, before normalization; nested percentage
            // viewports then use the same width for content and equation labels.
            // Keep MathJax's transforms and data-mjx-viewBox metadata unchanged.
            auto width = AttributeText(header, "width");
            if (width && (width->find('%') != std::string::npos ||
                          fragment.width > AttributeLength(header, "width", em)))
            {
                auto at = header.find("width=\"") + std::string_view("width=\"").size();
                svg.replace(at, width->size(), std::format("{}px", fragment.width));
            }
            fragment.svg = std::make_shared<std::string const>(std::move(svg));
            if (!rendered.fragments.empty())
            {
                auto between = output.substr(previousEnd, svgStart - previousEnd);
                fragment.breakBefore = between.find("<mjx-break") != std::string_view::npos;
                fragment.breakSpace = BreakSpacing(between, em);
            }
            if (fragment.width > 0.0f && fragment.height > 0.0f)
            {
                rendered.width += fragment.breakSpace + fragment.width;
                auto baseline = (std::clamp)(fragment.height + fragment.verticalAlign, 0.0f, fragment.height);
                ascent = (std::max)(ascent, baseline);
                descent = (std::max)(descent, fragment.height - baseline);
                rendered.fragments.push_back(std::move(fragment));
            }
            previousEnd = *svgEnd;
            cursor = *svgEnd;
        }

        rendered.height = ascent + descent;
        rendered.verticalAlign = -descent;
        if (!rendered)
        {
            rendered.error = "MathJax returned an invalid SVG";
            rendered.errorKind = MathJaxErrorKind::Infrastructure;
        }
        return rendered;
    }
}
