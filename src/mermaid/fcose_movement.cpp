module folia.mermaid.fcose_movement;
import std;
namespace folia::mermaid {
FCoseConstraintMovement::Axis FCoseConstraintMovement::build_axis(std::span<const FCoseMovementNode> nodes,const ArchitectureConstraints& constraints,bool x) {
    Axis axis;std::map<std::u16string,std::u16string> dummy_for;std::map<std::u16string,std::vector<std::size_t>> members;
    const auto index=[&](const std::u16string& id){return static_cast<std::size_t>(std::ranges::find(nodes,id,&FCoseMovementNode::id)-nodes.begin());};
    const auto& groups=x?constraints.vertical:constraints.horizontal;
    for(std::size_t i=0;i<groups.size();++i) {
        const auto n=std::to_string(i);const auto dummy=u"dummy"+std::u16string(n.begin(),n.end());
        std::vector<std::size_t> ids;for(const auto& id:groups[i]){dummy_for[id]=dummy;ids.push_back(index(id));}
        members[dummy]=ids;axis.alignments.push_back(std::move(ids));
    }
    const auto slot=[&](const std::u16string& id) {
        const auto found=dummy_for.find(id);const auto key=found==dummy_for.end()?id:found->second;
        for(std::size_t i=0;i<axis.slots.size();++i)if(axis.slots[i].id==key)return i;
        Slot value;value.id=key;
        if(const auto group=members.find(key);group!=members.end())value.members=group->second;else value.members.push_back(index(key));
        const auto& first=nodes[value.members.front()];value.position=x?first.x:first.y;
        axis.order.push_back(axis.slots.size());axis.slots.push_back(std::move(value));return axis.slots.size()-1;
    };
    for(const auto& relative:constraints.relative)if((!relative.left.empty())==x) {
        const auto a=slot(x?relative.left:relative.top),b=slot(x?relative.right:relative.bottom);
        axis.slots[a].relations.push_back({b,relative.gap,true});axis.slots[b].relations.push_back({a,relative.gap,false});
    }
    return axis;
}
std::expected<FCoseConstraintMovement,std::string> FCoseConstraintMovement::create(std::span<const FCoseMovementNode> nodes,const ArchitectureConstraints& constraints,std::uint32_t seed) {
    const auto known=[&](const std::u16string& id){return std::ranges::find(nodes,id,&FCoseMovementNode::id)!=nodes.end();};
    for(const auto* groups:{&constraints.horizontal,&constraints.vertical})for(const auto& group:*groups) {
        if(group.empty())return std::unexpected("fCoSE empty movement alignment");
        for(const auto& id:group)if(!known(id))return std::unexpected("fCoSE unknown movement node");
    }
    for(const auto& r:constraints.relative)if(!known(r.left.empty()?r.top:r.left)||!known(r.left.empty()?r.bottom:r.right))return std::unexpected("fCoSE unknown relative movement node");
    FCoseConstraintMovement result;result.x_=build_axis(nodes,constraints,true);result.y_=build_axis(nodes,constraints,false);result.state_=seed;return result;
}
double FCoseConstraintMovement::random() {
    // architectureSeed.ts mulberry32; uint32 arithmetic matches Math.imul.
    state_+=0x6d2b79f5u;auto value=state_;
    value=(value^(value>>15))*(value|1u);value^=value+(value^(value>>7))*(value|61u);
    return static_cast<double>(value^(value>>14))/4294967296.0;
}
void FCoseConstraintMovement::shuffle(Axis& axis) {
    if(axis.order.empty())return;
    for(std::size_t i=axis.order.size()-1;static_cast<double>(i)>=2.0*axis.order.size()/3;--i) {
        const auto j=static_cast<std::size_t>(std::floor(random()*(i+1)));std::swap(axis.order[i],axis.order[j]);
    }
}
void FCoseConstraintMovement::update_axis(Axis& axis,std::span<FCoseMovementNode> nodes,bool x) {
    const auto displacement=[&](std::size_t id)->double&{return x?nodes[id].dx:nodes[id].dy;};
    for(const auto index:axis.order) {
        auto& slot=axis.slots[index];double delta=displacement(slot.members.front());
        for(const auto& relative:slot.relations) {
            const auto other=axis.slots[relative.other].position;
            const auto difference=relative.forward?other-slot.position-delta:slot.position-other+delta;
            if(difference<relative.gap)delta+=(relative.forward?-1:1)*(relative.gap-difference);
        }
        slot.position+=delta;for(const auto id:slot.members)displacement(id)=delta;
    }
}
void FCoseConstraintMovement::update(std::span<FCoseMovementNode> nodes,std::size_t iteration) {
    const auto align=[&](const Axis& axis,bool x) {
        for(const auto& group:axis.alignments){double sum=0;for(const auto id:group)sum+=x?nodes[id].dx:nodes[id].dy;const auto average=sum/group.size();for(const auto id:group)(x?nodes[id].dx:nodes[id].dy)=average;}
    };
    align(x_,true);align(y_,false);
    if(iteration%10==0){shuffle(x_);shuffle(y_);}
    update_axis(x_,nodes,true);update_axis(y_,nodes,false);
}
}
