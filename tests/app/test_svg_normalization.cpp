#include "pch.h"
#include "support/folia_test.hpp"
#include "media/SvgNormalizer.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    suite svg_normalization_tests = []
    {
        "math SVG normalization batches preserve bytes and isolate invalid or canceled inputs"_test = []
        {
            SvgNormalizer normalizer;
            const std::array<std::string_view, 3> sources{
                R"(<svg xmlns="http://www.w3.org/2000/svg" width="200" height="40"><text y="25">质量 α Ж</text></svg>)",
                R"(<svg xmlns="http://www.w3.org/2000/svg" width="200" height="40"><text y="25" font-weight="bold">质量 中文</text></svg>)",
                R"(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="20"><path d="M0 0L10 10"/></svg>)"};
            std::vector<NormalizedSvg> expected;
            for (auto source : sources) expected.push_back(normalizer.NormalizeNow(source, 16));
            std::stop_source stop;
            stop.request_stop();
            std::vector<SvgNormalizationInput> inputs;
            for (auto source : sources) inputs.push_back({source, 16, {}});
            inputs.push_back({"invalid SVG", 16, {}});
            inputs.push_back({sources[0], 16, stop.get_token()});
            inputs.push_back({sources[0], 16, {}});
            for (int repeat = 0; repeat < 2; ++repeat)
            {
                auto results = normalizer.NormalizeBatchNow(inputs);
                expect(results.size() == inputs.size());
                for (std::size_t i = 0; i < sources.size(); ++i)
                {
                    expect(static_cast<bool>(expected[i]) && static_cast<bool>(results[i]));
                    if (expected[i] && results[i]) expect(*expected[i].svg == *results[i].svg);
                    expect(expected[i].width == results[i].width && expected[i].height == results[i].height);
                }
                expect(!static_cast<bool>(results[3]) && !results[3].error.empty());
                expect(!static_cast<bool>(results[4]) && !results[4].error.empty());
                expect(static_cast<bool>(results[5]));
                if (results[5] && expected[0]) expect(*results[5].svg == *expected[0].svg);
            }
            expect(normalizer.NormalizeBatchNow({}).empty());
        };
    };
}
