// Mermaid 12.2.0 Architecture grammar and ArchitectureDB. MIT; ../LICENSE.
export module folia.mermaid.architecture;
export import folia.mermaid.frontend;
import std;

export namespace folia::mermaid {
struct ArchitectureNode {
    std::u16string id,type,icon,icon_text,title,parent;
    std::vector<std::size_t> edges;
};
struct ArchitectureGroup {std::u16string id,icon,title,parent;};
struct ArchitectureEdge {
    std::u16string lhs_id,rhs_id,title;
    char16_t lhs_dir=u'L',rhs_dir=u'R';
    bool lhs_into=false,rhs_into=false,lhs_group=false,rhs_group=false;
};
struct ArchitectureHint {std::u16string direction;std::vector<std::u16string> members;};
struct ArchitectureSpatialNode {std::u16string id;int x=0,y=0;};
struct ArchitectureDocument {
    std::u16string title,acc_title,acc_description;
    std::vector<ArchitectureNode> nodes;
    std::vector<ArchitectureGroup> groups;
    std::vector<ArchitectureEdge> edges;
    std::vector<ArchitectureHint> hints;
    // Ordered Map projections, retaining original overwrite and BFS order.
    std::vector<std::pair<std::u16string,std::vector<std::pair<std::u16string,std::u16string>>>> adjacency;
    std::vector<std::vector<ArchitectureSpatialNode>> spatial_maps;
    std::vector<std::pair<std::u16string,std::u16string>> group_alignments;
    ConfigValue config,effective_config;
};
std::expected<ArchitectureDocument,std::string> prepare_architecture(
    std::u16string_view,const ConfigSession&,FrontendLimits limits={});
}
