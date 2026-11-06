module folia.mermaid.requirement;
import std;
namespace folia::mermaid {
namespace {
std::u16string trim(std::u16string_view s) {
    const auto first=s.find_first_not_of(u" \t\r\n");
    if(first==s.npos)return {};
    return std::u16string(s.substr(first,s.find_last_not_of(u" \t\r\n")-first+1));
}
std::u16string lower(std::u16string_view s) {
    std::u16string out(s);
    for(auto& c:out)if(c>=u'A'&&c<=u'Z')c=static_cast<char16_t>(c-u'A'+u'a');
    return out;
}
std::u16string value(std::u16string_view s) {
    auto v=trim(s);
    if(v.size()>=2&&v.front()==u'"'&&v.back()==u'"')return v.substr(1,v.size()-2);
    return v;
}
bool known(std::u16string_view v,std::initializer_list<std::u16string_view> choices) {
    return std::ranges::find(choices,v)!=choices.end();
}
struct Parser {
    std::u16string_view source;
    RequirementDocument doc;
    std::map<std::u16string,std::size_t,std::less<>> requirement_by_name,element_by_name,class_by_id;
    std::vector<std::u16string> lines;

    void fail(std::string_view message) { throw std::runtime_error(std::string(message)); }
    void read_lines() {
        std::size_t start=0;
        while(start<=source.size()) {
            auto end=source.find(u'\n',start); if(end==source.npos)end=source.size();
            lines.emplace_back(trim(source.substr(start,end-start)));
            if(end==source.size())break; start=end+1;
        }
    }
    std::vector<std::u16string> classes_from(std::u16string_view list) {
        std::vector<std::u16string> out;
        std::size_t start=0;
        while(start<=list.size()) {auto end=list.find(u',',start);if(end==list.npos)end=list.size();auto id=trim(list.substr(start,end-start));if(!id.empty())out.push_back(std::move(id));if(end==list.size())break;start=end+1;}
        return out;
    }
    void add_class(std::vector<std::u16string>& names,std::vector<std::u16string>& styles,const std::vector<std::u16string>& requested) {
        for(const auto& id:requested) {
            if(std::ranges::find(names,id)==names.end())names.push_back(id);
            if(auto it=class_by_id.find(id);it!=class_by_id.end()) {
                const auto& values=doc.classes[it->second].styles; styles.insert(styles.end(),values.begin(),values.end());
            }
        }
    }
    void add_definition(std::u16string_view line,bool requirement,std::size_t& i) {
        auto open=line.find(u'{'); if(open==line.npos)fail("Requirement definition requires a body");
        auto head=trim(line.substr(0,open));
        std::vector<std::u16string> classes;
        auto marker=head.find(u":::");
        if(marker!=head.npos){classes=classes_from(head.substr(marker+3));head=trim(head.substr(0,marker));}
        auto split=head.find_first_of(u" \t");
        if(split==head.npos)fail("Requirement definition requires a name");
        auto kind=lower(head.substr(0,split));
        auto name=value(trim(head.substr(split+1)));
        if(name.empty())fail("Requirement definition requires a name");
        const std::map<std::u16string,std::u16string,std::less<>> req_types={{u"requirement",u"Requirement"},{u"functionalrequirement",u"Functional Requirement"},{u"interfacerequirement",u"Interface Requirement"},{u"performancerequirement",u"Performance Requirement"},{u"physicalrequirement",u"Physical Requirement"},{u"designconstraint",u"Design Constraint"}};
        if(requirement&&!req_types.contains(kind))fail("Unknown requirement type");
        if(!requirement&&kind!=u"element")fail("Expected element definition");
        if(requirement) {
            if(!requirement_by_name.contains(name)) {
                RequirementRequirement r;r.name=name;r.type=req_types.at(kind);
                add_class(r.classes,r.css_styles,classes);
                requirement_by_name.emplace(name,doc.requirements.size());doc.requirements.push_back(std::move(r));
            }
        } else if(!element_by_name.contains(name)) {
            RequirementElement e;e.name=name;add_class(e.classes,e.css_styles,classes);
            element_by_name.emplace(name,doc.elements.size());doc.elements.push_back(std::move(e));
        }
        bool closed=false;
        for(++i;i<lines.size();++i) {
            auto body=trim(lines[i]); if(body.empty()||body.front()==u'#'||body.starts_with(u"%%"))continue;
            if(body==u"}"){closed=true;break;}
            auto colon=body.find(u':');if(colon==body.npos)fail("Malformed requirement field");
            auto key=lower(trim(body.substr(0,colon)));auto val=value(body.substr(colon+1));
            if(requirement) {
                auto& r=doc.requirements[requirement_by_name.at(name)];
                if(key==u"id")r.requirement_id=val;else if(key==u"text")r.text=val;else if(key==u"risk") {auto v=lower(val);if(!known(v,{u"low",u"medium",u"high"}))fail("Unknown requirement risk");r.risk=v==u"low"?u"Low":v==u"medium"?u"Medium":u"High";}
                else if(key==u"verifymethod") {auto v=lower(val);if(!known(v,{u"analysis",u"demonstration",u"inspection",u"test"}))fail("Unknown verification method");r.verify_method=v==u"analysis"?u"Analysis":v==u"demonstration"?u"Demonstration":v==u"inspection"?u"Inspection":u"Test";}
                else fail("Unknown requirement field");
            } else {
                auto& e=doc.elements[element_by_name.at(name)];if(key==u"type")e.type=val;else if(key==u"docref")e.doc_ref=val;else fail("Unknown element field");
            }
        }
        if(!closed)fail("Unclosed requirement definition");
    }
    void apply_style(std::u16string_view line) {
        auto body=trim(line);const bool def=lower(body).starts_with(u"classdef ");
        auto prefix=def?std::size_t(8):std::size_t(6);body=trim(body.substr(prefix));
        auto space=body.find_first_of(u" \t");if(space==body.npos)fail("Style statement requires styles");
        auto ids=classes_from(body.substr(0,space));auto style=trim(body.substr(space+1));
        std::vector<std::u16string> styles;std::size_t start=0;
        while(start<=style.size()){auto end=style.find(u',',start);if(end==style.npos)end=style.size();auto item=trim(style.substr(start,end-start));if(!item.empty())styles.push_back(item);if(end==style.size())break;start=end+1;}
        if(def) {
            for(const auto& id:ids){auto it=class_by_id.find(id);if(it==class_by_id.end()){class_by_id[id]=doc.classes.size();doc.classes.push_back({id,styles,{}});it=class_by_id.find(id);}else doc.classes[it->second].styles=styles;
                auto& c=doc.classes[it->second];for(const auto& s:styles)if(s.find(u"color")!=s.npos){auto t=s;auto p=t.find(u"fill");if(p!=t.npos)t.replace(p,4,u"bgFill");c.text_styles.push_back(std::move(t));}
                for(auto& r:doc.requirements)if(std::ranges::find(r.classes,id)!=r.classes.end())r.css_styles.insert(r.css_styles.end(),styles.begin(),styles.end());
                for(auto& e:doc.elements)if(std::ranges::find(e.classes,id)!=e.classes.end())e.css_styles.insert(e.css_styles.end(),styles.begin(),styles.end());
            }
        } else for(const auto& id:ids) {
            if(auto it=requirement_by_name.find(id);it!=requirement_by_name.end()){auto& v=doc.requirements[it->second].css_styles;v.insert(v.end(),styles.begin(),styles.end());}
            else if(auto it=element_by_name.find(id);it!=element_by_name.end()){auto& v=doc.elements[it->second].css_styles;v.insert(v.end(),styles.begin(),styles.end());}
        }
    }
    void apply_class(std::u16string_view line) {
        auto body=trim(line.substr(5));auto space=body.find_first_of(u" \t");
        if(space==body.npos)fail("Class statement requires class names");
        auto nodes=classes_from(body.substr(0,space));auto requested=classes_from(trim(body.substr(space+1)));
        for(const auto& id:nodes) {
            if(auto it=requirement_by_name.find(id);it!=requirement_by_name.end()){auto& n=doc.requirements[it->second];add_class(n.classes,n.css_styles,requested);}
            else if(auto it=element_by_name.find(id);it!=element_by_name.end()){auto& n=doc.elements[it->second];add_class(n.classes,n.css_styles,requested);}
        }
    }
    void parse() {
        read_lines();std::size_t i=0;while(i<lines.size()&&lines[i].empty())++i;
        if(i==lines.size()||lower(lines[i])!=u"requirementdiagram")fail("Expected requirementDiagram header");
        for(++i;i<lines.size();++i) {
            auto line=trim(lines[i]);if(line.empty()||line.front()==u'#'||line.starts_with(u"%%"))continue;
            auto l=lower(line);
            if(l.starts_with(u"direction ")){auto d=trim(line.substr(10));d=lower(d);if(!known(d,{u"tb",u"bt",u"lr",u"rl"}))fail("Invalid requirement direction");doc.direction=d==u"tb"?u"TB":d==u"bt"?u"BT":d==u"lr"?u"LR":u"RL";continue;}
            if(l.starts_with(u"acctitle:")){doc.acc_title=value(line.substr(line.find(u':')+1));continue;}
            if(l.starts_with(u"accdescr:")){doc.acc_description=value(line.substr(line.find(u':')+1));continue;}
            if(l.starts_with(u"accdescr {")){auto end=line.find(u'{');auto content=trim(line.substr(end+1));while(i<lines.size()&&lines[i]!=u"}"){if(!content.empty())content+=u"\n";content+=lines[++i];}if(i==lines.size())fail("Unclosed accessibility description");doc.acc_description=trim(content);continue;}
            if(l.starts_with(u"requirement ")||l.starts_with(u"functionalrequirement ")||l.starts_with(u"interfacerequirement ")||l.starts_with(u"performancerequirement ")||l.starts_with(u"physicalrequirement ")||l.starts_with(u"designconstraint ")){add_definition(line,true,i);continue;}
            if(l.starts_with(u"element ")){add_definition(line,false,i);continue;}
            if(l.starts_with(u"classdef ")||l.starts_with(u"style ")){apply_style(line);continue;}
            if(l.starts_with(u"class ")){apply_class(line);continue;}
            if(auto marker=line.find(u":::");marker!=line.npos){auto names=classes_from(line.substr(0,marker));auto requested=classes_from(line.substr(marker+3));for(const auto& id:names){if(auto it=requirement_by_name.find(id);it!=requirement_by_name.end()){auto& n=doc.requirements[it->second];add_class(n.classes,n.css_styles,requested);}else if(auto it=element_by_name.find(id);it!=element_by_name.end()){auto& n=doc.elements[it->second];add_class(n.classes,n.css_styles,requested);}}continue;}
            auto at=line.find(u" <- ");auto left_to=line.find(u" -> ");
            if(at!=line.npos){auto target=value(line.substr(0,at));auto rhs=trim(line.substr(at+4));auto dash=rhs.find(u" - ");if(dash==rhs.npos)fail("Malformed relationship");auto type=lower(trim(rhs.substr(0,dash)));auto src=value(rhs.substr(dash+3));if(!known(type,{u"contains",u"copies",u"derives",u"satisfies",u"verifies",u"refines",u"traces"}))fail("Unknown relationship type");doc.relationships.push_back({type,src,target});continue;}
            if(left_to!=line.npos){auto rhs=value(line.substr(left_to+4));auto lhs=trim(line.substr(0,left_to));auto dash=lhs.rfind(u" - ");if(dash==lhs.npos)fail("Malformed relationship");auto src=value(lhs.substr(0,dash));auto type=lower(lhs.substr(dash+3));if(!known(type,{u"contains",u"copies",u"derives",u"satisfies",u"verifies",u"refines",u"traces"}))fail("Unknown relationship type");doc.relationships.push_back({type,src,rhs});continue;}
            fail("Unsupported requirement statement");
        }
    }
};
}
std::expected<RequirementDocument,std::string> parse_requirement(std::u16string_view source) {
    try {Parser p{source};p.parse();return std::move(p.doc);}catch(const std::runtime_error& e){return std::unexpected(e.what());}
}
}
