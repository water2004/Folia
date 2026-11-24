#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

// Keep native AttributeDefaultValue alternatives unchanged. The owned storage
// outlives all string views in the expected entries and never mutates after load.
template <class Entry, class Value, class Null>
struct MmlDefaultFixtureData {
    struct Range { std::size_t offset, size; };

private:
    struct Nodes {
        std::vector<std::string> names;
        std::vector<bool> hasAttributes;
        std::vector<Range> ranges;
    };
    struct Entries {
        std::deque<std::u16string> strings;
        std::vector<Entry> values;
        std::u16string_view text(std::string_view field) {
            strings.push_back(folia::test::data::decode_utf16(field));
            return strings.back();
        }
        Value value(std::string_view kind, std::string_view field) {
            if (kind == "string") return Value{text(field)};
            if (kind == "number") return Value{std::bit_cast<double>(binary64(field))};
            if (kind == "boolean") return Value{boolean(field)};
            if (kind == "null" && field.empty()) return Value{Null{}};
            throw std::runtime_error("Invalid MML default reference value");
        }
    };
    static const Nodes& nodes() {
        static const auto result = [] {
            Nodes data;
            for (const auto& r : rows("math/fixtures/core/mml-defaults/nodes.tsv", 4, 43)) {
                data.names.push_back(bytes(r[0]));
                data.hasAttributes.push_back(boolean(r[1]));
                data.ranges.push_back({integer<std::size_t>(r[2]), integer<std::size_t>(r[3])});
            }
            return data;
        }();
        return result;
    }
    static Entries entries(std::string_view file, std::size_t count) {
        Entries data;
        for (const auto& r : rows(file, 3, count)) {
            const auto name = data.text(r[0]);
            data.values.push_back(Entry{name, data.value(r[1], r[2])});
        }
        return data;
    }

public:
    static const auto& nodeNames() { return nodes().names; }
    static const auto& nodeHasAttributes() { return nodes().hasAttributes; }
    static const auto& nodeRanges() { return nodes().ranges; }
    static const auto& nodeEntries() {
        static const auto data = entries("math/fixtures/core/mml-defaults/node-entries.tsv", 318);
        return data.values;
    }
    static const auto& globalEntries() {
        static const auto data = entries("math/fixtures/core/mml-defaults/global-entries.tsv", 28);
        return data.values;
    }
};

} // namespace math_test::fixtures
