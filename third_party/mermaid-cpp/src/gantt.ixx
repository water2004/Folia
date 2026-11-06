// Mermaid 12.2.0 Gantt parser and scheduling DB. MIT; see ../LICENSE.
export module folia.mermaid.gantt;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
using GanttTime=std::int64_t;
struct GanttTask {std::u16string id,text,section;GanttTime start=0,end=0;std::optional<GanttTime> render_end;std::int64_t order=0;bool manual_end=false,active=false,done=false,critical=false,milestone=false,vertical=false;};
struct GanttDocument {std::u16string title,acc_title,acc_description,date_format=u"YYYY-MM-DD",axis_format,tick_interval,today_marker,weekday=u"sunday",weekend=u"saturday";std::vector<std::u16string> sections,excludes,includes;std::vector<GanttTask> tasks;bool inclusive=false,top_axis=false;};
std::expected<GanttDocument,std::string> parse_gantt(std::u16string_view);
std::optional<GanttTime> gantt_parse_date(std::u16string_view,std::u16string_view);
std::u16string gantt_format_date(GanttTime,std::u16string_view);
bool gantt_excluded(GanttTime,const GanttDocument&);
}
