module folia.mermaid.architecture;
import std;

namespace folia::mermaid {
namespace {
using Text=std::u16string;using View=std::u16string_view;
[[noreturn]] void fail(const char* message){throw std::runtime_error(message);}
bool space(char16_t c){return c==u' '||c==u'\t'||c==u'\r';}
Text trim(View value){while(!value.empty()&&(space(value.front())||value.front()==u'\n'))value.remove_prefix(1);while(!value.empty()&&(space(value.back())||value.back()==u'\n'))value.remove_suffix(1);return Text(value);}
bool word(char16_t c){return c==u'_'||(c>=u'a'&&c<=u'z')||(c>=u'A'&&c<=u'Z')||(c>=u'0'&&c<=u'9');}
bool horizontal(char16_t c){return c==u'L'||c==u'R';}
template<class T> void ordered_set(std::vector<std::pair<Text,T>>& entries,Text key,T value){
    for(auto& [k,v]:entries)if(k==key){v=std::move(value);return;}entries.emplace_back(std::move(key),std::move(value));
}
class Parser {
public:
    Parser(View source,ArchitectureDocument document):source_(source),document_(std::move(document)){}
    ArchitectureDocument run() {
        whitespace();require(u"architecture-beta");end();
        std::vector<ArchitectureNode> services,junctions;
        while(true) {
            whitespace();if(pos_==source_.size())break;
            if(take(u"accTitle")){require(u":");document_.acc_title=trim(line());end();continue;}
            if(take(u"accDescr")){if(take(u":"))document_.acc_description=trim(line());else{require(u"{");const auto stop=source_.find(u'}',pos_);if(stop==View::npos)fail("Unclosed Architecture description");document_.acc_description=trim(source_.substr(pos_,stop-pos_));pos_=stop+1;}end();continue;}
            if(take(u"title")){document_.title=trim(line());end();continue;}
            if(take(u"group")) {
                ArchitectureGroup group;group.id=id();if(take(u"(")){group.icon=icon();}if(at(u"["))group.title=title();if(take(u"in"))group.parent=id();end();document_.groups.push_back(std::move(group));continue;
            }
            if(take(u"service")) {
                ArchitectureNode node;node.type=u"service";node.id=id();if(take(u"("))node.icon=icon();else if(at(u"\"")||at(u"'"))node.icon_text=quoted();
                if(at(u"["))node.title=title();if(take(u"in"))node.parent=id();end();services.push_back(std::move(node));continue;
            }
            if(take(u"junction")) {
                ArchitectureNode node;node.type=u"junction";node.id=id();if(take(u"in"))node.parent=id();end();junctions.push_back(std::move(node));continue;
            }
            if(take(u"align")) {
                ArchitectureHint hint;hint.direction=id();if(hint.direction!=u"row"&&hint.direction!=u"column")fail("Invalid Architecture alignment");
                while(pos_<source_.size()&&source_[pos_]!=u'\n'&&!at(u"%%"))hint.members.push_back(id());
                end();document_.hints.push_back(std::move(hint));continue;
            }
            ArchitectureEdge edge;edge.lhs_id=id();edge.lhs_group=take(u"{group}");require(u":");edge.lhs_dir=direction();edge.lhs_into=take(u"<")||take(u">");
            if(!take(u"--")){require(u"-");edge.title=title();require(u"-");}
            edge.rhs_into=take(u"<")||take(u">");edge.rhs_dir=direction();require(u":");edge.rhs_id=id();edge.rhs_group=take(u"{group}");end();document_.edges.push_back(std::move(edge));
        }
        // populateDb batches groups, then services, then junctions, then edges;
        // declaration order between those categories is not registration order.
        document_.nodes=std::move(services);document_.nodes.insert(document_.nodes.end(),std::make_move_iterator(junctions.begin()),std::make_move_iterator(junctions.end()));
        populate();return std::move(document_);
    }
private:
    void spaces(){while(pos_<source_.size()&&space(source_[pos_]))++pos_;}
    bool at(View value){spaces();return source_.substr(pos_).starts_with(value);}
    bool take(View value){if(!at(value))return false;const auto next=pos_+value.size();if(word(value.back())&&next<source_.size()&&(word(source_[next])||source_[next]==u'-'))return false;pos_=next;spaces();return true;}
    void require(View value){if(!take(value))fail("Unexpected Architecture token");}
    Text line(){spaces();const auto start=pos_;while(pos_<source_.size()&&source_[pos_]!=u'\n'&&!source_.substr(pos_).starts_with(u"%%"))++pos_;return Text(source_.substr(start,pos_-start));}
    void end(){spaces();if(at(u"%%"))while(pos_<source_.size()&&source_[pos_]!=u'\n')++pos_;if(pos_<source_.size()&&source_[pos_]!=u'\n')fail("Unexpected Architecture statement suffix");if(pos_<source_.size())++pos_;}
    void whitespace(){for(;;){spaces();if(pos_<source_.size()&&source_[pos_]==u'\n'){++pos_;continue;}if(at(u"%%")){end();continue;}break;}}
    Text id(){spaces();const auto start=pos_;if(pos_==source_.size()||!word(source_[pos_]))fail("Expected Architecture identifier");while(pos_<source_.size()&&(word(source_[pos_])||source_[pos_]==u'-'))++pos_;if(source_[pos_-1]==u'-')fail("Invalid Architecture identifier");auto value=Text(source_.substr(start,pos_-start));spaces();return value;}
    char16_t direction(){spaces();if(pos_==source_.size()||View{u"LRTB"}.find(source_[pos_])==View::npos)fail("Invalid Architecture port direction");const auto value=source_[pos_++];spaces();return value;}
    Text icon(){const auto start=pos_;while(pos_<source_.size()&&(word(source_[pos_])||source_[pos_]==u'-'||source_[pos_]==u':'))++pos_;if(start==pos_)fail("Empty Architecture icon");Text value(source_.substr(start,pos_-start));require(u")");return value;}
    Text quoted(){spaces();const auto quote=source_[pos_++];Text value;while(pos_<source_.size()&&source_[pos_]!=quote){if(source_[pos_]==u'\\'&&pos_+1<source_.size())++pos_;value+=source_[pos_++];}if(pos_==source_.size())fail("Unclosed Architecture string");++pos_;spaces();return value;}
    Text title(){require(u"[");Text value;if(at(u"\"")||at(u"'"))value=quoted();else{const auto start=pos_;while(pos_<source_.size()&&source_[pos_]!=u']'&&source_[pos_]!=u'\n')++pos_;value=source_.substr(start,pos_-start);}require(u"]");return trim(value);}
    void populate() {
        std::map<Text,bool> registered;std::map<Text,std::size_t> indices;
        const auto register_id=[&](const Text& id,const Text& parent,bool group){if(registered.contains(id))fail("Duplicate Architecture identifier");if(!parent.empty()&&(!registered.contains(parent)||!registered.at(parent)||parent==id))fail("Invalid Architecture parent group");registered.emplace(id,group);};
        for(const auto& group:document_.groups)register_id(group.id,group.parent,true);
        for(std::size_t i=0;i<document_.nodes.size();++i){const auto& node=document_.nodes[i];register_id(node.id,node.parent,false);indices.emplace(node.id,i);}
        for(std::size_t i=0;i<document_.edges.size();++i) {
            const auto& e=document_.edges[i];if(!indices.contains(e.lhs_id)||!indices.contains(e.rhs_id))fail("Architecture edge endpoint is not a service or junction");
            auto& a=document_.nodes[indices.at(e.lhs_id)];auto& b=document_.nodes[indices.at(e.rhs_id)];
            if((e.lhs_group||e.rhs_group)&&!a.parent.empty()&&a.parent==b.parent)fail("Architecture group endpoint must cross groups");
            a.edges.push_back(i);b.edges.push_back(i);
        }
        for(const auto& hint:document_.hints){if(hint.members.size()<2)fail("Architecture alignment needs two nodes");std::set<Text> seen;for(const auto& member:hint.members)if(!indices.contains(member)||!seen.insert(member).second)fail("Invalid Architecture alignment member");}
        for(const auto& node:document_.nodes) {
            std::vector<std::pair<Text,Text>> adjacency;
            for(const auto index:node.edges) {
                const auto& e=document_.edges[index];const auto& a=document_.nodes[indices.at(e.lhs_id)];const auto& b=document_.nodes[indices.at(e.rhs_id)];
                if(!a.parent.empty()&&!b.parent.empty()&&a.parent!=b.parent&&horizontal(e.lhs_dir)==horizontal(e.rhs_dir)) {
                    const auto& low=std::min(a.parent,b.parent);const auto& high=std::max(a.parent,b.parent);
                    ordered_set(document_.group_alignments,u"\""+low+u"\"-\""+high+u"\"",Text(horizontal(e.lhs_dir)?u"horizontal":u"vertical"));
                }
                if(e.lhs_dir==e.rhs_dir)continue;
                if(e.lhs_id==node.id)ordered_set(adjacency,Text{e.lhs_dir,e.rhs_dir},e.rhs_id);
                else ordered_set(adjacency,Text{e.rhs_dir,e.lhs_dir},e.lhs_id);
            }
            document_.adjacency.emplace_back(node.id,std::move(adjacency));
        }
        std::set<Text> visited;
        for(const auto& node:document_.nodes) {
            if(visited.contains(node.id))continue;
            std::vector<ArchitectureSpatialNode> spatial{{node.id,0,0}};std::vector<Text> queue{node.id};
            for(std::size_t q=0;q<queue.size();++q) {
                const auto id=queue[q];visited.insert(id);
                const auto position=*std::ranges::find(spatial,id,&ArchitectureSpatialNode::id);
                for(const auto& [pair,other]:document_.adjacency[indices.at(id)].second)if(!visited.contains(other)) {
                    int x=position.x,y=position.y;const auto a=pair[0],b=pair[1];
                    if(horizontal(a)){x+=a==u'L'?-1:1;if(!horizontal(b))y+=b==u'T'?1:-1;}
                    else {y+=a==u'T'?1:-1;if(horizontal(b))x+=b==u'L'?1:-1;}
                    if(auto old=std::ranges::find(spatial,other,&ArchitectureSpatialNode::id);old!=spatial.end()){old->x=x;old->y=y;}else spatial.push_back({other,x,y});
                    queue.push_back(other);
                }
            }
            document_.spatial_maps.push_back(std::move(spatial));
        }
    }
    View source_;std::size_t pos_=0;ArchitectureDocument document_;
};
}
std::expected<ArchitectureDocument,std::string> prepare_architecture(View source,const ConfigSession& session,FrontendLimits limits) {
    auto prepared=prepare_diagram(source,session,limits);if(!prepared)return std::unexpected(prepared.error());
    if(prepared->type!=u"architecture")return std::unexpected("Expected Architecture diagram");
    ArchitectureDocument document;document.title=prepared->title.value_or(u"");document.effective_config=std::move(prepared->effective_config);
    if(const auto* config=document.effective_config.find(u"architecture"))document.config=*config;
    try{return Parser{prepared->code.cleaned,std::move(document)}.run();}catch(const std::exception& e){return std::unexpected(e.what());}
}
}
