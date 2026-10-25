#include "pch.h"
#include "support/folia_test.hpp"
#include "media/MathJaxRenderer.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    suite math_cache_ownership_tests = []
    {
        "math cache repeated owner membership preserves pending work without IO"_test = []
        {
            MathJaxRenderer math;
            auto batch = math.BeginBatch();
            std::vector<std::string> formulas;
            for (int i = 0; i < 32; ++i) formulas.push_back("x+" + std::to_string(i));
            math.BeginBlock(1);
            for (auto const& formula : formulas) batch.Render(formula, false, 16, 500, "#000000");
            math.EndBlock();

            // Leave native work staged so this measures only the production
            // owner bookkeeping, independently of rendering and file timing.
            std::chrono::steady_clock::duration endBlockTime{};
            for (int repeat = 0; repeat < 512; ++repeat)
            {
                math.BeginBlock(1);
                for (auto const& formula : formulas) math.Lookup(formula, false, 16, 500, "#000000");
                auto start = std::chrono::steady_clock::now();
                math.EndBlock();
                endBlockTime += std::chrono::steady_clock::now() - start;
            }
            auto snapshot = math.Snapshot();
            expect(snapshot.entries == 32_u);
            expect(snapshot.pending == 32_u);
            expect(snapshot.nativeBatches == 0_u);
            expect(snapshot.fileReads == 0_u && snapshot.fileWrites == 0_u);
            expect(snapshot.svgBytes == 0_u);
            std::cout << "math cache EndBlock 512x32 membership microseconds="
                << std::chrono::duration_cast<std::chrono::microseconds>(endBlockTime).count() << '\n';
        };

        "math cache replaced membership releases only the last owner"_test = []
        {
            MathJaxRenderer math;
            auto batch = math.BeginBatch();
            math.BeginBlock(1);
            batch.Render("x", false, 16, 500, "#000000");
            batch.Render("y", false, 16, 500, "#000000");
            math.EndBlock();
            math.BeginBlock(2);
            batch.Render("y", false, 16, 500, "#000000");
            batch.Render("z", false, 16, 500, "#000000");
            math.EndBlock();
            batch.Submit();
            math.WaitForIdle();
            auto y = math.Lookup("y", false, 16, 500, "#000000");
            auto z = math.Lookup("z", false, 16, 500, "#000000");
            expect(y != nullptr && z != nullptr);
            expect(math.Snapshot().entries == 3_u);

            math.BeginBlock(1);
            expect(math.Lookup("y", false, 16, 500, "#000000") == y);
            expect(math.Lookup("z", false, 16, 500, "#000000") == z);
            math.EndBlock();
            expect(math.Lookup("x", false, 16, 500, "#000000") == nullptr);
            expect(math.Snapshot().entries == 2_u);
            math.BeginBlock(2);
            math.EndBlock();
            expect(math.Lookup("y", false, 16, 500, "#000000") == y);
            expect(math.Lookup("z", false, 16, 500, "#000000") == z);

            math.BeginBlock(1);
            math.EndBlock();
            expect(math.Snapshot().entries == 0_u);
            expect(math.Snapshot().pending == 0_u);
            expect(math.Snapshot().fileReads == 0_u);
            for (int repeat = 0; repeat < 64; ++repeat)
            {
                math.BeginBlock(1);
                math.EndBlock();
            }
            expect(math.Snapshot().entries == 0_u);
            expect(math.Snapshot().nativeBatches == 1_u);
        };
    };
}
