module folia.mermaid.mindmap;
import std;

namespace folia::mermaid {
namespace {
using Text = std::u16string;
using View = std::u16string_view;
Text number(std::size_t value) { const auto s=std::to_string(value); return Text{s.begin(),s.end()}; }
bool space(char16_t c) { return c==u' ' || c==u'\t' || c==u'\r'; }
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }

class Parser {
public:
    Parser(View source, MindmapInput input):source_(source),result_(std::move(input)) {}
    MindmapInput run() {
        skip_empty();
        constexpr View header=u"mindmap";
        for(const auto c:header) {
            if(pos_==source_.size()) fail("Expected Mindmap header");
            const auto actual=source_[pos_++];
            if(actual!=c && actual!=c-u'a'+u'A') fail("Expected Mindmap header");
        }
        while(pos_<source_.size()) {
            if(source_[pos_]==u'\n') {++pos_;continue;}
            int indent=0;while(pos_<source_.size() && space(source_[pos_])) {++indent;++pos_;}
            if(pos_==source_.size())break;
            if(at(u"%%")) {line();continue;}
            if(source_[pos_]==u'\n')continue;
            if(at(u":::")) {
                pos_+=3;if(result_.nodes.empty())fail("Mindmap class requires a node");
                result_.nodes.back().classes+=u" "+line();continue;
            }
            if(at(u"::icon(")) {
                pos_+=7;if(result_.nodes.empty())fail("Mindmap icon requires a node");
                const auto end=source_.find(u')',pos_);if(end==View::npos)fail("Unclosed Mindmap icon");
                result_.nodes.back().icon=source_.substr(pos_,end-pos_);pos_=end+1;stop();continue;
            }
            MindmapNode node;
            const auto begin=pos_;
            while(pos_<source_.size() && source_[pos_]!=u'\n' && View{u"([){}"}.find(source_[pos_])==View::npos)++pos_;
            node.node_id=source_.substr(begin,pos_-begin);
            node.label=node.node_id;
            if(pos_<source_.size() && source_[pos_]!=u'\n') {
                View opening;
                for(const auto candidate:{u"-)",u"(-",u"))",u"((",u"{{",u")",u"(",u"["})
                    if(at(candidate)) {opening=candidate;pos_+=opening.size();break;}
                if(opening.empty())fail("Invalid Mindmap shape");
                const bool markdown=at(u"\"`");const bool quoted=markdown||at(u"\"");
                const auto label_start=pos_+(markdown?2:quoted?1:0);pos_=label_start;
                if(quoted) {
                    const View quote=markdown?u"`\"":u"\"";const auto end=source_.find(quote,pos_);
                    if(end==View::npos)fail("Unclosed Mindmap label");
                    node.label=source_.substr(pos_,end-pos_);pos_=end+quote.size();
                } else {
                    while(pos_<source_.size() && View{u")](}"}.find(source_[pos_])==View::npos)++pos_;
                    node.label=source_.substr(label_start,pos_-label_start);
                }
                View closing;
                for(const auto candidate:{u"))",u"}}",u"(-",u"-)",u"((",u")",u"]",u"("})
                    if(at(candidate)) {closing=candidate;pos_+=closing.size();break;}
                if(closing.empty()||node.label.empty())fail("Unclosed Mindmap shape");
                if(node.node_id.empty())node.node_id=node.label;
                node.type=opening==u"["?2:opening==u"("?(closing==u")"?1:4):opening==u"(("?3:
                    opening==u")"?4:opening==u"))"?5:opening==u"{{"?6:0;
            }
            if(node.node_id.empty())fail("Expected Mindmap node");
            stop();add(std::move(node),indent);
        }
        if(result_.nodes.empty())fail("Expected Mindmap node");
        return std::move(result_);
    }
private:
    bool at(View value) const { return source_.substr(pos_).starts_with(value); }
    Text line() {const auto begin=pos_;while(pos_<source_.size()&&source_[pos_]!=u'\n')++pos_;return Text{source_.substr(begin,pos_-begin)};}
    void stop() {while(pos_<source_.size()&&space(source_[pos_]))++pos_;if(pos_<source_.size()&&source_[pos_]!=u'\n')fail("Unexpected text after Mindmap node");}
    void skip_empty() {while(pos_<source_.size()){if(space(source_[pos_])||source_[pos_]==u'\n'){++pos_;continue;}if(at(u"%%")){line();continue;}break;}}
    void add(MindmapNode node,int indent) {
        const auto index=result_.nodes.size();node.id=number(index);
        if(index==0)base_level_=indent;
        node.level=indent-base_level_;
        node.width=result_.max_node_width;
        node.padding=result_.padding*((node.type==1||node.type==2||node.type==6)?2:1);
        constexpr View shapes[]={u"defaultMindmapNode",u"rounded",u"rect",u"mindmapCircle",u"cloud",u"bang",u"hexagon"};
        node.shape=shapes[node.type];
        if(index==0)node.classes=u"mindmap-node section-root section--1";
        else {
            for(std::size_t i=index;i>0;--i)if(result_.nodes[i-1].level<node.level){node.parent=i-1;break;}
            if(!node.parent)fail("Mindmap requires exactly one root");
            const auto& parent=result_.nodes[*node.parent];
            const auto depth=depths_[*node.parent]+1;
            node.section=result_.color_by==u"depth"?(depth-1)%11:parent.level==0?root_children_++%11:*parent.section;
            node.classes=u"mindmap-node section-"+number(*node.section);
            result_.edges.push_back({u"edge_"+parent.id+u"_"+node.id,parent.id,node.id,
                u"edge section-edge-"+number(*node.section)+u" edge-depth-"+number(parent.level+1),parent.level,node.section});
        }
        depths_.push_back(node.parent?depths_[*node.parent]+1:0);
        result_.nodes.push_back(std::move(node));
    }
    View source_;std::size_t pos_=0;int base_level_=0,root_children_=0;
    MindmapInput result_;std::vector<int> depths_;
};
}

std::expected<MindmapInput,std::string> prepare_mindmap(View source,const ConfigSession& session,FrontendLimits limits) {
    auto prepared=prepare_diagram(source,session,limits);
    if(!prepared)return std::unexpected(prepared.error());
    if(prepared->type!=u"mindmap")return std::unexpected("Expected Mindmap diagram");
    // Follow getUserDefinedConfig after the same per-request reset. Using the
    // merged effective config here would mistake global default ELK for intent.
    auto user_session=session;
    if(auto r=user_session.reset();!r)return std::unexpected(r.error());
    if(auto r=user_session.add_directive(prepared->config);!r)return std::unexpected(r.error());
    auto user=user_session.user_config();if(!user)return std::unexpected(user.error());
    MindmapInput input;input.title=prepared->title.value_or(u"");
    input.effective_config=std::move(prepared->effective_config);
    if(!user->find(u"layout")) {
        ConfigValue value;value.kind=ConfigKind::string;value.string=u"cose-bilkent";
        input.effective_config.mapping[u"layout"]=std::move(value);
    }
    if(const auto* layout=input.effective_config.find(u"layout"))input.layout=layout->string;
    input.color_by=u"branch";
    if(const auto* config=input.effective_config.find(u"mindmap")) {
        if(const auto* p=config->find(u"padding"))input.padding=p->number;
        if(const auto* w=config->find(u"maxNodeWidth"))input.max_node_width=w->number;
        if(const auto* c=config->find(u"colorBy"))input.color_by=c->string;
    }
    try {return Parser{prepared->code.cleaned,std::move(input)}.run();}
    catch(const std::exception& error){return std::unexpected(std::string{error.what()});}
}
}
