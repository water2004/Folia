#include "support/folia_test.hpp"
import folia.mermaid.state_svg;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite state=[] {
 "mermaid_state_fork_join_and_class_styles_are_materialized"_test=[] {
  auto parsed=parse_state_document(u"stateDiagram-v2\nclassDef hot fill:#c00,color:#fff,stroke:#333;\nA:::hot --> B\nclass B hot\nstate fork <<fork>>\nstate join [[join]]");
  expect(fatal(parsed.has_value()))<<(parsed?"":parsed.error());if(!parsed)return;
  const auto a=std::ranges::find(parsed->document.nodes,u"A",&FlowchartNode::id);
  const auto b=std::ranges::find(parsed->document.nodes,u"B",&FlowchartNode::id);
  const auto fork=std::ranges::find(parsed->document.nodes,u"fork",&FlowchartNode::id);
  const auto join=std::ranges::find(parsed->document.nodes,u"join",&FlowchartNode::id);
  expect(a!=parsed->document.nodes.end());expect(b!=parsed->document.nodes.end());
  if(a!=parsed->document.nodes.end()){expect(a->classes==u"hot");expect(!a->compiled_styles.empty());}
  if(b!=parsed->document.nodes.end()){expect(b->classes==u"hot");expect(!b->compiled_styles.empty());}
  expect(fork!=parsed->document.nodes.end());if(fork!=parsed->document.nodes.end())expect(fork->shape==u"fork");
  expect(join!=parsed->document.nodes.end());if(join!=parsed->document.nodes.end())expect(join->shape==u"join");
 };
 "mermaid_state_annotations_and_concurrent_regions_parse_without_losing_source_semantics"_test=[] {
  auto parsed=parse_state_document(u"stateDiagram-v2\nstate Active {\nA --> B\n--\nC --> D\n}\nnote left of A : inline note\nnote right of D\nblock note\nsecond line\nend note");
  expect(fatal(parsed.has_value()))<<(parsed?"":parsed.error());if(!parsed)return;
  expect(parsed->annotations.size()==2_u);expect(bool(parsed->annotations[0].state==u"A"));expect(bool(parsed->annotations[0].position==u"left"));expect(bool(parsed->annotations[0].text==u"inline note"));expect(bool(parsed->annotations[1].state==u"D"));expect(bool(parsed->annotations[1].position==u"right"));expect(bool(parsed->annotations[1].text==u"block note\nsecond line"));
  auto divider=std::ranges::find(parsed->document.nodes,u"divider",&FlowchartNode::shape);expect(divider!=parsed->document.nodes.end());if(divider!=parsed->document.nodes.end()){expect(bool(divider->id==u"divider-id-1"));expect(bool(divider->parent==u"Active"));}
  const auto inline_note_declares_state=parse_state_document(
      u"stateDiagram-v2\nnote right of Missing : text");
  expect(fatal(inline_note_declares_state.has_value()))
      << "Mermaid 12.2 permits an inline note to declare its referenced state";
  if(inline_note_declares_state){
   expect(std::ranges::any_of(inline_note_declares_state->document.nodes,
       [](const auto& node){return node.id==u"Missing";}))
       << "inline note target should be materialized as a state";
   expect(inline_note_declares_state->annotations.size()==1_u)
       << "inline note should produce one annotation";
  }
  const auto unclosed_note=parse_state_document(
      u"stateDiagram-v2\nnote right of Missing\ntext");
  expect(!unclosed_note) << "block note without end note must be rejected";
  const auto separator_outside_group=parse_state_document(u"stateDiagram-v2\n--");
  expect(!separator_outside_group)
      << "concurrent separator outside a composite must be rejected";
 };
 "mermaid_state_concurrent_initial_states_are_scoped_per_partition"_test=[] {
  auto parsed=parse_state_document(u"stateDiagram-v2\nstate Active {\n[*] --> A\n--\n[*] --> B\n}");
  expect(fatal(parsed.has_value()))<<(parsed?"":parsed.error());if(!parsed)return;
  std::vector<std::u16string> starts;for(const auto& node:parsed->document.nodes)if(node.shape==u"stateStart")starts.push_back(node.id);
  expect(fatal(starts.size()==2_u));
  expect(bool(starts[0]!=starts[1]));
  expect(fatal(parsed->document.edges.size()==2_u));
  expect(bool(parsed->document.edges[0].start==starts[0]));
  expect(bool(parsed->document.edges[1].start==starts[1]));
  auto trailing=parse_state_document(u"stateDiagram-v2\nstate Active {\nA --> B\n--\n}");
  expect(fatal(trailing.has_value()))<<(trailing?"":trailing.error());if(trailing){const auto divider=std::ranges::find(trailing->document.nodes,u"divider",&FlowchartNode::shape);expect(divider!=trailing->document.nodes.end());if(divider!=trailing->document.nodes.end()){expect(!divider->group);expect(divider->parent==u"Active");expect(divider->direction.empty());}expect(std::ranges::all_of(trailing->document.nodes,[&](const auto& node){return(node.id==u"A"||node.id==u"B")?node.parent==u"Active":true;}));}
 };
 "mermaid_state_parser_and_config_match_original"_test=[] {
  const auto dir=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(dir?dir:FOLIA_MERMAID_FIXTURE_DIR)/"state.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
  while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;std::vector<std::string_view> columns;std::string_view rest=line;for(;;){auto tab=rest.find('\t');columns.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}expect(fatal(columns.size()==4_u));++count;
   auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(columns[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto actual=prepare_state(decode(columns[1]),*session);auto expected=json(decode(columns[3]));if(expected.kind==ConfigKind::null){expect(!actual)<<columns[0];continue;}expect(fatal(actual.has_value()))<<columns[0]<<(actual?"":actual.error());const auto& d=actual->document;expect(bool(d.direction==field(expected,u"direction").string));const auto& nodes=field(expected,u"nodes").sequence;expect(fatal(d.nodes.size()==nodes.size()));
   for(std::size_t i=0;i<nodes.size();++i){const auto& a=d.nodes[i];const auto& e=nodes[i];expect(bool(a.id==field(e,u"id").string));expect(bool(a.label==field(e,u"label").string));expect(bool(a.shape==field(e,u"shape").string));expect(bool(a.parent==field(e,u"parent").string));expect(a.group==field(e,u"group").boolean);expect(bool(a.direction==field(e,u"direction").string));const auto& styles=field(e,u"styles").sequence;expect(fatal(a.styles.size()==styles.size()));for(std::size_t j=0;j<styles.size();++j)expect(bool(a.styles[j]==styles[j].string));}
   const auto& edges=field(expected,u"edges").sequence;expect(fatal(d.edges.size()==edges.size()));for(std::size_t i=0;i<edges.size();++i){const auto& a=d.edges[i];const auto& e=edges[i];expect(bool(a.start==field(e,u"start").string));expect(bool(a.end==field(e,u"end").string));expect(bool(a.label==field(e,u"label").string));}const auto& annotations=field(expected,u"annotations").sequence;expect(fatal(actual->annotations.size()==annotations.size()));for(std::size_t i=0;i<annotations.size();++i){expect(bool(actual->annotations[i].state==field(annotations[i],u"state").string));expect(bool(actual->annotations[i].text==field(annotations[i],u"text").string));expect(bool(actual->annotations[i].position==field(annotations[i],u"position").string));}expect(!render_state_svg(*actual,{}));
  }expect(count==19_u);
 };
};
}
