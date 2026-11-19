module folia.mermaid.fcose_architecture;
import std;
namespace folia::mermaid {
std::expected<ArchitectureLayoutResult,std::string> layout_architecture(const ArchitectureDocument& document,const ArchitectureCyMeasurements* measured) {
    try {
        const auto number=[&](std::u16string_view key){const auto* value=document.config.find(key);if(!value)throw std::runtime_error("Missing Architecture layout configuration");return value->number;};
        if(const auto* random=document.config.find(u"randomize");random&&random->boolean)return std::unexpected("Architecture spectral initialization is not connected");
        const auto seed=number(u"seed"),icon=number(u"iconSize"),padding=number(u"padding");
        if(seed==0)return std::unexpected("Architecture unseeded fCoSE is not connected");
        if(!(icon>0))return std::unexpected("Architecture requires a positive icon size");
        std::vector<FCoseNode> data;std::map<std::u16string,std::size_t> ids;
        for(const auto& group:document.groups){ids.emplace(group.id,data.size());data.push_back({group.id,group.parent,0,0,0,0,padding});}
        for(const auto& node:document.nodes){ids.emplace(node.id,data.size());data.push_back({node.id,node.parent,0,0,icon,icon,0});}
        std::vector<std::vector<std::size_t>> children(data.size());std::vector<std::size_t> roots;
        for(std::size_t i=0;i<data.size();++i)if(data[i].parent.empty())roots.push_back(i);else children[ids.at(data[i].parent)].push_back(i);
        std::vector<std::size_t> order;
        const auto append=[&](auto&& self,const std::vector<std::size_t>& members)->void{order.insert(order.end(),members.begin(),members.end());for(const auto i:members)if(!children[i].empty())self(self,children[i]);};append(append,roots);
        // Cytoscape's compound auto-size uses child bounds expanded by one
        // pixel on each side, plus the group's CSS padding and 1px border.
        const auto measure=[&](auto&& self,std::size_t i)->void {
            if(children[i].empty())return;double l=std::numeric_limits<double>::infinity(),t=l,r=-l,b=-l;
            for(const auto child:children[i]){self(self,child);const auto& n=data[child];l=std::min(l,n.x-n.width/2);r=std::max(r,n.x+n.width/2);t=std::min(t,n.y-n.height/2);b=std::max(b,n.y+n.height/2);}
            auto& node=data[i];node.x=(l+r)/2;node.y=(t+b)/2;node.width=r-l+2+2*node.padding+1;node.height=b-t+2+2*node.padding+1;
        };
        const auto center=[&](const auto& nodes){double l=std::numeric_limits<double>::infinity(),t=l,r=-l,b=-l;for(const auto& n:nodes){l=std::min(l,n.x-n.width/2);r=std::max(r,n.x+n.width/2);t=std::min(t,n.y-n.height/2);b=std::max(b,n.y+n.height/2);}return std::pair{(l+r)/2,(t+b)/2};};
        const auto constraints=architecture_constraints(document);if(!constraints)return std::unexpected(constraints.error());
        FCoseLayoutInput request;request.constraints=*constraints;request.seed=static_cast<std::uint32_t>(seed);request.max_iterations=static_cast<std::size_t>(number(u"numIter"));
        for(const auto& edge:document.edges){const auto& a=data[ids.at(edge.lhs_id)];const auto& b=data[ids.at(edge.rhs_id)];const bool same=a.parent==b.parent;request.edges.push_back({a.id,b.id,icon*(same?number(u"idealEdgeLengthMultiplier"):.5),same?number(u"edgeElasticity"):.001});}
        ArchitectureLayoutResult output;
        for(std::size_t pass=0;pass<2;++pass) {
            std::pair<double,double> original_center;
            if(measured){const auto box=measure_architecture_cy_bounds(document,data,*measured);if(!box)return std::unexpected(box.error());original_center={box->x+box->width/2,box->y+box->height/2};}
            else{for(const auto i:roots)measure(measure,i);original_center=center(data);}
            request.nodes.clear();for(const auto i:order)request.nodes.push_back(data[i]);
            auto result=layout_fcose_incremental(request);if(!result)return std::unexpected(result.error());output.iterations[pass]=result->iterations;
            const auto laid_out_center=center(result->nodes);const double dx=original_center.first-laid_out_center.first,dy=original_center.second-laid_out_center.second;
            for(const auto& node:result->nodes){auto& actual=data[ids.at(node.id)];actual.x=node.x+dx;actual.y=node.y+dy;}
        }
        if(measured){const auto box=measure_architecture_cy_bounds(document,data,*measured);if(!box)return std::unexpected(box.error());}
        else for(const auto i:roots)measure(measure,i);
        for(std::size_t i=0;i<data.size();++i)if(!children[i].empty()){data[i].width-=2*data[i].padding+1;data[i].height-=2*data[i].padding+1;}
        output.nodes=std::move(data);return output;
    }catch(const std::exception& e){return std::unexpected(e.what());}
}
}
