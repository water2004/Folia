#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.filter_util;
import folia.math.input.tex.base_mappings;

namespace {
struct FilterFixture : Fixture {
    folia::math::core::AbstractMathItem math{u"", nullptr};
    TexFilterData data{math, document, *options};
    MmlNode &make(NodeKind kind, std::initializer_list<MmlNode *> children = {}) {
        return options->request().nodeFactory.makeNode(kind, {children.begin(), children.size()});
    }
    MmlNode &root(std::initializer_list<MmlNode *> children) {
        auto &node = make(NodeKind::math, children);
        options->request().root = &node;
        document.setRoot(node);
        return node;
    }
    MmlNode &scripts(NodeKind kind, MmlNode &base, MmlNode *low, MmlNode *up) {
        auto &node = make(kind);
        NodeUtil::setChild(node, 0, base);
        NodeUtil::setChild(node, 1, low);
        NodeUtil::setChild(node, 2, up);
        return node;
    }
    MmlNode &relation(std::u16string_view text) {
        auto &node = token(NodeKind::mo, text);
        node.setTexClass(TexClass::rel);
        return node;
    }
};
bool boolean(MmlNode &node, std::u16string_view key) {
    auto value = node.attributes().lookup(key);
    return value && std::holds_alternative<bool>(*value) && std::get<bool>(*value);
}
std::vector<MmlNode *> descendants(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
} // namespace

suite filter_util = [] {
    "filter util cleans all four incomplete script families and preserves attributes"_test = [] {
        struct Case {
            NodeKind source, result;
            bool lower;
        };
        const std::array cases{Case{NodeKind::msubsup, NodeKind::msub, true},
                               Case{NodeKind::msubsup, NodeKind::msup, false},
                               Case{NodeKind::munderover, NodeKind::munder, true},
                               Case{NodeKind::munderover, NodeKind::mover, false}};
        for (auto const &test : cases) {
            FilterFixture f;
            auto &base = f.token();
            auto &script = f.token(NodeKind::mi, u"s");
            auto &old = f.scripts(test.source, base, test.lower ? &script : nullptr,
                                  test.lower ? nullptr : &script);
            old.attributes().set(u"data-latex", u"x_s");
            old.attributes().setInherited(u"mathcolor", u"red");
            old.properties().tex.sub_sup_allowed = false;
            old.setTexClass(TexClass::rel);
            auto &atom = f.make(NodeKind::tex_atom, {&old});
            f.root({&atom});
            auto parent = old.parent();
            FilterUtil::cleanSubSup(f.data);
            auto &replacement = *parent->children()[0];
            expect(replacement.kind() == test.result);
            expect(replacement.children().size() == 2_u);
            expect(replacement.children()[0] == &base);
            expect(replacement.children()[1] == &script);
            expect(old.parent() == nullptr);
            expect(replacement.parent() == parent);
            expect(!replacement.texClass());
            expect(replacement.properties().tex.sub_sup_allowed == false);
            expect(attr(replacement, u"data-latex") == u"x_s");
            expect(attr(replacement, u"mathcolor") == u"red");
            auto originalName = folia::math::mml::nodeKindName(test.source);
            auto resultName = folia::math::mml::nodeKindName(test.result);
            expect(f.options->getList(std::u16string(originalName.begin(), originalName.end()))
                       .empty());
            expect(
                f.options->getList(std::u16string(resultName.begin(), resultName.end())).size() ==
                1_u);
        }
    };
    "filter util keeps complete scripts and skips cleanup after parse errors"_test = [] {
        FilterFixture f;
        auto &base = f.token();
        auto &sub = f.token(NodeKind::mi, u"i");
        auto &sup = f.token(NodeKind::mi, u"j");
        auto &complete = f.scripts(NodeKind::msubsup, base, &sub, &sup);
        auto &otherBase = f.token();
        auto &otherSub = f.token();
        auto &incomplete = f.scripts(NodeKind::msubsup, otherBase, &otherSub, nullptr);
        auto &root = f.root({&complete, &incomplete});
        f.options->request().error = true;
        FilterUtil::cleanSubSup(f.data);
        expect(descendants(root, NodeKind::msubsup).size() == 2_u);
        f.options->request().error = false;
        FilterUtil::cleanSubSup(f.data);
        expect(descendants(root, NodeKind::msubsup).size() == 1_u);
        expect(descendants(root, NodeKind::msub).size() == 1_u);
    };
    "filter util inheritance follows actual MathItem display and script levels"_test = [] {
        for (bool display : {false, true}) {
            FilterFixture f;
            f.math.setDisplay(display);
            auto &base = f.token();
            auto &sub = f.token(NodeKind::mi, u"i");
            auto &sup = f.token(NodeKind::mi, u"j");
            auto &scripts = f.scripts(NodeKind::msubsup, base, &sub, &sup);
            auto &root = f.root({&scripts});
            FilterUtil::setInherited(f.data);
            expect(boolean(root, u"displaystyle") == display);
            expect(boolean(base, u"displaystyle") == display);
            expect(!boolean(sub, u"displaystyle"));
            auto level = sub.attributes().lookup(u"scriptlevel");
            expect(level != nullptr);
            if (level)
                expect(std::get<double>(*level) == 1.0);
        }
    };
    "filter util removes only redundant single-child scriptlevel styles"_test = [] {
        FilterFixture f;
        auto &identifier = f.token();
        auto &redundant = f.make(NodeKind::mstyle, {&identifier});
        redundant.attributes().set(u"displaystyle", false);
        redundant.attributes().set(u"scriptlevel", 0.0);
        redundant.attributes().set(u"data-latex", u"\\textstyle x");
        auto &keptIdentifier = f.token();
        auto &kept = f.make(NodeKind::mstyle, {&keptIdentifier});
        kept.attributes().set(u"mathvariant", u"bold");
        auto &one = f.token();
        auto &two = f.token();
        auto &multiple = f.make(NodeKind::mstyle, {&one, &two});
        f.math.setDisplay(false);
        auto &root = f.root({&redundant, &kept, &multiple});
        FilterUtil::setInherited(f.data);
        auto parent = redundant.parent();
        FilterUtil::checkScriptlevel(f.data);
        expect(redundant.parent() == nullptr);
        expect(identifier.parent() == parent);
        expect(attr(identifier, u"data-latex") == u"\\textstyle x");
        expect(kept.parent() != nullptr);
        expect(multiple.parent() != nullptr);
        expect(f.options->getList(u"mstyle").size() == 2_u);
        expect(descendants(root, NodeKind::mstyle).size() == 2_u);
    };
    "filter util moves all limit families only for inline non-explicit movable bases"_test = [] {
        struct Case {
            NodeKind source, result;
        };
        const std::array cases{Case{NodeKind::munderover, NodeKind::msubsup},
                               Case{NodeKind::munder, NodeKind::msub},
                               Case{NodeKind::mover, NodeKind::msup}};
        for (auto const &test : cases)
            for (bool display : {false, true})
                for (bool explicitLimits : {false, true}) {
                    FilterFixture f;
                    auto &base = f.token(NodeKind::mi, u"lim");
                    base.properties().tex.movable_limits = true;
                    if (explicitLimits)
                        base.attributes().set(u"movablelimits", false);
                    auto &a = f.token(), &b = f.token();
                    MmlNode *scripts{};
                    if (test.source == NodeKind::munderover)
                        scripts = &f.scripts(test.source, base, &a, &b);
                    else
                        scripts = &f.make(test.source, {&base, &a});
                    scripts->attributes().set(u"data-latex", u"limits");
                    scripts->properties().tex.move_sup_sub = true;
                    f.math.setDisplay(display);
                    auto &root = f.root({scripts});
                    FilterUtil::setInherited(f.data);
                    auto parent = scripts->parent();
                    FilterUtil::moveLimits(f.data);
                    auto moved = !display && !explicitLimits;
                    expect((parent->children()[0]->kind() == test.result) == moved);
                    expect((scripts->parent() == nullptr) == moved);
                    if (moved) {
                        auto &replacement = *parent->children()[0];
                        expect(replacement.properties().tex.move_sup_sub == true);
                        expect(attr(replacement, u"data-latex") == u"limits");
                        expect(descendants(root, test.source).empty());
                    }
                }
    };
    "filter util cleans dictionary stretchy flags and preserves false markers"_test = [] {
        FilterFixture f;
        auto &stretchy = f.relation(u"→"), &ordinary = f.relation(u"="),
             &falseMarker = f.relation(u"↔");
        stretchy.properties().tex.fix_stretchy = true;
        ordinary.properties().tex.fix_stretchy = true;
        falseMarker.properties().tex.fix_stretchy = false;
        f.options->addNode(u"fixStretchy", stretchy);
        f.options->addNode(u"fixStretchy", ordinary);
        f.options->addNode(u"fixStretchy", falseMarker);
        f.root({&stretchy, &ordinary, &falseMarker});
        FilterUtil::cleanStretchy(f.data);
        expect(!boolean(stretchy, u"stretchy"));
        expect(!stretchy.properties().tex.fix_stretchy);
        expect(!ordinary.properties().tex.fix_stretchy);
        expect(falseMarker.properties().tex.fix_stretchy == false);
        expect(f.options->getList(u"fixStretchy").size() == 3_u);
    };
    "filter util removes inherited duplicate attributes but keeps requested attributes"_test = [] {
        FilterFixture f;
        auto &identifier = f.token();
        identifier.attributes().set(u"mathvariant", u"normal");
        identifier.attributes().setInherited(u"mathvariant", u"normal");
        identifier.attributes().set(u"mathcolor", u"red");
        identifier.attributes().setInherited(u"mathcolor", u"red");
        identifier.attributes().set(u"data-latex-item", u"x");
        identifier.attributes().set(u"data-latex", u"x");
        identifier.properties().tex.keep_attributes = u"mathcolor data-latex-item";
        f.root({&identifier});
        FilterUtil::cleanAttributes(f.data);
        expect(identifier.attributes().own(u"mathvariant") == nullptr);
        expect(attr(identifier, u"mathcolor") == u"red");
        expect(identifier.attributes().own(u"data-latex-item") == nullptr);
        expect(attr(identifier, u"data-latex") == u"x");
    };
    "filter util combines adjacent relations and transfers text metadata properties and list state"_test =
        [] {
            FilterFixture f;
            auto &first = f.relation(u"<"), &second = f.relation(u"="), &third = f.relation(u">");
            first.attributes().set(u"lspace", u"1em");
            first.attributes().set(u"stretchy", false);
            first.attributes().set(u"data-latex", u"<");
            second.attributes().set(u"rspace", u"2em");
            second.attributes().set(u"data-latex", u"=");
            second.properties().tex.math_accent = false;
            second.properties().layout.breakable = true;
            third.attributes().set(u"mathvariant", u"bold");
            auto &row = f.make(NodeKind::mrow, {&first, &second, &third});
            f.root({&row});
            FilterUtil::combineRelations(f.data);
            expect(row.children().size() == 2_u);
            expect(row.children()[0] == &first);
            expect(NodeUtil::getText(first) == u"<=");
            expect(second.parent() == nullptr);
            expect(second.properties().tex.relations_combined == true);
            expect(attr(first, u"data-latex") == u"<=");
            expect(attr(first, u"lspace") == u"1em");
            expect(attr(first, u"rspace") == u"2em");
            expect(attr(third, u"lspace") == u"0pt");
            expect(first.properties().tex.math_accent == false);
            expect(first.properties().layout.breakable == true);
            expect(first.properties().tex.explicit_tex_class == TexClass::rel);
            expect(f.options->getList(u"mo").size() == 2_u);
            expect(first.attributes().inherited(u"form") != nullptr);
        };
    "filter util relation variants explicit spaces and non-row parents prevent combination"_test =
        [] {
            FilterFixture f;
            auto &first = f.relation(u"="), &second = f.relation(u"=");
            first.properties().tex.variant_form = false;
            second.properties().tex.variant_form = true;
            first.attributes().set(u"rspace", u"3pt");
            second.attributes().set(u"lspace", u"4pt");
            auto &row = f.make(NodeKind::mrow, {&first, &second});
            auto &inside = f.relation(u"=");
            auto &script = f.token();
            auto &sup = f.make(NodeKind::msup, {&inside, &script});
            f.root({&row, &sup});
            FilterUtil::combineRelations(f.data);
            expect(row.children().size() == 2_u);
            expect(attr(first, u"rspace") == u"3pt");
            expect(attr(second, u"lspace") == u"4pt");
            expect(inside.attributes().inherited(u"form") == nullptr);
        };
};
