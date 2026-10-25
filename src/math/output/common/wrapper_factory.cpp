// Native Common factory ownership and source initialization.
// Source responsibilities: MathJax 4.1.3 output/common/WrapperFactory.ts
// and core/Tree/WrapperFactory.ts.
module folia.math.output.common;
import :wrapper_factory;

import :output_jax;
import :math;
import :mrow;
import :inferred_mrow;
import :mi;
import :mn;
import :mo;
import :ms;
import :mtext;
import :mspace;
import :mfrac;
import :msqrt;
import :mroot;
import :mfenced;
import :mpadded;
import :mphantom;
import :msubsup;
import :munderover;
import :mmultiscripts;
import :mtable;
import :mtr;
import :mtd;
import :maction;
import :menclose;
import :semantics;
import :mglyph;
import :tex_atom;
import :text_node;
import std;

namespace folia::math::output::common {
namespace {

void requireRegistration(CommonWrapperRegistration const &value) {
    if (!value.creator.construct || !value.creator.isInstance) {
        throw std::invalid_argument(
            "Common wrapper registration requires constructor and type predicate");
    }
}

template <class Wrapper> CommonWrapperRegistration registration(mml::NodeKind kind) {
    using Args = WrapperBuildArguments;
    tree::FactoryClass<CommonWrapper, Args> creator;
    creator.construct = [](Args const &args) -> std::unique_ptr<CommonWrapper> {
        return std::make_unique<Wrapper>(args.factory, args.node, args.parent);
    };
    creator.isInstance = [](CommonWrapper const &product) {
        return dynamic_cast<Wrapper const *>(&product) != nullptr;
    };
    return {kind, std::move(creator), CommonWrapperClassData::commonDefaults()};
}

CommonWrapperRegistration generic(mml::NodeKind kind) {
    return registration<CommonWrapper>(kind);
}

CommonWrapperRegistration unsupported(mml::NodeKind kind, std::string_view family) {
    tree::FactoryClass<CommonWrapper, WrapperBuildArguments> creator;
    const std::string message =
        "Common standard catalog family is not constructible: " + std::string(family);
    creator.construct = [message](WrapperBuildArguments const &) -> std::unique_ptr<CommonWrapper> {
        throw std::runtime_error(message);
    };
    creator.isInstance = [](CommonWrapper const &) { return false; };
    return {kind, std::move(creator), CommonWrapperClassData::commonDefaults()};
}

CommonWrapperRegistration mencloseRegistration(std::span<NotationSpec const> notationProfile) {
    if (notationProfile.empty())
        return unsupported(mml::NodeKind::menclose, "menclose notation profile");

    using Args = WrapperBuildArguments;
    tree::FactoryClass<CommonWrapper, Args> creator;
    creator.construct = [notationProfile](Args const &args) -> std::unique_ptr<CommonWrapper> {
        return std::make_unique<CommonMenclose>(args.factory, args.node, args.parent,
                                                notationProfile);
    };
    creator.isInstance = [](CommonWrapper const &product) {
        return dynamic_cast<CommonMenclose const *>(&product) != nullptr;
    };
    return {mml::NodeKind::menclose, std::move(creator), CommonWrapperClassData::commonDefaults()};
}

CommonWrapperCatalog makeStandardNodes(std::span<NotationSpec const> notationProfile) {
    using K = mml::NodeKind;
    CommonWrapperCatalog result;
    result.reserve(mml::allNodeKinds.size());
    result.push_back(registration<CommonMath>(K::math));
    result.push_back(registration<CommonMi>(K::mi));
    result.push_back(registration<CommonMn>(K::mn));
    result.push_back(registration<CommonMo>(K::mo));
    result.push_back(registration<CommonMtext>(K::mtext));
    result.push_back(registration<CommonMspace>(K::mspace));
    result.push_back(registration<CommonMs>(K::ms));
    result.push_back(registration<CommonMrow>(K::mrow));
    result.push_back(registration<CommonInferredMrow>(K::inferred_mrow));
    result.push_back(registration<CommonMfrac>(K::mfrac));
    result.push_back(registration<CommonMsqrt>(K::msqrt));
    result.push_back(registration<CommonMroot>(K::mroot));
    result.push_back(generic(K::mstyle));
    result.push_back(generic(K::merror));
    result.push_back(registration<CommonMpadded>(K::mpadded));
    result.push_back(registration<CommonMphantom>(K::mphantom));
    result.push_back(registration<CommonMfenced>(K::mfenced));
    // The catalog is borrowed from the composition boundary. Common owns no
    // copy and does not import the SVG profile that defines it.
    result.push_back(mencloseRegistration(notationProfile));
    result.push_back(registration<CommonMaction>(K::maction));
    result.push_back(registration<CommonMsub>(K::msub));
    result.push_back(registration<CommonMsup>(K::msup));
    result.push_back(registration<CommonMsubsup>(K::msubsup));
    result.push_back(registration<CommonMunder>(K::munder));
    result.push_back(registration<CommonMover>(K::mover));
    result.push_back(registration<CommonMunderover>(K::munderover));
    result.push_back(registration<CommonMmultiscripts>(K::mmultiscripts));
    result.push_back(generic(K::mprescripts));
    result.push_back(generic(K::none));
    result.push_back(registration<CommonMtable>(K::mtable));
    result.push_back(registration<CommonMlabeledtr>(K::mlabeledtr));
    result.push_back(registration<CommonMtr>(K::mtr));
    result.push_back(registration<CommonMtd>(K::mtd));
    result.push_back(generic(K::maligngroup));
    result.push_back(generic(K::malignmark));
    result.push_back(registration<CommonMglyph>(K::mglyph));
    result.push_back(registration<CommonSemantics>(K::semantics));
    result.push_back(generic(K::annotation));
    result.push_back(generic(K::annotation_xml));
    result.push_back(registration<CommonTeXAtom>(K::tex_atom));
    result.push_back(generic(K::math_choice));
    result.push_back(registration<CommonTextNode>(K::text));
    // CommonXmlNode is an abstract Common mixin: the missing addHDW
    // implementation belongs to an output backend, so it cannot be silently
    // represented by generic row behavior in a backend-neutral Scene.
    result.push_back(unsupported(K::xml, "abstract CommonXmlNode"));
    result.push_back(unsupported(K::html, "abstract CommonXmlNode"));
    return result;
}

} // namespace

CommonWrapperClassData CommonWrapperClassData::commonDefaults() {
    CommonWrapperClassData data;

    // output/common/Wrapper.ts: CommonWrapperClassData defaults.
    // Keep the nested maps as value data; no live Source class objects or
    // callable projections are retained in the native factory.
    // Construct the inner maps in their final nodes. A nested initializer_list
    // copies its const map values, allocating and discarding every inner node
    // for each registered wrapper family in every layout request.
    auto &bold = data.boldVariants.try_emplace("bold").first->second;
    bold.emplace("normal", "bold");
    bold.emplace("italic", "bold-italic");
    bold.emplace("fraktur", "bold-fraktur");
    bold.emplace("script", "bold-script");
    bold.emplace("sans-serif", "bold-sans-serif");
    bold.emplace("sans-serif-italic", "sans-serif-bold-italic");
    auto &unbold = data.boldVariants.try_emplace("normal").first->second;
    unbold.emplace("bold", "normal");
    unbold.emplace("bold-italic", "italic");
    unbold.emplace("bold-fraktur", "fraktur");
    unbold.emplace("bold-script", "script");
    unbold.emplace("bold-sans-serif", "sans-serif");
    unbold.emplace("sans-serif-bold-italic", "sans-serif-italic");
    auto &italic = data.italicVariants.try_emplace("italic").first->second;
    italic.emplace("normal", "italic");
    italic.emplace("bold", "bold-italic");
    italic.emplace("sans-serif", "sans-serif-italic");
    italic.emplace("bold-sans-serif", "sans-serif-bold-italic");
    auto &unitalic = data.italicVariants.try_emplace("normal").first->second;
    unitalic.emplace("italic", "normal");
    unitalic.emplace("bold-italic", "bold");
    unitalic.emplace("sans-serif-italic", "sans-serif");
    unitalic.emplace("sans-serif-bold-italic", "bold-sans-serif");
    return data;
}

CommonWrapperFactory::CommonWrapperFactory(CommonOutputJax &jax, CommonWrapperCatalog catalog)
    : Base(), jax_(jax) {
    classData_.reserve(catalog.size());
    for (auto &entry : catalog) {
        setWrapperClass(std::move(entry));
    }
}

WrapperBuildArguments CommonWrapperFactory::creationArguments(mml::MmlNode &node,
                                                              CommonWrapper *parent) {
    return {*this, node, parent};
}

CommonWrapper &CommonWrapperFactory::adopt(std::unique_ptr<CommonWrapper> product) {
    if (!product) {
        throw std::invalid_argument("Cannot adopt an empty CommonWrapper");
    }
    if (&product->factory() != this) {
        throw std::invalid_argument("CommonWrapper belongs to another factory");
    }
    if (&product->node->document() != &jax_.document()) {
        throw std::invalid_argument("CommonWrapper MML node belongs to another document");
    }

    CommonWrapper *const result = product.get();
    wrappers_.push_back(std::move(product));
    // Source initialization is deliberately after ownership transfer and
    // exactly once per factory-created wrapper. Tree/index linkage remains in
    // CommonWrapper's Source-aligned wrap methods, not in factory dispatch.
    result->initializeSource();
    return *result;
}

CommonOutputJax &CommonWrapperFactory::jax() const noexcept {
    return jax_;
}

CommonWrapperCatalog CommonWrapperFactory::defaultNodes() {
    // MathJax CommonWrapperFactory intentionally has no default kind table;
    // concrete SVG/CHTML classes are supplied by their profile constructors.
    return {};
}

CommonWrapperCatalog CommonWrapperFactory::standardNodes(
    std::span<NotationSpec const> notationProfile) {
    return makeStandardNodes(notationProfile);
}

std::vector<mml::NodeKind> CommonWrapperFactory::getKinds() const {
    return Base::getKinds();
}

std::optional<tree::FactoryClass<CommonWrapper, WrapperBuildArguments>>
CommonWrapperFactory::getWrapperClass(mml::NodeKind kind) const {
    return Base::getNodeClass(kind);
}

void CommonWrapperFactory::setWrapperClass(CommonWrapperRegistration value) {
    requireRegistration(value);
    const auto found = std::find_if(classData_.begin(), classData_.end(),
                                    [&](auto const &entry) { return entry.first == value.kind; });
    const bool insertMetadata = found == classData_.end();
    const auto metadataIndex =
        insertMetadata ? classData_.size() : static_cast<std::size_t>(found - classData_.begin());
    if (insertMetadata) {
        classData_.reserve(classData_.size() + 1);
    }

    // The inherited Tree registry remains the only creator storage. Style
    // metadata is a separate value record, not a copied creator object.
    Base::setNodeClass(value.kind, std::move(value.creator));
    if (insertMetadata) {
        classData_.emplace_back(value.kind, std::move(value.data));
    } else {
        classData_[metadataIndex].second = std::move(value.data);
    }
}

CommonWrapperClassData const &CommonWrapperFactory::classData(mml::NodeKind kind) const {
    const auto found = std::find_if(classData_.begin(), classData_.end(),
                                    [&](auto const &entry) { return entry.first == kind; });
    if (found == classData_.end()) {
        throw std::out_of_range("No Common wrapper class data for MML kind");
    }
    return found->second;
}

std::size_t CommonWrapperFactory::wrapperCount() const noexcept {
    return wrappers_.size();
}

} // namespace folia::math::output::common
