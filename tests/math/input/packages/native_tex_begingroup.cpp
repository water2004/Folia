#include "support/folia_test.hpp"
import folia.math.input.tex.begingroup.stack;

using namespace boost::ut;
using namespace folia::math::tex;

suite native_tex_begingroup = [] {
    "native definition groups restore shadowed commands delimiters and environments"_test = [] {
        TexSessionState session;
        BegingroupStack groups(session.definitions());
        session.defineMacro(u"foo", {u"outer", {}, {}, 0, {}});
        session.defineDelimiter(u"\\foo", Token(u"\\foo", u"("));
        session.defineEnvironment(u"foo", {u"a", u"b", 0, {}});
        groups.push();
        expect(groups.index() == -101);
        session.defineMacro(u"foo", {u"inner", {}, {}, 0, {}});
        session.defineDelimiter(u"\\foo", Token(u"\\foo", u"["));
        session.defineEnvironment(u"foo", {u"c", u"d", 0, {}});
        expect(session.findMacro(u"foo")->replacement_source == u"inner");
        expect(session.findDelimiter(u"\\foo")->character == u"[");
        expect(session.findEnvironment(u"foo")->end_source == u"d");
        groups.pop();
        expect(groups.index() == -100);
        expect(session.findMacro(u"foo")->replacement_source == u"outer");
        expect(session.findDelimiter(u"\\foo")->character == u"(");
        expect(session.findEnvironment(u"foo")->end_source == u"b");
        expect(session.definitions().groups.empty());
        bool missing{};
        try { groups.pop(); }
        catch (TexError const& error) { missing = error.id() == "MissingBegingroup"; }
        expect(missing);
    };

    "native global selection clears shadows and the terminal target in all three domains"_test = [] {
        TexSessionState session;
        BegingroupStack groups(session.definitions());
        auto define = [&] {
            session.defineMacro(u"foo", {u"x", {}, {}, 0, {}});
            session.defineDelimiter(u"\\foo", Token(u"\\foo", u"("));
            session.defineEnvironment(u"foo", {u"a", u"b", 0, {}});
            session.defineMacro(u"unrelated", {u"kept", {}, {}, 0, {}});
        };
        define(); groups.push(); define(); groups.push(); define();
        const std::array keys{DefinitionKey{DefinitionKind::command, u"foo"},
            DefinitionKey{DefinitionKind::delimiter, u"\\foo"},
            DefinitionKey{DefinitionKind::environment, u"foo"}};
        auto& target = groups.checkGlobal(keys);
        expect(&target == &session.definitions().global);
        expect(session.findCommand(u"foo") == nullptr);
        expect(session.findDelimiter(u"\\foo") == nullptr);
        expect(session.findEnvironment(u"foo") == nullptr);
        expect(session.findMacro(u"unrelated")->replacement_source == u"kept");
        target.commands.insert_or_assign(u"foo", CommandDefinition{
            u"foo", MacroDefinition{u"global", {}, {}, 0, {}}});
        groups.pop(); groups.pop();
        expect(session.findMacro(u"foo")->replacement_source == u"global");
    };

    "native definition checkpoints persist across service lifetimes and remove only new groups"_test = [] {
        TexSessionState session;
        {
            BegingroupStack first(session.definitions());
            first.push();
            session.defineMacro(u"kept", {u"x", {}, {}, 0, {}});
            first.finish();
            first.push();
            session.defineMacro(u"temporary", {u"y", {}, {}, 0, {}});
        }
        BegingroupStack next(session.definitions());
        expect(next.index() == -102);
        expect(next.top() == -101);
        next.remove();
        expect(next.index() == -101);
        expect(session.findMacro(u"kept") != nullptr);
        expect(session.findMacro(u"temporary") == nullptr);
        next.reset();
        expect(next.index() == -100);
        expect(next.top() == -100);
        expect(session.findMacro(u"kept") == nullptr);
    };

    "native sandbox global writes stop at protected base and later sandbox reveals original globals"_test = [] {
        TexSessionState session;
        BegingroupStack groups(session.definitions());
        session.defineMacro(u"foo", {u"original", {}, {}, 0, {}});
        groups.sandbox();
        expect(groups.base() == -101);
        expect(groups.index() == -101);
        expect(session.findMacro(u"foo")->replacement_source == u"original");
        groups.push();
        session.defineMacro(u"foo", {u"local", {}, {}, 0, {}});
        const std::array keys{DefinitionKey{DefinitionKind::command, u"foo"}};
        auto& target = groups.checkGlobal(keys);
        expect(target.priority == -101);
        expect(session.findMacro(u"foo")->replacement_source == u"original");
        target.commands.insert_or_assign(u"foo", CommandDefinition{
            u"foo", MacroDefinition{u"sandbox", {}, {}, 0, {}}});
        groups.pop();
        expect(session.findMacro(u"foo")->replacement_source == u"sandbox");
        expect(throws<TexError>([&] { groups.pop(); }));
        groups.finish();
        groups.sandbox();
        expect(groups.base() == -101);
        expect(session.findMacro(u"foo")->replacement_source == u"original");
        expect(session.definitions().groups.size() == 1_u);
    };

    "native scope release removes aliases and keeps copied sessions independent"_test = [] {
        TexSessionState session;
        BegingroupStack groups(session.definitions());
        groups.push();
        session.defineCommand(u"alias", {u"alias", CharacterMacroAlias{
            TokenParserId{2}, Token(u"alpha", u"α")}});
        auto copy = session;
        groups.pop();
        expect(session.findCommand(u"alias") == nullptr);
        expect(copy.findCommand(u"alias") != nullptr);
        BegingroupStack copiedGroups(copy.definitions());
        copiedGroups.finish();
        copiedGroups.remove();
        expect(copy.findCommand(u"alias") != nullptr);
        copiedGroups.reset();
        expect(copy.findCommand(u"alias") == nullptr);
    };
};
