module folia.mermaid.cose_tree;
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
struct Node {
    Rect rect;std::vector<std::size_t> neighbors,surrounding;
    double spring_x=0,spring_y=0,repulsion_x=0,repulsion_y=0;
    int start_x=0,finish_x=0,start_y=0,finish_y=0;
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
Point separation(const Rect& a,const Rect& b) {
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
    return {(a.cx()<b.cx()?1:-1)*(dx/2+25),(a.cy()<b.cy()?1:-1)*(dy/2+25)};
}
class Layout {
public:
    std::vector<Node> nodes;
    std::vector<std::pair<std::size_t,std::size_t>> edges;
    std::size_t iterations=0;
    void run(const std::vector<std::size_t>& tree) {
        radial(tree);
        const auto count=nodes.size();
        double cooling=count>1000?std::max(.33,1.0-(count-1000)/4000.0*(1-.33)):1;
        const double initial=cooling;
        const std::size_t max_iterations=std::max<std::size_t>(count*5,2500);
        double displacement=0,old_displacement=0;std::size_t cycle=0;
        for(;;) {
            ++iterations;
            if(iterations==max_iterations)break;
            if(iterations%100==0) {
                const bool oscillating=iterations>max_iterations/3.0&&std::abs(displacement-old_displacement)<2;
                const bool converged=displacement<1.5*count;
                old_displacement=displacement;
                if(converged||oscillating)break;
                ++cycle;
                cooling=std::max(initial-std::pow(double(cycle),std::log(100*(initial-.04))/std::log(25.0))/100,.04);
            }
            displacement=0;
            for(const auto [a,b]:edges)spring(nodes[a],nodes[b]);
            if(iterations%10==1)update_grid();
            for(std::size_t i=0;i<count;++i)for(const auto j:nodes[i].surrounding)repulsion(nodes[i],nodes[j]);
            // A Mindmap tree is connected: original calculateNodesToApply-
            // GravitationTo returns no nodes, and no compound moves apply.
            for(auto& node:nodes) {
                double dx=cooling*(node.spring_x+node.repulsion_x),dy=cooling*(node.spring_y+node.repulsion_y);
                if(std::abs(dx)>cooling*300)dx=cooling*300*sign(dx);
                if(std::abs(dy)>cooling*300)dy=cooling*300*sign(dy);
                node.rect.x+=dx;node.rect.y+=dy;
                displacement+=std::abs(dx)+std::abs(dy);
                node.spring_x=node.spring_y=node.repulsion_x=node.repulsion_y=0;
            }
        }
        transform(0,0); // Layout.doPostLayout -> transform, including graph margin.
    }
private:
    Rect bounds()const {
        double left=std::numeric_limits<double>::infinity(),top=left,right=-left,bottom=-left;
        for(const auto& n:nodes){left=std::min(left,n.rect.x);top=std::min(top,n.rect.y);right=std::max(right,n.rect.right());bottom=std::max(bottom,n.rect.bottom());}
        return {left,top,right-left,bottom-top};
    }
    void transform(double x,double y) {
        const auto box=bounds();const double left=box.x-15,top=box.y-15;
        for(auto& n:nodes){n.rect.x=n.rect.x-left+x;n.rect.y=n.rect.y-top+y;}
    }
    void branch(std::size_t index,std::optional<std::size_t> parent,double begin,double end,double distance,double separation) {
        double half=((end-begin)+1)/2;if(half<0)half+=180;
        const double angle=std::fmod(half+begin,360.0)*2*std::numbers::pi/360;
        auto& node=nodes[index];node.rect.x=distance*std::cos(angle)-node.rect.w/2;node.rect.y=distance*std::sin(angle)-node.rect.h/2;
        const auto children=node.neighbors.size()-(parent?1:0);
        const auto start=parent?(std::ranges::find(node.neighbors,*parent)-node.neighbors.begin()+1)%node.neighbors.size():0;
        const double step=std::abs(end-begin)/children;
        std::size_t processed=0;
        for(std::size_t i=start;processed!=children;i=(i+1)%node.neighbors.size()) {
            const auto next=node.neighbors[i];if(parent&&next==*parent)continue;
            const double child_start=std::fmod(begin+processed*step,360.0),child_end=std::fmod(child_start+step,360.0);
            branch(next,index,child_start,child_end,distance+separation,separation);++processed;
        }
    }
    void radial(std::vector<std::size_t> tree) {
        // Preserve layout-base 1.0.2 findCenterOfTree's actual list mutation:
        // its loop removes the current index, then advances over the next item.
        // The degree bookkeeping does not affect the returned list[0].
        while(tree.size()>2)for(std::size_t i=0;i<tree.size();++i)tree.erase(tree.begin()+i);
        double separation=50;for(const auto& n:nodes)separation=std::max(separation,std::sqrt(n.rect.w*n.rect.w+n.rect.h*n.rect.h));
        branch(tree.front(),{},0,359,0,separation);
        auto box=bounds();
        for(auto& n:nodes){n.rect.x-=box.x;n.rect.y-=box.y;}
        transform(1200-box.w/2,900-box.h/2);
    }
    static void spring(Node& a,Node& b) {
        if(a.rect.intersects(b.rect))return;
        const auto target=clip(b.rect,a.rect),source=clip(a.rect,b.rect);
        double dx=target.x-source.x,dy=target.y-source.y;
        if(std::abs(dx)<1)dx=sign(dx);if(std::abs(dy)<1)dy=sign(dy);
        const double length=std::sqrt(dx*dx+dy*dy);if(length==0)return;
        const double force=.45*(length-50),fx=force*(dx/length),fy=force*(dy/length);
        a.spring_x+=fx;a.spring_y+=fy;b.spring_x-=fx;b.spring_y-=fy;
    }
    static void repulsion(Node& a,Node& b) {
        double fx,fy;
        if(a.rect.intersects(b.rect)){const auto overlap=separation(a.rect,b.rect);fx=overlap.x;fy=overlap.y;}
        else {
            const auto start=clip(a.rect,b.rect),end=clip(b.rect,a.rect);
            double dx=end.x-start.x,dy=end.y-start.y;
            if(std::abs(dx)<5)dx=sign(dx)*5;if(std::abs(dy)<5)dy=sign(dy)*5;
            const double squared=dx*dx+dy*dy,distance=std::sqrt(squared),force=4500/squared;
            fx=force*dx/distance;fy=force*dy/distance;
        }
        a.repulsion_x-=fx;a.repulsion_y-=fy;b.repulsion_x+=fx;b.repulsion_y+=fy;
    }
    void update_grid() {
        const auto box=bounds();const double left=box.x-15,top=box.y-15;
        const int width=static_cast<int>(std::ceil((box.w+30)/100)),height=static_cast<int>(std::ceil((box.h+30)/100));
        std::vector<std::vector<std::size_t>> cells(static_cast<std::size_t>(width)*height);
        for(std::size_t i=0;i<nodes.size();++i) {
            auto& n=nodes[i];n.start_x=static_cast<int>(std::floor((n.rect.x-left)/100));n.finish_x=static_cast<int>(std::floor((n.rect.right()-left)/100));
            n.start_y=static_cast<int>(std::floor((n.rect.y-top)/100));n.finish_y=static_cast<int>(std::floor((n.rect.bottom()-top)/100));
            for(int x=n.start_x;x<=n.finish_x;++x)for(int y=n.start_y;y<=n.finish_y;++y)cells[x*height+y].push_back(i);
        }
        for(std::size_t i=0;i<nodes.size();++i) {
            auto& n=nodes[i];n.surrounding.clear();std::vector<bool> seen(nodes.size());
            for(int x=n.start_x-1;x<n.finish_x+2;++x)for(int y=n.start_y-1;y<n.finish_y+2;++y) {
                if(x<0||y<0||x>=width||y>=height)continue;
                for(const auto j:cells[x*height+y]) {
                    if(j<=i||seen[j])continue;const auto& b=nodes[j].rect;
                    const double dx=std::abs(n.rect.cx()-b.cx())-(n.rect.w/2+b.w/2),dy=std::abs(n.rect.cy()-b.cy())-(n.rect.h/2+b.h/2);
                    if(dx<=100&&dy<=100){seen[j]=true;n.surrounding.push_back(j);}
                }
            }
        }
    }
};
}

std::expected<CoseTreeLayout,std::string> layout_cose_tree(std::span<const CoseTreeNode> input,std::span<const CoseTreeEdge> edges) {
    if(input.empty()||edges.size()+1!=input.size())return std::unexpected("CoSE Mindmap layout requires one tree");
    Layout layout;std::map<std::u16string,std::size_t> ids;
    for(const auto& n:input) {
        if(!std::isfinite(n.width)||!std::isfinite(n.height)||n.width<=0||n.height<=0)return std::unexpected("Invalid CoSE measured box");
        if(!ids.emplace(n.id,layout.nodes.size()).second)return std::unexpected("Duplicate CoSE node id");
        Node node;node.rect={-n.width/2,-n.height/2,n.width,n.height};layout.nodes.push_back(std::move(node));
    }
    for(const auto& e:edges) {
        const auto a=ids.find(e.start),b=ids.find(e.end);
        if(a==ids.end()||b==ids.end()||a==b)return std::unexpected("Invalid CoSE tree edge");
        layout.edges.emplace_back(a->second,b->second);
        layout.nodes[a->second].neighbors.push_back(b->second);layout.nodes[b->second].neighbors.push_back(a->second);
    }
    std::vector<std::size_t> tree{0};std::vector<bool> seen(input.size());seen[0]=true;
    for(std::size_t i=0;i<tree.size();++i)for(const auto neighbor:layout.nodes[tree[i]].neighbors)if(!seen[neighbor]){seen[neighbor]=true;tree.push_back(neighbor);}
    if(tree.size()!=input.size())return std::unexpected("CoSE Mindmap layout requires a connected tree");
    layout.run(tree);CoseTreeLayout result;result.iterations=layout.iterations;
    for(std::size_t i=0;i<input.size();++i)result.nodes.push_back({input[i].id,layout.nodes[i].rect.cx(),layout.nodes[i].rect.cy()});
    return result;
}
}
