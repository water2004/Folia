#include "support/folia_test.hpp"
import folia.mermaid.gantt_svg;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "mermaid_cpp/support/theme_fixture.hpp"

suite gantt_regressions = [] {
    "mermaid_gantt_empty_document_renders_the_official_empty_axis"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        auto input = prepare_gantt(
            u"%%{init: {\"gantt\":{\"useWidth\":800}}}%%\n"
            u"gantt\ntodayMarker off\ntickInterval 1week\nweekday monday", *session);
        expect(fatal(input.has_value()));
        // The real upstream renderer produces an 800 x 100 SVG, no ticks or
        // task rectangles, this domain path, and one empty title text node.
        // Empty diagrams need no fabricated font measurements.
        const auto svg = render_gantt_svg(*input, {});
        expect(fatal(svg.has_value())) << (svg ? "" : svg.error());
        expect(svg->width == 800.0);
        expect(svg->height == 100.0);
        expect(svg->svg.contains("M0.5,-15V0.5H650.5V-15"));
        expect(!svg->svg.contains("class=\"tick\""));
        expect(!svg->svg.contains("<rect"));
        expect(svg->svg.contains("<text x=\"400\" y=\"25\" class=\"titleText\"></text>"));
        input->document.today_marker.clear();
        const auto default_marker = render_gantt_svg(*input, {});
        expect(fatal(default_marker.has_value()));
        expect(!default_marker->svg.contains("NaN"));
        expect(!default_marker->svg.contains("class=\"today\""));
    };

    "mermaid_gantt_axis_formats_match_d3_calendar_and_iso_weeks"_test = [] {
        const auto override_dir = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path(
            override_dir ? override_dir : FOLIA_MERMAID_FIXTURE_DIR) / "gantt-formats.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count = 0;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto first = line.find('\t');
            const auto second = line.find('\t', first + 1);
            expect(fatal(first != line.npos && second != line.npos));
            const auto value = gantt_parse_date(decode(line.substr(0, first)),
                u"YYYY-MM-DD HH:mm:ss");
            expect(fatal(value.has_value()));
            const auto format = decode(line.substr(first + 1, second - first - 1));
            const auto expected = decode(line.substr(second + 1));
            expect(bool(gantt_format_date(*value, format) == expected)) << line;
            ++count;
        }
        expect(count == 72_u);
    };

    "mermaid_gantt_epoch_starts_and_ends_follow_the_official_db"_test = [] {
        // Mermaid's digit-only X start uses milliseconds, while X end uses
        // Dayjs seconds. Round trips avoid assuming the machine's timezone.
        const auto document = parse_gantt(
            u"gantt\ndateFormat X\ntodayMarker off\n"
            u"First :a, 0, 71\nSecond :b, 1000, 5\n");
        expect(fatal(document.has_value())) << (document ? "" : document.error());
        expect(fatal(document->tasks.size() == 2_u));
        const auto& first = document->tasks[0];
        const auto& second = document->tasks[1];
        expect(first.end - first.start == 71000_i);
        expect(second.start - first.start == 1000_i);
        expect(second.end - first.start == 5000_i);
        expect(bool(gantt_format_date(first.start, u"%s") == u"0"));
        expect(bool(gantt_format_date(first.end, u"%s") == u"71"));
        expect(bool(gantt_format_date(second.start, u"%Q") == u"1000"));

        const auto milliseconds = gantt_parse_date(u"1234", u"x");
        expect(fatal(milliseconds.has_value()));
        expect(bool(gantt_format_date(*milliseconds, u"%Q") == u"1234"));
        const auto negative = gantt_parse_date(u"-1", u"X");
        expect(fatal(negative.has_value()));
        expect(bool(gantt_format_date(*negative, u"%s") == u"-1"));
        expect(!gantt_parse_date(u"8640000000000001", u"x"));
        expect(!gantt_parse_date(u"not a timestamp", u"X"));
    };

    "mermaid_gantt_multiline_accessibility_preserves_schedule"_test = [] {
        const auto document = parse_gantt(
            u"gantt\naccTitle: Release plan\n"
            u"accDescr {\n  First phase.\n  Second phase.\n}\n"
            u"dateFormat YYYY-MM-DD\ntodayMarker off\n"
            u"Design :design, 2025-03-10, 3d\n"
            u"Build :build, after design, 5d\n");
        expect(fatal(document.has_value())) << (document ? "" : document.error());
        expect(bool(document->acc_title == u"Release plan"));
        expect(bool(document->acc_description == u"First phase.\n  Second phase."));
        expect(fatal(document->tasks.size() == 2_u));
        expect(document->tasks[1].start == document->tasks[0].end);
        expect(document->tasks[1].end - document->tasks[1].start == 432000000_i);
        expect(!parse_gantt(u"gantt\naccDescr { unfinished"));
        const auto brace = parse_gantt(u"gantt\naccDescr: Literal { brace\n");
        expect(fatal(brace.has_value()));
        expect(bool(brace->acc_description == u"Literal { brace"));
        const auto next_line = parse_gantt(u"gantt\naccDescr\n{\n  On the next line.\n}\n");
        expect(fatal(next_line.has_value()));
        expect(bool(next_line->acc_description == u"On the next line."));
    };
};
}
