// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_direction;
import std;

namespace folia::mermaid {
ElkGraphPadding elk_normalize_padding(ElkGraphPadding p,ElkLayoutDirection direction) {
    if(direction==ElkLayoutDirection::left)std::swap(p.left,p.right);
    else if(direction==ElkLayoutDirection::down||direction==ElkLayoutDirection::up) {
        p={p.left,p.top,p.right,p.bottom};
        if(direction==ElkLayoutDirection::up)std::swap(p.left,p.right);
    }
    return p;
}
ElkNodeBox elk_export_direction_box(ElkNodeBox b,double width,double height,ElkLayoutDirection direction) {
    if(direction==ElkLayoutDirection::left||direction==ElkLayoutDirection::up)b.x=width-b.x-b.width;
    if(direction==ElkLayoutDirection::down||direction==ElkLayoutDirection::up){std::swap(b.x,b.y);std::swap(b.width,b.height);}
    return b;
}
ElkGraphPadding elk_group_content_padding(const ElkGroupLayoutBox& box) {
    auto padding=box.padding;
    if(box.title)padding.top+=box.title->height+box.title->padding;
    return padding;
}
ElkNodeBox elk_group_title_box(const ElkNodeBox& group,const ElkGroupTitle& title) {
    return {group.x+(group.width-title.width)/2,group.y+title.padding,title.width,title.height};
}
}
