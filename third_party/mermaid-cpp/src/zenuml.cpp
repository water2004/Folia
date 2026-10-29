module folia.mermaid.zenuml;
import std;
namespace folia::mermaid {
namespace {
bool space(char16_t c){return c==u' '||c==u'\t'||c==u'\r'||c==u'\n';}
std::u16string trim(std::u16string_view s){while(!s.empty()&&space(s.front()))s.remove_prefix(1);while(!s.empty()&&space(s.back()))s.remove_suffix(1);return std::u16string(s);}
std::u16string compact(std::u16string_view s){std::u16string out;for(auto c:s)if(!space(c))out+=c;return out;}
struct Parser {
 std::u16string_view source;std::size_t pos{};ZenDocument doc;bool starter{};std::string error;
 void ws(){while(pos<source.size()&&(space(source[pos])||source[pos]==u';'))++pos;}
 void participant(std::u16string name){if(name==u"_STARTER_"){starter=true;return;}if(name.empty())return;for(auto& p:doc.participants)if(p.name==name)return;doc.participants.push_back({std::move(name),{}, {}});}
 std::vector<ZenStatement> block(std::u16string owner,std::u16string caller,bool nested){
  std::vector<ZenStatement> result;
  while(true){ws();if(pos==source.size()){if(nested)error="Unclosed ZenUML block";break;}if(source[pos]==u'}'){if(!nested)error="Unexpected ZenUML closing brace";else ++pos;break;}
   ZenStatement s;s.begin=pos;int parens=0;bool quote=false;
   while(pos<source.size()){auto c=source[pos];if(c==u'"')quote=!quote;if(!quote){if(c==u'(')++parens;if(c==u')')--parens;if(!parens&&(c==u'{'||c==u'}'||c==u'\n'||c==u';'))break;}++pos;}
   auto raw=source.substr(s.begin,pos-s.begin);auto t=trim(raw);s.from=owner;s.to=owner;
   if(t.empty()){error="Empty ZenUML statement";break;}
   if(t.starts_with(u"if(")||t.starts_with(u"if (")||t.starts_with(u"while(")||t.starts_with(u"while (")){
    s.kind=t.starts_with(u"if")?ZenKind::alternative:ZenKind::loop;auto first=t.find(u'('),last=t.rfind(u')');if(last==t.npos){error="Missing ZenUML condition terminator";break;}s.condition=t.substr(first+1,last-first-1);
   }else if(t.starts_with(u"return")){s.kind=ZenKind::returned;s.text=trim(std::u16string_view(t).substr(6));s.to=caller;
   }else if(auto arrow=t.find(u"->");arrow!=t.npos){s.kind=ZenKind::async;s.from=trim(std::u16string_view(t).substr(0,arrow));auto colon=t.find(u':',arrow+2);s.to=trim(std::u16string_view(t).substr(arrow+2,colon==t.npos?t.size()-arrow-2:colon-arrow-2));if(colon!=t.npos){s.text=t.substr(colon+1);s.label=trim(s.text);for(std::size_t i=0;i+1<s.text.size();++i)if(s.text[i]==u',')while(i+1<s.text.size()&&space(s.text[i+1]))s.text.erase(i+1,1);}participant(s.from);participant(s.to);
   }else{
    auto eq=t.find(u'=');if(eq!=t.npos){s.assignment=trim(std::u16string_view(t).substr(0,eq));t=trim(std::u16string_view(t).substr(eq+1));}
    if(t.starts_with(u"new ")){s.kind=ZenKind::creation;t=trim(std::u16string_view(t).substr(4));auto p=t.find(u'(');s.to=trim(std::u16string_view(t).substr(0,p));s.text=u"\u00ab"+(p==t.npos?std::u16string(u"create"):compact(std::u16string_view(t).substr(p+1,t.size()-p-2)))+u"\u00bb";
    }else{s.kind=ZenKind::sync;auto p=t.find(u'(');if(p==t.npos||t.back()!=u')'){error="Unsupported ZenUML statement";break;}auto dot=t.rfind(u'.',p);if(dot!=t.npos){s.to=t.substr(0,dot);s.text=compact(std::u16string_view(t).substr(dot+1));}else s.text=compact(t);}
    participant(s.from);participant(s.to);
   }
   ws();if(pos<source.size()&&source[pos]==u'{'){s.body=true;++pos;s.children=block(s.kind==ZenKind::sync||s.kind==ZenKind::creation?s.to:owner,s.kind==ZenKind::sync||s.kind==ZenKind::creation?s.from:caller,true);}
   s.end=pos?pos-1:0;
   if(s.kind==ZenKind::alternative){ws();if(source.substr(pos).starts_with(u"else")){pos+=4;ws();if(pos>=source.size()||source[pos]!=u'{'){error="Missing ZenUML else block";break;}++pos;s.otherwise=block(owner,caller,true);s.end=pos-1;}}
   result.push_back(std::move(s));if(!error.empty())break;
  }return result;
 }
};
struct Dual{double p{},v{};};
std::vector<double> optimal(const std::vector<std::vector<double>>& m){
 const auto n=m.size();std::vector<Dual> gaps(n? n-1:0,{0,1});
 for(;;){double delta=std::numeric_limits<double>::infinity();std::vector<Dual> values;std::vector<int> args;Dual maximum{};
  for(std::size_t j=0;j<n;++j){int arg=-1;if(j){maximum.p+=gaps[j-1].p;maximum.v+=gaps[j-1].v;}
   for(std::size_t i=0;i<j;++i)if(m[i][j]>0){Dual x{values[i].p+m[i][j],values[i].v};auto a=maximum,b=x;double d=a.p-b.p;bool less=d < -std::sqrt(std::numeric_limits<double>::epsilon())||(std::abs(d)<=std::sqrt(std::numeric_limits<double>::epsilon())&&a.v<b.v);if(less)std::swap(a,b);if(a.v<b.v)delta=std::min(delta,(a.p-b.p)/(b.v-a.v));if(less){arg=static_cast<int>(i);maximum=x;}}
   values.push_back(maximum);args.push_back(arg);
  }
  if(!std::isfinite(delta)){std::vector<double> result;for(auto v:values)result.push_back(v.p);return result;}
  if(values.back().v>0){std::size_t j=n-1;while(j){if(args[j]>=0)j=static_cast<std::size_t>(args[j]);else gaps[--j].v=0;}}
  else for(auto& g:gaps)g.p+=g.v*delta;
 }
}
double vertical(const std::vector<ZenStatement>& statements,double top,std::vector<ZenCoordinate>& out,bool occurrence){
 if(statements.empty())return top;double cursor=top+16;
 for(std::size_t i=0;i<statements.size();++i){const auto& s=statements[i];double start=cursor;
  switch(s.kind){
   case ZenKind::async:cursor+=16;break;
   case ZenKind::returned:cursor+=s.from==s.to?20:((i+1<statements.size()||!occurrence)?16:0);break;
   case ZenKind::sync:case ZenKind::creation:cursor+=s.kind==ZenKind::creation?40:(s.from==s.to?30:16);if(!s.children.empty())cursor=vertical(s.children,cursor,out,true)+2;else cursor+=22;if(!s.assignment.empty()&&(s.kind==ZenKind::creation||s.from!=s.to))cursor+=12;break;
   case ZenKind::loop:cursor+=26+(!s.condition.empty()?20:0);cursor=vertical(s.children,cursor,out,occurrence)+11;break;
   case ZenKind::alternative:cursor+=46;cursor=vertical(s.children,cursor,out,occurrence);if(!s.otherwise.empty())cursor=vertical(s.otherwise,cursor+29,out,occurrence);cursor+=11;break;
  }out.push_back({s.begin,s.end,s.kind,start,cursor-start});cursor+=16;
 }return cursor;
}
}
std::expected<ZenDocument,std::string> parse_zenuml(std::u16string_view source){
 Parser p;p.source=source;if(source.starts_with(u"zenuml"))p.source.remove_prefix(6);p.ws();
 while(p.pos<p.source.size()){
  auto end=p.source.find(u'\n',p.pos);if(end==p.source.npos)end=p.source.size();auto line=trim(p.source.substr(p.pos,end-p.pos));
  if(line.starts_with(u"title "))p.doc.title=trim(std::u16string_view(line).substr(6));
  else if(line.starts_with(u"@")){auto spaceAt=line.find(u' ');if(spaceAt==line.npos)return std::unexpected("Missing ZenUML participant name");auto type=line.substr(1,spaceAt-1),rest=trim(std::u16string_view(line).substr(spaceAt+1));auto as=rest.find(u" as ");auto name=rest.substr(0,as);p.participant(name);auto& part=p.doc.participants.back();part.type=type;if(as!=rest.npos)part.label=rest.substr(as+4);}
  else break;p.pos=end;p.ws();
 }
 p.doc.statements=p.block(u"_STARTER_",u"_STARTER_",false);if(!p.error.empty())return std::unexpected(p.error);if(p.starter)p.doc.participants.insert(p.doc.participants.begin(),{u"_STARTER_",{}, {}});return std::move(p.doc);
}
std::expected<ZenLayout,std::string> layout_zenuml(const ZenDocument& doc,const ZenMeasure& measure){
 if(!measure)return std::unexpected("ZenUML requires real text measurements");ZenLayout out;const auto n=doc.participants.size();if(!n)return out;
 for(const auto& p:doc.participants){double width=measure(trim(p.label.empty()?p.name:p.label));if(!std::isfinite(width)||width<0)return std::unexpected("Invalid ZenUML text measurement");out.widths.push_back(std::max(std::floor(width+.5)+(p.type.empty()?0:40),80.0)+20);}
 out.constraints.assign(n,std::vector<double>(n));for(std::size_t i=1;i<n;++i)out.constraints[i-1][i]=(out.widths[i-1]+out.widths[i])/2;
 auto index=[&](std::u16string_view name){for(std::size_t i=0;i<n;++i)if(doc.participants[i].name==name)return i;return n;};
 std::function<void(const std::vector<ZenStatement>&)> walk=[&](const auto& list){for(const auto& s:list){if(s.kind!=ZenKind::loop&&s.kind!=ZenKind::alternative){auto a=index(s.from),b=index(s.kind==ZenKind::returned?s.from:s.to);if(a<n&&b<n){auto text=s.text;if(s.from==s.to&&!s.assignment.empty())text=s.assignment+u" = "+text;double w=std::floor(measure(trim(text))+.5)+25;if(s.kind==ZenKind::creation)w+=out.widths[b]/2;auto l=std::min(a,b),r=std::max(a,b);out.constraints[l][r]=std::max(out.constraints[l][r],w);}}walk(s.children);walk(s.otherwise);}};walk(doc.statements);
 out.positions=optimal(out.constraints);for(auto& p:out.positions)p+=out.widths.front()/2;out.width=std::max(200.0,out.positions.back()+out.widths.back()/2);out.height=vertical(doc.statements,56,out.vertical,false);return out;
}
}
