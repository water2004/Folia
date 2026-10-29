module folia.mermaid.architecture_constraints;
import std;

namespace folia::mermaid {
namespace {
using Text=std::u16string;using Texts=std::vector<Text>;
template<class K,class V> using Ordered=std::vector<std::pair<K,V>>;
template<class K,class V> V& entry(Ordered<K,V>& map,const K& key){for(auto& [k,v]:map)if(k==key)return v;return map.emplace_back(key,V{}).second;}
Text number(int n){const auto s=std::to_string(n);return Text(s.begin(),s.end());}
using Axis=Ordered<int,Ordered<Text,Texts>>;
std::vector<Texts> flatten(const Axis& axis,const ArchitectureDocument& doc,std::u16string_view direction) {
    Ordered<Text,Texts> flattened;
    for(const auto& [coordinate,groups]:axis) {
        const auto key=number(coordinate);int count=0;
        if(groups.size()==1){entry(flattened,key)=groups.front().second;continue;}
        for(std::size_t i=0;i+1<groups.size();++i)for(std::size_t j=i+1;j<groups.size();++j) {
            const auto& a=groups[i];const auto& b=groups[j];
            const auto pair=u"\""+std::min(a.first,b.first)+u"\"-\""+std::max(a.first,b.first)+u"\"";
            const auto alignment=std::ranges::find(doc.group_alignments,pair,&std::pair<Text,Text>::first);
            if((alignment!=doc.group_alignments.end()&&alignment->second==direction)||a.first==u"default"||b.first==u"default") {
                auto& values=entry(flattened,key);values.insert(values.end(),a.second.begin(),a.second.end());values.insert(values.end(),b.second.begin(),b.second.end());
            } else {entry(flattened,key+u"-"+number(count++))=a.second;entry(flattened,key+u"-"+number(count++))=b.second;}
        }
    }
    std::vector<Texts> result;for(auto& [key,values]:flattened)if(values.size()>1)result.push_back(std::move(values));return result;
}
}
std::expected<ArchitectureConstraints,std::string> architecture_constraints(const ArchitectureDocument& doc) {
    const auto* size=doc.config.find(u"iconSize");const auto* multiplier=doc.config.find(u"idealEdgeLengthMultiplier");
    if(!size||!multiplier)return std::unexpected("Architecture constraints require icon sizing configuration");
    const double gap=size->number*multiplier->number;
    ArchitectureConstraints result;std::map<Text,Text> parents;
    for(const auto& n:doc.nodes)parents.emplace(n.id,n.parent.empty()?u"default":n.parent);
    for(const auto& map:doc.spatial_maps) {
        Axis horizontal,vertical;
        for(const auto& n:map){entry(entry(horizontal,n.y),parents.at(n.id)).push_back(n.id);entry(entry(vertical,n.x),parents.at(n.id)).push_back(n.id);}
        auto h=flatten(horizontal,doc,u"horizontal"),v=flatten(vertical,doc,u"vertical");
        result.horizontal.insert(result.horizontal.end(),std::make_move_iterator(h.begin()),std::make_move_iterator(h.end()));
        result.vertical.insert(result.vertical.end(),std::make_move_iterator(v.begin()),std::make_move_iterator(v.end()));
    }
    std::set<Text> declared;std::set<std::pair<Text,Text>> pairs;
    for(const auto& hint:doc.hints)for(const auto& id:hint.members)declared.insert(id);
    const auto overlapping=[&](const Texts& group){return std::ranges::any_of(group,[&](const Text& id){return declared.contains(id);});};
    std::erase_if(result.horizontal,overlapping);std::erase_if(result.vertical,overlapping);
    for(const auto& hint:doc.hints) {
        (hint.direction==u"row"?result.horizontal:result.vertical).push_back(hint.members);
        for(std::size_t i=1;i<hint.members.size();++i) {
            const auto& a=hint.members[i-1];const auto& b=hint.members[i];pairs.emplace(a,b);pairs.emplace(b,a);
            ArchitectureRelativePlacement placement;placement.gap=gap;
            if(hint.direction==u"row"){placement.left=a;placement.right=b;}else{placement.top=a;placement.bottom=b;}
            result.relative.push_back(std::move(placement));
        }
    }
    using Position=std::pair<int,int>;
    for(const auto& spatial:doc.spatial_maps) {
        std::map<Position,Text> inverse;for(const auto& n:spatial)inverse[{n.x,n.y}]=n.id;
        std::vector<Position> queue{{0,0}};std::set<Position> visited;
        for(std::size_t i=0;i<queue.size();++i) {
            const auto current=queue[i];visited.insert(current);const auto found=inverse.find(current);if(found==inverse.end())continue;
            constexpr Position directions[]={{-1,0},{1,0},{0,1},{0,-1}};
            for(std::size_t d=0;d<4;++d) {
                Position next{current.first+directions[d].first,current.second+directions[d].second};const auto neighbor=inverse.find(next);
                if(neighbor==inverse.end()||visited.contains(next))continue;queue.push_back(next);
                if(pairs.contains({found->second,neighbor->second}))continue;
                ArchitectureRelativePlacement placement;placement.gap=gap;
                if(d==0){placement.left=neighbor->second;placement.right=found->second;}
                else if(d==1){placement.left=found->second;placement.right=neighbor->second;}
                else if(d==2){placement.top=neighbor->second;placement.bottom=found->second;}
                else{placement.top=found->second;placement.bottom=neighbor->second;}
                result.relative.push_back(std::move(placement));
            }
        }
    }
    return result;
}
}
