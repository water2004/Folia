#include "support/folia_test.hpp"
import folia.math.input.tex.tex_scanner;
import folia.math.input.tex.tex_error;

using namespace boost::ut;
using folia::math::tex::TexScanner;
using folia::math::tex::TexError;

suite native_tex_scanner_tests = [] {
    "immutable source snapshots outlive replacements and scanner while UTF16 cursor rules remain unchanged"_test = [] {
        std::shared_ptr<std::u16string const> snapshot;
        std::weak_ptr<std::u16string const> retired;
        {
            TexScanner scan(u"𝑥e\u0301");
            snapshot = scan.sourceSnapshot();
            retired = snapshot;
            expect(*snapshot == u"𝑥e\u0301");
            expect(scan.source() == u"𝑥e\u0301");
            scan.setPosition(2);
            expect(scan.getCodePoint() == u"e");
            scan.replaceSource(u"new");
            expect(scan.source() == u"new");
            expect(scan.position() == 2u);
            expect(*snapshot == u"𝑥e\u0301");
            scan.replaceSource(u"final");
            expect(scan.source() == u"final");
            expect(*snapshot == u"𝑥e\u0301");
        }
        expect(!retired.expired());
        expect(*snapshot == u"𝑥e\u0301");
        snapshot.reset();
        expect(retired.expired());
    };
    "code point and get next retain source UTF16 cursor rules"_test = [] {
        TexScanner scan(u"𝑥 y");
        expect(scan.getCodePoint() == u"𝑥");
        scan.setPosition(2);
        expect(scan.nextIsSpace());
        expect(scan.getNext() == u"y");
        expect(scan.position() == 3u);
        expect(scan.getCodePoint() == u"y");
        TexScanner end;
        expect(end.getCodePoint().empty());
        expect(!end.nextIsSpace());
        expect(end.getNext().empty());
        expect(end.position() == 0u);
        TexScanner loneSurrogate(std::u16string(1, char16_t(0xD800)));
        expect(loneSurrogate.getCodePoint().compare(std::u16string(1, char16_t(0xD800))) == 0);
        TexScanner replace(u"abc");
        replace.setPosition(2);
        replace.replaceSource(u"xy");
        expect(replace.source() == u"xy");
        expect(replace.position() == 2u);
    };
    "ECMAScript whitespace is skipped by code unit"_test = [] {
        TexScanner ascii(u" \t\nX");
        expect(ascii.nextIsSpace());
        expect(ascii.getNext() == u"X");
        expect(ascii.position() == 3u);
        TexScanner nbsp(u"\u00a0x");
        expect(nbsp.nextIsSpace());
        expect(nbsp.getNext() == u"x");
        expect(nbsp.position() == 1u);
        TexScanner bom(u"\ufeffx");
        expect(bom.nextIsSpace());
        expect(bom.getNext() == u"x");
        TexScanner astral(u"😀x");
        expect(!astral.nextIsSpace());
        expect(astral.getNext() == u"😀");
        expect(astral.position() == 0u);
    };
    "control sequence matching preserves optional ASCII-space consumption"_test = [] {
        TexScanner name(u"alpha X");
        expect(name.getCS() == u"alpha");
        expect(name.position() == 6u);
        TexScanner astral(u"😀X");
        expect(astral.getCS() == u"😀");
        expect(astral.position() == 2u);
        TexScanner empty;
        expect(empty.getCS() == u" ");
        expect(empty.position() == 1u);
        TexScanner line(u"\u2028x");
        expect(line.getCS() == u" ");
        expect(line.position() == 1u);
    };
    "argument scanner covers characters controls nesting escapes and unicode"_test = [] {
        TexScanner one(u"xR");
        expect(one.getArgument() == std::optional<std::u16string>(u"x"));
        expect(one.position() == 1u);
        TexScanner control(u"\\alpha tail");
        expect(control.getArgument() == std::optional<std::u16string>(u"\\alpha"));
        expect(control.position() == 7u);
        TexScanner group(u"{a{b}c}z");
        expect(group.getArgument() == std::optional<std::u16string>(u"a{b}c"));
        expect(group.position() == 7u);
        TexScanner escaped(u"{a\\}b}");
        expect(escaped.getArgument() == std::optional<std::u16string>(u"a\\}b"));
        TexScanner unicode(u"😀x");
        expect(unicode.getArgument() == std::optional<std::u16string>(u"😀"));
        expect(unicode.position() == 2u);
        TexScanner optional(u"   ");
        expect(!optional.getArgument({}, true).has_value());
        expect(optional.position() == 3u);
        TexScanner optionalClose(u"}");
        expect(!optionalClose.getArgument({}, true).has_value());
        expect(optionalClose.position() == 0u);
    };
    "required argument failures retain Source ids messages and cursor"_test = [] {
        TexScanner missing;
        missing.setCurrentControlSequence(u"\\probe");
        try { (void)missing.getArgument(); expect(false); }
        catch (const TexError& error) {
            expect(error.id() == "MissingArgFor");
            expect(error.message() == u"Missing argument for \\probe");
            expect(missing.position() == 0u);
        }
        TexScanner extra(u"}");
        try { (void)extra.getArgument(); expect(false); }
        catch (const TexError& error) {
            expect(error.id() == "ExtraCloseMissingOpen");
            expect(error.message() == u"Extra close brace or missing open brace");
            expect(extra.position() == 0u);
        }
        TexScanner unclosed(u"{open");
        try { (void)unclosed.getArgument(); expect(false); }
        catch (const TexError& error) {
            expect(error.id() == "MissingCloseBrace");
            expect(error.message() == u"Missing close brace");
            expect(unclosed.position() == 5u);
        }
    };
    "optional brackets match nested braces escapes and defaults"_test = [] {
        TexScanner nested(u"[a[b]c]z");
        expect(nested.getBrackets({}, u"DEFAULT", true) == std::optional<std::u16string>(u"a[b]c"));
        expect(nested.position() == 7u);
        TexScanner first(u"[a[b]c]z");
        expect(first.getBrackets({}, u"DEFAULT", false) == std::optional<std::u16string>(u"a[b"));
        expect(first.position() == 5u);
        TexScanner brace(u"[{]}]z");
        expect(brace.getBrackets({}, u"DEFAULT", true) == std::optional<std::u16string>(u"{]}"));
        TexScanner escaped(u"[a\\]b]");
        expect(escaped.getBrackets({}, u"DEFAULT", true) == std::optional<std::u16string>(u"a\\]b"));
        TexScanner absent(u"x");
        expect(absent.getBrackets({}, u"DEFAULT") == std::optional<std::u16string>(u"DEFAULT"));
        expect(absent.position() == 0u);
    };
    "bracket failures preserve error details and source cursor"_test = [] {
        TexScanner unclosed(u"[open");
        unclosed.setCurrentControlSequence(u"\\probe");
        try { (void)unclosed.getBrackets(); expect(false); }
        catch (const TexError& error) {
            expect(error.id() == "MissingCloseBracket");
            expect(error.message() == u"Could not find closing ']' for argument to \\probe");
            expect(unclosed.position() == 5u);
        }
        TexScanner extra(u"[}]");
        try { (void)extra.getBrackets(); expect(false); }
        catch (const TexError& error) {
            expect(error.id() == "ExtraCloseLooking");
            expect(error.message() == u"Extra close brace while looking for ']'");
            expect(extra.position() == 2u);
        }
    };
    "get up to respects nesting escaped tokens whitespace and typed failures"_test = [] {
        TexScanner success(u"no ;");
        expect(success.getUpTo({}, u";") == u"no");
        expect(success.position() == 4u);
        TexScanner nested(u"a{b;c};rest");
        expect(nested.getUpTo({}, u";") == u"a{b;c}");
        expect(nested.position() == 7u);
        TexScanner extra(u"a}token");
        try { (void)extra.getUpTo({}, u";"); expect(false); }
        catch (const TexError& error) {
            expect(error.id() == "ExtraCloseLooking");
            expect(error.message() == u"Extra close brace while looking for ;");
            expect(extra.position() == 2u);
        }
        TexScanner missing(u"no token");
        missing.setCurrentControlSequence(u"\\probe");
        try { (void)missing.getUpTo({}, u";"); expect(false); }
        catch (const TexError& error) {
            expect(error.id() == "TokenNotFoundForCommand");
            expect(error.message() == u"Could not find ; for \\probe");
            expect(missing.position() == 8u);
        }
        TexScanner escaped(u"a\\; rest");
        try { (void)escaped.getUpTo({}, u";"); expect(false); }
        catch (const TexError& error) {
            expect(error.id() == "TokenNotFoundForCommand");
            expect(escaped.position() == 8u);
        }
    };
    "star scanner consumes only a literal star after whitespace"_test = [] {
        TexScanner yes(u"*x");
        expect(yes.getStar());
        expect(yes.position() == 1u);
        TexScanner no(u" x");
        expect(!no.getStar());
        expect(no.position() == 1u);
        TexScanner end;
        expect(!end.getStar());
        expect(end.position() == 0u);
    };
};
