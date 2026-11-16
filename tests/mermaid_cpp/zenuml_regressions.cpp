#include "support/folia_test.hpp"
import folia.mermaid.zenuml;
import folia.mermaid.directives;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"

std::u16string_view vertical_kind(ZenKind kind) {
    switch (kind) {
    case ZenKind::sync:
        return u"sync";
    case ZenKind::async:
        return u"async";
    case ZenKind::creation:
        return u"creation";
    case ZenKind::returned:
        return u"return";
    case ZenKind::loop:
        return u"loop";
    case ZenKind::alternative:
        return u"alt";
    case ZenKind::optional:
        return u"opt";
    case ZenKind::parallel:
        return u"par";
    case ZenKind::critical:
        return u"critical";
    case ZenKind::section:
        return u"section";
    case ZenKind::try_catch:
        return u"tcf";
    }
    return {};
}

suite zenuml_regressions = [] {
    "mermaid_zenuml_complex_horizontal_constraints_with_real_canvas_metrics"_test = [] {
        const auto directory = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path(directory ? directory : FOLIA_MERMAID_FIXTURE_DIR) /
                            "zenuml-canvas.tsv"};
        expect(fatal(input.is_open()));
        std::string row;
        std::size_t count{};
        while (std::getline(input, row)) {
            if (!row.empty() && row.back() == '\r')
                row.pop_back();
            if (row.empty())
                continue;
            const auto first = row.find('\t');
            const auto second = row.find('\t', first + 1);
            expect(fatal(first != row.npos && second != row.npos));
            const auto name = row.substr(0, first);
            const auto document =
                parse_zenuml(decode(std::string_view(row).substr(first + 1, second - first - 1)));
            const auto expected = json(decode(std::string_view(row).substr(second + 1)));
            expect(document.has_value()) << name;
            ++count;
            if (!document)
                continue;
            const auto measure = [&](std::u16string_view text) {
                for (const auto &value : field(expected, u"measurements").sequence) {
                    if (value.sequence[0].string == text)
                        return value.sequence[1].number;
                }
                throw std::runtime_error("Missing measured ZenUML Canvas text in " + name);
            };
            const auto layout = layout_zenuml(*document, measure);
            expect(layout.has_value()) << name;
            if (!layout)
                continue;
            const auto &positions = field(expected, u"positions").sequence;
            expect(layout->positions.size() == positions.size()) << name;
            for (std::size_t i = 0; i < std::min(positions.size(), layout->positions.size()); ++i) {
                expect(std::abs(layout->positions[i] - field(positions[i], u"x").number) < 1e-8) << name << i;
            }
            const auto &constraints = field(expected, u"constraints").sequence;
            expect(layout->constraints.size() == constraints.size()) << name;
            for (std::size_t i = 0; i < std::min(constraints.size(), layout->constraints.size()); ++i) {
                expect(layout->constraints[i].size() == constraints[i].sequence.size()) << name << i;
                for (std::size_t j = 0;
                     j < std::min(constraints[i].sequence.size(), layout->constraints[i].size()); ++j) {
                    expect(std::abs(layout->constraints[i][j] - constraints[i].sequence[j].number) < 1e-8)
                        << name << i << j;
                }
            }
        }
        expect(count == 24_u);
    };
    "mermaid_zenuml_official_documentation_and_complex_control_flow"_test = [] {
        const auto directory = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path(directory ? directory : FOLIA_MERMAID_FIXTURE_DIR) /
                            "zenuml-regression.tsv"};
        expect(fatal(input.is_open()));
        std::string row;
        std::size_t count{};
        while (std::getline(input, row)) {
            if (!row.empty() && row.back() == '\r')
                row.pop_back();
            if (row.empty())
                continue;
            const auto first = row.find('\t');
            const auto second = row.find('\t', first + 1);
            expect(fatal(first != row.npos && second != row.npos));
            const auto name = row.substr(0, first);
            const auto source = decode(std::string_view(row).substr(first + 1, second - first - 1));
            const auto expected = json(decode(std::string_view(row).substr(second + 1)));
            const auto document = parse_zenuml(source);
            ++count;
            expect(document.has_value()) << name << (document ? "" : document.error());
            if (!document)
                continue;
            expect(document->title == field(expected, u"title").string) << name;
            const auto &participants = field(expected, u"participants").sequence;
            expect(document->participants.size() == participants.size()) << name;
            for (std::size_t i = 0; i < std::min(participants.size(), document->participants.size()); ++i) {
                expect(document->participants[i].name == field(participants[i], u"name").string) << name << i;
                if (const auto *label = participants[i].find(u"label")) {
                    expect(document->participants[i].label == label->string) << name << i;
                }
                if (const auto *type = participants[i].find(u"type")) {
                    expect(document->participants[i].type == type->string) << name << i;
                }
            }
            const auto layout = layout_zenuml_vertical(*document);
            expect(layout.height == field(expected, u"height").number) << name << layout.height;
            const auto &coordinates = field(expected, u"vertical").sequence;
            expect(layout.coordinates.size() == coordinates.size()) << name;
            for (std::size_t i = 0; i < std::min(coordinates.size(), layout.coordinates.size()); ++i) {
                const auto &actual = layout.coordinates[i];
                expect(vertical_kind(actual.kind) == field(coordinates[i], u"kind").string) << name << i;
                expect(actual.top == field(coordinates[i], u"top").number) << name << i << actual.top;
                expect(actual.height == field(coordinates[i], u"height").number)
                    << name << i << actual.height;
            }
        }
        expect(count == 24_u);
    };
};
} // namespace
