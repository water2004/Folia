#include "support/folia_test.hpp"
import folia.mermaid.elk_end_labels;
import folia.mermaid.elk_ports;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK end label fixture number");return value;
}
std::size_t number(std::string_view text){return static_cast<std::size_t>(decimal(text));}
void near(double a,double b,std::string_view name){expect(std::abs(a-b)<1e-9)<<name<<a<<b;}
suite elk_end_labels=[] {
    "mermaid_elk_class_end_labels_match_original_sides_margins_positions"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-end-labels.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==8_u));const auto name=row[0];
            ElkCycleGraph source{.node_count=number(row[1])};
            for(const auto pair:split(row[2],',')) {
                const auto ends=split(pair,':');const auto p=source.ports.size();
                source.ports.push_back({number(ends[0])});source.ports.push_back({number(ends[1])});source.edges.push_back({p,p+1});
            }
            ElkLayeringResult layering{.layers=std::vector<int>(source.node_count),.layer_nodes={std::vector<std::size_t>(source.node_count)}};
            std::iota(layering.layer_nodes[0].begin(),layering.layer_nodes[0].end(),0);
            for(const auto e:split(row[4],','))layering.reversed_edges.push_back(number(e));
            auto made=make_elk_layered_graph(source,layering);expect(fatal(made.has_value()));auto& g=*made;
            elk_assign_flat_port_sides(g);
            std::vector<ElkHorizontalMargin> margins(g.nodes.size(),{1,2});std::vector<double> x(g.nodes.size()),y(g.nodes.size());
            for(std::size_t n=0;n<g.nodes.size();++n) {
                auto& node=g.nodes[n];node.width=80+n*5;node.height=60;node.margin_top=3;node.margin_bottom=4;x[n]=n*150;y[n]=n*20;
                std::stable_sort(node.ports.begin(),node.ports.end(),[&](auto a,auto b){return g.ports[a].side<g.ports[b].side;});
                for(std::size_t i=0;i<node.ports.size();++i){auto& p=g.ports[node.ports[i]];p.x=p.side==ElkPortSide::east?node.width:0;p.y=12+i*8;}
            }
            std::vector<ElkEndLabel> labels;
            for(const auto item:split(row[3],',')) {
                const auto v=split(item,':');labels.push_back({labels.size(),number(v[0]),number(v[1])!=0,decimal(v[2]),decimal(v[3])});
            }
            const bool vertical=number(row[5])!=0;
            const auto cells=elk_prepare_end_labels(g,labels,margins,vertical,2,3);expect(fatal(cells.has_value()))<<name;
            const auto expected_margins=split(row[6],';');expect(fatal(expected_margins.size()==g.nodes.size()));
            for(std::size_t n=0;n<g.nodes.size();++n){const auto v=split(expected_margins[n],',');near(margins[n].left,decimal(v[0]),name);near(margins[n].right,decimal(v[1]),name);near(g.nodes[n].margin_top,decimal(v[2]),name);near(g.nodes[n].margin_bottom,decimal(v[3]),name);}
            const auto placed=elk_restore_end_labels(*cells,x,y,vertical,3);const auto expected=split(row[7],';');expect(fatal(placed.size()==expected.size()));
            for(std::size_t i=0;i<placed.size();++i){const auto v=split(expected[i],',');expect(placed[i].id==i&&placed[i].original_edge==labels[i].original_edge)<<name;near(placed[i].x,decimal(v[0]),name);near(placed[i].y,decimal(v[1]),name);}
            ++count;
        }
        expect(count==8_u);
    };
};
}
