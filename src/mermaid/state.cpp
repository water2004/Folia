module folia.mermaid.state;
import std;
namespace folia::mermaid {
namespace {
std::u16string trim(std::u16string_view s){auto a=s.find_first_not_of(u" \t\r\n");return a==s.npos?u"":std::u16string(s.substr(a,s.find_last_not_of(u" \t\r\n")-a+1));}
struct Parser {
 FlowchartInput document;std::vector<std::u16string> parents;std::vector<StateAnnotation> annotations;std::size_t divider_count=0;std::map<std::u16string,std::size_t,std::less<>> region_start,region_count;std::map<std::u16string,std::u16string,std::less<>> region_direction,region_scope;
 std::size_t node(std::u16string id,bool start=true){const auto scope=parents.empty()?std::u16string{}:region_scope[parents.back()];if(id==u"[*]")id=(scope.empty()?u"root":scope)+std::u16string(start?u"_start":u"_end");if(id.empty()||id.find_first_of(u" \t\n")!=id.npos)throw std::runtime_error("Expected State identifier");auto it=std::ranges::find(document.nodes,id,&FlowchartNode::id);if(it!=document.nodes.end())return static_cast<std::size_t>(it-document.nodes.begin());FlowchartNode n;n.id=id;n.label=id;n.shape=u"rect";n.padding=8;n.label_type=u"markdown";n.parent=scope;document.nodes.push_back(std::move(n));return document.nodes.size()-1;}
 std::size_t endpoint(std::u16string id,bool start){const bool terminal=id==u"[*]";auto i=node(std::move(id),start);if(terminal)document.nodes[i].shape=start?u"stateStart":u"stateEnd";return i;}
 void region(std::u16string_view parent){
  const std::u16string key(parent);const auto scope=region_scope[key];
  const auto begin=region_start[key];const auto number=std::to_string(++divider_count);
  const std::u16string id=u"divider-id-"+std::u16string(number.begin(),number.end());
  FlowchartNode divider;divider.id=id;divider.shape=u"divider";divider.group=true;divider.padding=8;
  divider.direction=region_direction.contains(key)?region_direction[key]:u"TB";divider.parent=key;
  std::map<std::u16string,std::u16string,std::less<>> renamed_terminals;
  for(std::size_t i=begin;i<document.nodes.size();++i){
   auto& child=document.nodes[i];if(child.parent!=scope)continue;child.parent=id;
   if(child.shape==u"stateStart"||child.shape==u"stateEnd"){
    auto replacement=id+(child.shape==u"stateStart"?u"_start":u"_end");
    renamed_terminals.emplace(child.id,replacement);child.id=replacement;child.label=std::move(replacement);
   }
  }
  for(auto& edge:document.edges){if(auto found=renamed_terminals.find(edge.start);found!=renamed_terminals.end())edge.start=found->second;if(auto found=renamed_terminals.find(edge.end);found!=renamed_terminals.end())edge.end=found->second;}
  document.nodes.push_back(std::move(divider));region_start[key]=document.nodes.size();
  ++region_count[key];region_direction[key]=u"TB";
 }
 void line(std::u16string value,bool trailing_separator=false){value=trim(value);if(value.empty()||value.starts_with(u"%%"))return;if(value==u"}"){if(parents.empty())throw std::runtime_error("Unexpected State composite end");const auto parent=parents.back();if(region_count[parent]&&region_start[parent]<document.nodes.size())region(parent);if(region_count[parent])document.nodes[node(parent)].direction=u"TB";parents.pop_back();return;}
  if(value==u"--"){if(parents.empty())throw std::runtime_error("Concurrent region separator requires a composite state");const auto parent=parents.back();if(trailing_separator){FlowchartNode divider;const auto number=std::to_string(++divider_count);divider.id=u"divider-id-"+std::u16string(number.begin(),number.end());divider.shape=u"divider";divider.parent=parent;divider.padding=8;document.nodes.push_back(std::move(divider));return;}region(parent);document.nodes[node(parent)].direction=u"TB";const auto region_number=std::to_string(region_count[parent]+1);region_scope[parent]=parent+u"----region-"+std::u16string(region_number.begin(),region_number.end());return;}
  if(value.starts_with(u"note ")){auto left=value.find(u" left of "),right=value.find(u" right of ");auto marker=left!=value.npos?left:right;if(marker==value.npos)throw std::runtime_error("Unsupported floating State note");const auto width=left!=value.npos?std::u16string_view(u" left of ").size():std::u16string_view(u" right of ").size();auto start=marker+width;auto colon=value.find(u':',start);if(colon==value.npos)throw std::runtime_error("Expected inline State note text");auto state=trim(std::u16string_view(value).substr(start,colon-start));auto text=trim(std::u16string_view(value).substr(colon+1));if(state.empty()||text.empty())throw std::runtime_error("State note requires a state and text");node(state);annotations.push_back({state,text,left!=value.npos?u"left":u"right"});return;}
  if(value.starts_with(u"direction ")){auto dir=trim(std::u16string_view(value).substr(10));if(dir!=u"TB"&&dir!=u"BT"&&dir!=u"LR"&&dir!=u"RL")throw std::runtime_error("Invalid State direction");if(parents.empty())document.direction=dir;else{document.nodes[node(parents.back())].direction=dir;region_direction[parents.back()]=dir;}return;}
  for(auto key:{u"accTitle:",u"accDescr:"})if(value.starts_with(key)){auto label=trim(std::u16string_view(value).substr(std::u16string_view(key).size()));if(key==std::u16string_view(u"accTitle:"))document.acc_title=label;else document.acc_description=label;return;}
  if(value.starts_with(u"style ")){auto space=value.find(u' ',6);if(space==value.npos)throw std::runtime_error("Expected State style");auto ids=value.substr(6,space-6);auto styles=value.substr(space+1);std::vector<std::u16string> declarations;for(std::size_t p=0;p<styles.size();){auto end=styles.find(u',',p);if(end==styles.npos)end=styles.size();declarations.push_back(trim(std::u16string_view(styles).substr(p,end-p)));p=end+1;}for(std::size_t p=0;p<ids.size();){auto end=ids.find(u',',p);if(end==ids.npos)end=ids.size();document.nodes[node(trim(std::u16string_view(ids).substr(p,end-p)))].styles=declarations;p=end+1;}return;}
  auto arrow=value.find(u"-->");if(arrow!=value.npos){auto left=trim(std::u16string_view(value).substr(0,arrow));auto rest=std::u16string_view(value).substr(arrow+3);auto colon=rest.find(u':');auto right=trim(rest.substr(0,colon));auto a=endpoint(left,true),b=endpoint(right,false);FlowchartEdge edge;auto id=std::to_string(document.edges.size());edge.id=u"edge"+std::u16string(id.begin(),id.end());edge.start=document.nodes[a].id;edge.end=document.nodes[b].id;edge.label=colon==rest.npos?u"":trim(rest.substr(colon+1));edge.label_type=u"markdown";edge.arrow_end=u"arrow_barb";edge.type=u"arrow_barb";edge.curve=u"basis";document.edges.push_back(std::move(edge));return;}
  bool group=value.ends_with(u"{");if(group)value=trim(std::u16string_view(value).substr(0,value.size()-1));if(value.starts_with(u"state "))value=trim(std::u16string_view(value).substr(6));std::u16string id,label;
  if(value.starts_with(u"\"")){auto end=value.find(u'"',1);if(end==value.npos)throw std::runtime_error("Unclosed State label");label=value.substr(1,end-1);auto rest=trim(std::u16string_view(value).substr(end+1));if(!rest.starts_with(u"as "))throw std::runtime_error("Expected State alias");id=trim(std::u16string_view(rest).substr(3));}else{auto colon=value.find(u':');id=trim(std::u16string_view(value).substr(0,colon));if(colon!=value.npos)label=trim(std::u16string_view(value).substr(colon+1));}
  auto i=node(id);auto& n=document.nodes[i];if(!label.empty())n.label=label;if(group){n.group=true;n.shape=u"roundedWithTitle";n.direction=u"TB";parents.push_back(id);region_start[id]=document.nodes.size();region_count[id]=0;region_scope[id]=id;}
 }
 void parse(std::u16string_view source){
  const auto header_end=source.find(u'\n');
  const auto header=trim(source.substr(0,header_end));
  if(header!=u"stateDiagram-v2"&&header!=u"stateDiagram")
   throw std::runtime_error("Expected State diagram header");
  if(header_end==source.npos)return;

  std::vector<std::u16string> lines;
  auto remaining=source.substr(header_end+1);
  while(!remaining.empty()){
   const auto end=remaining.find(u'\n');
   lines.emplace_back(remaining.substr(0,end));
   if(end==remaining.npos)break;
   remaining.remove_prefix(end+1);
  }
  for(std::size_t i=0;i<lines.size();++i){
   const auto value=trim(lines[i]);
   bool trailing_separator=false;
   if(value==u"--"){
    for(std::size_t next=i+1;next<lines.size();++next){
     const auto following=trim(lines[next]);
     if(following.empty()||following.starts_with(u"%%"))continue;
     trailing_separator=following==u"}";
     break;
    }
   }
   if(value.starts_with(u"note ")&&value.find(u':')==value.npos){
    const auto left=value.find(u" left of ");
    const auto right=value.find(u" right of ");
    const auto marker=left!=value.npos?left:right;
    if(marker==value.npos)throw std::runtime_error("Unsupported floating State note");
    const auto position=left!=value.npos?u"left":u"right";
    const auto keyword=left!=value.npos?std::u16string_view(u" left of "):std::u16string_view(u" right of ");
    const auto state=trim(std::u16string_view(value).substr(marker+keyword.size()));
    std::u16string body;bool closed=false;
    for(++i;i<lines.size();++i){
     const auto content=trim(lines[i]);
     if(content==u"end note"){closed=true;break;}
     if(!body.empty())body+=u'\n';
     body+=content;
    }
    if(!closed)throw std::runtime_error("Unclosed State note");
    if(body.empty())throw std::runtime_error("State note requires text");
    node(state);annotations.push_back({state,std::move(body),position});
    continue;
   }
   std::u16string statement;bool quote=false;
   for(const auto c:lines[i]){
    if(c==u'"')quote=!quote;
    if(!quote&&(c==u';'||c==u'{'||c==u'}')){
     if(c==u'{')statement+=c;
     line(std::move(statement));statement.clear();
     if(c==u'}')line(u"}");
    }else statement+=c;
   }
   line(std::move(statement),trailing_separator);
  }
  if(!parents.empty())throw std::runtime_error("Unclosed State composite");
  std::map<std::u16string,std::vector<std::size_t>,std::less<>> children;
  for(std::size_t i=0;i<document.nodes.size();++i)children[document.nodes[i].parent].push_back(i);
  std::vector<FlowchartNode> ordered;ordered.reserve(document.nodes.size());
  std::function<void(std::size_t)> visit=[&](std::size_t index){
   ordered.push_back(document.nodes[index]);
   const auto found=children.find(document.nodes[index].id);
   if(found!=children.end())for(const auto child:found->second)visit(child);
  };
  if(const auto roots=children.find(u"");roots!=children.end())
   for(const auto root:roots->second)visit(root);
  if(ordered.size()!=document.nodes.size())throw std::runtime_error("Invalid State composite tree");
  document.nodes=std::move(ordered);
 }
};
}
std::expected<StateParseResult,std::string> parse_state_document(std::u16string_view source){try{Parser p;p.parse(source);return StateParseResult{std::move(p.document),std::move(p.annotations)};}catch(const std::runtime_error& e){return std::unexpected(e.what());}}
std::expected<FlowchartInput,std::string> parse_state(std::u16string_view source){auto parsed=parse_state_document(source);if(!parsed)return std::unexpected(parsed.error());return std::move(parsed->document);}
}
