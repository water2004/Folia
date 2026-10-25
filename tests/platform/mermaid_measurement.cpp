#include "support/folia_test.hpp"
import folia.platform.mermaid_measurer;
import folia.platform.mermaid_pie;
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_measurement = [] {
    "mermaid_class_source_to_measured_svg"_test = [] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"note-emphasis",u"classDiagram\nclass Document\nnote for Document \"**Validate** the *document*\nKeep revisions\""},
            {"note-nowrap",u"classDiagram\nclass Document\nnote for Document \"Validate the document before saving changes to local storage and keep revisions\""},
            {"wrapped-edge",u"classDiagram\nclass Document\nDocument --> Store : **Validate** the document before saving changes to local storage"},
            {"display-emphasis",u"classDiagram\nclass D[\"Document **model**\"] {\n+**name** String\n+save() *bool*\n}\nD --> Store : **save** changes"},
            {"display-lines",u"classDiagram\nclass D[\"Document<br/>model\"] {\n+name<br/>String\n+save()<br/>bool\n}\nD --> Store : save<br/>changes"},
            {"display-entities",u"classDiagram\nclass D[\"Editor &amp; viewer\"]\nD --> Store : save #38; reload"},
            {"namespace",u"classDiagram\nnamespace Storage {\nclass Document {\n+save() bool\n}\nclass Store\nnote for Store \"Local storage\"\n}\nDocument --> Store : persists"},
            {"namespace-nested",u"classDiagram\nnamespace App[\"Application\"] {\nnamespace Storage {\nclass Document\n}\nclass Editor\n}\nEditor --> Document : saves"},
            {"namespace-qualified",u"classDiagram\nnamespace App.Storage {\nclass Store\n}\nnamespace App {\nclass Editor\n}\nEditor --> Store"},
            {"note-only",u"classDiagram\nnote \"Document model\""},
            {"note-attached",u"classDiagram\nnote for Document \"Validate before saving\nKeep revisions\"\nclass Document {\n+save() bool\n}"},
            {"note-related",u"classDiagram\ndirection LR\nclass Document\nDocument --> Store : persists\nnote for Store \"Local storage\"\nnote for Missing \"Unattached\""},
            {"self",u"classDiagram\nclass Document {\n+validate() bool\n}\nDocument --> Document : validates"},
            {"self-related",u"classDiagram\ndirection LR\nclass Document {\n+save() bool\n}\nDocument --> Store : persists\nDocument \"1\" o-- \"*\" Document : revisions"},
            {"title",u"---\ntitle: Document storage\n---\nclassDiagram\nclass Document {\n+save() bool\n}\nDocument --> Store : persists"},
            {"title-wide",u"---\ntitle: Read and validate the document before saving to storage\nconfig:\n  fontSize: 20\n  state:\n    titleTopMargin: 40\n---\nclassDiagram\nclass Document"},
            {"members",u"classDiagram\nclass Document {\n+String title\n+save() bool\n}\nclass Store {\n+load(id) Document\n}\nDocument --> Store : persists"},
            {"relations",u"classDiagram\ndirection LR\nclass Shape\n<<interface>> Shape\nclass Circle\nShape <|-- Circle\nCircle \"1\" o-- \"*\" Point : contains"},
            {"empty-boxes",u"classDiagram\nclass Document\nclass Store\nDocument ..> Store : uses"},
            {"classifiers",u"classDiagram\nclass Repository~T~ {\n<<service>>\n-items List~T~\n+count int$\n#find(id) T*\n~reset()$\n}"},
            {"attributes",u"classDiagram\nclass Document {\n+String title\n+int revision\n}"},
            {"configured",u"%%{init: {\"fontSize\": 20, \"class\": {\"padding\": 8, \"hideEmptyMembersBox\": true}, \"state\": {\"nodeSpacing\": 60, \"rankSpacing\": 70}}}%%\nclassDiagram\ndirection RL\nclass View\nclass Document\nView *-- Document"}
        };
        for(const auto theme_name:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme_name},{u"look",u"classic"},{u"layout",u"dagre"},{u"fontFamily",u"Arial"}})
                site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases) {
                auto svg=render_mermaid(source,*session);
                expect(svg.has_value()) << name << (svg?"":svg.error());if(!svg)continue;
                expect(svg->width>0 && svg->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    const std::u16string theme{theme_name};
                    std::ofstream output(std::filesystem::path(directory)/("class-"+name+"-"+std::string(theme.begin(),theme.end())+".svg"),std::ios::binary);
                    output<<svg->svg;expect(output.good());
                }
            }
            ConfigValue elk=site;elk.mapping.at(u"layout").string=u"elk";
            expect(fatal(session->save_initialize_config(elk).has_value()));
            expect(fatal(session->set_site_config(elk).has_value()));
            expect(!render_mermaid(u"classDiagram\nclass Document",*session));
        }
    };

    "mermaid_sequence_measured_source_to_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"participant-lifecycle",u"sequenceDiagram\nparticipant U as User\ncreate participant W as Worker\nU->>W: Start\nW->>W: Work\ndestroy W\nW-->>U: Done"},
            {"lifecycle-late",u"sequenceDiagram\nautonumber\nparticipant L as Log\nparticipant U as User\nU->>L: Open\ncreate actor W as Worker\nU->>W: Start\nW-->>U: Ready\ndestroy W\nU-xW: Stop"},
            {"lifecycle-no-footer",u"%%{init: {\"sequence\": {\"mirrorActors\": false}}}%%\nsequenceDiagram\nparticipant A\ncreate participant B\nA->>+B: Start\nB-->>-A: Ready\ndestroy B\nA-xB: Stop"},
            {"lifecycle-reverse",u"sequenceDiagram\nparticipant A@{type: database} as Store\nparticipant B as Client\nB->>A: Open\ndestroy A\nB-xA: Close"},
            {"participant-boxes",u"sequenceDiagram\nbox rgb(230,240,255) Client\nactor U as User\nparticipant E as Editor\nend\nbox transparent Storage\nparticipant D@{type: database} as Documents\nend\nU->>E: Save\nE->>D: Commit\nD-->>U: Saved"},
            {"participant-box-title",u"sequenceDiagram\nbox LightGray Document processing service\nparticipant A as Service\nend\nparticipant B as Store\nA->>+B: Save\nB-->>-A: Done"},
            {"participant-box-wrap",u"%%{init: {\"sequence\": {\"mirrorActors\": false}}}%%\nsequenceDiagram\nbox transparent wrap: Document processing and synchronization service across local storage\nparticipant A as Service\nend\nbox\nparticipant B as Storage\nend\nA->>B: Save\nB-->>A: Done"},
            {"typed-participants",u"sequenceDiagram\nparticipant U@{type: boundary, alias: Editor}\nparticipant C@{type: control} as Save service\nparticipant D@{type: database} as Storage\nU->>C: Save\nC->>D: Commit\nD-->>U: Saved"},
            {"typed-lifelines",u"sequenceDiagram\nparticipant E@{type: entity} as Document\nparticipant Q@{type: queue} as Pending\nparticipant C@{type: collections} as Revisions\nE->>+Q: Enqueue\nQ->>C: Append\nQ-->>-E: Done"},
            {"numbered",u"sequenceDiagram\nautonumber\nA->>+B: Save\nNote over B: Validate\nB->>B: Commit\nB-->>-A: Done"},
            {"number-controls",u"sequenceDiagram\nautonumber 10 5\nA->>B: Open\nautonumber off\nB-->>A: Ready\nautonumber\nA->>B: Save"},
            {"number-config",u"%%{init: {\"sequence\": {\"showSequenceNumbers\": true}}}%%\nsequenceDiagram\nA->>B: Open\nB-->>A: Ready"},
            {"participant-footers",u"%%{init: {\"sequence\": {\"mirrorActors\": false, \"hideUnusedParticipants\": true}}}%%\nsequenceDiagram\nparticipant Unused\nactor A as User\nparticipant B as Storage\nA->>B: Save"},
            {"number-decimals",u"sequenceDiagram\nautonumber 10.1 .1\nA<<->>B: Open\nB<<-->>A: Ready\nA<<->>A: Check\nautonumber 1000\nA->>B: Save"},
            {"cross-async",u"sequenceDiagram\nautonumber\nA-xB: Cancel\nB--xA: Rejected\nA-)B: Start\nB--)A: Accepted\nB-)B: Continue"},
            {"cross-async-activation",u"%%{init: {\"sequence\": {\"rightAngles\": true}}}%%\nsequenceDiagram\nA-)+B: Start\nB--)-A: Accepted\nA-xA: Cancel"},
            {"basic",u"sequenceDiagram\nparticipant U as User\nparticipant S as Service\nU->>S: Read document\nS-->>U: Document"},
            {"wide",u"sequenceDiagram\nparticipant A as Document editor\nparticipant B as Storage\nA->>B: Read and validate the document before saving changes\nB-->>A: Complete"},
            {"self",u"sequenceDiagram\nparticipant A as Worker\nA->>A: Save"},
            {"reverse",u"sequenceDiagram\nparticipant A as Editor\nparticipant B as Store\nparticipant C as Watcher\nC->>A: Open\nA->B: Read\nB-->A: Ready\nA<<->>C: Update\nC<<-->>A: Done"},
            {"spacing",u"%%{init: {\"sequence\": {\"width\": 120, \"height\": 55, \"actorMargin\": 40, \"boxMargin\": 8, \"rightAngles\": true}}}%%\nsequenceDiagram\nA->>A: Check\nA-->>B: Ready"},
            {"font",u"%%{init: {\"fontSize\": 20}}%%\nsequenceDiagram\nparticipant A as Source document\nA->>B: Read the updated document\nB-->>A: Saved"},
            {"activation-note",u"sequenceDiagram\nactor U as User\nparticipant S as Service\nU->>+S: Save document\nNote right of S: Validate input\nS->>S: Write output\nS-->>-U: Saved"},
            {"note-over",u"sequenceDiagram\nparticipant A as Editor\nparticipant B as Store\nNote over A,B: Save operations\nA->>B: Commit\nNote left of A: Ready"},
            {"note-spacing",u"sequenceDiagram\nA->>B: Read\nNote right of A: Validate document before saving all changes\nNote over B: Storage ready\nNote left of B: Return the current document to the editor\nB-->>A: Done"},
            {"nested-activation",u"sequenceDiagram\nactor A as User\nparticipant B as Service\nA->>+B: Save\nB->>+B: Validate\nNote over B: Checking\nB-->>-B: Valid\nB-->>-A: Saved"},
            {"loop",u"sequenceDiagram\nloop Retry\nA->>B: Save\nB-->>A: Done\nend"},
            {"alt",u"sequenceDiagram\nalt Valid\nA->>B: Save\nelse Invalid\nB-->>A: Reject\nend"},
            {"nested",u"sequenceDiagram\nopt Cached\nloop Read\nA->>+B: Read\nNote over B: Loading\nB-->>-A: Data\nend\nend"},
            {"parallel",u"sequenceDiagram\npar Read\nA->>B: Fetch\nand Write\nB->>A: Store\nend"},
            {"critical-break",u"sequenceDiagram\ncritical Save\nA->>B: Write\noption Failure\nbreak Abort\nB-->>A: Failed\nend\nend"},
            {"region-spacing",u"%%{init: {\"fontSize\": 20, \"sequence\": {\"boxMargin\": 8, \"boxTextMargin\": 7, \"labelBoxWidth\": 60, \"labelBoxHeight\": 24}}}%%\nsequenceDiagram\nloop Retry\nA->>A: Check\nA->>B: Save\nend"},
            {"background",u"sequenceDiagram\nrect rgb(220, 240, 255)\nA->>+B: Save\nNote over B: Write\nB-->>-A: Done\nend"},
            {"nested-background",u"sequenceDiagram\nrect rgba(200, 230, 255, 0.4)\nloop Retry\nrect rgb(245, 245, 200)\nA->>B: Save\nend\nB-->>A: Done\nend\nend"},
            {"title",u"sequenceDiagram\ntitle Save document\nA->>B: Save\nB-->>A: Done"},
            {"line-breaks",u"sequenceDiagram\nparticipant A as Document<br/>editor\nA->>B: Read<br/>document\nNote over A,B: Save<br/>changes\nB-->>A: Done"},
            {"entities",u"sequenceDiagram\nparticipant A as Editor #38; viewer\nA->>B: Save #60;document#62;\nNote over B: Value #35;1"},
            {"wrapped-labels",u"sequenceDiagram\nparticipant A as wrap: A document editor with a descriptive name\nA->>B: wrap: Read the document and validate every required field before saving\nNote over B: wrap: Store the updated document after validation succeeds"},
            {"region-wrap",u"sequenceDiagram\nloop Retry until the document has been saved successfully\nA->>B: Save\nB-->>A: Done\nend"},
            {"nested-wrap",u"sequenceDiagram\nalt The current document has unsaved local changes\nloop Retry saving the edited document to storage\nA->>+B: wrap: Save all changes made to the current document\nNote right of B: wrap: Validate the document before committing to storage\nB-->>-A: Saved\nend\nelse No local changes need to be written to storage\nA->>A: wrap: Continue editing the current document\nend"},
            {"tall-wrap",u"%%{init: {\"sequence\": {\"width\": 100, \"height\": 40}}}%%\nsequenceDiagram\nparticipant A as wrap: A document editor with a descriptive name\nparticipant B as Storage\nA->>B: wrap: Save the current document\nNote left of B: wrap: Review the current document before saving\nNote over A,B: wrap: Both participants finish the save operation"},
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            const ConfigValue site{.kind=ConfigKind::mapping,.mapping={
                {u"theme",ConfigValue{.kind=ConfigKind::string,.string=theme}},
                {u"look",ConfigValue{.kind=ConfigKind::string,.string=u"classic"}},
                {u"fontFamily",ConfigValue{.kind=ConfigKind::string,.string=u"Arial"}},
            }};
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases) {
                auto svg=render_mermaid(source,*session);
                expect(svg.has_value())<<name<<(svg?"":svg.error());
                if(!svg) continue;
                expect(svg->width>0&&svg->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    const std::u16string_view theme_name{theme};
                    std::ofstream output(std::filesystem::path(directory)/("sequence-"+name+"-"+std::string(theme_name.begin(),theme_name.end())+".svg"),std::ios::binary);
                    output<<svg->svg;expect(output.good());
                }
            }
            expect(!render_mermaid(u"sequenceDiagram\nA->>B: <b>Go</b>",*session));

        }
    };
    "mermaid_flowchart_measured_source_to_svg"_test=[] {
        auto session=ConfigSession::create();expect(fatal(session.has_value()));
        const ConfigValue site{.kind=ConfigKind::mapping,.mapping={
            {u"theme",ConfigValue{.kind=ConfigKind::string,.string=u"default"}},
            {u"look",ConfigValue{.kind=ConfigKind::string,.string=u"classic"}},
            {u"layout",ConfigValue{.kind=ConfigKind::string,.string=u"dagre"}},
        }};
        expect(fatal(session->save_initialize_config(site).has_value()));
        expect(fatal(session->set_site_config(site).has_value()));
        struct Example {std::string name;std::u16string body,title;};
        const std::vector<Example> cases={
            {"rich-markdown",u"flowchart LR\nA[\"`Read **bold** and *italic*`\"] -->|\"`next **operation**`\"| B[\"`Save ***both***\nthen finish`\"]"},
            {"rich-markdown-group",u"flowchart TD\nsubgraph g[Operations]\ndirection LR\nA[\"`Read the __source document__ and _validate input_`\"] -->|\"`continue with **next step** and *save*`\"| B[\"`Write **updated _data_**`\"]\nend\ng --> C[Done]"},
            {"rich-markdown-literal",u"flowchart LR\nA[\"`Read \\*literal\\* &amp; **bold**`\"] --> B[\"`Save file_name and *unfinished`\"]"},
            {"rich-html",u"flowchart LR\nA[\"Read <b>bold</b> and <i>italic</i>\"] -->|\"next <strong>operation</strong>\"| B[\"Save <strong><em>both</em></strong><br/>then finish\"]"},
            {"rich-html-group",u"flowchart TD\nsubgraph g[Operations]\ndirection LR\nA[\"Read the <strong>source document</strong> and <em>validate input</em>\"] -->|\"continue with <b>next step</b> and <i>save</i>\"| B[\"Write <i>updated data</i>\"]\nend\ng --> C[Done]"},
            {"rich-html-font",u"flowchart LR\nA[\"Read <b>bold</b> and normal\"] --> B[\"Write <em>italic</em> and normal\"]\nstyle A font-size:20px,font-weight:300\nstyle B font-size:18px,font-weight:bold"},
            {"fonts-nodes",u"flowchart LR\nA[Read the source document]:::large -->|next| B[Save the updated document]:::large\nclassDef large font-size:20px,font-weight:bold,color:#334466\nstyle B font-size:18px,font-weight:normal,font-style:italic"},
            {"fonts-edges",u"flowchart TD\nA[Read] -->|Continue with the next operation| B[Write]\nB -->|Saved output| C[Done]\nlinkStyle 0 font-size:20px,font-weight:bold,color:#773311\nlinkStyle 1 font-size:18px,font-family:Courier New,font-style:italic"},
            {"fonts-group",u"flowchart LR\nsubgraph g[Operations]\ndirection TB\nA[Read input]:::mono --> B[Write output]\nend\ng -->|done| C[Exit]\nclassDef mono font-family:Courier New,font-size:18px,font-weight:700\nstyle B font-size:22px\nstyle g color:#887733"},

            {"styles-nodes",u"flowchart LR\nA[Read]:::base -->|next| B([Write]):::base\nclassDef default fill:#eeeeee,stroke:#444444\nclassDef base fill:#ddeeff,stroke:#224466,color:#112233,stroke-width:2px,stroke-dasharray:5 3\nstyle A fill:#ffddaa,color:#663300\nstyle B stroke:#aa3300,stroke-width:3px"},
            {"styles-group",u"flowchart LR\nsubgraph g[Storage]\ndirection TB\nA[(Records)]:::record --> B{Ready?}\nend\ng -->|done| C(((Exit)))\nclassDef record fill:#ddffee,stroke:#228855,color:#114433\nclassDef group fill:#fff4cc,stroke:#997711,color:#665500,stroke-width:2px,stroke-dasharray:6 2\nclass g group\nstyle g fill:#ffeebb\nstyle B fill:none,stroke:#aa5522,stroke-width:2px\nstyle C fill:#eeddee,stroke:#884488,color:#442244"},
            {"styles-edges",u"flowchart TD\nA[Read] -->|next| B[Check]\nB o--o|open| C[Save]\nC x--x|stop| D[Done]\nlinkStyle 0 stroke:#bb3300,color:#663300,stroke-width:3px,stroke-dasharray:5 3\nlinkStyle 1 stroke:#228844,color:#114422,stroke-width:2px\nlinkStyle 2 stroke:#3344bb,color:#112266,stroke-width:2px"},
            {"styles-default-link",u"flowchart LR\nsubgraph g[Work]\ndirection TB\nA[Read] -->|retry| A\nA --> B[Write]\nend\ng -->|done| C[Exit]\nlinkStyle default stroke:#226688,color:#114466,stroke-width:2px\nlinkStyle 2 stroke:#aa3355,stroke-width:3px,stroke-dasharray:4 2"},

            {"title-short",u"flowchart TD\nA[Read] --> B[Write]",u"Document pipeline"},
            {"title-wide",u"flowchart TD\nA[Read] --> B[Write]",u"Read and validate the source document before saving the updated output"},
            {"title-group",u"%%{init: {\"flowchart\": {\"titleTopMargin\": 40}}}%%\nflowchart LR\nsubgraph g[Storage]\ndirection TB\nA[Read] --> B[Write]\nend\ng --> C[Done]",u"Storage & recovery"},
            {"markers-ends",u"flowchart LR\nA[Read] --o|open| B[Check]\nB --x|stop| C[Done]"},
            {"markers-both",u"flowchart TD\nA[Read] o--o|open| B[Check]\nB x--x|stop| C[Save]\nC <--> D[Done]"},
            {"markers-group",u"flowchart RL\nsubgraph g[Operations]\ndirection BT\nA[Read] x--x B[Check]\nend\ng o--o|ready| C[Done]"},
            {"labels-wrap",u"flowchart TD\nA[\"Read the input and validate the document\"] -->|\"continue with the next operation\"| B(\"Write the updated document to disk\")"},
            {"labels-wrap-group",u"%%{init: {\"flowchart\": {\"wrappingWidth\": 150, \"minNodeWidth\": 0}}}%%\nflowchart LR\nsubgraph g[\"Input and output operations\"]\ndirection TB\nA[\"Read the source document\"] --> B[\"Write the output document\"]\nend\ng --> C[\"All work completed\"]"},
            {"labels-entities",u"flowchart LR\nA[\"Say #quot;hello#quot;\"] -->|\"A &amp; B\"| B[\"Cost #36;5\"]\nB --> C[\"x &lt; 5\"]"},
            {"labels-lines",u"flowchart TD\nA[\"Read<br/>input\"] -->|\"next\\nstep\"| B(\"Check\nresult\")\nB --> C[(\"Save<br>record\")]"},
            {"labels-group",u"flowchart LR\nsubgraph g[\"Read<br/>and write\"]\ndirection TB\nA[\"Input #38; data\"] --> B[\"Write<br />output\"]\nend\ng -->|\"all<br/>done\"| C[\"End\"]"},
            {"shapes-polygons",u"flowchart TD\nA{{Prepare}} --> B[/Read/]\nA --> C[\\Write\\]\nB --> D[/Collect\\]\nC --> E[\\Expand/]\nD --> F>Route]\nE --> F\nF --> G(((Done)))"},
            {"shapes-polygons-group",u"%%{init: {\"flowchart\": {\"minNodeWidth\": 0}}}%%\nflowchart LR\nsubgraph g[Steps]\ndirection BT\nA{{Init}} --> B[/In/] --> C[\\Out\\] --> D[/Join\\] --> E[\\Fork/] --> F>Next]\nend\ng --> G(((End)))"},
            {"shapes-down",u"flowchart TD\nA([Start]) -->|run| B[[Process]]\nA --> C[(Database)]\nB --> D([Finish])\nC --> D"},
            {"shapes-right",u"flowchart LR\nA([Start]) --> B[[Process]] --> C[(Database)]"},
            {"shapes-group",u"flowchart TD\nsubgraph g[Storage]\ndirection LR\nA([Begin]) --> B[[Save]] --> C[(Records)]\nC -->|retry| C\nend\ng --> D([End])"},
            {"chain",u"flowchart TD\nA[Start] --> B[Finish]"},
            {"decision",u"flowchart TD\nA[Read] --> B{Valid?}\nB -->|yes| C(Done)\nB -->|no| A"},
            {"horizontal",u"flowchart LR\nA((Input)) --> B[Process]\nA --> C[Store]\nB --> D(Output)\nC --> D"},
            {"reverse",u"flowchart RL\nA[First] -->|next| B[Second]"},
            {"upward",u"flowchart BT\nA[First] --> B[Second]"},
            {"parallel",u"flowchart TD\nA[Source] -->|one| B[Target]\nA -. two .-> B"},
            {"loop-down",u"flowchart TD\nA[Retry] -->|again| A\nA --> B[Done]"},
            {"loop-right",u"flowchart LR\nA((Retry)) -->|again| A\nA --> B[Done]"},
            {"loop-left",u"flowchart RL\nA[Retry] --> A\nA --> B[Done]"},
            {"loop-up",u"flowchart BT\nA{Retry?} -->|yes| A\nA --> B[Done]"},
            {"group-independent",u"flowchart TD\nsubgraph g[Group]\ndirection LR\nA[Read]-->B[Write]\nend\ng-->|done|C[Exit]"},
            {"group-incoming",u"flowchart LR\nC[Enter]-->g\nsubgraph g[Group]\ndirection BT\nA[Read]-->B[Write]\nend"},
            {"group-external",u"flowchart TD\nsubgraph g[Group]\ndirection LR\nA[Read]-->B[Write]\nend\nB-->C[Exit]\ng-->|done|D[Report]"},
            {"group-nested",u"flowchart TD\nsubgraph g[Outer]\ndirection LR\nsubgraph h[Inner]\ndirection BT\nA[Read]-->B[Write]\nend\nh-->C[Store]\nend\ng-->D[Exit]"},
            {"group-loop",u"flowchart LR\nsubgraph g[Group]\ndirection TB\nA[Retry]-->|again|A\nA-->B[Done]\nend\ng-->C[Exit]"},
            {"group-margins",u"%%{init: {\"flowchart\": {\"subGraphTitleMargin\": {\"top\": 6, \"bottom\": 4}}}}%%\nflowchart LR\nsubgraph g[Group title]\ndirection TB\nA[Read]-->|next|B[Write]\nend\ng-->|done|C[Exit]"},
            {"unicode",u"flowchart LR\nA[\u4e2d\u6587] --> B[Caf\u00e9 \U0001f600]"},
        };
        for(const auto theme:{u"default",u"dark"}) for(const auto& sample:cases) {
            const auto& name=sample.name;
            const auto& body=sample.body;
            const auto source=std::u16string(u"---\n")+(sample.title.empty()?std::u16string{}:u"title: "+sample.title+u"\n")+u"config:\n  theme: "+theme+
                u"\n  look: classic\n  layout: dagre\n  fontFamily: Arial\n  themeVariables: {fontFamily: Arial}\n  flowchart: {htmlLabels: true}\n---\n"+body;
            const auto svg=render_mermaid(source,*session);
            expect(svg.has_value())<<name<<(svg?"":svg.error());
            if(!svg) continue;
            expect(svg->width>0 && svg->height>0);
            expect(svg->svg.find("font-family=\"Arial\"")!=std::string::npos);
            expect(svg->svg.find("class=\"flowchart-link\"")!=std::string::npos);
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                std::filesystem::create_directories(directory);
                const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                std::ofstream out(std::filesystem::path(directory)/("flowchart-"+name+"-"+suffix+".svg"),std::ios::binary);
                out<<svg->svg;expect(out.good());
            }
        }
        const std::u16string prefix=u"---\nconfig: {theme: default, look: classic, layout: dagre, fontFamily: Arial}\n---\n";
        expect(!render_mermaid(prefix+u"flowchart TD\nA-->",*session))<<"unfinished edge";
        expect(!render_mermaid(prefix+u"flowchart TD\nA[\"<img src='image'>\"]",*session))<<"image label adapter pending";
        expect(!render_mermaid(prefix+u"flowchart TD\nA-->B\nstyle A font-size:0px",*session))<<"invalid label font size";
        expect(render_mermaid(prefix+u"flowchart TD\nsubgraph G\nA-->B\nend",*session).has_value())<<"cluster adapter";
        expect(render_mermaid(prefix+u"flowchart TD\nA-->A",*session).has_value())<<"self-loop adapter";
        const auto recovered=render_mermaid(prefix+u"flowchart TD\nA-->B",*session);
        expect(recovered.has_value())<<"request after errors"<<(recovered?"":recovered.error());
    };
    "mermaid_source_to_svg_resolves_fonts_and_measures_display_text"_test=[] {
        MermaidMeasurer measurer;
        auto family=measurer.resolve_family(u"'Folia Missing Family 792743', Arial, sans-serif");
        expect(fatal(family.has_value())); expect(bool(*family==u"Arial"));
        expect(!measurer.resolve_family(u"'Folia Missing Family 792743'"));
        auto session=ConfigSession::create(); expect(fatal(session.has_value()));
        for(const auto theme:{u"default",u"dark"}) {
            auto source=std::u16string(u"---\ntitle: Drinks\nconfig:\n  theme: ")+theme+
                u"\n  themeVariables:\n    fontFamily: Arial\n  pie:\n    legendPosition: bottom\n    donutHole: 0.5\n---\npie showData\n\"#quot;Tea#quot; #9733;\":3\n\"Coffee\":2";
            auto svg=render_mermaid(source,*session);
            expect(svg.has_value())<<(svg?"":svg.error());
            expect(fatal(svg.has_value()));
            expect(svg->svg.find("font-family=\"Arial\"")!=std::string::npos);
            expect(svg->svg.find("#quot;")==std::string::npos);
            expect(svg->svg.find("[3]")!=std::string::npos);
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                std::filesystem::create_directories(directory);
                std::ofstream out(std::filesystem::path(directory)/(theme==std::u16string_view(u"dark")?"entry-dark.svg":"entry-default.svg"),std::ios::binary);
                out<<svg->svg; expect(out.good());
            }
        }
        auto packet=render_mermaid(u"---\nconfig: {packet: {showBits: false, bitsPerRow: 8}}\n---\npacket\n+8: \"Data\"",*session);
        expect(fatal(packet.has_value())); expect(packet->width==258.0);
        expect(packet->svg.find("class=\"packetByte")==std::string::npos);
        expect(!render_mermaid(u"pie\n\"Tea\":",*session));
        expect(render_mermaid(u"pie\n\"Tea\":1",*session).has_value());
    };
    "mermaid_real_font_measurement_has_no_estimate_fallback"_test=[] {
        MermaidMeasurer measurer;
        auto narrow=measurer.measure(u"iiii",{u"Arial",17});
        auto wide=measurer.measure(u"WWWW",{u"Arial",17});
        expect(fatal(narrow.has_value() && wide.has_value()));
        expect(wide->bounds_width > narrow->bounds_width*2);
        auto spaces=measurer.measure(u" A  B\tC\r\nD ",{u"Arial",17});
        auto normalized=measurer.measure(u"A B C D",{u"Arial",17});
        expect(fatal(spaces.has_value() && normalized.has_value()));
        expect(spaces->bounds_width==normalized->bounds_width);
        expect(!measurer.measure(u"x",{u"Folia Missing Family 792743",17}));
        expect(!measurer.measure(u"x",{u"Arial",0}));
        expect(!measurer.measure(std::u16string(1,0xdc00),{u"Arial",17}));
    };
    "mermaid_measured_pie_uses_resolved_font_for_title_and_deduplicated_showdata"_test=[] {
        PieDocument document;
        document.show_data=true;
        document.sections={{u"Wide WWW",3},{u"Wide WWW",9},{u"\u4e2d\u6587 \U0001f600",2}};
        document.title=std::u16string(100,u'W');
        PieSvgTheme theme;
        theme.colors.fill("#4477aa");
        theme.stroke_color=theme.outer_stroke_color=theme.title_color=
            theme.section_color=theme.legend_color="#112233";
        theme.font_family=u"Arial";
        auto svg=render_measured_pie(document,theme);
        expect(fatal(svg.has_value()));
        expect(svg->width>1000.0);
        expect(svg->svg.find("Wide WWW [3]")!=std::string::npos);
        expect(svg->svg.find("Wide WWW [9]")==std::string::npos);
        if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
            std::filesystem::create_directories(directory);
            std::ofstream output(std::filesystem::path(directory)/"measured-pie.svg",std::ios::binary);
            output<<svg->svg;
            expect(output.good());
        }
        theme.font_family=u"Folia Missing Family 792743";
        expect(!render_measured_pie(document,theme));
        theme.font_family=u"Arial";
        document.title=std::u16string(1,0xd800);
        expect(!render_measured_pie(document,theme));
    };
};
}
