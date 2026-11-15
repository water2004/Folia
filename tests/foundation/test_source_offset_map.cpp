#include "support/folia_test.hpp"

import folia.markdown.source_offset_map;

using namespace folia;
using namespace boost::ut;

suite source_offset_map_tests = [] {
    "source_offset_map_linear_and_repeated_projections_need_no_heap"_test = [] {
        SourceOffsetMap linear;
        linear.assign_linear(37, 10'000'001);
        expect(linear.size() == 10'000'001_u);
        expect(linear.run_count() == 1_u);
        expect(linear.allocated_bytes() == 0_u);
        expect(linear.front() == 37_u);
        expect(linear.back() == 10'000'037_u);
        expect(linear[4'000'000] == 4'000'037_u);
        expect(linear.lower_bound(5'000'037) - linear.begin() == 5'000'000);
        expect(linear.lower_bound(10'000'038) == linear.end());
        for (std::size_t index = 0; index < 10'000; ++index) linear.push_back(linear.back() + 1);
        expect(linear.run_count() == 1_u);
        expect(linear.allocated_bytes() == 0_u);

        SourceOffsetMap repeated;
        repeated.assign(1'000'000, 12);
        expect(repeated.size() == 1'000'000_u);
        expect(repeated.run_count() == 1_u);
        expect(repeated.allocated_bytes() == 0_u);
        expect(repeated[600'000] == 12_u);
        expect(repeated.lower_bound(12) == repeated.begin());
        expect(repeated.lower_bound(13) == repeated.end());
    };

    "source_offset_map_preserves_gaps_repetitions_and_terminal_boundaries"_test = [] {
        const std::vector<std::size_t> expected{3, 4, 5, 10, 10, 10, 11, 12, 20};
        SourceOffsetMap mapping(expected.begin(), expected.end());
        expect(std::ranges::equal(mapping, expected));
        expect(mapping.front() == 3_u);
        expect(mapping.back() == 20_u);
        for (std::size_t value = 0; value != 23; ++value) {
            const auto reference = std::ranges::lower_bound(expected, value) - expected.begin();
            expect(mapping.lower_bound(value) - mapping.begin() == reference);
            expect(std::ranges::lower_bound(mapping, value) - mapping.begin() == reference);
        }
        for (std::size_t index = 0; index < expected.size(); ++index) {
            expect(mapping[index] == expected[index]);
            expect(mapping.begin()[static_cast<std::ptrdiff_t>(index)] == expected[index]);
            expect(*(mapping.end() - static_cast<std::ptrdiff_t>(expected.size() - index)) == expected[index]);
        }
        expect(throws<std::out_of_range>([&] { static_cast<void>(mapping.at(expected.size())); }));
        expect(throws<std::out_of_range>([&] { static_cast<void>(mapping.slice(2, expected.size() + 1)); }));
    };

    "source_offset_map_discontinuous_inputs_never_expand_to_large_run_tables"_test = [] {
        SourceOffsetMap mapping;
        std::vector<std::size_t> reference;
        for (std::size_t index = 0; index < 10'000; ++index) {
            const auto value = index * 7;
            mapping.push_back(value);
            reference.push_back(value);
        }
        expect(mapping.uses_dense_storage());
        expect(mapping.allocated_bytes() <= reference.capacity() * sizeof(std::size_t));
        expect(std::ranges::equal(mapping, reference));
        expect(mapping.lower_bound(7001) - mapping.begin() == 1001);
        const auto slice = mapping.slice(777, 999);
        expect(std::ranges::equal(slice.begin(), slice.end(), reference.begin() + 777, reference.begin() + 999));
        mapping.assign_linear(15, 1'000'000);
        expect(!mapping.uses_dense_storage());
        expect(mapping.allocated_bytes() == 0_u);
    };

    "source_offset_map_copies_and_slices_keep_independent_exact_values"_test = [] {
        SourceOffsetMap original{1, 2, 7, 8, 8, 8, 9, 20};
        auto copy = original;
        copy.push_back(21);
        expect(original.size() == 8_u);
        expect(copy.size() == 9_u);
        expect(original.back() == 20_u);
        const auto slice = original.slice(2, 7);
        expect(std::ranges::equal(slice, std::vector<std::size_t>{7, 8, 8, 8, 9}));
        auto moved = std::move(copy);
        expect(moved.back() == 21_u);
        moved.assign_linear(40, 5);
        expect(moved.allocated_bytes() == 0_u);
        expect(std::ranges::equal(moved, std::vector<std::size_t>{40, 41, 42, 43, 44}));
        expect(original.back() == 20_u);
        expect(original.slice(original.size(), original.size()).empty());
        moved.clear();
        expect(moved.empty());
        expect(moved.begin() == moved.end());
        expect(moved.lower_bound(0) == moved.end());
    };

    "source_offset_map_matches_dense_oracle_for_mixed_projection_runs"_test = [] {
        std::mt19937_64 random(0x736f75726365);
        for (std::size_t trial = 0; trial < 256; ++trial) {
            std::vector<std::size_t> reference;
            auto value = static_cast<std::size_t>(random() % 31);
            for (auto count = std::size_t{64} + random() % 512; reference.size() < count;) {
                value += random() % 7 == 0 ? random() % 29 : random() % 8 == 0 ? 0 : 1;
                reference.push_back(value);
            }
            SourceOffsetMap mapping(reference.begin(), reference.end());
            expect(std::ranges::equal(mapping, reference));
            auto reversed = mapping.end();
            bool exact_reverse = true;
            for (auto index = reference.size(); index > 0; --index)
                exact_reverse = exact_reverse && *--reversed == reference[index - 1];
            expect(exact_reverse);
            for (std::size_t sample = 0; sample < 16; ++sample) {
                const auto offset = static_cast<std::size_t>(random() % (value + 10));
                expect(mapping.lower_bound(offset) - mapping.begin()
                    == std::ranges::lower_bound(reference, offset) - reference.begin());
                auto begin = static_cast<std::size_t>(random() % (reference.size() + 1));
                auto end = static_cast<std::size_t>(random() % (reference.size() + 1));
                if (end < begin) std::swap(begin, end);
                const auto sliced = mapping.slice(begin, end);
                expect(std::ranges::equal(sliced.begin(), sliced.end(),
                    reference.begin() + begin, reference.begin() + end));
            }
        }
    };
};
