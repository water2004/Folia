module folia.mermaid.architecture_measurement;
import std;
namespace folia::mermaid {
namespace {
struct Bounds {
    double l=std::numeric_limits<double>::infinity(),t=l,r=-l,b=-l;
    void add(double x1,double y1,double x2,double y2){l=std::min(l,x1);t=std::min(t,y1);r=std::max(r,x2);b=std::max(b,y2);}
    void add(const Bounds& value){add(value.l,value.t,value.r,value.b);}
    void expand(double padding){l-=padding;t-=padding;r+=padding;b+=padding;}
};
std::pair<double,double> endpoint(const FCoseNode& n,char16_t direction){switch(direction){case u'L':return {n.x,n.y+n.height/2};case u'R':return {n.x+n.width,n.y+n.height/2};case u'T':return {n.x+n.width/2,n.y};default:return {n.x+n.width/2,n.y+n.height};}}
}
std::expected<ArchitectureCyBounds,std::string> measure_architecture_cy_bounds(const ArchitectureDocument& document,std::span<FCoseNode> nodes,const ArchitectureCyMeasurements& measured) {
    if(measured.nodes.size()!=document.nodes.size()||measured.edges.size()!=document.edges.size())return std::unexpected("Architecture needs measured Cytoscape labels");
    std::map<std::u16string,std::size_t> ids,label_ids;for(std::size_t i=0;i<nodes.size();++i)ids.emplace(nodes[i].id,i);for(std::size_t i=0;i<document.nodes.size();++i)label_ids.emplace(document.nodes[i].id,i);
    std::vector<std::vector<std::size_t>> children(nodes.size());for(std::size_t i=0;i<nodes.size();++i)if(!nodes[i].parent.empty())children[ids.at(nodes[i].parent)].push_back(i);
    const auto node_bounds=[&](std::size_t i,bool full) {
        const auto& n=nodes[i];Bounds body;body.add(n.x-n.width/2,n.y-n.height/2,n.x+n.width/2,n.y+n.height/2);
        if(!full)body.expand(1);
        if(const auto found=label_ids.find(n.id);found!=label_ids.end()&&!document.nodes[found->second].title.empty()) {
            const auto& label=measured.nodes[found->second];const auto width=std::ceil(label.width);
            body.add(n.x-width/2-2,n.y+n.height/2-2,n.x+width/2+2,n.y+n.height/2+label.height+2);
        }
        if(full)body.expand(1);return body;
    };
    const auto update=[&](auto&& self,std::size_t i)->void {
        if(children[i].empty())return;Bounds body;
        for(const auto child:children[i]){self(self,child);body.add(node_bounds(child,false));}
        auto& n=nodes[i];n.x=(body.l+body.r)/2;n.y=(body.t+body.b)/2;n.width=body.r-body.l+2*n.padding+1;n.height=body.b-body.t+2*n.padding+1;
    };
    for(std::size_t i=0;i<nodes.size();++i)if(nodes[i].parent.empty())update(update,i);
    Bounds all;for(std::size_t i=0;i<nodes.size();++i)all.add(node_bounds(i,true));
    for(std::size_t i=0;i<document.edges.size();++i) {
        const auto& edge=document.edges[i];const auto [sx,sy]=endpoint(nodes[ids.at(edge.lhs_id)],edge.lhs_dir);const auto [tx,ty]=endpoint(nodes[ids.at(edge.rhs_id)],edge.rhs_dir);
        Bounds line;line.add(std::min(sx,tx)-1.5,std::min(sy,ty)-1.5,std::max(sx,tx)+1.5,std::max(sy,ty)+1.5);
        if(!edge.title.empty()){const auto& label=measured.edges[i];const auto mx=(sx+tx)/2,my=(sy+ty)/2;line.add(mx-std::ceil(label.width)/2-2,my-label.height/2-2,mx+std::ceil(label.width)/2+2,my+label.height/2+2);}
        line.expand(1);all.add(line);
    }
    return ArchitectureCyBounds{all.l,all.t,all.r-all.l,all.b-all.t};
}
}
