#include "support/folia_test.hpp"
import folia.mermaid.elk_placement;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> fields(std::string_view text, char delimiter) {
    std::vector<std::string_view> result;
    if (text == "-") return result;
    for (;;) {
        const auto end = text.find(delimiter); result.push_back(text.substr(0,end));
        if (end == text.npos) break;
        text.remove_prefix(end+1);
    }
    return result;
}
std::size_t number(std::string_view text) {
    std::size_t value{};
    const auto parsed = std::from_chars(text.data(),text.data()+text.size(),value);
    if (parsed.ec != std::errc{}) throw std::runtime_error("Invalid ELK placement fixture number");
    return value;
}
double decimal(std::string_view text) {
    double value{};
    const auto parsed = std::from_chars(text.data(),text.data()+text.size(),value);
    if (parsed.ec != std::errc{}) throw std::runtime_error("Invalid ELK placement fixture decimal");
    return value;
}
ElkLayeredGraph make_graph(const std::vector<std::string_view>& row) {
    ElkLayeredGraph graph;
    const auto layers = fields(row[1],',');
    graph.nodes.resize(layers.size());
    for (std::size_t i=0;i<layers.size();++i) {
        auto& n=graph.nodes[i]; n.layer=number(layers[i]);
        n.height=24+i*3; n.margin_top=i%3; n.margin_bottom=(i+1)%4;
        if (graph.layers.size()<=n.layer) graph.layers.resize(n.layer+1);
        graph.layers[n.layer].push_back(i);
    }
    for (const auto dummy : fields(row[3],',')) {
        auto& n=graph.nodes[number(dummy)]; n.kind=ElkLayerNodeKind::long_edge; n.height=1;
    }
    for (const auto pair : fields(row[2],',')) {
        const auto ends=fields(pair,':'); const auto s=number(ends[0]),t=number(ends[1]);
        const auto e=graph.edges.size(),p=graph.ports.size();
        graph.ports.push_back({.node=s,.side=ElkPortSide::east,.y=static_cast<double>((e*7)%23),.outgoing={e}});
        graph.ports.push_back({.node=t,.side=ElkPortSide::west,.y=static_cast<double>((e*11)%19),.incoming={e}});
        graph.nodes[s].ports.push_back(p); graph.nodes[t].ports.push_back(p+1);
        graph.edges.push_back({.source_port=p,.target_port=p+1});
    }
    return graph;
}
suite elk_placement = [] {
    "mermaid_elk_bk_alignment_matches_original_four_directions"_test = [] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-placement.tsv"};
        expect(fatal(input.is_open())); std::string line; std::size_t count=0;
        while (std::getline(input,line)) {
            if (!line.empty()&&line.back()=='\r') line.pop_back();
            if (line.empty()||line.front()=='#') continue;
            const auto row=fields(line,'\t'); expect(fatal(row.size()==11_u));
            const auto graph=make_graph(row);
            const auto result=elk_align_flat_blocks(graph); expect(fatal(result.has_value()))<<row[0];
            const auto marked=fields(row[4],','); expect(fatal(marked.size()==result->marked_edges.size()));
            for (std::size_t e=0;e<marked.size();++e)
                expect(result->marked_edges[e]==(number(marked[e])!=0))<<row[0]<<e;
            for (std::size_t d=0;d<4;++d) {
                const auto& b=result->layouts[d]; const auto nodes=fields(row[5+d],';');
                expect(fatal(nodes.size()==graph.nodes.size()));
                for (std::size_t n=0;n<nodes.size();++n) {
                    const auto values=fields(nodes[n],','); expect(fatal(values.size()==5_u));
                    expect(b.root[n]==number(values[0])&&b.align[n]==number(values[1]))<<row[0]<<d<<n;
                    expect(std::abs(b.inner_shift[n]-decimal(values[2]))<1e-9)<<row[0]<<d<<n;
                    expect(std::abs(b.block_size[n]-decimal(values[3]))<1e-9)<<row[0]<<d<<n;
                    expect(b.only_dummies[n]==(number(values[4])!=0))<<row[0]<<d<<n;
                }
            }
            for (std::size_t mode=0;mode<2;++mode) {
                const auto positions=elk_place_flat_nodes(graph,{},mode==0);
                expect(fatal(positions.has_value()))<<row[0];
                const auto expected=fields(row[9+mode],',');
                expect(fatal(expected.size()==positions->size()));
                for (std::size_t n=0;n<expected.size();++n)
                    expect(std::abs((*positions)[n]-decimal(expected[n]))<1e-9)<<row[0]<<mode<<n<<(*positions)[n];
            }
            ++count;
        }
        expect(count==7_u);
    };
};
}
