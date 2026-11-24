#include "support/folia_test.hpp"
import folia.math.input.tex.node_factory;

using namespace boost::ut;
using folia::math::tex::NodeFactory;
using folia::math::tex::NodeKindRegistration;
using folia::math::mml::MmlDocument;
using folia::math::mml::MmlNode;
using folia::math::mml::NodeKind;

namespace {
struct RegistrationEvent {
    NodeKind kind;
    MmlNode* node;
    bool fullyInitialized;
};
struct RecordingRegistration final : NodeKindRegistration {
    std::vector<RegistrationEvent> events;
    bool throwOnMerror = false;
    void addNode(NodeKind kind, MmlNode& node) override {
        bool ready = node.kind() == kind;
        for (auto* child : node.children()) ready = ready && child->parent() == &node;
        if (kind == NodeKind::merror)
            ready = ready && node.attributes().own(u"data-mjx-error") != nullptr;
        events.push_back({kind, &node, ready});
        if (throwOnMerror && kind == NodeKind::merror)
            throw std::runtime_error("registration observer failure");
    }
};
}

suite node_factory_registration = [] {
    "successful Source node creation routes notify once in nested order"_test = [] {
        MmlDocument document;
        RecordingRegistration registrations;
        NodeFactory factory(document.factory(), &registrations);

        auto& textOnly = *factory.makeText(u"unregistered text");
        expect(&textOnly.document() == &document);
        expect(registrations.events.empty());

        auto& token = factory.makeToken(NodeKind::mi, u"𝑥");
        expect(token.kind() == NodeKind::mi);
        expect(&token.document() == &document);
        expect(token.children().size() == 1u);
        expect(token.children()[0]->kind() == NodeKind::text);
        expect(token.children()[0]->text() == u"𝑥");
        expect(registrations.events.size() == 1u);
        expect(registrations.events[0].kind == NodeKind::mi);
        expect(registrations.events[0].node == &token);
        expect(registrations.events[0].fullyInitialized);

        const auto beforeError = registrations.events.size();
        auto& error = factory.makeError(u"bad input");
        expect(error.kind() == NodeKind::merror);
        expect(&error.document() == &document);
        expect(registrations.events.size() == beforeError + 2u);
        expect(registrations.events[beforeError].kind == NodeKind::mtext);
        expect(registrations.events[beforeError].node->kind() == NodeKind::mtext);
        expect(registrations.events[beforeError + 1].kind == NodeKind::merror);
        expect(registrations.events[beforeError + 1].node == &error);
        expect(registrations.events[beforeError].fullyInitialized);
        expect(registrations.events[beforeError + 1].fullyInitialized);
        expect(error.attributes().own(u"data-mjx-error") != nullptr);
        expect(error.children().size() == 1u);

        auto& child = *factory.makeText(u"row child");
        MmlNode* inferredChildren[]{&child};
        auto& inferred = factory.makeNode(NodeKind::inferred_mrow, inferredChildren);
        expect(inferred.kind() == NodeKind::inferred_mrow);
        expect(&inferred.document() == &document);
        expect(registrations.events.back().kind == NodeKind::inferred_mrow);
        expect(registrations.events.back().node == &inferred);
        expect(registrations.events.back().fullyInitialized);
    };

    "invalid token and invalid MML creation do not register, and observer remains optional"_test = [] {
        MmlDocument document;
        RecordingRegistration registrations;
        NodeFactory observed(document.factory(), &registrations);
        bool invalidTokenThrew = false;
        try { (void)observed.makeToken(NodeKind::math, u"x"); }
        catch (const std::invalid_argument&) { invalidTokenThrew = true; }
        expect(invalidTokenThrew);
        expect(registrations.events.empty());
        bool invalidKindThrew = false;
        try { (void)observed.makeNode(static_cast<NodeKind>(255)); }
        catch (const std::invalid_argument&) { invalidKindThrew = true; }
        expect(invalidKindThrew);
        expect(registrations.events.empty());

        NodeFactory unobserved(document.factory());
        auto& token = unobserved.makeToken(NodeKind::mn, u"2");
        expect(token.kind() == NodeKind::mn);
        expect(&token.document() == &document);
        expect(registrations.events.empty());
    };

    "foreign-child failure leaves existing document tree and registration untouched"_test = [] {
        MmlDocument document;
        RecordingRegistration registrations;
        NodeFactory factory(document.factory(), &registrations);
        auto& existingChild = *factory.makeText(u"existing");
        MmlNode* existingChildren[]{&existingChild};
        auto& existingRoot = factory.makeNode(NodeKind::mrow, existingChildren);
        document.setRoot(existingRoot);
        registrations.events.clear();

        MmlDocument foreignDocument;
        auto& foreignChild = foreignDocument.factory().make(NodeKind::mi);
        MmlNode* foreignChildren[]{&foreignChild};
        bool threw = false;
        try { (void)factory.makeNode(NodeKind::mrow, foreignChildren); }
        catch (const std::invalid_argument&) { threw = true; }
        expect(threw);
        expect(registrations.events.empty());
        expect(document.root() == &existingRoot);
        expect(existingRoot.children().size() == 1u);
        expect(existingRoot.children()[0] == &existingChild);
        expect(existingChild.parent() == &existingRoot);
        expect(foreignChild.parent() == nullptr);
    };

    "observer throw keeps completed document-owned nodes and prior notification"_test = [] {
        MmlDocument document;
        RecordingRegistration registrations;
        registrations.throwOnMerror = true;
        NodeFactory factory(document.factory(), &registrations);
        bool threw = false;
        try { (void)factory.makeError(u"observer throws at merror"); }
        catch (const std::runtime_error&) { threw = true; }
        expect(threw);
        expect(registrations.events.size() == 2u);
        expect(registrations.events[0].kind == NodeKind::mtext);
        expect(registrations.events[0].fullyInitialized);
        expect(registrations.events[1].kind == NodeKind::merror);
        expect(registrations.events[1].fullyInitialized);
        expect(&registrations.events[0].node->document() == &document);
        expect(&registrations.events[1].node->document() == &document);
        expect(registrations.events[1].node->children().size() == 1u);
        expect(registrations.events[1].node->attributes().own(u"data-mjx-error") != nullptr);
    };
};
