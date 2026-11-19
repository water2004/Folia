module folia.mermaid.block;
import std;
namespace folia::mermaid {
namespace {
std::u16string trim(std::u16string_view s){auto a=s.find_first_not_of(u" \t\r\n");return a==s.npos?u"":std::u16string(s.substr(a,s.find_last_not_of(u" \t\r\n")-a+1));}
std::vector<std::u16string> split(std::u16string_view s){std::vector<std::u16string> result;for(std::size_t p=0;p<=s.size();){auto e=s.find(u',',p);if(e==s.npos)e=s.size();result.push_back(trim(s.substr(p,e-p)));p=e+1;}return result;}
struct Parser {
 std::u16string_view source;std::size_t pos=0,anonymous=0;BlockDocument db;
 bool at(std::u16string_view s)const{return source.substr(pos,s.size())==s;}
 void skip(){while(pos<source.size()&&(source[pos]==u' '||source[pos]==u'\t'||source[pos]==u'\r'||source[pos]==u'\n'))++pos;}
 std::u16string word(){auto start=pos;while(pos<source.size()&&std::u16string_view(u" \t\r\n[](){}<>:-=,").find(source[pos])==std::u16string_view::npos)++pos;if(start==pos)throw std::runtime_error("Expected Block identifier");return std::u16string(source.substr(start,pos-start));}
 std::u16string rest(){auto end=source.find(u'\n',pos);if(end==source.npos)end=source.size();auto s=trim(source.substr(pos,end-pos));pos=end;return s;}
 int integer(){auto begin=pos;while(pos<source.size()&&source[pos]>=u'0'&&source[pos]<=u'9')++pos;std::string digits(source.begin()+begin,source.begin()+pos);int n=0;auto r=std::from_chars(digits.data(),digits.data()+digits.size(),n);if(r.ec!=std::errc{}||n<=0)throw std::runtime_error("Expected positive Block count");return n;}
 std::size_t find(std::u16string_view id){for(std::size_t i=0;i<db.nodes.size();++i)if(db.nodes[i].id==id)return i;return db.nodes.size();}
 std::size_t add(BlockNode node,std::size_t parent){auto i=find(node.id);if(i<db.nodes.size()){if(node.type!=u"na")db.nodes[i].type=node.type;if(node.label!=node.id)db.nodes[i].label=node.label;return i;}i=db.nodes.size();db.nodes.push_back(std::move(node));db.nodes[parent].children.push_back(i);return i;}
 struct LinkToken {std::u16string_view text;bool complete;};
 std::optional<LinkToken> link_token(bool allow_start) {
     // Match the complete Block lexer token, not all adjacent arrow-like
     // characters: in A-->x the final x belongs to the next node identifier.
     std::string candidate;
     for(auto i=pos;i<source.size()&&std::u16string_view(u"<>=-.~xo").find(source[i])!=source.npos;++i)
         candidate+=static_cast<char>(source[i]);
     // In the initial lexer state NODE_ID precedes LINK. Thus `B x--x C`
     // starts a separate node x, while the closing LLABEL token can carry an
     // x/o start marker. Preserve that upstream ordering instead of guessing.
     if(allow_start&&!candidate.empty()&&std::string_view{"xo.~"}.find(candidate.front())!=std::string_view::npos)
         return {};
     static const std::regex complete(R"((?:[xo<]?(?:--+[-xo>]|==+[=xo>]|-?\.+-[xo>]?)|~{3,}))");
     static const std::regex start(R"([xo<]?(?:--|==|-\.))");
     std::smatch match;
     bool full=std::regex_search(candidate,match,complete,std::regex_constants::match_continuous);
     if(!full&&(!allow_start||!std::regex_search(candidate,match,start,std::regex_constants::match_continuous)))
         return {};
     const auto length=static_cast<std::size_t>(match.length());
     const auto text=source.substr(pos,length);pos+=length;
     return LinkToken{text,full};
 }
 BlockNode node(){BlockNode n;n.id=word();n.label=n.id;
  // Longest delimiters must win before their square/round prefixes. These
  // names are BlockDB.typeStr2Type values, not Flowchart aliases.
  static constexpr std::tuple<std::u16string_view,std::u16string_view,std::u16string_view> shapes[]={
      {u"<[",u"]>",u"block_arrow"},{u"(((",u")))",u"doublecircle"},
      {u"((",u"))",u"circle"},{u"{{",u"}}",u"hexagon"},
      {u"([",u"])",u"stadium"},{u"[[",u"]]",u"subroutine"},
      {u"[(",u")]",u"cylinder"},{u"[/",u"\\]",u"trapezoid"},
      {u"[\\",u"/]",u"inv_trapezoid"},{u"[/",u"/]",u"lean_right"},
      {u"[\\",u"\\]",u"lean_left"},{u">",u"]",u"rect_left_inv_arrow"},
      {u"[",u"]",u"square"},{u"(",u")",u"round"},{u"{",u"}",u"diamond"}};
  std::size_t opening=0,closing=source.npos;
  std::u16string_view delimiter,type;
  bool quoted=false;
  for(const auto& [open,close,shape]:shapes) {
      if(!at(open)||open.size()<opening) continue;
      const auto start=pos+open.size();
      const bool is_quoted=start<source.size()&&source[start]==u'"';
      const auto end=source.find(is_quoted?u"\"":close,start+(is_quoted?1:0));
      if(end==source.npos || (is_quoted && source.substr(end+1,close.size())!=close)) continue;
      if(open.size()>opening || end<closing) {
          opening=open.size();closing=end;delimiter=close;type=shape;quoted=is_quoted;
      }
  }
  if(opening) {
      const auto start=pos+opening+(quoted?1:0);
      n.label=source.substr(start,closing-start);n.type=type;
      pos=closing+(quoted?1:0)+delimiter.size();
      if(type==u"block_arrow") {
          skip();if(!at(u"("))throw std::runtime_error("Expected Block arrow direction");
          const auto end=source.find(u')',++pos);
          if(end==source.npos)throw std::runtime_error("Unclosed Block arrow direction");
          n.directions=split(source.substr(pos,end-pos));pos=end+1;
      }
  } else if(pos<source.size()&&std::u16string_view{u"[({><"}.find(source[pos])!=std::u16string_view::npos&&
      !at(u"<-")&&!at(u"<=")&&!at(u"<.")) {
      throw std::runtime_error("Unclosed Block shape");
  }
  if(at(u":")){++pos;n.span=integer();}return n;
 }
 void style(std::u16string_view command){skip();auto ids=word();while(at(u",")){++pos;skip();ids+=u',';ids+=word();}skip();auto value=rest();if(command==u"classDef"){BlockClass c;c.id=ids;c.styles=split(value);for(auto s:c.styles)if(auto p=s.find(u"color");p!=s.npos){s.replace(p,5,u"fill");c.text_styles.push_back(s);}db.classes.push_back(std::move(c));}else for(const auto& id:split(ids)){auto i=find(id);if(i==db.nodes.size())throw std::runtime_error("Unknown Block style target");if(command==u"class")db.nodes[i].classes.push_back(value);else db.nodes[i].styles=split(value);}}
 void body(std::size_t parent,bool nested){while(true){skip();if(pos==source.size()){if(nested)throw std::runtime_error("Unclosed Block composite");return;}if(at(u"end")&&(pos+3==source.size()||std::u16string_view(u" \t\r\n").find(source[pos+3])!=std::u16string_view::npos)){pos+=3;if(!nested)throw std::runtime_error("Unexpected Block end");return;}
   if(at(u"columns ")){pos+=8;skip();if(at(u"auto")){pos+=4;db.nodes[parent].columns=-1;}else db.nodes[parent].columns=integer();continue;}
   bool styled=false;for(auto command:{u"classDef",u"class",u"style"})if(at(command)&&source.substr(pos+std::u16string_view(command).size(),1)==u" "){pos+=std::u16string_view(command).size();style(command);styled=true;break;}if(styled)continue;
   if(at(u"space")&&(pos+5==source.size()||std::u16string_view(u": \t\r\n").find(source[pos+5])!=std::u16string_view::npos)){pos+=5;int count=1;if(at(u":")){++pos;count=integer();}for(int j=0;j<count;++j){auto id=std::to_string(anonymous++);BlockNode n;n.id=u"space-"+std::u16string(id.begin(),id.end());n.type=u"space";add(std::move(n),parent);}continue;}
   if(at(u"block:")){pos+=6;auto n=node();n.type=u"composite";if(n.label==n.id)n.label.clear();auto i=add(std::move(n),parent);body(i,true);continue;}
   auto first=node();auto start=first.id;add(std::move(first),parent);skip();
   while(auto token=link_token(true)) {
       BlockEdge edge;edge.start=start;auto link=token->text;skip();
       if(!token->complete) {
           if(!at(u"\"")) throw std::runtime_error("Expected quoted Block edge label");
           const auto end=source.find(u'"',++pos);
           if(end==source.npos) throw std::runtime_error("Unclosed Block edge label");
           edge.label=source.substr(pos,end-pos);pos=end+1;skip();
           const auto closing=link_token(false);
           if(!closing) throw std::runtime_error("Expected complete Block edge after label");
           // The grammar takes arrow data from the closing LINK token.
           link=closing->text;skip();
       }
       auto target=node();edge.end=target.id;add(std::move(target),parent);
       edge.thickness=link.find(u"==")!=link.npos?u"thick":u"normal";
       edge.pattern=link.find(u".-")!=link.npos?u"dotted":u"solid";
       if(link.ends_with(u">")) edge.arrow_end=u"arrow_point";
       else if(link.ends_with(u"x")) edge.arrow_end=u"arrow_cross";
       else if(link.ends_with(u"o")) edge.arrow_end=u"arrow_circle";
       if(link.starts_with(u"<")) edge.arrow_start=u"arrow_point";
       else if(link.starts_with(u"x")) edge.arrow_start=u"arrow_cross";
       else if(link.starts_with(u"o")) edge.arrow_start=u"arrow_circle";
       const auto base=edge.start+u"-"+edge.end;
       std::size_t count=1;
       for(const auto& existing:db.edges) if(existing.start==edge.start&&existing.end==edge.end) ++count;
       const auto ordinal=std::to_string(count);
       edge.id=std::u16string(ordinal.begin(),ordinal.end())+u"-"+base;
       start=edge.end;db.edges.push_back(std::move(edge));skip();
   }
  }}
};
}
std::expected<BlockDocument,std::string> parse_block(std::u16string_view source){try{Parser p{source};p.skip();if(p.at(u"block-beta"))p.pos+=10;else if(p.at(u"block"))p.pos+=5;else throw std::runtime_error("Expected Block header");BlockNode root;root.id=u"root";root.type=u"composite";p.db.nodes.push_back(std::move(root));p.body(0,false);return std::move(p.db);}catch(const std::runtime_error& e){return std::unexpected(e.what());}}
}
