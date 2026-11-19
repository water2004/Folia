// Mermaid 12.2.0 Wardley map model and parser. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.wardley;
import std;
export namespace folia::mermaid {
struct WardleyNode { std::u16string id,label,class_name; double x=0,y=0,label_x=0,label_y=0; bool has_label_offset=false,inertia=false,pipeline_parent=false,in_pipeline=false; std::u16string strategy; };
struct WardleyLink { std::u16string source,target,label,flow; bool dashed=false; };
struct WardleyTrend { std::u16string node; double target=0; };
struct WardleyNote { std::u16string text; double x=0,y=0; };
struct WardleyAnnotation { int number=0; double x=0,y=0; std::u16string text; };
struct WardleyPipeline { std::u16string parent; std::vector<std::u16string> components; };
struct WardleyMarker { std::u16string name; double x=0,y=0; };
struct WardleyDiagram { std::u16string title,x_label=u"Evolution",y_label=u"Visibility"; std::vector<std::u16string> stages; std::vector<double> boundaries; std::vector<WardleyNode> nodes; std::vector<WardleyLink> links; std::vector<WardleyTrend> trends; std::vector<WardleyPipeline> pipelines; std::vector<WardleyNote> notes; std::vector<WardleyAnnotation> annotations; std::vector<WardleyMarker> accelerators,deaccelerators; std::optional<std::pair<double,double>> annotations_box; double width=900,height=600; bool explicit_size=false,show_grid=false,use_max_width=true; };
std::expected<WardleyDiagram,std::string> parse_wardley(std::u16string_view source);
}
