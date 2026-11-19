// Block uses the shared Mermaid node handlers twice: intrinsic measurement,
// then painting at the grid's assigned size. Circles and stadiums deliberately
// keep their intrinsic size; other handlers consume the assigned dimensions.
namespace block_shapes {
std::string number(double value) { return num(value); }
std::string point(FlowchartPoint value) { return number(value.x)+","+number(value.y); }
#include "../flowchart/flowchart_shapes.inc"

std::u16string_view canonical(std::u16string_view type) {
    if(type==u"rect_left_inv_arrow") return u"odd";
    if(type==u"square"||type==u"round"||type==u"na") return u"squareRect";
    return type;
}
FlowchartNodeBox intrinsic(std::u16string_view type,double width,double height,double padding) {
    const auto box=classic_shape_box(canonical(type),width,height,0,padding);
    if(!box) throw std::runtime_error("Unsupported Block node shape");
    return *box;
}
FlowchartNodeBox positioned(std::u16string_view type,double width,double height,
    double padding,FlowchartNodeBox assigned) {
    const auto shape=canonical(type);
    const auto minimum=intrinsic(type,width,height,padding);
    if(shape==u"circle"||shape==u"diamond"||shape==u"stadium") return minimum;
    if(shape==u"doublecircle") {
        const auto diameter=assigned.width+2*padding+10;
        return {diameter,diameter};
    }
    if(shape==u"cylinder") {
        const auto original_radius=cylinder_radius_y(assigned.width);
        const auto w=std::max(width,std::max(8.0,assigned.width-padding))+padding;
        const auto h=std::max(height,std::max(8.0,assigned.height-padding-3*original_radius))+
            padding+3*cylinder_radius_y(w);
        return {w,h};
    }
    if(classic_polygon(shape)&&shape!=u"hexagon") {
        const auto p=shape==u"inv_trapezoid"?2*padding:padding;
        const auto h=std::max(height+p,assigned.height);
        const auto slant=shape==u"odd"?h/4:h;
        return {std::max(width+p,assigned.width-slant)+slant,h};
    }
    return {std::max(minimum.width,assigned.width),std::max(minimum.height,assigned.height)};
}
FlowchartPoint label_offset(std::u16string_view type,FlowchartNodeBox box,double padding) {
    if(type==u"cylinder") return {0,padding/1.5};
    if(type==u"rect_left_inv_arrow") return {box.height/8,0};
    return {};
}
std::string paint(std::u16string_view type,FlowchartNodeBox box,std::string_view attributes) {
    const auto shape=canonical(type);
    const auto w=box.width,h=box.height;
    const std::string style{attributes};
    if(shape==u"circle"||shape==u"doublecircle") {
        auto svg="<circle r=\""+number(w/2)+"\""+style+"/>";
        if(shape==u"doublecircle") svg+="<circle r=\""+number(w/2-5)+"\""+style+"/>";
        return svg;
    }
    std::string path;
    if(shape==u"stadium") path=polygon_path(stadium_points(w,h));
    else if(shape==u"subroutine") path=polygon_path(subroutine_points(w,h));
    else if(shape==u"cylinder") path=cylinder_path(w,h);
    else if(classic_polygon(shape)) path=polygon_path(classic_polygon_points(shape,w,h));
    if(!path.empty()) return "<path d=\""+path+"\""+style+"/>";
    return "<rect x=\""+number(-w/2)+"\" y=\""+number(-h/2)+"\" width=\""+number(w)+
        "\" height=\""+number(h)+"\" rx=\""+(type==u"round"?"5":"0")+"\""+style+"/>";
}
FlowchartPoint intersect(std::u16string_view type,const Size& size,FlowchartPoint target) {
    return classic_shape_intersection({.x=size.x,.y=size.y,.width=size.w,.height=size.h},canonical(type),target);
}
}
