#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

namespace svg_paint_artifacts {

// Opt-in evidence from the existing direct MathML fixture pipeline. Both inputs
// are original bytes: never export the expected-side paint projection as Source.
inline void capture(std::string_view family, std::string_view name,
                    std::string_view source, std::string_view native,
                    double em, double ex, double container_width) {
    const auto *requested = std::getenv("FOLIA_MATH_SVG_PAINT_CAPTURE_DIR");
    if (!requested || !*requested)
        return;
    for (const auto component : {family, name}) {
        if (component.empty())
            throw std::invalid_argument("SVG paint artifact name is empty");
        for (const auto value : component)
            if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                  (value >= '0' && value <= '9') || value == '-' || value == '_'))
                throw std::invalid_argument("SVG paint artifact name contains an unsafe character");
    }
    const auto directory = std::filesystem::path(requested) / std::string(family);
    std::filesystem::create_directories(directory);
    const auto write = [&](std::string_view suffix, std::string_view bytes) {
        std::ofstream stream;
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        stream.open(directory / (std::string(name) + std::string(suffix)), std::ios::binary);
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    };
    write(".source.svg", source);
    write(".native.svg", native);
    std::ofstream manifest;
    manifest.exceptions(std::ios::failbit | std::ios::badbit);
    manifest.open(directory / (std::string(name) + ".json"), std::ios::binary);
    manifest << std::setprecision(17)
             << "{\"schema\":1,\"family\":" << std::quoted(std::string(family))
             << ",\"name\":" << std::quoted(std::string(name))
             << ",\"em\":" << em << ",\"ex\":" << ex
             << ",\"containerWidth\":" << container_width << "}\n";
}

} // namespace svg_paint_artifacts
