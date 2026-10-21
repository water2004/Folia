// Source-derived specialization of d3-shape 3.2.0 src/pie.js, src/arc.js,
// src/math.js, src/path.js and d3-path 3.1.0 src/path.js for Mermaid Pie.
// d3-shape: Copyright 2010-2022 Mike Bostock.
// d3-path: Copyright 2015-2022 Mike Bostock.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
// RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
// NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
// USE OR PERFORMANCE OF THIS SOFTWARE.
module folia.mermaid.pie_geometry;
import std;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {
constexpr double pi = std::numbers::pi;
constexpr double tau = 2 * pi;
constexpr double shape_epsilon = 1e-12;
constexpr double path_epsilon = 1e-6;
constexpr double radius = 185;

// The subset of d3-path Path used by the unpadded, square-cornered Pie arc.
// Keep unrounded pen coordinates: D3 only rounds when serializing commands.
class PiePath {
    std::string output_;
    std::optional<double> x_, y_;
    bool valid_ = true;

    void number(double value) {
        // Math.round ties toward positive infinity, including negative ties;
        // std::round has different semantics. The fractional test also avoids
        // the extra rounding that can occur in floor(value + 0.5).
        double const scaled = value * 1000;
        double const below = std::floor(scaled);
        double const rounded = (scaled - below >= 0.5 ? below + 1 : below) / 1000;
        auto formatted = format_number(rounded);
        if (!formatted) {
            valid_ = false;
            return;
        }
        output_ += *formatted;
    }

    void point(double x, double y) {
        number(x);
        output_ += ',';
        number(y);
    }

    void arc_command(double r, bool large, bool clockwise, double x, double y) {
        output_ += 'A';
        number(r);
        output_ += ',';
        number(r);
        output_ += ",0,";
        output_ += large ? '1' : '0';
        output_ += ',';
        output_ += clockwise ? '1' : '0';
        output_ += ',';
        point(x, y);
    }

public:
    void move_to(double x, double y) {
        x_ = x;
        y_ = y;
        output_ += 'M';
        point(x, y);
    }

    void line_to(double x, double y) {
        x_ = x;
        y_ = y;
        output_ += 'L';
        point(x, y);
    }

    void arc(double r, double a0, double a1, bool counterclockwise) {
        double const dx = r * std::cos(a0);
        double const dy = r * std::sin(a0);
        double const x0 = 0 + dx;
        double const y0 = 0 + dy;
        bool const clockwise = !counterclockwise;
        double da = counterclockwise ? a0 - a1 : a1 - a0;
        if (!x_) {
            output_ += 'M';
            point(x0, y0);
        } else if (std::abs(*x_ - x0) > path_epsilon || std::abs(*y_ - y0) > path_epsilon) {
            // As in d3-path, this connecting command does not update the pen
            // until the arc command below has a non-empty angular span.
            output_ += 'L';
            point(x0, y0);
        }
        if (r == 0) return;
        if (da < 0) da = std::fmod(da, tau) + tau;
        if (da > tau - path_epsilon) {
            arc_command(r, true, clockwise, 0 - dx, 0 - dy);
            arc_command(r, true, clockwise, x0, y0);
            x_ = x0;
            y_ = y0;
        } else if (da > path_epsilon) {
            x_ = 0 + r * std::cos(a1);
            y_ = 0 + r * std::sin(a1);
            arc_command(r, da >= pi, clockwise, *x_, *y_);
        }
    }

    std::optional<std::string> finish() {
        if (!valid_) return std::nullopt;
        // All arcs in this specialization start with move_to. No commands
        // follow closePath, so resetting the pen to the subpath is unnecessary.
        output_ += 'Z';
        return std::move(output_);
    }
};

std::optional<std::string> sector_path(double start, double end, double inner_radius) {
    double const a0 = start - pi / 2;
    double const a1 = end - pi / 2;
    double const da = std::abs(a1 - a0);
    bool const clockwise = a1 > a0;
    PiePath path;
    // d3-shape's circle test deliberately has a different epsilon from the
    // d3-path arc test. Near-full sectors can therefore have a full outer arc
    // followed by a radial line, exactly as in the original implementations.
    if (da > tau - shape_epsilon) {
        path.move_to(radius * std::cos(a0), radius * std::sin(a0));
        path.arc(radius, a0, a1, !clockwise);
        if (inner_radius > shape_epsilon) {
            path.move_to(inner_radius * std::cos(a1), inner_radius * std::sin(a1));
            path.arc(inner_radius, a1, a0, clockwise);
        }
    } else {
        path.move_to(radius * std::cos(a0), radius * std::sin(a0));
        if (da > shape_epsilon) path.arc(radius, a0, a1, !clockwise);
        if (!(inner_radius > shape_epsilon) || !(da > shape_epsilon)) {
            path.line_to(inner_radius * std::cos(a1), inner_radius * std::sin(a1));
        } else {
            path.arc(inner_radius, a1, a0, clockwise);
        }
    }
    return path.finish();
}

}

std::expected<std::vector<PieSectorGeometry>, PieGeometryError> layout_pie_geometry(
    std::span<double const> values, PieGeometryConfig const& config) {
    auto const configuration_error = [&](std::string message) {
        return std::unexpected(PieGeometryError{values.size(), std::move(message)});
    };
    if (!std::isfinite(config.donut_hole) || !std::isfinite(config.text_position)) {
        return configuration_error("Pie geometry configuration must be finite.");
    }
    double const hole = config.donut_hole > 0 && config.donut_hole <= 0.9 ? config.donut_hole : 0;
    double const inner_radius = hole * radius;
    double const label_radius = radius * config.text_position;
    // Retain arc.centroid's operation order, including the radius addition.
    double const centroid_radius = (label_radius + label_radius) / 2;
    if (!std::isfinite(centroid_radius)) {
        return configuration_error("Pie label radius exceeds finite geometry limits.");
    }
    double sum = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        double const value = values[index];
        if (!std::isfinite(value)) {
            return std::unexpected(PieGeometryError{index, "Pie values must be finite."});
        }
        if (value > 0) sum += value;
    }
    if (!std::isfinite(sum)) {
        return configuration_error("Pie positive value sum exceeds finite geometry limits.");
    }
    double const scale = sum != 0 ? tau / sum : 0;
    if (!std::isfinite(scale)) {
        return configuration_error("Pie positive value sum is too small for finite angles.");
    }
    std::vector<PieSectorGeometry> result;
    result.reserve(values.size());
    double start = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        double const value = values[index];
        // pie().sort(null) keeps input order; nonpositive values add no angle.
        double const end = start + (value > 0 ? value * scale : 0) + 0;
        double const middle = (start + end) / 2 - pi / 2;
        double const label_x = std::cos(middle) * centroid_radius;
        double const label_y = std::sin(middle) * centroid_radius;
        if (!std::isfinite(end) || !std::isfinite(label_x) || !std::isfinite(label_y)) {
            return std::unexpected(PieGeometryError{index, "Pie sector exceeds finite geometry limits."});
        }
        auto path = sector_path(start, end, inner_radius);
        if (!path) {
            return std::unexpected(PieGeometryError{index, "Pie path contains an unrepresentable coordinate."});
        }
        result.push_back(PieSectorGeometry{value, start, end, std::move(*path), label_x, label_y});
        start = end;
    }
    return result;
}

}
