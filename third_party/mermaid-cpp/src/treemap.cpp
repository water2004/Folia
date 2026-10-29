module folia.mermaid.treemap;
import std;
namespace folia::mermaid {
namespace {
bool ws(char16_t c){return c==u' '||c==u'\t'||c==u'\r'||c==u'\n';}
std::u16string trim(std::u16string_view s){while(!s.empty()&&ws(s.front()))s.remove_prefix(1);while(!s.empty()&&ws(s.back()))s.remove_suffix(1);return std::u16string(s);}
struct Row{int indent;TreemapNode node;};
void tile(std::vector<TreemapNode>& nodes,TreemapNode& parent,double x0,double y0,double x1,double y1){
    const auto& children=parent.children;std::size_t i0=0,i1=0;double value=parent.value;const auto ratio=(1+std::sqrt(5.0))/2;
    while(i0<children.size()){auto dx=x1-x0,dy=y1-y0;double sum;do{sum=nodes[children[i1++]].value;}while(!sum&&i1<children.size());auto min=sum,max=sum,alpha=std::max(dy/dx,dx/dy)/(value*ratio),beta=sum*sum*alpha,best=std::max(max/beta,beta/min);
        for(;i1<children.size();++i1){auto next=nodes[children[i1]].value;sum+=next;min=std::min(min,next);max=std::max(max,next);beta=sum*sum*alpha;auto now=std::max(max/beta,beta/min);if(now>best){sum-=next;break;}best=now;}
        if(dx<dy){auto bottom=value?y0+dy*sum/value:y1,scale=sum?(x1-x0)/sum:0,position=x0;for(auto i=i0;i<i1;++i){auto& n=nodes[children[i]];n.x0=position;n.x1=position+=n.value*scale;n.y0=y0;n.y1=bottom;}y0=bottom;}
        else{auto right=value?x0+dx*sum/value:x1,scale=sum?(y1-y0)/sum:0,position=y0;for(auto i=i0;i<i1;++i){auto& n=nodes[children[i]];n.x0=x0;n.x1=right;n.y0=position;n.y1=position+=n.value*scale;}x0=right;}value-=sum;i0=i1;
    }
}
}
std::expected<TreemapDocument,std::string> parse_treemap(std::u16string_view source){try{
    TreemapDocument d;std::vector<Row> rows;std::map<std::u16string,std::vector<std::u16string>> classes;bool header=false;
    std::size_t pos=0;while(pos<source.size()){auto end=source.find(u'\n',pos);if(end==source.npos)end=source.size();auto line=source.substr(pos,end-pos);pos=end+1;int indent=0;while(indent<line.size()&&(line[indent]==u' '||line[indent]==u'\t'))++indent;auto content=trim(line.substr(indent));if(content.empty()||content.starts_with(u"%%"))continue;if(!header){if(content!=u"treemap-beta"&&content!=u"treemap")throw std::runtime_error("Expected Treemap header");header=true;continue;}
        if(content.starts_with(u"title ")){d.title=trim(content.substr(6));continue;}if(content.starts_with(u"accTitle:")){d.acc_title=trim(content.substr(9));continue;}if(content.starts_with(u"accDescr:")){d.acc_description=trim(content.substr(9));continue;}
        if(content.starts_with(u"classDef ")){auto rest=trim(content.substr(9));auto split=rest.find_first_of(u" \t");if(split==rest.npos)throw std::runtime_error("Incomplete Treemap classDef");auto name=rest.substr(0,split),styles=rest.substr(split+1);std::size_t a=0;for(std::size_t i=0;i<=styles.size();++i)if(i==styles.size()||styles[i]==u';'||(styles[i]==u','&&(i==0||styles[i-1]!=u'\\'))){auto s=trim(std::u16string_view(styles).substr(a,i-a));if(!s.empty())classes[name].push_back(std::move(s));a=i+1;}continue;}
        if(content.front()!=u'"'&&content.front()!=u'\'')throw std::runtime_error("Treemap names must be quoted");auto quote=content.front();auto close=content.find(quote,1);if(close==content.npos)throw std::runtime_error("Incomplete Treemap name");Row row{indent,TreemapNode{.name=content.substr(1,close-1)}};auto rest=trim(content.substr(close+1));auto selector=rest.find(u":::");if(selector!=rest.npos){row.node.class_selector=trim(rest.substr(selector+3));rest=trim(rest.substr(0,selector));}
        if(!rest.empty()){if(rest.front()!=u':'&&rest.front()!=u',')throw std::runtime_error("Unexpected Treemap value separator");rest=trim(rest.substr(1));std::string number;for(auto c:rest){if(c!=u',')number+=char(c);}double value;auto parsed=std::from_chars(number.data(),number.data()+number.size(),value);if(parsed.ec!=std::errc{}||!std::isfinite(value)||value<0)throw std::runtime_error("Invalid Treemap value");row.node.own_value=value;}
        rows.push_back(std::move(row));
    }
    if(!header)throw std::runtime_error("Expected Treemap header");std::vector<std::pair<int,std::size_t>> stack;for(auto& row:rows){while(!stack.empty()&&stack.back().first>=row.indent)stack.pop_back();auto parent=stack.empty()?0:stack.back().second;auto index=d.nodes.size();row.node.parent=parent;row.node.depth=d.nodes[parent].depth+1;if(auto cls=classes.find(row.node.class_selector);cls!=classes.end())row.node.styles=cls->second;bool branch=!row.node.own_value.has_value();d.nodes.push_back(std::move(row.node));d.nodes[parent].children.push_back(index);if(branch)stack.push_back({row.indent,index});}return d;
}catch(const std::runtime_error& e){return std::unexpected(e.what());}}
TreemapDocument layout_treemap(TreemapDocument d,const TreemapConfig& c){auto& nodes=d.nodes;for(auto i=nodes.size();i-->0;){auto& n=nodes[i];n.value=n.own_value.value_or(0);for(auto child:n.children)n.value+=nodes[child].value;std::stable_sort(n.children.begin(),n.children.end(),[&](auto a,auto b){return nodes[a].value>nodes[b].value;});}nodes[0].x1=c.width;nodes[0].y1=c.height;std::vector<std::size_t> stack{0};while(!stack.empty()){auto index=stack.back();stack.pop_back();auto& n=nodes[index];auto p=index?c.padding/2:0;n.x0+=p;n.y0+=p;n.x1-=p;n.y1-=p;if(n.x1<n.x0)n.x0=n.x1=(n.x0+n.x1)/2;if(n.y1<n.y0)n.y0=n.y1=(n.y0+n.y1)/2;if(!n.children.empty()){p=c.padding/2;auto x0=n.x0+10-p,y0=n.y0+35-p,x1=n.x1-10+p,y1=n.y1-10+p;if(x1<x0)x0=x1=(x0+x1)/2;if(y1<y0)y0=y1=(y0+y1)/2;tile(nodes,n,x0,y0,x1,y1);for(auto i=n.children.rbegin();i!=n.children.rend();++i)stack.push_back(*i);}}for(auto& n:nodes){n.x0=std::floor(n.x0+.5);n.y0=std::floor(n.y0+.5);n.x1=std::floor(n.x1+.5);n.y1=std::floor(n.y1+.5);}return d;}
}
