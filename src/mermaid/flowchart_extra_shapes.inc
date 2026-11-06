// Pinned Mermaid 12.2.0 metadata-only polygon and compartment shapes.
bool extra_flowchart_shape(std::u16string_view shape) {
    return shape==u"notch-rect"||shape==u"tri"||shape==u"flip-tri"||shape==u"notch-pent"||
        shape==u"sl-rect"||shape==u"lin-rect"||shape==u"div-rect"||shape==u"win-pane";
}

FlowchartNodeBox extra_shape_box(std::u16string_view shape,double w,double h,double padding,bool neo) {
    const double px=neo?16:padding,py=neo?12:padding;
    if(shape==u"notch-rect") return {w+(neo?56:padding+12),h+(neo?48:padding)};
    if(shape==u"tri"||shape==u"flip-tri") {
        const auto side=w+h+padding*(neo?2:1);
        return {side,side};
    }
    if(shape==u"sl-rect") return {w+2*px,1.5*(h+2*py)};
    if(shape==u"lin-rect") return {w+2*px+(neo?8:16),h+2*py};
    if(shape==u"div-rect") {
        const auto p=neo?16:padding;
        return {w+p,1.2*(h+p)};
    }
    if(shape==u"win-pane") return {w+2*px+10,h+2*py+10};
    return {w+2*px,h+2*py};
}

std::vector<FlowchartPoint> extra_shape_outline(std::u16string_view shape,double w,double h) {
    const double l=-w/2,r=w/2,t=-h/2,b=h/2;
    if(shape==u"notch-rect") return {{l+12,t},{r,t},{r,b},{l,b},{l,t+12}};
    if(shape==u"tri") return {{l,b},{r,b},{0,t}};
    if(shape==u"flip-tri") return {{l,t},{r,t},{0,b}};
    if(shape==u"notch-pent") return {{l*.8,t},{r*.8,t},{r,t*.6},{r,b},{l,b},{l,t*.6}};
    if(shape==u"sl-rect") return {{l,-h/6},{l,b},{r,b},{r,t}};
    return {{l,t},{r,t},{r,b},{l,b}};
}

std::string extra_shape_path(std::u16string_view shape,double w,double h) {
    auto path=polygon_path(extra_shape_outline(shape,w,h));
    const auto append=[&](FlowchartPoint a,FlowchartPoint b) {path+="M"+point(a)+"L"+point(b);};
    if(shape==u"lin-rect") append({-w/2+8,-h/2},{-w/2+8,h/2});
    if(shape==u"div-rect") append({-w/2,-h/3},{w/2,-h/3});
    if(shape==u"win-pane") {
        append({-w/2+10,-h/2},{-w/2+10,h/2});
        append({-w/2,-h/2+10},{w/2,-h/2+10});
    }
    return path;
}

FlowchartPoint extra_shape_label_offset(std::u16string_view shape,FlowchartNodeBox box,
    double label_width,double label_height,double padding,bool html) {
    if(shape==u"tri") return {0,(box.height-label_height)/2-padding/(html?2:1)};
    if(shape==u"flip-tri") return {0,(label_height-box.height+padding)/2};
    if(shape==u"sl-rect") return {(label_width-box.width)/2+padding,-box.height/6+padding+label_height/2};
    if(shape==u"lin-rect") return {4,0};
    if(shape==u"div-rect") return {(label_width-box.width+padding)/2,-box.height/3+(padding+label_height)/2};
    if(shape==u"win-pane") return {5,5};
    return {};
}
