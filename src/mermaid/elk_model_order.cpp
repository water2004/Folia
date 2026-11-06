// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_model_order;
import std;
namespace folia::mermaid {
namespace {
// Comparators retain transitive associations, as the original comparator does.
struct Associations {
 std::vector<std::vector<bool>> less;
 explicit Associations(std::size_t size):less(size,std::vector<bool>(size)){}
 template<class F> bool compare(std::size_t a,std::size_t b,F fallback){
  if(a==b)return false;if(less[a][b])return true;if(less[b][a])return false;
  const int result=fallback();if(!result)return false;
  const auto lo=result<0?a:b,hi=result<0?b:a;
  for(std::size_t x=0;x<less.size();++x)if(x==lo||less[x][lo])
   for(std::size_t y=0;y<less.size();++y)if(y==hi||less[hi][y])less[x][y]=true;
  return result<0;
 }
};
int compare(std::size_t a,std::size_t b){return a<b?-1:a>b?1:0;}
}
void elk_sort_input_model(ElkLayeredGraph& g){
 const auto edge_order=[&](std::size_t e){return g.edges[e].original_edge;};
 const auto target=[&](std::size_t p)->std::optional<std::size_t>{
  auto edge=g.ports[p].outgoing.front();
  for(;;){const auto n=g.ports[g.edges[edge].target_port].node;if(g.nodes[n].kind==ElkLayerNodeKind::normal)return n;
   std::optional<std::size_t> next;for(auto port:g.nodes[n].ports)if(!g.ports[port].outgoing.empty()){next=g.ports[port].outgoing.front();break;}
   if(!next)return {};edge=*next;}
 };
 for(std::size_t layer=0;layer<g.layers.size();++layer){
  const auto previous=g.layers[layer?layer-1:0];
  const auto position=[&](std::size_t n){return std::ranges::find(previous,n)-previous.begin();};
  for(auto n:g.layers[layer]){
   auto& node=g.nodes[n];if(node.fixed_port_order||node.fixed_port_positions)continue;
   std::vector<std::optional<std::size_t>> targets(g.ports.size());std::map<std::size_t,std::size_t> target_order;
   for(auto p:node.ports)if(!g.ports[p].outgoing.empty()){
    targets[p]=target(p);const auto e=g.ports[p].outgoing.front();
    if(targets[p]&&!g.edges[e].reversed){auto [it,added]=target_order.emplace(*targets[p],edge_order(e));if(!added)it->second=std::min(it->second,edge_order(e));}
   }
   Associations relations(g.ports.size());
   std::stable_sort(node.ports.begin(),node.ports.end(),[&](auto a,auto b){return relations.compare(a,b,[&](){
    const auto& x=g.ports[a];const auto& y=g.ports[b];if(x.side!=y.side)return compare(static_cast<std::size_t>(x.side),static_cast<std::size_t>(y.side));
    if(!x.incoming.empty()&&!y.incoming.empty()){
     const auto ea=x.incoming.front(),eb=y.incoming.front(),na=g.ports[g.edges[ea].source_port].node,nb=g.ports[g.edges[eb].source_port].node;
     if(na==nb)return compare(edge_order(ea),edge_order(eb));return compare(position(nb),position(na));
    }
    if(!x.outgoing.empty()&&!y.outgoing.empty()){
     const auto ea=x.outgoing.front(),eb=y.outgoing.front();
     // Original comparator initializes both orders from the first edge.
     auto oa=edge_order(ea),ob=oa;
     if(targets[a]&&targets[a]==targets[b]){if(g.edges[ea].reversed!=g.edges[eb].reversed)return g.edges[ea].reversed?1:-1;return compare(oa,ob);}
     if(targets[a]&&target_order.contains(*targets[a]))oa=target_order.at(*targets[a]);
     if(targets[b]&&target_order.contains(*targets[b]))ob=target_order.at(*targets[b]);return compare(oa,ob);
    }
    if(!x.incoming.empty()&&!y.outgoing.empty())return 1;
    if(!x.outgoing.empty()&&!y.incoming.empty())return -1;return -1;
   });});
  }
  Associations relations(g.nodes.size());
  const auto source_port=[&](std::size_t n){std::optional<std::size_t> result;for(auto p:g.nodes[n].ports)if(!g.ports[p].incoming.empty())result=g.edges[g.ports[p].incoming.front()].source_port;return result;};
  const auto incoming_order=[&](std::size_t n){for(auto p:g.nodes[n].ports)if(!g.ports[p].incoming.empty())return edge_order(g.ports[p].incoming.front());return std::size_t{0};};
  std::stable_sort(g.layers[layer].begin(),g.layers[layer].end(),[&](auto a,auto b){return relations.compare(a,b,[&](){
   const auto& x=g.nodes[a];const auto& y=g.nodes[b];
   if(x.kind==ElkLayerNodeKind::normal&&y.kind==ElkLayerNodeKind::normal)return compare(*x.original_node,*y.original_node);
   const auto pa=source_port(a),pb=source_port(b);
   if(pa&&pb){const auto na=g.ports[*pa].node,nb=g.ports[*pb].node;
    if(na==nb){const auto& ports=g.nodes[na].ports;return compare(std::ranges::find(ports,*pa)-ports.begin(),std::ranges::find(ports,*pb)-ports.begin());}
    return compare(position(na),position(nb));}
   return compare(incoming_order(a),incoming_order(b));
  });});
 }
}
}
