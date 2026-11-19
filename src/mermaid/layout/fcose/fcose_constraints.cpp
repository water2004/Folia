module folia.mermaid.fcose_constraints;
import std;
namespace folia::mermaid {
namespace {
using Text=std::u16string;
using Ids=std::vector<std::size_t>;
void insert(Ids& ids,std::size_t id){if(std::ranges::find(ids,id)==ids.end())ids.push_back(id);}
struct Link {std::size_t id;double gap;bool x;};
struct Vertex {Text id;std::vector<Link> edges;};
std::size_t entry(std::vector<Vertex>& graph,const Text& id){for(std::size_t i=0;i<graph.size();++i)if(graph[i].id==id)return i;graph.push_back({id,{}});return graph.size()-1;}
void solve_axis(std::span<FCoseConstraintNode> nodes,const std::vector<Vertex>& original,const std::vector<std::vector<Text>>& alignment,bool x) {
    const auto index=[&](const Text& id){return static_cast<std::size_t>(std::ranges::find(nodes,id,&FCoseConstraintNode::id)-nodes.begin());};
    const auto coord=[&](std::size_t id)->double&{return x?nodes[id].x:nodes[id].y;};
    for(const auto& group:alignment) {
        Ids unique;for(const auto& id:group)insert(unique,index(id));
        double sum=0;for(const auto id:unique)sum+=coord(id);
        for(const auto id:unique)coord(id)=sum/static_cast<double>(unique.size());
    }
    std::map<Text,Text> dummy_for;std::map<Text,Ids> members;std::map<Text,double> dummy_position;
    for(std::size_t i=0;i<alignment.size();++i) {
        const auto suffix=std::to_string(i);const Text dummy=u"dummy"+Text(suffix.begin(),suffix.end());
        for(const auto& id:alignment[i]){dummy_for[id]=dummy;members[dummy].push_back(index(id));}
        dummy_position[dummy]=coord(index(alignment[i].front()));
    }
    const auto mapped=[&](const Text& id)->Text{const auto it=dummy_for.find(id);return it==dummy_for.end()?id:it->second;};
    std::vector<Vertex> graph;
    for(const auto& source:original)for(const auto& edge:source.edges)if(edge.x==x) {
        const auto a=entry(graph,mapped(source.id)),b=entry(graph,mapped(original[edge.id].id));
        graph[a].edges.push_back({b,edge.gap,x});
    }
    const auto before=[&](std::size_t i){const auto found=index(graph[i].id);return found<nodes.size()?coord(found):dummy_position.at(graph[i].id);};
    std::vector<Ids> undirected(graph.size()),past(graph.size());std::vector<int> degree(graph.size());
    for(std::size_t i=0;i<graph.size();++i){past[i].push_back(i);for(const auto& edge:graph[i].edges){++degree[edge.id];undirected[i].push_back(edge.id);undirected[edge.id].push_back(i);}}
    Ids queue;std::vector<double> positions(graph.size(),-std::numeric_limits<double>::infinity());
    for(std::size_t i=0;i<graph.size();++i)if(degree[i]==0)queue.push_back(i);
    std::vector<bool> visited(graph.size());
    for(std::size_t i=0;i<graph.size();++i)if(!visited[i]) {
        Ids component{i};visited[i]=true;
        for(std::size_t j=0;j<component.size();++j)for(const auto neighbor:undirected[component[j]])if(!visited[neighbor]){visited[neighbor]=true;component.push_back(neighbor);}
        double sum=0;std::size_t count=0;for(const auto id:component)if(degree[id]==0){sum+=before(id);++count;}
        if(count)for(const auto id:component)if(degree[id]==0)positions[id]=sum/static_cast<double>(count);
    }
    for(std::size_t i=0;i<queue.size();++i) {
        const auto current=queue[i];for(const auto& edge:graph[current].edges) {
            positions[edge.id]=std::max(positions[edge.id],positions[current]+edge.gap);
            if(--degree[edge.id]==0)queue.push_back(edge.id);
            Ids merged=past[current];for(const auto id:past[edge.id])insert(merged,id);past[edge.id]=std::move(merged);
        }
    }
    std::vector<Ids> components;
    for(std::size_t i=0;i<graph.size();++i)if(graph[i].edges.empty()) {
        auto found=components.end();for(auto it=components.begin();it!=components.end();++it)if(std::ranges::find(*it,past[i].front())!=it->end())found=it;
        if(found==components.end())components.push_back(past[i]);else for(const auto id:past[i])insert(*found,id);
    }
    for(const auto& component:components) {
        double min_before=std::numeric_limits<double>::infinity(),min_after=min_before,max_before=-min_before,max_after=-min_before;
        for(const auto id:component){min_before=std::min(min_before,before(id));max_before=std::max(max_before,before(id));min_after=std::min(min_after,positions[id]);max_after=std::max(max_after,positions[id]);}
        const auto difference=(min_before+max_before)/2-(min_after+max_after)/2;
        for(const auto id:component)positions[id]+=difference;
    }
    for(std::size_t i=0;i<graph.size();++i) {
        const auto found=members.find(graph[i].id);if(found!=members.end())for(const auto id:found->second)coord(id)=positions[i];else coord(index(graph[i].id))=positions[i];
    }
}
}
std::expected<void,std::string> enforce_fcose_constraints(std::span<FCoseConstraintNode> nodes,const ArchitectureConstraints& constraints) {
    const auto known=[&](const Text& id){return std::ranges::find(nodes,id,&FCoseConstraintNode::id)!=nodes.end();};
    for(const auto* groups:{&constraints.vertical,&constraints.horizontal})for(const auto& group:*groups){if(group.empty())return std::unexpected("fCoSE empty alignment");for(const auto& id:group)if(!known(id))return std::unexpected("fCoSE unknown aligned node");}
    std::vector<Vertex> graph;
    for(const auto& edge:constraints.relative) {
        const bool x=!edge.left.empty();const auto& from=x?edge.left:edge.top;const auto& to=x?edge.right:edge.bottom;
        if(!known(from)||!known(to))return std::unexpected("fCoSE unknown relative node");
        const auto a=entry(graph,from),b=entry(graph,to);graph[a].edges.push_back({b,edge.gap,x});
    }
    solve_axis(nodes,graph,constraints.vertical,true);solve_axis(nodes,graph,constraints.horizontal,false);
    return {};
}
}
