// Port of dagre-d3-es 7.0.14 position/bk.js, coordinate-system.js and the
// post-order layout helpers. Centered edge labels; MIT, ../DAGRE-LICENSE.
using Conflicts=std::set<std::pair<Id,Id>>;
void conflict(Conflicts& conflicts,Id a,Id b) {conflicts.emplace(std::min(a,b),std::max(a,b));}
bool has_conflict(const Conflicts& conflicts,Id a,Id b) {return conflicts.contains({std::min(a,b),std::max(a,b)});}

Conflicts position_conflicts(const Graph& g,const Layers& layers) {
    Conflicts result;
    const auto dummy=[&](Id v){return g.nodes[v].kind!=Dummy::none;};
    for(Id rank=1;rank<layers.size();++rank) {
        const auto& north=layers[rank-1];const auto& south=layers[rank];
        int k0=0;Id scan_pos=0;
        for(Id i=0;i<south.size();++i) {
            const auto v=south[i];Id inner=absent;
            if(dummy(v)) for(auto u:g.neighbors(v,false)) if(dummy(u)) {inner=u;break;}
            const int k1=inner==absent?static_cast<int>(north.size()):g.nodes[inner].order;
            if(inner!=absent || i+1==south.size()) {
                for(Id j=scan_pos;j<=i;++j) for(auto u:g.neighbors(south[j],false)) {
                    const auto pos=g.nodes[u].order;
                    if((pos<k0||k1<pos) && !(dummy(u)&&dummy(south[j]))) conflict(result,u,south[j]);
                }
                scan_pos=i+1;k0=k1;
            }
        }
        // Border segments impose the second class of BK conflicts.
        double previous=-1,next=std::numeric_limits<double>::quiet_NaN();Id start=0;
        const auto scan=[&](Id begin,Id end,double before,double after) {
            for(Id i=begin;i<end;++i) if(dummy(south[i])) for(auto u:g.neighbors(south[i],false))
                if(dummy(u) && (g.nodes[u].order<before || g.nodes[u].order>after)) conflict(result,u,south[i]);
        };
        for(Id i=0;i<south.size();++i) {
            const auto v=south[i];
            if(g.nodes[v].kind==Dummy::border) {
                auto predecessors=g.neighbors(v,false);
                if(!predecessors.empty()) {
                    next=g.nodes[predecessors.front()].order;scan(start,i,previous,next);start=i;previous=next;
                }
            }
            scan(start,south.size(),next,static_cast<double>(north.size()));
        }
    }
    return result;
}

std::vector<double> compact_alignment(const Graph& g,const Layers& layers,
    const Conflicts& conflicts,bool down,bool right,double node_spacing) {
    const auto size=g.nodes.size();std::vector<Id> root(size),align(size);std::vector<int> pos(size);
    for(const auto& layer:layers) for(Id i=0;i<layer.size();++i) {auto v=layer[i];root[v]=align[v]=v;pos[v]=static_cast<int>(i);}
    for(const auto& layer:layers) {
        int previous=-1;
        for(auto v:layer) {
            auto neighbors=g.neighbors(v,down);
            std::stable_sort(neighbors.begin(),neighbors.end(),[&](Id a,Id b){return pos[a]<pos[b];});
            if(neighbors.empty()) continue;
            for(Id i=(neighbors.size()-1)/2;i<=neighbors.size()/2;++i) {
                const auto w=neighbors[i];
                if(align[v]==v && previous<pos[w] && !has_conflict(conflicts,v,w)) {
                    align[w]=v;align[v]=root[v]=root[w];previous=pos[w];
                }
            }
        }
    }
    struct BlockEdge {Id v,w;double gap;};
    std::vector<BlockEdge> edges;std::vector<Id> blocks;
    for(const auto& layer:layers) {
        Id previous=absent;
        for(auto v:layer) {
            const auto r=root[v];if(std::ranges::find(blocks,r)==blocks.end()) blocks.push_back(r);
            if(previous!=absent) {
                const auto& a=g.nodes[previous];const auto& b=g.nodes[v];
                const auto gap=(a.width+b.width)/2+(a.kind!=Dummy::none?20:node_spacing)/2+
                    (b.kind!=Dummy::none?20:node_spacing)/2;
                auto found=std::ranges::find_if(edges,[&](const BlockEdge& e){return e.v==root[previous]&&e.w==r;});
                if(found==edges.end()) edges.push_back({root[previous],r,gap});else found->gap=std::max(found->gap,gap);
            }
            previous=v;
        }
    }
    g.key_order(blocks);
    std::vector<double> xs(size);std::vector<unsigned char> state(size);
    const auto pass1=[&](auto&& self,Id v)->void {
        if(state[v]==2) return;
        if(state[v]==1) throw std::runtime_error("Dagre block alignment cycle");
        state[v]=1;double x=0;
        for(const auto& e:edges) if(e.w==v) {self(self,e.v);x=std::max(x,xs[e.v]+e.gap);}
        xs[v]=x;state[v]=2;
    };
    for(auto v:blocks) pass1(pass1,v);
    std::fill(state.begin(),state.end(),0);
    const auto pass2=[&](auto&& self,Id v)->void {
        if(state[v]) return;state[v]=1;double x=std::numeric_limits<double>::infinity();
        for(const auto& e:edges) if(e.v==v) {self(self,e.w);x=std::min(x,xs[e.w]-e.gap);}
        if(std::isfinite(x) && g.nodes[v].border_side!=(right?-1:1)) xs[v]=std::max(xs[v],x);
    };
    for(auto v:blocks) pass2(pass2,v);
    for(const auto& layer:layers) for(auto v:layer) xs[v]=xs[root[v]];
    if(right) for(auto& x:xs) x=-x;
    return xs;
}

void position_nodes(Graph& g,double node_spacing,double rank_spacing) {
    const auto layers=layer_matrix(g);double y=0;
    for(const auto& layer:layers) {
        double height=0;for(auto v:layer) height=std::max(height,g.nodes[v].height);
        for(auto v:layer) g.nodes[v].y=y+height/2;
        y+=height+rank_spacing;
    }
    const auto conflicts=position_conflicts(g,layers);
    std::array<std::vector<double>,4> alignments;
    for(int direction=0;direction<4;++direction) {
        const bool down=direction>=2,right=direction%2!=0;auto adjusted=layers;
        if(down) std::reverse(adjusted.begin(),adjusted.end());
        if(right) for(auto& layer:adjusted) std::reverse(layer.begin(),layer.end());
        alignments[direction]=compact_alignment(g,adjusted,conflicts,down,right,node_spacing);
    }
    Id smallest=0;double best=std::numeric_limits<double>::infinity();
    std::array<double,4> minimum,maximum;
    for(Id i=0;i<4;++i) {
        double min_box=std::numeric_limits<double>::infinity(),max_box=-min_box;
        minimum[i]=min_box;maximum[i]=max_box;
        for(const auto& layer:layers) for(auto v:layer) {
            const auto x=alignments[i][v],half=g.nodes[v].width/2;
            min_box=std::min(min_box,x-half);max_box=std::max(max_box,x+half);
            minimum[i]=std::min(minimum[i],x);maximum[i]=std::max(maximum[i],x);
        }
        if(max_box-min_box<best) {smallest=i;best=max_box-min_box;}
    }
    for(Id i=0;i<4;++i) if(i!=smallest) {
        const double delta=i%2?maximum[smallest]-maximum[i]:minimum[smallest]-minimum[i];
        for(auto& x:alignments[i]) x+=delta;
    }
    for(const auto& layer:layers) for(auto v:layer) {
        std::array<double,4> xs;for(Id i=0;i<4;++i) xs[i]=alignments[i][v];
        std::sort(xs.begin(),xs.end());g.nodes[v].x=(xs[1]+xs[2])/2;
    }
}

FlowchartPoint rectangle_intersection(const FlowchartPlacedNode& n,FlowchartPoint p) {
    const double dx=p.x-n.x,dy=p.y-n.y,w=n.width/2,h=n.height/2;
    if(dx==0 && dy==0) throw std::runtime_error("Dagre edge cannot intersect its node center");
    if(std::abs(dy)*w>std::abs(dx)*h) {
        const double y=dy<0?-h:h;return {n.x+y*dx/dy,n.y+y};
    }
    const double x=dx<0?-w:w;return {n.x+x,n.y+x*dy/dx};
}

FlowchartLayout place_graph(Graph& g,const FlowchartInput& input,const FlowchartRanks& ranks,
    std::span<const FlowchartEdgeLabelSize> labels,const std::vector<DummyChain>& chains) {
    FlowchartLayout result;result.nodes.resize(input.nodes.size());result.edges.resize(input.edges.size());
    std::unordered_map<std::u16string,Id> ids;
    for(Id i=0;i<input.nodes.size();++i) {
        ids.emplace(input.nodes[i].id,i);result.nodes[i].rank=g.nodes[i].rank;
        if(g.nodes[i].rank) result.nodes[i].order=g.nodes[i].order;
    }
    // Self-loop dummy insertion happens after crossing reduction, as upstream.
    for(const auto& layer:layer_matrix(g)) {
        int shift=0;
        for(Id i=0;i<layer.size();++i) {
            const auto v=layer[i];g.nodes[v].order=static_cast<int>(i)+shift;
            for(Id edge=0;edge<input.edges.size();++edge) if(ranks.edges[edge].self && ids.at(input.edges[edge].start)==v) {
                const auto dummy=g.dummy(u"_se");auto& n=g.nodes[dummy];
                n.kind=Dummy::self;n.edge=edge;n.rank=g.nodes[v].rank;n.order=static_cast<int>(i)+ ++shift;
                n.width=labels[edge].width;n.height=labels[edge].height;
            }
        }
    }
    const bool horizontal=input.direction==u"LR"||input.direction==u"RL";
    if(horizontal) for(auto& n:g.nodes) std::swap(n.width,n.height);
    position_nodes(g,input.node_spacing,ranks.rank_spacing);
    for(const auto& n:g.nodes) if(n.active && n.kind==Dummy::self) {
        const auto& node=g.nodes[ids.at(input.edges[n.edge].start)];
        const auto x=node.x+node.width/2,y=node.y,dx=n.x-x,dy=node.height/2;
        auto& e=result.edges[n.edge];
        e.points={{x+2*dx/3,y-dy},{x+5*dx/6,y-dy},{x+dx,y},{x+5*dx/6,y+dy},{x+2*dx/3,y+dy}};
        e.label=FlowchartPoint{n.x,n.y};
    }
    for(Id i=0;i<input.nodes.size();++i) {
        auto& n=g.nodes[i];auto& out=result.nodes[i];
        if(n.top!=absent) {
            const auto& left=g.nodes[n.left.back()];const auto& right=g.nodes[n.right.back()];
            n.width=std::abs(right.x-left.x);n.height=std::abs(g.nodes[n.bottom].y-g.nodes[n.top].y);
            n.x=left.x+n.width/2;n.y=g.nodes[n.top].y+n.height/2;
        }
        out.id=n.id;out.x=n.x;out.y=n.y;out.width=n.width;out.height=n.height;
    }
    for(const auto& chain:chains) for(auto v:chain.nodes) {
        const auto& n=g.nodes[v];auto& e=result.edges[chain.edge.original];e.points.push_back({n.x,n.y});
        if(n.kind==Dummy::label) e.label=FlowchartPoint{n.x,n.y};
    }
    const auto transform=[&](FlowchartPoint p) {
        if(input.direction==u"BT"||input.direction==u"RL") p.y=-p.y;
        if(horizontal) std::swap(p.x,p.y);return p;
    };
    double min_x=std::numeric_limits<double>::infinity(),min_y=min_x,max_x=0,max_y=0;
    const auto extremes=[&](FlowchartPoint p,double width,double height) {
        min_x=std::min(min_x,p.x-width/2);min_y=std::min(min_y,p.y-height/2);
        max_x=std::max(max_x,p.x+width/2);max_y=std::max(max_y,p.y+height/2);
    };
    for(auto& n:result.nodes) {
        auto p=transform({n.x,n.y});n.x=p.x;n.y=p.y;
        if(horizontal) std::swap(n.width,n.height);extremes(p,n.width,n.height);
    }
    for(Id i=0;i<result.edges.size();++i) {
        auto& e=result.edges[i];for(auto& p:e.points) p=transform(p);
        if(e.label) {*e.label=transform(*e.label);extremes(*e.label,labels[i].width,labels[i].height);}
    }
    result.width=max_x-min_x;result.height=max_y-min_y;
    for(auto& n:result.nodes) {n.x-=min_x;n.y-=min_y;}
    for(Id i=0;i<result.edges.size();++i) {
        auto& e=result.edges[i];for(auto& p:e.points) {p.x-=min_x;p.y-=min_y;}
        if(e.label) {e.label->x-=min_x;e.label->y-=min_y;}
        auto from=ids.at(input.edges[i].start),to=ids.at(input.edges[i].end);
        if(ranks.edges[i].reversed) std::swap(from,to);
        const auto& a=result.nodes[from];const auto& b=result.nodes[to];
        const auto first=e.points.empty()?FlowchartPoint{b.x,b.y}:e.points.front();
        const auto last=e.points.empty()?FlowchartPoint{a.x,a.y}:e.points.back();
        e.points.insert(e.points.begin(),rectangle_intersection(a,first));
        e.points.push_back(rectangle_intersection(b,last));
        if(ranks.edges[i].reversed) std::reverse(e.points.begin(),e.points.end());
    }
    return result;
}
