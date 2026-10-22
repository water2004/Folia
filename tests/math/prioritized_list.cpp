#include "support/folia_test.hpp"
import folia.math.util.prioritized_list;

using namespace boost::ut;
using folia::math::PrioritizedList;

namespace {
template<class T> std::vector<T> order(PrioritizedList<T> const& list) {
    std::vector<T> result;
    for (auto entry : list) result.push_back(entry.item);
    return result;
}
}

suite prioritized_list = [] {
    "priority insertion is stable and removes the last matching item"_test = [] {
        PrioritizedList<std::string> list;
        expect(list.add("a") == "a");
        list.add("b", 1); list.add("c", 5); list.add("d", -2); list.add("e", 1);
        expect(order(list) == std::vector<std::string>{"d", "b", "e", "a", "c"});
        list.add("a", 8);
        expect(&list.remove("a") == &list);
        expect(order(list) == std::vector<std::string>{"d", "b", "e", "a", "c"});
        expect(&list.remove("missing") == &list);
        PrioritizedList<std::string> numeric;
        numeric.add("a", 5); numeric.add("n", std::numeric_limits<double>::quiet_NaN());
        numeric.add("l", -10); numeric.add("i", std::numeric_limits<double>::infinity());
        numeric.add("m", -std::numeric_limits<double>::infinity());
        expect(order(numeric) == std::vector<std::string>{"a", "n", "m", "l", "i"});
    };

};

// Append to tests/math/prioritized_list.cpp's existing typed owner.
namespace {
struct SourcePrioritySnapshot {
    std::vector<double> items, priorities;
};
#include "fixtures/tex-source/primitive_priority_values.inc"

bool sourcePriorityEqual(double actual, double expected) {
    if (std::isnan(expected)) return std::isnan(actual);
    if (expected == 0) return actual == 0 && std::signbit(actual) == std::signbit(expected);
    return actual == expected;
}

suite native_primitive_priority_values = [] {
    "native complete primitive priority item and order values"_test = [] {
        PrioritizedList<double> list;
        std::size_t snapshot = 0;
        const auto verify = [&] {
            const auto &expected = sourcePrioritySnapshots[snapshot++];
            std::vector<folia::math::PrioritizedListItem<double>> actual;
            for (auto entry : list) actual.push_back(entry);
            expect(actual.size() == expected.items.size());
            if (actual.size() != expected.items.size()) return;
            for (std::size_t i = 0; i < actual.size(); ++i) {
                expect(sourcePriorityEqual(actual[i].item, expected.items[i]));
                expect(sourcePriorityEqual(actual[i].priority, expected.priorities[i]));
            }
        };
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        const auto infinity = std::numeric_limits<double>::infinity();
        const std::array<std::pair<double, double>, 9> inputs{{
            {1, 5}, {2, 5}, {3, -7}, {1, 9}, {4, 0},
            {5, nan}, {6, infinity}, {7, -infinity}, {nan, 2}}};
        for (const auto &[item, priority] : inputs) {
            list.add(item, priority);
            verify();
        }
        for (double item : {1.0, nan, 99.0, 2.0, 1.0}) {
            list.remove(item);
            verify();
        }
        // The typed caller supplies the value 13 explicitly. Source's mutable
        // constructor DEFAULTPRIORITY and record identity remain Source-only.
        list.add(88, 13);
        verify();
        expect(snapshot == std::size(sourcePrioritySnapshots));

        PrioritizedList<int> dynamic;
        dynamic.add(1, 0);
        dynamic.add(2, 1);
        auto cursor = dynamic.begin();
        std::vector<int> seen{(*cursor).item};
        ++cursor;
        dynamic.remove(1);
        dynamic.add(3, 2);
        for (; cursor != dynamic.end(); ++cursor) seen.push_back((*cursor).item);
        expect(seen == std::vector<int>{1, 3});
    };
};
}
