// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_compound_ports;
import std;

namespace folia::mermaid {
ElkCompoundPortDummy elk_make_compound_port_dummy(ElkLayoutDirection direction,bool outgoing,double border_offset) {
    ElkPortSide side=ElkPortSide::east;
    switch(direction){case ElkLayoutDirection::left:side=ElkPortSide::west;break;
        case ElkLayoutDirection::down:side=ElkPortSide::south;break;
        case ElkLayoutDirection::up:side=ElkPortSide::north;break;default:break;}
    const auto opposite=[](ElkPortSide s){switch(s){case ElkPortSide::north:return ElkPortSide::south;
        case ElkPortSide::south:return ElkPortSide::north;case ElkPortSide::east:return ElkPortSide::west;default:return ElkPortSide::east;}};
    if(!outgoing)side=opposite(side);
    ElkBoundaryConstraint constraint=ElkBoundaryConstraint::first_separate;
    switch(side){case ElkPortSide::east:constraint=ElkBoundaryConstraint::last_separate;break;
        case ElkPortSide::north:constraint=ElkBoundaryConstraint::top;break;
        case ElkPortSide::south:constraint=ElkBoundaryConstraint::bottom;break;default:break;}
    return {.side=side,.inner_port_side=opposite(side),.constraint=constraint,.border_offset=border_offset};
}
void elk_resize_compound_content(ElkCompoundContent& content,std::span<ElkCompoundPortDummy> dummies,
    double minimum_width,double minimum_height,bool center_horizontal) {
    const auto width=content.width+content.padding_left+content.padding_right;
    const auto height=content.height+content.padding_top+content.padding_bottom;
    const auto extra_x=std::max(0.0,minimum_width-width),extra_y=std::max(0.0,minimum_height-height);
    if(center_horizontal)content.offset.x+=extra_x/2;
    for(auto& dummy:dummies){if(dummy.side==ElkPortSide::east)dummy.position.x+=extra_x;
        else if(dummy.side==ElkPortSide::south)dummy.position.y+=extra_y;}
    content.width+=extra_x;content.height+=extra_y;
}
ElkRoutingPoint elk_export_compound_port(const ElkCompoundContent& c,ElkCompoundPortDummy& d) {
    auto result=d.position;
    switch(d.side) {
    case ElkPortSide::north:
        result.x+=c.padding_left+c.offset.x;result.y=-d.border_offset;
        d.position.y=-(c.padding_top+d.border_offset+c.offset.y);break;
    case ElkPortSide::east:
        result.x=c.width+c.padding_left+c.padding_right+d.border_offset;
        result.y+=c.padding_top+c.offset.y;
        d.position.x=c.width+c.padding_right+d.border_offset-c.offset.x;break;
    case ElkPortSide::south:
        result.x+=c.padding_left+c.offset.x;
        result.y=c.height+c.padding_top+c.padding_bottom+d.border_offset;
        d.position.y=c.height+c.padding_bottom+d.border_offset-c.offset.y;break;
    case ElkPortSide::west:
        result.x=-d.border_offset;result.y+=c.padding_top+c.offset.y;
        d.position.x=-(c.padding_left+d.border_offset+c.offset.x);break;
    default:break;
    }
    return result;
}
}
