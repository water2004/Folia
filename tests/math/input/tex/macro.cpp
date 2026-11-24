#include "support/folia_test.hpp"
import folia.math.input.tex.parser;

using namespace boost::ut;
using namespace folia::math::tex;
namespace {
DispatchOutcome macroA(TexParser&, MacroInvocation const&) { return Handled{}; }
DispatchOutcome macroB(TexParser&, MacroInvocation const&) { return ContinueDispatch{}; }
DispatchOutcome macroC(TexParser&, MacroInvocation const&) { return NoParseResult{}; }
DispatchOutcome tokenA(TexParser&, TokenInput const&) { return Handled{}; }
DispatchOutcome tokenB(TexParser&, TokenInput const&) { return NoParseResult{}; }
}

suite macro = [] {
    "macro callbacks bind stable typed indices without retaining caller arrays"_test = [] {
        std::array<MacroHandler, 2> macros{macroA, macroB};
        std::array<TokenInputHandler, 1> tokens{tokenA};
        MacroCallbacks callbacks(macros, tokens);
        macros[0] = macroC;
        tokens[0] = tokenB;
        expect(callbacks.macroBinding(macroA) == MacroHandlerId{1});
        expect(callbacks.macroBinding(macroB) == MacroHandlerId{2});
        expect(callbacks.macroHandler(MacroHandlerId{1}) == macroA);
        expect(callbacks.macroHandler(MacroHandlerId{2}) == macroB);
        expect(callbacks.tokenBinding(tokenA) == TokenParserId{1});
        expect(callbacks.tokenParser(TokenParserId{1}) == tokenA);
        expect(callbacks.macroBinding(macroC) == MacroHandlerId{});
        expect(callbacks.tokenBinding(tokenB) == TokenParserId{});
        expect(callbacks.macroBinding(nullptr) == MacroHandlerId{});
        expect(throws<std::out_of_range>([&] { callbacks.macroHandler(MacroHandlerId{}); }));
        expect(throws<std::out_of_range>([&] { callbacks.tokenParser(TokenParserId{2}); }));
    };

    "macro callbacks reject absent compiled functions and permit an empty profile table"_test = [] {
        std::array<MacroHandler, 1> missing{nullptr};
        expect(throws<std::invalid_argument>([&] { MacroCallbacks callbacks(missing, {}); }));
        MacroCallbacks empty({}, {});
        expect(empty.macroBinding(macroA) == MacroHandlerId{});
        expect(throws<std::out_of_range>([&] { empty.macroHandler(MacroHandlerId{1}); }));
    };
};
