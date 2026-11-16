module folia.math.core.mml_tree.mml_nodes.mpadded;
import std;
import folia.math.core.mml_tree.node_defaults;

namespace folia::math::mml {
MmlMpadded::MmlMpadded(MmlDocument &document)
    : AbstractMmlLayoutNode(document, NodeKind::mpadded) {}

AttributeSet MmlMpadded::defaults() {
    return nodeDefaultAttributes(NodeKind::mpadded);
}

NodeKind MmlMpadded::kind() const noexcept {
    return NodeKind::mpadded;
}

bool MmlMpadded::linebreakContainer() const {
    return true;
}

MmlNode *MmlMpadded::setTeXclass(MmlNode *previous) {
    const auto &vertical_box = properties().tex.vertical_box;
    if (!vertical_box || vertical_box->empty())
        return AbstractMmlLayoutNode::setTeXclass(previous);
    getPrevClass(previous);
    setTexClass(TexClass::ord);
    if (children().empty())
        throw std::out_of_range("mpadded requires its inferred row");
    children()[0]->setTeXclass(nullptr);
    return this;
}
} // namespace folia::math::mml
