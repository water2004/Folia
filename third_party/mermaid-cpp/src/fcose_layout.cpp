module folia.mermaid.fcose_layout;
import std;
namespace folia::mermaid {
namespace {
double sign(double x){return x<0?-1:x>0?1:0;}
struct Point{double x=0,y=0;};
struct Rect {
    double x=0,y=0,w=0,h=0;
    double cx()const{return x+w/2;}double cy()const{return y+h/2;}
    double right()const{return x+w;}double bottom()const{return y+h;}
    bool intersects(const Rect& b)const{return !(right()<b.x||bottom()<b.y||b.right()<x||b.bottom()<y);}
};
// IGeometry.getIntersection2: cardinal-side clipping of center-to-center line.
Point clip(const Rect& a,const Rect& b) {
    const double x=a.cx(),y=a.cy(),bx=b.cx(),by=b.cy();
    if(x==bx)return {x,y>by?a.y:a.bottom()};
    if(y==by)return {x>bx?a.x:a.right(),y};
    const double slope=(by-y)/(bx-x),diagonal=a.h/a.w;
    if(slope==-diagonal)return x>bx?Point{a.x,a.bottom()}:Point{a.right(),a.y};
    if(slope==diagonal)return x>bx?Point{a.x,a.y}:Point{a.right(),a.bottom()};
    int cardinal;
    const auto direction=[](double side,double line,int value){return side>line?value:1+value%4;};
    if(x>bx)cardinal=y>by?direction(diagonal,slope,4):direction(-diagonal,slope,3);
    else cardinal=y>by?direction(-diagonal,slope,1):direction(diagonal,slope,2);
    switch(cardinal){
    case 1:return {x+(-a.h/2)/slope,a.y};
    case 2:return {a.right(),y+a.w/2*slope};
    case 3:return {x+a.h/2/slope,a.bottom()};
    default:return {a.x,y+(-a.w/2)*slope};
    }
}
Point separation(const Rect& a,const Rect& b,double buffer) {
    double ox=std::min(a.right(),b.right())-std::max(a.x,b.x);
    double oy=std::min(a.bottom(),b.bottom())-std::max(a.y,b.y);
    if(a.x<=b.x&&a.right()>=b.right())ox+=std::min(b.x-a.x,a.right()-b.right());
    else if(b.x<=a.x&&b.right()>=a.right())ox+=std::min(a.x-b.x,b.right()-a.right());
    if(a.y<=b.y&&a.bottom()>=b.bottom())oy+=std::min(b.y-a.y,a.bottom()-b.bottom());
    else if(b.y<=a.y&&b.bottom()>=a.bottom())oy+=std::min(a.y-b.y,b.bottom()-a.bottom());
    double slope=std::abs((b.cy()-a.cy())/(b.cx()-a.cx()));
    if(b.cy()==a.cy()&&b.cx()==a.cx())slope=1;
    double dy=slope*ox,dx=oy/slope;
    if(ox<dx)dx=ox;else dy=oy;
    return {(a.cx()<b.cx()?1:-1)*(dx/2+buffer),(a.cy()<b.cy()?1:-1)*(dy/2+buffer)};
}
constexpr auto absent=std::numeric_limits<std::size_t>::max();
struct Node {
    Rect rect;std::size_t parent=absent;std::vector<std::size_t> children,surrounding;
    double padding=0,estimated=0,weight=1,fx=0,fy=0,rx=0,ry=0,gx=0,gy=0,dx=0,dy=0;bool gravity=false;
    int sx=0,ex=0,sy=0,ey=0;
};
struct Edge {std::size_t a,b;double ideal,elasticity;};
class Layout {
    const FCoseLayoutInput& input;std::vector<Node> nodes;std::vector<Edge> edges;std::vector<std::size_t> roots;
    std::map<std::u16string,std::size_t> ids;double ideal=50;Rect root_bounds;
    std::size_t depth(std::size_t i)const{return nodes[i].parent==absent?1:depth(nodes[i].parent)+1;}
    std::size_t ancestor(std::size_t i,std::size_t owner)const {while(i!=absent&&nodes[i].parent!=owner)i=nodes[i].parent;return i;}
    Rect bounds(const std::vector<std::size_t>& members,double padding) {
        double l=std::numeric_limits<double>::infinity(),t=l,r=-l,b=-l;
        for(const auto i:members){auto& n=nodes[i];if(!n.children.empty())n.rect=bounds(n.children,n.padding);l=std::min(l,n.rect.x);r=std::max(r,n.rect.right());t=std::min(t,n.rect.y);b=std::max(b,n.rect.bottom());}
        return {l-padding,t-padding,r-l+2*padding,b-t+2*padding};
    }
    double estimate(std::size_t i) {
        auto& n=nodes[i];if(n.children.empty())return n.estimated=(n.rect.w+n.rect.h)/2;
        double size=0;n.weight=0;for(const auto child:n.children){size+=estimate(child);n.weight+=nodes[child].weight;}
        n.estimated=size/std::sqrt(static_cast<double>(n.children.size()));n.rect.w=n.rect.h=n.estimated;return n.estimated;
    }
    void connected(const std::vector<std::size_t>& members,std::size_t owner) {
        if(members.empty())return;std::set<std::size_t> visited{members.front()};bool changed=true;
        while(changed){changed=false;for(const auto& e:edges){const auto a=ancestor(e.a,owner),b=ancestor(e.b,owner);if(a==absent||b==absent)continue;if(visited.contains(a)&&!visited.contains(b)){visited.insert(b);changed=true;}if(visited.contains(b)&&!visited.contains(a)){visited.insert(a);changed=true;}}}
        for(const auto i:members){nodes[i].gravity=visited.size()!=members.size();connected(nodes[i].children,i);}
    }
    void grid() {
        const double range=2*std::max(10.0,ideal);const int width=static_cast<int>(std::ceil(root_bounds.w/range)),height=static_cast<int>(std::ceil(root_bounds.h/range));
        std::vector<std::vector<std::size_t>> cells(static_cast<std::size_t>(width)*height);
        for(std::size_t i=0;i<nodes.size();++i){auto& n=nodes[i];n.sx=static_cast<int>(std::floor((n.rect.x-root_bounds.x)/range));n.ex=static_cast<int>(std::floor((n.rect.right()-root_bounds.x)/range));n.sy=static_cast<int>(std::floor((n.rect.y-root_bounds.y)/range));n.ey=static_cast<int>(std::floor((n.rect.bottom()-root_bounds.y)/range));for(int x=n.sx;x<=n.ex;++x)for(int y=n.sy;y<=n.ey;++y)cells[x*height+y].push_back(i);}
        for(std::size_t i=0;i<nodes.size();++i){auto& n=nodes[i];n.surrounding.clear();std::vector<bool> seen(nodes.size());for(int x=n.sx-1;x<n.ex+2;++x)for(int y=n.sy-1;y<n.ey+2;++y){if(x<0||y<0||x>=width||y>=height)continue;for(const auto j:cells[x*height+y]){const auto& b=nodes[j];if(j<=i||seen[j]||n.parent!=b.parent)continue;const auto dx=std::abs(n.rect.cx()-b.rect.cx())-(n.rect.w/2+b.rect.w/2),dy=std::abs(n.rect.cy()-b.rect.cy())-(n.rect.h/2+b.rect.h/2);if(dx<=range&&dy<=range){seen[j]=true;n.surrounding.push_back(j);}}}}
    }
    void spring(const Edge& e) {
        auto& a=nodes[e.a];auto& b=nodes[e.b];if(a.rect.intersects(b.rect))return;
        const auto s=clip(a.rect,b.rect),t=clip(b.rect,a.rect);double dx=t.x-s.x,dy=t.y-s.y;
        if(std::abs(dx)<1)dx=sign(dx);if(std::abs(dy)<1)dy=sign(dy);
        const auto length=std::sqrt(dx*dx+dy*dy);if(length==0)return;
        const auto force=e.elasticity*(length-e.ideal);const auto fx=force*(dx/length),fy=force*(dy/length);
        a.fx+=fx;a.fy+=fy;b.fx-=fx;b.fy-=fy;
    }
    void repulse(std::size_t i,std::size_t j) {
        auto& a=nodes[i];auto& b=nodes[j];double fx,fy;
        if(a.rect.intersects(b.rect)){const auto overlap=separation(a.rect,b.rect,ideal/2);const auto weight=a.weight*b.weight/(a.weight+b.weight);fx=weight*2*overlap.x;fy=weight*2*overlap.y;}
        else {const auto s=clip(a.rect,b.rect),t=clip(b.rect,a.rect);double dx=t.x-s.x,dy=t.y-s.y;const double minimum=ideal/10;if(std::abs(dx)<minimum)dx=sign(dx)*minimum;if(std::abs(dy)<minimum)dy=sign(dy)*minimum;const auto square=dx*dx+dy*dy,distance=std::sqrt(square),force=4500*a.weight*b.weight/square;fx=force*dx/distance;fy=force*dy/distance;}
        a.rx-=fx;a.ry-=fy;b.rx+=fx;b.ry+=fy;
    }
    void propagate(std::size_t i,double dx,double dy){for(const auto child:nodes[i].children){auto& n=nodes[child];if(n.children.empty()){n.dx+=dx;n.dy+=dy;}else propagate(child,dx,dy);}}
public:
    explicit Layout(const FCoseLayoutInput& value):input(value){}
    FCoseLayoutResult run() {
        for(const auto& n:input.nodes){ids.emplace(n.id,nodes.size());Node node;node.rect={n.x-n.width/2,n.y-n.height/2,n.width,n.height};node.padding=n.padding;nodes.push_back(node);}
        for(std::size_t i=0;i<nodes.size();++i){const auto& parent=input.nodes[i].parent;if(parent.empty())roots.push_back(i);else{nodes[i].parent=ids.at(parent);nodes[nodes[i].parent].children.push_back(i);}}
        double total=0;for(const auto& e:input.edges){edges.push_back({ids.at(e.source),ids.at(e.target),e.ideal,e.elasticity});total+=e.ideal;}if(!edges.empty())ideal=total/edges.size();
        connected(roots,absent);double root_estimated=0;for(const auto i:roots)root_estimated+=estimate(i);root_estimated/=std::sqrt(static_cast<double>(roots.size()));
        for(auto& e:edges)if(nodes[e.a].parent!=nodes[e.b].parent){auto owner=nodes[e.a].parent;while(owner!=absent&&ancestor(e.b,owner)==absent)owner=nodes[owner].parent;const auto a=ancestor(e.a,owner),b=ancestor(e.b,owner);const auto level=owner==absent?1:depth(owner);e.ideal+=nodes[a].estimated+nodes[b].estimated-80+e.ideal*.1*(depth(e.a)+depth(e.b)-2*level);}
        std::vector<FCoseConstraintNode> constrained;std::vector<std::size_t> leaf_ids;
        for(std::size_t i=0;i<nodes.size();++i)if(nodes[i].children.empty()){leaf_ids.push_back(i);constrained.push_back({input.nodes[i].id,nodes[i].rect.cx(),nodes[i].rect.cy()});}
        const auto enforced=enforce_fcose_constraints(constrained,input.constraints);if(!enforced)throw std::runtime_error(enforced.error());
        for(std::size_t i=0;i<leaf_ids.size();++i){auto& r=nodes[leaf_ids[i]].rect;r.x=constrained[i].x-r.w/2;r.y=constrained[i].y-r.h/2;}
        std::vector<FCoseMovementNode> moving;for(std::size_t i=0;i<nodes.size();++i)moving.push_back({input.nodes[i].id,nodes[i].rect.cx(),nodes[i].rect.cy()});
        auto movement=FCoseConstraintMovement::create(moving,input.constraints,input.seed);if(!movement)throw std::runtime_error(movement.error());
        const auto max_iterations=std::max(input.max_iterations,nodes.size()*5);double cooling=.3,initial=.3,displacement=0,old=0;std::size_t cycle=0,iterations=0;
        for(;;){++iterations;if(iterations==max_iterations)break;if(iterations%100==0){const bool oscillating=iterations>max_iterations/3.0&&std::abs(displacement-old)<2;old=displacement;if(displacement<.03*ideal*nodes.size()||oscillating)break;++cycle;cooling=std::max(initial-std::pow(static_cast<double>(cycle),std::log(100*(initial-.04))/std::log(max_iterations/100.0))/100,.04);}
            displacement=0;root_bounds=bounds(roots,15);for(const auto& edge:edges)spring(edge);if(iterations%10==1)grid();for(std::size_t i=0;i<nodes.size();++i)for(const auto j:nodes[i].surrounding)repulse(i,j);
            for(std::size_t i=0;i<nodes.size();++i){auto& n=nodes[i];if(n.gravity){const auto owner=n.parent;const auto box=owner==absent?root_bounds:nodes[owner].rect;const auto estimate=owner==absent?root_estimated:nodes[owner].estimated;const double dx=n.rect.cx()-box.cx(),dy=n.rect.cy()-box.cy(),range=estimate*(owner==absent?3.8:1.5);if(std::abs(dx)+n.rect.w/2>range||std::abs(dy)+n.rect.h/2>range){n.gx=-.25*dx;n.gy=-.25*dy;}}
                n.dx+=cooling*(n.fx+n.rx+n.gx)/n.weight;n.dy+=cooling*(n.fy+n.ry+n.gy)/n.weight;n.dx=std::clamp(n.dx,-cooling*100,cooling*100);n.dy=std::clamp(n.dy,-cooling*100,cooling*100);if(!n.children.empty())propagate(i,n.dx,n.dy);
            }
            for(std::size_t i=0;i<nodes.size();++i){moving[i].dx=nodes[i].dx;moving[i].dy=nodes[i].dy;}movement->update(moving,iterations);
            for(std::size_t i=0;i<nodes.size();++i){auto& n=nodes[i];if(n.children.empty()){n.rect.x+=moving[i].dx;n.rect.y+=moving[i].dy;displacement+=std::abs(moving[i].dx)+std::abs(moving[i].dy);}n.dx=n.dy=n.fx=n.fy=n.rx=n.ry=n.gx=n.gy=0;}
        }
        root_bounds=bounds(roots,15);FCoseLayoutResult result;result.iterations=iterations;result.nodes=input.nodes;
        for(std::size_t i=0;i<nodes.size();++i){const auto& r=nodes[i].rect;auto& n=result.nodes[i];n.x=r.cx();n.y=r.cy();n.width=r.w;n.height=r.h;}return result;
    }
};
}
std::expected<FCoseLayoutResult,std::string> layout_fcose_incremental(const FCoseLayoutInput& input) {
    if(input.nodes.empty())return std::unexpected("fCoSE requires nodes");
    try{return Layout(input).run();}catch(const std::exception& e){return std::unexpected(e.what());}
}
}
