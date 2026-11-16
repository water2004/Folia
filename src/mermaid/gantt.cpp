module folia.mermaid.gantt;
import std;
namespace folia::mermaid {
namespace {
constexpr GanttTime day=86400000;
std::optional<GanttTime> parse_epoch(std::u16string_view value, bool seconds) {
    std::string raw;
    for (const auto c : value) {
        if (c > 127) return {};
        raw += static_cast<char>(c);
    }
    double count = 0;
    const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), count);
    if (parsed.ec != std::errc{} || parsed.ptr != raw.data() + raw.size() ||
        !std::isfinite(count)) return {};
    const auto milliseconds = count * (seconds ? 1000.0 : 1.0);
    // ECMAScript Date's TimeClip bound; conversion must not overflow int64_t.
    if (std::abs(milliseconds) > 8.64e15) return {};
    using namespace std::chrono;
    const sys_time<std::chrono::milliseconds> instant{
        std::chrono::milliseconds{static_cast<GanttTime>(milliseconds)}};
    return current_zone()->to_local(instant).time_since_epoch().count();
}
std::optional<GanttTime> start_date(std::u16string_view value, std::u16string_view format) {
    // Mermaid getStartDate deliberately treats a digit-only x/X start as
    // milliseconds. getEndDate uses Dayjs, where X denotes seconds.
    if ((format == u"x" || format == u"X") && !value.empty() &&
        std::ranges::all_of(value, [](char16_t c) { return c >= u'0' && c <= u'9'; })) {
        return parse_epoch(value, false);
    }
    return gantt_parse_date(value, format);
}
std::u16string trim(std::u16string_view s){auto first=s.find_first_not_of(u" \t\r\n");if(first==s.npos)return {};return std::u16string(s.substr(first,s.find_last_not_of(u" \t\r\n")-first+1));}
std::u16string lower(std::u16string s){for(auto& c:s)if(c>=u'A'&&c<=u'Z')c+=32;return s;}
std::vector<std::u16string> split(std::u16string_view s,char16_t sep){std::vector<std::u16string> out;for(;;){auto p=s.find(sep);out.push_back(trim(s.substr(0,p)));if(p==s.npos)return out;s.remove_prefix(p+1);}}
GanttTime add_duration(GanttTime start,std::u16string_view value){std::size_t end=0;while(end<value.size()&&((value[end]>=u'0'&&value[end]<=u'9')||value[end]==u'.'))++end;if(end==0)return start;std::string raw(value.begin(),value.begin()+end);double count=0;auto p=std::from_chars(raw.data(),raw.data()+raw.size(),count);if(p.ec!=std::errc{})return start;auto unit=value.substr(end);if(unit==u"d"||unit==u"w")return start+static_cast<GanttTime>(std::floor(count*(unit==u"w"?7:1)+.5))*day;if(unit==u"h")return start+static_cast<GanttTime>(count*3600000);if(unit==u"m")return start+static_cast<GanttTime>(count*60000);if(unit==u"s")return start+static_cast<GanttTime>(count*1000);if(unit==u"ms")return start+static_cast<GanttTime>(count);if(unit==u"M"||unit==u"y"){using namespace std::chrono;const sys_time<milliseconds> date{milliseconds{start}};const auto base=floor<days>(date);year_month_day ymd{base};auto target=ymd.year()/ymd.month()+months{static_cast<int>(count*(unit==u"y"?12:1))};auto last=year_month_day_last{target.year(),month_day_last{target.month()}};auto result=target/std::chrono::day{std::min(unsigned(ymd.day()),unsigned(last.day()))};return duration_cast<milliseconds>((sys_days{result}+(date-base)).time_since_epoch()).count();}return start;}
}
std::optional<GanttTime> gantt_parse_date(std::u16string_view text,std::u16string_view format){if(format==u"X"||format==u"x")return parse_epoch(text,format==u"X");int year=1970,month=1,date=1,hour=0,minute=0,second=0;std::size_t offset=0;for(std::size_t i=0;i<format.size();){int* target=nullptr;std::size_t count=0;if(format.substr(i,4)==u"YYYY"){target=&year;count=4;}else if(format.substr(i,2)==u"MM"){target=&month;count=2;}else if(format.substr(i,2)==u"DD"){target=&date;count=2;}else if(format.substr(i,2)==u"HH"){target=&hour;count=2;}else if(format.substr(i,2)==u"mm"){target=&minute;count=2;}else if(format.substr(i,2)==u"ss"){target=&second;count=2;}if(target){if(offset+count>text.size())return {};*target=0;for(std::size_t j=0;j<count;++j){auto c=text[offset+j];if(c<u'0'||c>u'9')return {};*target=*target*10+c-u'0';}offset+=count;i+=count;}else{if(offset>=text.size()||text[offset++]!=format[i++])return {};}}if(offset!=text.size()||hour>23||minute>59||second>59)return {};using namespace std::chrono;const year_month_day ymd{std::chrono::year{year},std::chrono::month{static_cast<unsigned>(month)},std::chrono::day{static_cast<unsigned>(date)}};if(!ymd.ok())return {};return duration_cast<milliseconds>(sys_days{ymd}.time_since_epoch()+hours{hour}+minutes{minute}+seconds{second}).count();}
std::u16string gantt_format_date(GanttTime value, std::u16string_view format) {
    using namespace std::chrono;
    const sys_time<milliseconds> civil_time{milliseconds{value}};
    const auto date = floor<days>(civil_time);
    const year_month_day ymd{date};
    const hh_mm_ss clock{civil_time - date};
    const auto weekday = std::chrono::weekday{date};
    const auto year_start = sys_days{ymd.year()/January/1};
    const auto day_index = (date - year_start).count();
    const auto thursday = date + days{4 - static_cast<int>(weekday.iso_encoding())};
    const auto iso_year = year_month_day{thursday}.year();
    const auto january_fourth = sys_days{iso_year/January/4};
    const auto first_thursday = january_fourth +
        days{4 - static_cast<int>(std::chrono::weekday{january_fourth}.iso_encoding())};
    constexpr std::array<std::u16string_view, 7> days_long{
        u"Sunday", u"Monday", u"Tuesday", u"Wednesday", u"Thursday", u"Friday", u"Saturday"};
    constexpr std::array<std::u16string_view, 12> months_long{
        u"January", u"February", u"March", u"April", u"May", u"June",
        u"July", u"August", u"September", u"October", u"November", u"December"};
    auto instant = [&] {
        return current_zone()->to_sys(local_time<milliseconds>{milliseconds{value}}, choose::earliest);
    };
    std::u16string out;
    for (std::size_t i = 0; i < format.size(); ++i) {
        if (format[i] != u'%' || i + 1 == format.size()) {
            out += format[i];
            continue;
        }
        auto directive = format[++i];
        char16_t padding = directive == u'e' ? u' ' : u'0';
        if (directive == u'-' || directive == u'_' || directive == u'0') {
            padding = directive == u'-' ? 0 : directive == u'_' ? u' ' : u'0';
            if (++i == format.size()) break;
            directive = format[i];
        }
        auto append = [&](std::int64_t number, unsigned width = 2) {
            const auto bytes = std::to_string(number);
            if (padding && bytes.size() < width) out.append(width - bytes.size(), padding);
            out.append(bytes.begin(), bytes.end());
        };
        switch (directive) {
        case u'a': out += days_long[weekday.c_encoding()].substr(0, 3); break;
        case u'A': out += days_long[weekday.c_encoding()]; break;
        case u'b': out += months_long[unsigned(ymd.month()) - 1].substr(0, 3); break;
        case u'B': out += months_long[unsigned(ymd.month()) - 1]; break;
        case u'c': out += gantt_format_date(value, u"%x, %X"); break;
        case u'd': case u'e': append(unsigned(ymd.day())); break;
        case u'f': append(clock.subseconds().count() * 1000, 6); break;
        case u'g': append(int(iso_year) % 100); break;
        case u'G': append(int(iso_year), 4); break;
        case u'H': append(clock.hours().count()); break;
        case u'I': append(clock.hours().count() % 12 == 0 ? 12 : clock.hours().count() % 12); break;
        case u'j': append(day_index + 1, 3); break;
        case u'L': append(clock.subseconds().count(), 3); break;
        case u'm': append(unsigned(ymd.month())); break;
        case u'M': append(clock.minutes().count()); break;
        case u'p': out += clock.hours().count() < 12 ? u"AM" : u"PM"; break;
        case u'q': append((unsigned(ymd.month()) - 1) / 3 + 1, 1); break;
        case u'Q': append(instant().time_since_epoch().count(), 1); break;
        case u's': append(floor<seconds>(instant()).time_since_epoch().count(), 1); break;
        case u'S': append(clock.seconds().count()); break;
        case u'u': append(weekday.iso_encoding(), 1); break;
        case u'U': append((day_index + 7 - weekday.c_encoding()) / 7); break;
        case u'V': append((thursday - first_thursday).count() / 7 + 1); break;
        case u'w': append(weekday.c_encoding(), 1); break;
        case u'W': append((day_index + 7 - (weekday.c_encoding() + 6) % 7) / 7); break;
        case u'x': out += gantt_format_date(value, u"%-m/%-d/%Y"); break;
        case u'X': out += gantt_format_date(value, u"%-I:%M:%S %p"); break;
        case u'y': append(int(ymd.year()) % 100); break;
        case u'Y': append(int(ymd.year()), 4); break;
        case u'Z': {
            const auto offset = duration_cast<minutes>(current_zone()->get_info(instant()).offset).count();
            out += offset < 0 ? u'-' : u'+';
            padding = u'0';
            append(std::abs(offset) / 60);
            append(std::abs(offset) % 60);
            break;
        }
        case u'%': out += u'%'; break;
        default: out += directive; break;
        }
    }
    return out;
}
bool gantt_excluded(GanttTime value,const GanttDocument& d){auto date=gantt_format_date(value,u"%Y-%m-%d");if(std::ranges::find(d.includes,date)!=d.includes.end())return false;using namespace std::chrono;const auto weekday=std::chrono::weekday{floor<days>(sys_time<milliseconds>{milliseconds{value}})}.iso_encoding();constexpr std::array<std::u16string_view,7> names{u"monday",u"tuesday",u"wednesday",u"thursday",u"friday",u"saturday",u"sunday"};for(const auto& excluded:d.excludes)if(excluded==date||excluded==names[weekday-1]||(excluded==u"weekends"&&(weekday==(d.weekend==u"friday"?5:6)||weekday==(d.weekend==u"friday"?6:7))))return true;return false;}
std::expected<GanttDocument,std::string> parse_gantt(std::u16string_view source){try{GanttDocument d;struct Raw{GanttTask task;std::u16string start,end;bool previous=false;};std::vector<Raw> raw;std::u16string section;std::size_t offset=0,generated=0,order=0;bool header=false;while(offset<source.size()){auto end=source.find(u'\n',offset);if(end==source.npos)end=source.size();auto line=trim(source.substr(offset,end-offset));const auto line_start=offset;offset=end+1;
        if (header && lower(line).starts_with(u"accdescr")) {
            const auto keyword = source.find_first_not_of(u" \t\r\n", line_start);
            const auto open = source.find_first_not_of(u" \t\r\n", keyword + 8);
            if (open != source.npos && source[open] == u'{') {
                const auto close = source.find(u'}', open + 1);
                if (close == source.npos) throw std::runtime_error("Incomplete Gantt accessibility description");
                d.acc_description = trim(source.substr(open + 1, close - open - 1));
                offset = close + 1;
                continue;
            }
        }
        auto comment=line.find(u"%%");if(comment!=line.npos)line=trim(line.substr(0,comment));if(line.empty())continue;auto key=lower(line);if(!header){if(key!=u"gantt")throw std::runtime_error("Expected Gantt header");header=true;continue;}bool directive=false;for(auto [prefix,target]:{std::pair{u"title ",&d.title},{u"section ",&section},{u"dateformat ",&d.date_format},{u"axisformat ",&d.axis_format},{u"tickinterval ",&d.tick_interval},{u"todaymarker ",&d.today_marker},{u"weekday ",&d.weekday},{u"weekend ",&d.weekend},{u"acctitle:",&d.acc_title},{u"accdescr:",&d.acc_description}})if(key.starts_with(prefix)){*target=trim(line.substr(std::u16string_view(prefix).size()));if(std::u16string_view(prefix)==u"section ")d.sections.push_back(section);directive=true;break;}if(directive)continue;if(key==u"inclusiveenddates"){d.inclusive=true;continue;}if(key==u"topaxis")throw std::runtime_error("Original Gantt topAxis statement is invalid; use configuration");if(key.starts_with(u"excludes ")||key.starts_with(u"includes ")){auto& target=key.starts_with(u"excludes ")?d.excludes:d.includes;auto items=split(line.substr(9),u',');for(auto& item:items)for(auto& token:split(item,u' '))if(!token.empty()&&std::ranges::find(target,token)==target.end())target.push_back(lower(token));continue;}
        auto colon=line.find(u':');if(colon==line.npos||colon+1==line.size())throw std::runtime_error("Gantt task needs schedule data");Raw r;r.task.text=line.substr(0,colon);r.task.section=section;auto data=split(line.substr(colon+1),u',');while(!data.empty()){bool tag=false;for(auto [name,target]:{std::pair{u"active",&r.task.active},{u"done",&r.task.done},{u"crit",&r.task.critical},{u"milestone",&r.task.milestone},{u"vert",&r.task.vertical}})if(data.front()==name){*target=true;tag=true;break;}if(!tag)break;data.erase(data.begin());}if(data.empty()||data.size()>3)throw std::runtime_error("Invalid Gantt task data");r.task.order=r.task.vertical?-1:static_cast<std::int64_t>(order++);if(data.size()==3){r.task.id=data[0];r.start=data[1];r.end=data[2];}else{auto n=std::to_string(++generated);r.task.id=u"task"+std::u16string(n.begin(),n.end());r.end=data.back();if(data.size()==1)r.previous=true;else r.start=data[0];}raw.push_back(std::move(r));}
    if(!header)throw std::runtime_error("Expected Gantt header");std::vector<bool> ready(raw.size());for(std::size_t pass=0;pass<=raw.size();++pass){bool progress=false;for(std::size_t i=0;i<raw.size();++i){if(ready[i])continue;auto& r=raw[i];auto reference=[&](std::u16string_view value,bool latest)->std::optional<GanttTime>{std::optional<GanttTime> result;for(const auto& id:split(value,u' ')){if(id.empty())continue;auto found=std::ranges::find_if(raw,[&](const Raw& t){return t.task.id==id;});if(found==raw.end())throw std::runtime_error("Gantt dependency refers to an unknown task");auto index=found-raw.begin();if(!ready[index])return {};auto time=latest?found->task.end:found->task.start;result=result?(latest?std::max(*result,time):std::min(*result,time)):time;}return result;};std::optional<GanttTime> start;if(r.previous){if(i&&ready[i-1])start=raw[i-1].task.end;}else if(r.start.starts_with(u"after "))start=reference(std::u16string_view(r.start).substr(6),true);else{start=start_date(r.start,d.date_format);if(!start)throw std::runtime_error("Invalid Gantt start date");}if(!start)continue;std::optional<GanttTime> finish;if(r.end.starts_with(u"until "))finish=reference(std::u16string_view(r.end).substr(6),false);else{finish=gantt_parse_date(r.end,d.date_format);if(finish&&d.inclusive)*finish+=day;if(!finish)finish=add_duration(*start,r.end);}if(!finish)continue;r.task.start=*start;r.task.end=*finish;r.task.manual_end=gantt_parse_date(r.end,u"YYYY-MM-DD").has_value();if(!d.excludes.empty()&&!r.task.manual_end){bool invalid=false;const auto limit=r.task.end+10000*day;for(auto cursor=r.task.start+day;cursor<=r.task.end;cursor+=day){if(!invalid)r.task.render_end=r.task.end;invalid=gantt_excluded(cursor,d);if(invalid)r.task.end+=day;if(r.task.end>limit)throw std::runtime_error("Gantt exclusions leave no valid date");}}ready[i]=true;progress=true;}if(std::ranges::all_of(ready,[](bool v){return v;}))break;if(!progress)throw std::runtime_error("Unresolved Gantt task dependency");}for(auto& r:raw)d.tasks.push_back(std::move(r.task));return d;
}catch(const std::runtime_error& e){return std::unexpected(e.what());}}
}
