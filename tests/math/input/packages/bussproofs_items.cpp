#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.bussproofs.items;
import folia.math.util.utf16;

namespace {
std::shared_ptr<ProofTreeItem> proof(Fixture &f) {
    return std::make_shared<ProofTreeItem>(f.factory());
}
void addProofNode(ProofTreeItem &frame, MmlNode &node) {
    MmlNode *value = &node;
    frame.Push({&value, 1});
}
FrameHandle proofEnd(Fixture &f, std::u16string name = u"prooftree") {
    auto frame = f.item(StackKind::end);
    dynamic_cast<EndItem &>(*frame).name = std::move(name);
    return frame;
}
std::u16string proofText(MmlNode &root) {
    std::u16string result;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            result += NodeUtil::getText(node);
    });
    return result;
}
struct FrameEvents {
    std::size_t created{}, destroyed{}, checks{};
    BaseItem *lastReceiver{};
    BaseItem *lastIncoming{};
};
class TrackedOpen final : public OpenItem {
    FrameEvents &events_;

  public:
    TrackedOpen(StackItemFactory &factory, FrameEvents &events)
        : OpenItem(factory), events_(events) {
        ++events_.created;
    }
    ~TrackedOpen() override {
        ++events_.destroyed;
    }
    CheckResult checkItem(FrameHandle const &incoming) override {
        ++events_.checks;
        events_.lastReceiver = this;
        events_.lastIncoming = incoming.get();
        return OpenItem::checkItem(incoming);
    }
};
struct NodeRequest {
    NodeKind kind;
    std::vector<MmlNode *> children;
    MmlNode *result;
};
thread_local std::vector<NodeRequest> *proofRequests{};
MmlNode &proofCreator(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                      NodeDefinition definition, MmlNode *text) {
    auto pointers = std::vector<MmlNode *>(children.begin(), children.end());
    auto &result = NodeFactory::createNode(factory, kind, children, std::move(definition), text);
    if (proofRequests)
        proofRequests->push_back({kind, std::move(pointers), &result});
    return result;
}
struct ProofFrameTree {
    std::u16string_view name, snapshot;
};
struct ProofFrameError {
    std::u16string_view name;
    std::string_view id;
    std::u16string_view message;
};
#include "math/fixtures/bussproofs_items.inc"
std::u16string proofSnapshot(MmlNode const &root) {
    std::u16string result;
    auto add = [&](std::u16string_view value) {
        auto count = std::to_string(value.size());
        result.append(count.begin(), count.end());
        result += u":";
        result += value;
    };
    auto visit = [&](auto &&self, MmlNode const &node) -> void {
        auto kind = nodeKindName(node.kind());
        add(std::u16string(kind.begin(), kind.end()));
        add(node.isToken() ? NodeUtil::getText(node) : u"");
        expect(node.attributes().ownEntries().empty());
        auto marker = node.properties().proof.proof();
        add(marker ? (*marker ? u"true" : u"false") : u"");
        auto count = std::to_string(node.isToken() ? 0 : node.children().size());
        add(std::u16string(count.begin(), count.end()));
        if (!node.isToken())
            for (auto *child : node.children()) {
                expect(child->parent() == &node);
                self(self, *child);
            }
    };
    visit(visit, root);
    return result;
}
MmlNode &sourceTree(Fixture &f, std::u16string_view name) {
    auto frame = proof(f);
    frame->name = u"prooftree";
    if (name == u"outer-replay") {
        Stack outer(f.factory(), {}, false);
        auto begin = f.item(StackKind::begin);
        dynamic_cast<BeginItem &>(*begin).name = u"prooftree";
        outer.Push(begin);
        outer.Push(frame);
        outer.Push(f.item(StackKind::open));
        outer.Push(&f.token(NodeKind::mi, u"p"));
        outer.Push(f.item(StackKind::close));
        addProofNode(*frame, f.token(NodeKind::mi, u"x"));
        outer.Push(proofEnd(f));
        expect(outer.height() == 1_u);
        outer.Push(f.item(StackKind::stop));
        expect(outer.Top()->kind() == StackKind::mml);
        return *outer.Top()->First();
    }
    if (name == u"empty")
        return frame->toMml();
    if (name == u"grouped-prefix")
        frame->checkItem(f.item(StackKind::open));
    if (name == u"prefix" || name == u"grouped-prefix" || name == u"prefix-empty-proof" ||
        name == u"matching-end-prefix")
        expect(frame->checkItem(f.mml(f.token(NodeKind::mi, u"p"))).action == CheckAction::drop);
    if (name == u"grouped-prefix")
        frame->checkItem(f.item(StackKind::close));
    if (name != u"prefix-empty-proof")
        addProofNode(*frame, f.token(NodeKind::mi, u"x"));
    if (name == u"multiple" || name == u"ignored-flags")
        addProofNode(*frame, f.token(NodeKind::mi, u"y"));
    if (name == u"ignored-flags" || name == u"ignored-force-single")
        return frame->toMml(false, true);
    if (name == u"matching-end" || name == u"matching-end-prefix") {
        auto result = frame->checkItem(proofEnd(f));
        expect(result.action == CheckAction::replay);
        expect(std::holds_alternative<ReuseIncoming>(result.replacements.at(1)));
        return replayNode(result);
    }
    return frame->toMml();
}
void sourceError(Fixture &f, std::u16string_view name) {
    auto frame = proof(f);
    if (name != u"stop-unnamed")
        frame->name = u"prooftree";
    if (name == u"stop" || name == u"stop-unnamed") {
        frame->checkItem(f.item(StackKind::stop));
    } else if (name == u"mismatched-end") {
        frame->checkItem(proofEnd(f, u"other"));
    } else if (name == u"extra-close") {
        frame->checkItem(f.item(StackKind::close));
    } else {
        frame->checkItem(f.item(StackKind::open));
        if (name == u"unfinished-group")
            frame->toMml();
        else
            frame->checkItem(proofEnd(f));
    }
}
} // namespace

suite bussproofs_items = [] {
    "bussproofs proof frame empty name and inherited flags retain original behavior"_test = [] {
        for (auto const &entry : proofFrameTrees) {
            Fixture fixture;
            auto actual = proofSnapshot(sourceTree(fixture, entry.name));
            auto equal = std::ranges::equal(actual, entry.snapshot);
            if (!equal) {
                auto mismatch = std::ranges::mismatch(actual, entry.snapshot);
                auto offset = static_cast<std::size_t>(mismatch.in1 - actual.begin());
                expect(false) << "Bussproofs frame " << folia::math::utf16::encode(entry.name)
                              << " Source difference at " << offset << " actual "
                              << folia::math::utf16::encode(actual) << " expected "
                              << folia::math::utf16::encode(entry.snapshot);
            } else
                expect(true);
        }
        Fixture f;
        auto frame = proof(f);
        expect(frame->kind() == StackKind::proof_tree);
        expect(!frame->isOpen());
        expect(!frame->isClose());
        expect(!frame->isFinal());
        expect(!frame->getName().has_value());
        frame->name = u"prooftree";
        expect(std::ranges::equal(frame->getName().value(), std::u16string_view(u"prooftree")));
        frame->name = u"";
        expect(frame->getName().has_value());
        expect(frame->getName()->empty());
        auto &empty = frame->toMml();
        expect(empty.kind() == NodeKind::inferred_mrow);
        expect(empty.children().empty());
        expect(!empty.properties().proof.proof().has_value());
        expect(&empty.document() == &f.document);
    };
    "bussproofs direct proof nodes stay borrowed and toMml ignores both inherited flags"_test = [] {
        for (bool inferred : {false, true})
            for (bool forceRow : {false, true}) {
                Fixture f;
                auto frame = proof(f);
                auto &one = f.token(NodeKind::mi, u"A");
                addProofNode(*frame, one);
                auto &single = frame->toMml(inferred, forceRow);
                expect(&single == &one);
                expect(frame->Size() == 1_u);
                auto &two = f.token(NodeKind::mi, u"B");
                addProofNode(*frame, two);
                auto &multiple = frame->toMml(inferred, forceRow);
                expect(multiple.kind() == NodeKind::inferred_mrow);
                expect(multiple.children().size() == 2_u);
                expect(multiple.children()[0] == &one);
                expect(multiple.children()[1] == &two);
                expect(one.parent() == &multiple);
                expect(two.parent() == &multiple);
                expect(std::ranges::equal(proofText(multiple), std::u16string_view(u"AB")));
            }
    };
    "bussproofs ordinary content is an inner prefix before direct proof material"_test = [] {
        Fixture f;
        auto frame = proof(f);
        auto &prefix = f.token(NodeKind::mi, u"P");
        auto &tree = f.token(NodeKind::mi, u"T");
        auto incoming = f.mml(prefix);
        auto result = frame->checkItem(incoming);
        expect(result.action == CheckAction::drop);
        expect(frame->Size() == 0_u);
        addProofNode(*frame, tree);
        auto &combined = frame->toMml(false, true);
        expect(combined.kind() == NodeKind::mrow);
        expect(combined.children().size() == 2_u);
        expect(combined.children()[0] == &prefix);
        expect(combined.children()[1] == &tree);
        expect(prefix.parent() == &combined);
        expect(tree.parent() == &combined);
        expect(std::ranges::equal(proofText(combined), std::u16string_view(u"PT")));
    };
    "bussproofs grouped prefix closes through the actual inner stack before combination"_test = [] {
        Fixture f;
        auto frame = proof(f);
        expect(frame->checkItem(f.item(StackKind::open)).action == CheckAction::drop);
        expect(frame->checkItem(f.mml(f.token(NodeKind::mi, u"G"))).action == CheckAction::drop);
        expect(frame->checkItem(f.item(StackKind::close)).action == CheckAction::drop);
        addProofNode(*frame, f.token(NodeKind::mi, u"T"));
        auto &combined = frame->toMml();
        expect(combined.kind() == NodeKind::mrow);
        expect(combined.children()[0]->kind() == NodeKind::tex_atom);
        expect(combined.children()[0]->parent() == &combined);
        expect(std::ranges::equal(proofText(combined), std::u16string_view(u"GT")));
    };
    "bussproofs matching end marks proof and replays MmlItem before the incoming end"_test = [] {
        Fixture f;
        auto frame = proof(f);
        frame->name = u"custom"; // Closing dispatch checks the incoming literal prooftree name.
        auto &node = f.token(NodeKind::mi, u"A");
        addProofNode(*frame, node);
        auto incoming = proofEnd(f);
        auto result = frame->checkItem(incoming);
        expect(result.action == CheckAction::replay);
        expect(result.replacements.size() == 2_u);
        auto replay = std::get<FrameHandle>(result.replacements[0]);
        expect(replay->kind() == StackKind::mml);
        expect(replay->First() == &node);
        expect(std::holds_alternative<ReuseIncoming>(result.replacements[1]));
        expect(node.properties().proof.proof().value_or(false));
        expect(incoming->kind() == StackKind::end);
    };
    "bussproofs stop mismatched end and unfinished group retain original TeX errors"_test = [] {
        for (auto const &entry : proofFrameErrors) {
            Fixture fixture;
            try {
                sourceError(fixture, entry.name);
                expect(false) << "Expected Source proof-frame error";
            } catch (TexError const &error) {
                expect(error.id() == entry.id);
                expect(std::ranges::equal(error.message(), entry.message))
                    << folia::math::utf16::encode(entry.name);
            }
        }
        Fixture f;
        auto frame = proof(f);
        try {
            frame->checkItem(f.item(StackKind::stop));
            expect(false) << "Expected EnvMissingEnd";
        } catch (TexError const &error) {
            expect(error.id() == "EnvMissingEnd");
            expect(std::ranges::equal(error.message(), std::u16string_view(u"Missing \\end{}")));
        }
        frame->name = u"prooftree";
        texError([&] { frame->checkItem(f.item(StackKind::stop)); }, "EnvMissingEnd");
        texError([&] { frame->checkItem(proofEnd(f, u"other")); }, "MissingBeginExtraEnd");
        frame->checkItem(f.item(StackKind::open));
        texError([&] { frame->toMml(); }, "ExtraOpenMissingClose");
        texError([&] { frame->checkItem(proofEnd(f)); }, "ExtraOpenMissingClose");
    };
    "bussproofs forwards the same frame into inner ownership after outer Drop"_test = [] {
        Fixture f;
        FrameEvents events;
        std::weak_ptr<TrackedOpen> retained;
        auto proofFrame = proof(f);
        {
            Stack outer(f.factory(), {}, true);
            outer.Push(proofFrame);
            auto group = std::make_shared<TrackedOpen>(f.factory(), events);
            retained = group;
            auto *identity = group.get();
            outer.Push(group);
            expect(outer.height() == 2_u);
            expect(&group->global() != &outer.global());
            expect(group->global().is_inner);
            group.reset();
            expect(!retained.expired());
            auto incoming = f.mml(f.token(NodeKind::mi, u"P"));
            outer.Push(incoming);
            expect(events.lastReceiver == identity);
            expect(events.lastIncoming == incoming.get());
            expect(events.checks == 1_u);
            expect(outer.height() == 2_u);
            outer.Push(f.item(StackKind::close));
            expect(retained.expired());
            expect(events.destroyed == 1_u);
        }
        proofFrame.reset();
        expect(events.created == events.destroyed);
    };
    "bussproofs real outer begin proof end stop replay releases proof activation"_test = [] {
        Fixture f;
        Stack outer(f.factory(), {}, true);
        auto begin = f.item(StackKind::begin);
        dynamic_cast<BeginItem &>(*begin).name = u"prooftree";
        outer.Push(begin);
        auto frame = proof(f);
        frame->name = u"prooftree";
        auto &node = f.token(NodeKind::mi, u"A");
        addProofNode(*frame, node);
        std::weak_ptr<ProofTreeItem> lifetime = frame;
        outer.Push(frame);
        expect(outer.height() == 3_u);
        frame.reset();
        expect(!lifetime.expired());
        outer.Push(proofEnd(f));
        expect(lifetime.expired());
        expect(outer.height() == 1_u);
        expect(outer.Top()->First() == &node);
        outer.Push(f.item(StackKind::stop));
        expect(outer.Top()->kind() == StackKind::mml);
        expect(&outer.Top()->toMml() == &node);
        expect(node.properties().proof.proof().value_or(false));
    };
    "bussproofs prefix creator receives original document children without cloning"_test = [] {
        Fixture f;
        auto frame = proof(f);
        auto &prefix = f.token(NodeKind::mi, u"P");
        auto &tree = f.token(NodeKind::mi, u"T");
        frame->checkItem(f.mml(prefix));
        addProofNode(*frame, tree);
        std::vector<NodeRequest> requests;
        struct Restore {
            NodeFactory &factory;
            NodeCreators creators;
            std::vector<NodeRequest> *previous{proofRequests};
            ~Restore() {
                proofRequests = previous;
                factory.setCreators(creators);
            }
        } restore{f.options->request().nodeFactory, f.options->request().nodeFactory.creators()};
        proofRequests = &requests;
        f.options->request().nodeFactory.setCreators(NodeCreators{.node = &proofCreator});
        auto &combined = frame->toMml();
        auto found = std::find_if(requests.begin(), requests.end(),
                                  [&](auto const &request) { return request.result == &combined; });
        expect(found != requests.end());
        if (found != requests.end()) {
            expect(found->kind == NodeKind::mrow);
            expect(found->children.size() == 2_u);
            expect(found->children[0] == &prefix);
            expect(found->children[1] == &tree);
        }
        expect(&combined.document() == &f.document);
    };
    "bussproofs mixed completed and unfinished inner groups release all frame owners"_test = [] {
        Fixture f;
        FrameEvents events;
        for (int round = 0; round < 12; ++round) {
            std::weak_ptr<TrackedOpen> groupLife;
            std::weak_ptr<ProofTreeItem> proofLife;
            {
                Stack outer(f.factory(), {}, true);
                auto begin = f.item(StackKind::begin);
                dynamic_cast<BeginItem &>(*begin).name = u"prooftree";
                outer.Push(begin);
                auto frame = proof(f);
                proofLife = frame;
                outer.Push(frame);
                auto group = std::make_shared<TrackedOpen>(f.factory(), events);
                groupLife = group;
                outer.Push(group);
                group.reset();
                outer.Push(&f.token(NodeKind::mi, u"P"));
                addProofNode(*frame, f.token(NodeKind::mi, u"T"));
                frame.reset();
                if (round % 3 == 0) {
                    outer.Push(f.item(StackKind::close));
                    outer.Push(proofEnd(f));
                    outer.Push(f.item(StackKind::stop));
                    expect(outer.Top()->kind() == StackKind::mml);
                } else if (round % 3 == 1) {
                    texError([&] { outer.Push(proofEnd(f)); }, "ExtraOpenMissingClose");
                } else {
                    texError([&] { outer.Push(f.item(StackKind::stop)); }, "EnvMissingEnd");
                }
            }
            expect(groupLife.expired());
            expect(proofLife.expired());
            expect(events.created == events.destroyed);
        }
        expect(events.created == 12_u);
    };
};
