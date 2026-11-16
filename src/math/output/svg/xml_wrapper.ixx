/*************************************************************
 * Native Source output/svg/Wrappers/semantics.ts and HtmlNode.ts owners.
 *************************************************************/
export module folia.math.output.svg.xml_wrapper;
import std;
export import folia.math.output.common;
import folia.math.core.dom_adaptor;
import folia.math.util.styles;

export namespace folia::math::output::svg {
class SvgXmlNode : public common::CommonXmlNode {
public:
    using CommonXmlNode::CommonXmlNode;
    DOMNodeRef addHDW(DOMNodeRef html, styles::StyleList &styles) override;
    common::SceneNodes emitScene(scene::Builder &, std::span<scene::NodeId const> parents) override;
};

class SvgHtmlNode final : public SvgXmlNode {
public:
    using SvgXmlNode::SvgXmlNode;
};

class SvgAnnotationXML final : public common::CommonWrapper {
  public:
    using common::CommonWrapper::CommonWrapper;
};

common::CommonWrapperRegistration xmlRegistration();
common::CommonWrapperRegistration htmlRegistration();
common::CommonWrapperRegistration annotationXmlRegistration();
} // namespace folia::math::output::svg
