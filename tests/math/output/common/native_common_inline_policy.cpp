#include "support/folia_test.hpp"
import folia.math.output.common;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.util.utf16;

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mml = folia::math::mml;
namespace style_json = folia::math::style_json;

namespace {
struct BuildSnapshot {
    bool rootInitializeSeen{};
    bool inlineMarked{};
    bool inlineForced{};
    bool operatorBreakable{};
    bool operatorForceBreak{};
    bool ownsOverflow{};
    double containerWidth{};
    std::optional<mml::TexClass> operatorClass;
    std::u16string overflowInherited;
    std::u16string breakStyle;
};

class Measurement final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        return {};
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        return {};
    }
};

class SnapshotWrapper final : public common::CommonWrapper {
    BuildSnapshot &snapshot_;

  public:
    SnapshotWrapper(common::CommonWrapperFactory &factory, mml::MmlNode &node,
                    common::CommonWrapper *parent, BuildSnapshot &snapshot)
        : CommonWrapper(factory, node, parent), snapshot_(snapshot) {
    }

    void initializeSource() override {
        if (node->kind() == mml::NodeKind::math) {
            snapshot_.rootInitializeSeen = true;
            auto &jax = factory().jax();
            auto &root = *node;
            const auto &layout = root.properties().layout;
            snapshot_.inlineMarked = layout.inline_marked.value_or(false);
            snapshot_.inlineForced = layout.inline_forced.value_or(false);
            snapshot_.ownsOverflow = root.attributes().own(u"overflow") != nullptr;
            if (const auto *value = root.attributes().inherited(u"overflow")) {
                if (const auto *text = std::get_if<std::u16string>(value))
                    snapshot_.overflowInherited = *text;
            }
            snapshot_.containerWidth = jax.containerWidth();
            root.walkTree([&](mml::MmlNode &descendant) {
                auto *const op = dynamic_cast<mml::MmlMo *>(&descendant);
                if (!op)
                    return;
                snapshot_.operatorClass = op->texClass();
                snapshot_.operatorBreakable = op->properties().layout.breakable.value_or(false);
                snapshot_.operatorForceBreak = op->properties().layout.force_break.value_or(false);
                if (const auto *value = op->attributes().lookup(u"linebreakstyle")) {
                    if (const auto *text = std::get_if<std::u16string>(value))
                        snapshot_.breakStyle = *text;
                }
            });
        }
        CommonWrapper::initializeSource();
    }
};

common::CommonWrapperCatalog snapshotCatalog(BuildSnapshot &snapshot) {
    common::CommonWrapperCatalog catalog;
    for (const auto kind : {mml::NodeKind::math, mml::NodeKind::inferred_mrow, mml::NodeKind::mi,
                            mml::NodeKind::mo, mml::NodeKind::text}) {
        common::CommonWrapperRegistration registration;
        registration.kind = kind;
        registration.data = common::CommonWrapperClassData::commonDefaults();
        registration.creator.construct = [&snapshot](common::WrapperBuildArguments const &args) {
            return std::make_unique<SnapshotWrapper>(args.factory, args.node, args.parent,
                                                     snapshot);
        };
        registration.creator.isInstance = [](common::CommonWrapper const &wrapper) {
            return dynamic_cast<SnapshotWrapper const *>(&wrapper) != nullptr;
        };
        catalog.push_back(std::move(registration));
    }
    return catalog;
}

common::FontRepository fonts() {
    common::FontRepositoryBuilder builder;
    common::FontFaceData face;
    face.name = "InlinePolicy";
    builder.addFace(std::move(face));
    builder.addParameter("InlinePolicy", "x_height", .5);
    return std::move(builder).freeze();
}

common::CommonOutputJaxConfig configFor(common::FontRepository const &repository,
                                        common::InlineBreakPolicy policy, bool inlineBreaks,
                                        bool display, std::string overflow = "linebreak") {
    common::ExtendedMetrics metrics;
    metrics.em = 16;
    metrics.ex = 8;
    metrics.containerWidth = 400;
    metrics.display = display;
    common::CommonOptions options;
    options.displayOverflow = std::move(overflow);
    options.linebreaks.inlineBreaks = inlineBreaks;
    options.linebreaks.width = "100%";
    return {std::move(metrics), std::move(options), repository, "InlinePolicy", policy};
}

std::unique_ptr<common::Linebreaks> rejectLinebreaks(common::CommonWrapperFactory &factory) {
    class Reject final : public common::Linebreaks {
      public:
        explicit Reject(common::CommonWrapperFactory &owner) : Linebreaks(owner) {
        }

        void breakToWidth(common::CommonWrapper &, double) override {
            throw std::logic_error("unexpected linebreak visitor invocation");
        }
    };

    return std::make_unique<Reject>(factory);
}

struct Fixture {
    mml::MmlDocument document;
    mml::MmlNode *root{};
    mml::MmlMo *op{};

    Fixture() {
        auto &math = document.factory().make(mml::NodeKind::math);
        root = &math;
        auto children = math.children();
        if (children.empty())
            throw std::logic_error("MmlMath factory omitted its inferred row");
        auto &row = *children.front();
        auto addToken = [&](mml::NodeKind kind, std::u16string text) -> mml::MmlNode & {
            auto &token = document.factory().make(kind);
            auto &textNode = document.factory().make(mml::NodeKind::text);
            textNode.setText(std::move(text));
            document.append(token, textNode);
            document.append(row, token);
            return token;
        };
        (void)addToken(mml::NodeKind::mi, u"a");
        auto &operatorNode = addToken(mml::NodeKind::mo, u"+");
        op = dynamic_cast<mml::MmlMo *>(&operatorNode);
        if (!op)
            throw std::logic_error("MML mo factory returned wrong dynamic type");
        op->attributes().set(u"linebreak", u"before");
        op->attributes().set(u"linebreakstyle", u"before");
        (void)addToken(mml::NodeKind::mi, u"b");
        math.attributes().set(u"maxwidth", u"50%");
        document.setRoot(math);
    }
};
} // namespace

suite math_native_common_inline_policy = [] {
    "svg_forced_policy_marks_inline_math_and_job_initializes_before_wrapping"_test = [] {
        auto repository = fonts();
        Fixture fixture;
        fixture.root->attributes().setInherited(u"overflow", u"scroll");
        BuildSnapshot snapshot;
        Measurement measurement;
        auto config = configFor(repository, common::InlineBreakPolicy::forced, true, false);
        common::LayoutJob job(fixture.document, std::move(config), measurement,
                              snapshotCatalog(snapshot), &rejectLinebreaks);
        (void)job.build();

        expect(job.jax().forceInlineBreaks());
        expect(snapshot.rootInitializeSeen);
        expect(snapshot.inlineMarked);
        expect(snapshot.inlineForced);
        expect(snapshot.operatorBreakable);
        expect(snapshot.operatorForceBreak);
        expect(snapshot.operatorClass.has_value());
        expect(snapshot.breakStyle.compare(u"before") == 0);
        expect(snapshot.overflowInherited.compare(u"linebreak") == 0);
        expect(!snapshot.ownsOverflow);
        expect(snapshot.containerWidth == 12.5);
    };

    "one_document_reconciles_forced_opportunistic_disabled_and_display_jobs"_test = [] {
        auto repository = fonts();
        Fixture fixture;
        Measurement measurement;

        BuildSnapshot forced;
        common::LayoutJob forcedJob(
            fixture.document, configFor(repository, common::InlineBreakPolicy::forced, true, false),
            measurement, snapshotCatalog(forced), &rejectLinebreaks);
        (void)forcedJob.build();
        expect(fixture.root->properties().layout.inline_marked.value_or(false));
        expect(fixture.root->properties().layout.inline_forced.value_or(false));
        expect(fixture.op->properties().layout.force_break.value_or(false));

        BuildSnapshot display;
        common::LayoutJob displayJob(
            fixture.document, configFor(repository, common::InlineBreakPolicy::forced, true, true),
            measurement, snapshotCatalog(display), &rejectLinebreaks);
        (void)displayJob.build();
        expect(displayJob.jax().forceInlineBreaks());
        expect(!fixture.root->properties().layout.inline_marked.value_or(false));
        expect(!fixture.root->properties().layout.inline_forced.value_or(false));
        expect(!fixture.op->properties().layout.breakable.value_or(false));
        expect(!fixture.op->properties().layout.force_break.value_or(false));

        BuildSnapshot opportunistic;
        common::LayoutJob opportunisticJob(
            fixture.document,
            configFor(repository, common::InlineBreakPolicy::opportunistic, true, false),
            measurement, snapshotCatalog(opportunistic), &rejectLinebreaks);
        (void)opportunisticJob.build();
        expect(opportunisticJob.jax().forceInlineBreaks() == false);
        expect(fixture.root->properties().layout.inline_marked.value_or(false));
        expect(!fixture.root->properties().layout.inline_forced.value_or(true));
        expect(fixture.op->properties().layout.breakable.value_or(false));
        expect(!fixture.op->properties().layout.force_break.value_or(false));

        BuildSnapshot disabled;
        common::LayoutJob disabledJob(
            fixture.document,
            configFor(repository, common::InlineBreakPolicy::forced, false, false), measurement,
            snapshotCatalog(disabled), &rejectLinebreaks);
        (void)disabledJob.build();
        expect(!disabledJob.jax().forceInlineBreaks());
        expect(!fixture.root->properties().layout.inline_marked.value_or(false));
        expect(!fixture.root->properties().layout.inline_forced.value_or(false));
        expect(!fixture.op->properties().layout.breakable.value_or(false));
        expect(!fixture.op->properties().layout.force_break.value_or(false));
    };

    "explicit_overflow_survives_source_default_update"_test = [] {
        auto repository = fonts();
        Fixture fixture;
        fixture.root->attributes().set(u"overflow", u"scroll");
        fixture.root->attributes().setInherited(u"overflow", u"truncate");
        const auto savedDefault = *fixture.root->attributes().defaultValue(u"overflow");
        BuildSnapshot snapshot;
        Measurement measurement;
        common::LayoutJob job(fixture.document,
                              configFor(repository, common::InlineBreakPolicy::opportunistic, true,
                                        false, "linebreak"),
                              measurement, snapshotCatalog(snapshot), &rejectLinebreaks);
        (void)job.build();

        expect(snapshot.ownsOverflow);
        expect(snapshot.overflowInherited.compare(u"linebreak") == 0);
        expect(std::isnan(snapshot.containerWidth));
        expect(*fixture.root->attributes().defaultValue(u"overflow") == savedDefault);
    };
};
