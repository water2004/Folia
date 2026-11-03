#include "support/folia_test.hpp"
import folia.completion;

using namespace boost::ut;
namespace {
suite completion_tests = [] {
    "completion request remains bounded for 10 MB documents"_test = [] {
        std::u32string source(10 * 1024 * 1024, U'x');
        for (auto offset : {std::size_t{0}, source.size() / 2, source.size()}) {
            std::u32string_view neighbors[] = {source, source, source};
            auto c = folia::completion::capture(source, offset, neighbors);
            auto p = folia::completion::make_prompt(c);
            expect(c.prefix.size() <= 4096_u);
            expect(c.suffix.size() <= 1024_u);
            expect(c.preceding.size() <= 2048_u);
            expect(p.instructions.size() + p.context.size() + p.cursor.size() < 8192_u);
            expect(p.output_tokens == 128_u);
        }
    };
    "completion budgets bound encoded Unicode and untrusted custom limits"_test = [] {
        std::u32string source(10000, U'\U0001f642');
        auto c = folia::completion::capture(source, 5000, {}, {(std::numeric_limits<std::size_t>::max)(), (std::numeric_limits<std::size_t>::max)(), (std::numeric_limits<std::size_t>::max)(), (std::numeric_limits<std::uint32_t>::max)()});
        expect(c.prefix.size() > 3072_u && c.prefix.size() <= 4096_u);
        expect(c.suffix.size() == 1024_u);
        expect(folia::completion::make_prompt(c, {.output_tokens = (std::numeric_limits<std::uint32_t>::max)()}).output_tokens == 256_u);
        expect(c.prefix.substr(0, 4) == std::string("\xf0\x9f\x99\x82"));
        expect(source.size() == 10000_u);
    };
    "completion long paragraph context keeps a stable provider cache prefix while typing"_test = [] {
        std::u32string source(10000, U'x');
        auto before = folia::completion::capture(source, 5000);
        source[5000] = U'y';
        auto after = folia::completion::capture(source, 5001);
        expect(after.prefix.starts_with(before.prefix));
        expect(after.prefix.size() == before.prefix.size() + 1);
        expect(after.prefix.back() == 'y');
        auto first = folia::completion::make_prompt(before), second = folia::completion::make_prompt(after);
        expect(first.instructions == second.instructions); expect(first.context == second.context);
    };
    "completion proposal consumes typed and accepted prefixes without regeneration"_test = [] {
        folia::completion::Proposal proposal;
        proposal.set(9, 2, U" hello 世界");
        expect(proposal.consume(9, 2, U" hello"));
        expect(proposal.at(9, 8));
        expect(proposal.text() == U" 世界");
        expect(!proposal.consume(10, 8, U" "));
        expect(proposal.text().empty());
        proposal.set(9, 2, U"abc");
        expect(!proposal.consume(9, 2, U"abx"));
        expect(proposal.text().empty());
    };
    "completion cache is bounded LRU and clears document-derived text"_test = [] {
        folia::completion::Cache cache(64, 2);
        folia::completion::Context a{{}, "a", {}}, b{{}, "b", {}}, c{{}, "c", {}};
        cache.put(a, U"one"); cache.put(b, U"two");
        expect(cache.get(a).has_value());
        cache.put(c, U"three");
        expect(!cache.get(b));
        expect(cache.get(a).value() == U"one");
        cache.put(c, std::u32string(40, U'x'));
        expect(cache.bytes() <= 64_u);
        expect(cache.size() <= 2_u);
        cache.clear();
        expect(cache.bytes() == 0_u); expect(cache.size() == 0_u);
    };
    "completion output has hard character and line limits"_test = [] {
        auto output = folia::completion::sanitize(std::u32string(9999, U'a'), {});
        expect(output.size() == 512_u);
        expect(folia::completion::sanitize(U"a\nb\nc\nd\ne", {}) == U"a\nb\nc\nd");
    };
};
}

