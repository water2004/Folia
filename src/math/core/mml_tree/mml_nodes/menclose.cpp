module folia.math.core.mml_tree.mml_nodes.menclose;
import std;
import folia.math.core.mml_tree.node_defaults;

namespace folia::math::mml {
MmlMenclose::MmlMenclose(MmlDocument &document) : AbstractMmlNode(document, NodeKind::menclose) {}

AttributeSet MmlMenclose::defaults() {
    return nodeDefaultAttributes(NodeKind::menclose);
}

NodeKind MmlMenclose::kind() const noexcept {
    return NodeKind::menclose;
}

double MmlMenclose::arity() const {
    return -1;
}

bool MmlMenclose::linebreakContainer() const {
    return true;
}

MmlNode *MmlMenclose::setTeXclass(MmlNode *previous) {
    if (children().empty())
        throw std::out_of_range("menclose requires its inferred row");
    previous = children()[0]->setTeXclass(previous);
    if (children().empty())
        throw std::out_of_range("menclose child was removed during class assignment");
    updateTeXclass(children()[0]);
    return previous;
}
} // namespace folia::math::mml
