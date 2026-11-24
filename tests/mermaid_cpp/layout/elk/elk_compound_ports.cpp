#include "support/folia_test.hpp"
import folia.mermaid.elk_compound_ports;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK compound port fixture number");return value;
}
void near(double a,double b,std::string_view name){expect(std::abs(a-b)<1e-9)<<name<<a<<b;}
suite elk_compound_ports=[] {
    "mermaid_elk_compound_boundary_ports_match_original_geometry"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-compound-ports.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==7_u));const auto name=row[0];const auto expected=split(row[6],',');expect(fatal(expected.size()==17_u));
            auto dummy=elk_make_compound_port_dummy(static_cast<ElkLayoutDirection>(static_cast<int>(decimal(row[1]))),decimal(row[2])!=0,decimal(row[3]));
            expect(static_cast<int>(dummy.side)==static_cast<int>(decimal(expected[0])))<<name;
            expect(static_cast<int>(dummy.inner_port_side)==static_cast<int>(decimal(expected[1])))<<name;
            expect(static_cast<int>(dummy.constraint)==static_cast<int>(decimal(expected[2])))<<name;
            for(std::size_t i=3;i<7;++i)near(0,decimal(expected[i]),name);
            dummy.position={20,30};std::array dummies{dummy};
            ElkCompoundContent content{.width=100,.height=60,.padding_left=24,.padding_right=24,.padding_top=40,.padding_bottom=24,.offset={2,-3}};
            elk_resize_compound_content(content,dummies,decimal(row[4]),decimal(row[5]));
            near(content.width,decimal(expected[7]),name);near(content.height,decimal(expected[8]),name);near(content.offset.x,decimal(expected[9]),name);near(content.offset.y,decimal(expected[10]),name);
            near(dummies[0].position.x,decimal(expected[11]),name);near(dummies[0].position.y,decimal(expected[12]),name);
            const auto port=elk_export_compound_port(content,dummies[0]);near(port.x,decimal(expected[13]),name);near(port.y,decimal(expected[14]),name);
            near(dummies[0].position.x,decimal(expected[15]),name);near(dummies[0].position.y,decimal(expected[16]),name);++count;
        }
        expect(count==10_u);
    };
};
}
