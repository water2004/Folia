# 当前开发交接

恢复/压缩后实际读本文，只复查新变化。数学入口：[架构](architecture.md)、[所有权约束](native-raii-architecture.md)、[文件图](native-raii-files.json)、[验收](native-raii-acceptance.md)。ignored 候选用 `rg --no-ignore`。

## 当前 checkpoint（2026-11-04，0.6.0本地验收完成，待推送发布）

- 用户明确授权**做好验证后推送、沿既有CI/CD发布0.6.0**。版本已同步CMakeLists/build_msi/README/Package.appxmanifest/Folia.rc/About，release-notes/v0.6.0.md已写；仍未推送/tag/发布。不要agents/CUA/自动鼠标，native串行≤4；可正常关闭Folia用于构建但保留未保存提示。GitHub公开API确认最新发布v0.5.5，origin/master到本轮前HEAD有11个AI/设置/页签本地提交，需要一并推送。gh未安装，公开GitHub REST可查CI，发布由tag触发workflow，不要改流程。
- 核心光标修复已独立提交**2b36da6e**：通用insert_text按FencedCode CST补缺失正文换行，paste同一路径（借用view），空code正文Source锚点；独立header归现有BoundaryDecoration，AddBlockLines跳过纯装饰行且检查terminal Source边界，保留真实空行。**无新增Chrome/Control enum或引用/列表特判**（已撤销实验），临时friend/dump已清。普通/closed/tilde/quote/list双affinity42断言通过；最终core completion-decoration-core-tests.log765项500165断言、platform completion-decoration-platform-tests.log85项4256全过，portable completion-portable-tests.log13项74全过。无需重开这些审计。
- AI批已提交**673a4f94**：上下文KB（1000bytes/KB）/output token可设1–64/16–4096，默认7/128；三档prompt主动程度（保守/均衡/积极）独立模型reasoning；异步请求native ring/Ready/Empty/BudgetExhausted/Failed，generation/取消、限额空不缓存。**用户明确额度用尽只保留短提示，不额外解释**。新API Fetch读取GET /models的data[].id→effort.supported_levels，按API顺序去重/有界、协议校验过滤，不预设档位/不猜型号/不发生成试探；初始仅服务默认及已保存值。Model/base/protocol变化/Unloaded取消，弱UI/generation过滤旧结果；共享ReadHttp维持异步WinHTTP、GET/POST取消和大小时限。来源 https://api-docs.deepseek.com/api/list-models/，meta字段可省略；未真实请求上游，本轮仅loopback fixtures。新model max128与adapter一致。
- 滚轮：SettingsViewSupport.h ChoiceBox直接用SDK ComboBoxT composable覆写，闭合不委托base，未处理事件交ScrollViewer；展开用base，无IDL/模板。全设置页下拉采用Choice；预算改普通TextBox+from_chars范围检查，不再NumberBox/事后恢复数值。不存在DefaultNumberBoxStyle导致打开AI页退出的问题已随控件替换根除。UI fixtures先用fixture-key获取，**Save前清空PasswordBox，绝不能写用户凭据**。
- 已验：build/folia-060-debug-build.log标准Debug成功，EXE版本0.6.0.0/0.6.0；folia-060-completion-tests.log HTTP11/90+layout2/1056+controller13/127=26项1273断言通过，含metadata GET/缺失/取消/限额状态与代码围栏accept/undo。folia-060-presentation-tests.log排版9/316+加载5/22通过。最终folia-060-ui-tests.log设置/页签/chrome共35项1241断言全过（metadata save/切模型取消、无预设、闭合wheel原生IControlOverrides事件数据测试，无桌面输入）；明暗/720/1120截图已检查。folia-060-startup.log正式Debug两轮启动/正常关闭exit0；folia-060-activation.log Unicode/relative路径、多窗口、独立实例及正常退出全部通过。无待完成的功能测试。
- 本地MSI完整成功：build/folia-060-msi.log，Release自包含程序startup smoke正常退出0，WiX成功生成**build/installer/bin/Folia-0.6.0-x64.msi**（53886657 bytes）；无本地build/test句柄，不安装。WiX ICE61为既有AllowSameVersionUpgrades配置警告，0 errors。AI批与版本批分别提交；docs/SETTINGS.md已去QuickJS/旧schema/旧返回描述，inline-completion.md已更新API获取/预算/滚轮合同。下一步提交版本/notes，然后push master、创建/push v0.6.0，等CI与Release资产成功。
- 发布入口.github/workflows/release.yml唯一workflow：push v*.*.*触发；tagged commit须在origin/master，release-notes/v0.6.0.md必需；CI self-contained MSI+SHA256并publish。全部本地检查成功后git push origin master，再tag v0.6.0并push；监测CI及release资产成功才结束。公开API可GET api.github.com/repos/water2004/Folia/actions/runs与releases（User-Agent），日志可用GitHub插件fetch_workflow_job_logs；没有gh不需要安装。不要提前宣称发布。旧ignored候选/诊断清理自动审批拒绝，不重试绕过。

## 窗口退出与Base URL（已完成，历史）

- 后续用户报告Debug不补全、状态提示无效补全设置；用户重启后已自行恢复。读取当前Debug/Assets/settings.json（不输出密钥）enabled/baseUrl/model/protocol有效，凭据存在；复用生产obj作一次性native诊断，LoadAppSettings/Validate均通过，以固定短句实际RequestCompletionHttp返回6字符/1124ms（build/completion-config-diagnostic.log），未读取文档。Debug无需MSI。未复现重启前状态，不能宣称已定位该次根因；不加兼容或猜测性改动。临时诊断源码/脚本/exe/obj/pdb删除被自动审批拒绝（仅blocked by policy），留ignored build目录，未重试绕过；日志保留。下步仅按用户再次复现的新证据跟进。
- 用户要求AI配置只填Base URL，且明确**不要旧配置处理**。已将CompletionConfig/JSON/UI改baseUrl，ResolveCompletionEndpoint统一补协议suffix，裸host默认/v1，保留自定义路径/query、去末尾斜杠；未加完整端点模式/迁移。参考本地Fairy ResolveModelEndpoint的base规则，无Fairy依赖。正式test_completion_http覆盖三协议路径/loopback，全部通过，无真实模型请求。
- 用户指出窗口已关却仍占文件。进程12368为标准Debug，非侵入cdb证据build/folia-closed-process-stacks.log：主线程GetMessage等待、多个FrameScheduler线程；实际枚举9个不可见DWMCLOAK_APP=1的WinUIDesktopWin32WindowClass，为拖出预备窗口残留。已对其发正常WM_CLOSE并确认退出，未强杀。首轮Base构建因exe占用失败；释放后completion-base-url-build-retry.log成功。
- 已建立prepared window到source的弱所有权关系；源窗口Closed清理未使用host，Reveal清除归属，其他正常/已拉出窗口保留；App注册表清空后撤销激活注册并Exit。原测试before日志稳定6断言失败（3个hidden host未关）；修复后build/window-lifecycle-tests.log真实WinUI窗口/页签26项949断言全过，含跨owner隔离及已拉出文档保留。build/window-lifecycle-build.log标准Debug成功；window-lifecycle-startup.log正式程序2轮正常退出code0，window-lifecycle-activation.log多窗口/独立实例/激活/正常退出全过。
- Base URL/controller build/completion-base-url-tests.log共17项1175断言通过（HTTP5/41、layout2/1056、controller10/78）；设置UI回归build/completion-base-url-settings-tests.log共5项149断言全过。合计48项2273断言及真实启动/激活退出验证。生命周期独立本地提交77f2b46c，Base URL及本checkpoint同批本地提交（见HEAD）。标准Debug已更新，无运行Folia/build/test/agent；不自动鼠标、不用CUA，编译≤4。下步按用户反馈继续，无剩余本轮必做项，不重开已验结论。
- 仍0.5.5，未推送/安装/发布；之前蓝条简化提交453524e8已完成，用户允许1–2帧差，不能重开复杂同步。临时候选build/candidates/tab-presentation仍因自动审查拒绝递归删除保留ignored，不再重试绕过。

## 页签帧协调简化实现（453524e8，历史）

- 新证据：用户明确上一轮没有解决底部小蓝条延迟。已只读核实运行进程21972为Debug/Folia.exe，02:01:53启动，WorkspaceChrome.xbf为01:58:39新构建，不能归因于旧安装版本。上一轮修复了真实状态名错误，但只验Opacity/状态机，不证明屏幕呈现时序；不再宣称已解决用户反馈。
- 新验收边界：用户明确允许一两帧正常差异，要求干净实现；不追求0帧。桌面同帧采样已证实旧路径正文约20ms、蓝条53ms（before-atomic日志），简化Rendered版本多为同帧，偶有一帧差，符合新边界。上一轮0差异断言不再适用。
- 当前只留FrameUpdate RAII合并ActivateDocument中间绘制，Rendered一次性提交最终缓冲；关闭撤销订阅，正常滚动Present(0,0)/帧调度不变。**已移除**实验的CommitAsync/serial/CallbackState重命名、额外脚本参数和145行GDI像素测试（诊断日志留ignored build）。正式测试用DXGI PresentCount，验连续4次切页无中间提交、shell完成仅一次最终提交、选择/蓝条状态同步；产品改动49行，没有新线程/计时器/文档队列。
- 验证完成：标准Debug build/tab-presentation-simple-build.log成功；窗口/页签24项935断言tab-presentation-simple-tests.log，调度3项52断言tab-presentation-simple-frame-tests.log全过，合计27项987断言。调度ready-signal中位8.32ms仅为诊断，不当实际显示FPS。未改CST、文档事务、公式/AI策略。辅助脚本已清理；ignored临时候选build/candidates/tab-presentation仍在，自动审批拒绝递归删除，理由仅blocked by policy，未重试绕过。
- 停止点：本批实现/正式测试/本交接一同本地提交（见HEAD），标准Debug已更新，结束时无agent/build/test/Folia进程。版本仍0.5.5，未MSI/安装/推送/发布；后续按用户反馈继续，不重开0帧追逐或旧滚动问题。用户授权可关闭Folia用于构建（未保存提示保留），其余仍不开agents/CUA、不自动鼠标，native串行≤4。

## 页签模板状态名修复（818f3b5c，历史；不代表蓝条时序问题已解决）

- 用户报告内容已切换、页签高亮后出现。已核对当前安装SDK Microsoft.WindowsAppSDK.WinUI.2.2.1的generic.xaml：TabViewItem原生CommonStates是PointerOverSelected/PressedSelected，WorkspaceChrome误写成SelectedPointerOver/SelectedPressed，悬停/按下选中无法匹配，后续状态变化才显示。FolderExplorer的TreeViewItem有同一错误，一并仅改状态名；brush key仍为BackgroundSelectedPointerOver/BackgroundSelectedPressed，不能跟着换名。未改文档激活/调度/拖动协议。
- 修复前真实WinUI测试build/tab-highlight-before.log稳定8次状态匹配失败；修复后build/tab-highlight-final-tests.log页签/窗口迁移16项165断言、chrome7项762断言全过，包含重复原生选择、选中/旧选中indicator同步值，以及Selected/PointerOverSelected/PressedSelected与未选中状态的即时setters。文件树build/tab-highlight-tree-tests.log 6项98断言通过；合计29项1025断言。最终Debug build/tab-highlight-verified-build.log成功。未模拟鼠标；实际物理点击体验未另行人工复测。
- 停止点：本批模板/测试/交接同批本地提交（见HEAD），工作结束无运行agents/build/test。版本仍0.5.5，未MSI/安装/推送/发布；系统安装版未替换。不开agents/CUA、不自动鼠标/不关用户窗口，native串行≤4。后续按用户反馈或明确打包要求继续，不跟进旧流水线；设置列表修复93b70db1见下方历史基线。

## 设置列表卡顿修复（93b70db1，历史）

- 用户报告快捷键/LaTeX设置页卡顿。根因：StackPanel+外层ScrollViewer一次创建全部56/266行，Build/Navigate/Reset反复销毁重建；旧Debug同尺寸切页快捷键185–192ms、LaTeX123–127ms，后者1342个TextBlock。未涉及编辑器/CST/数学渲染/AI策略；版本仍0.5.5，未MSI/安装/推送/发布。
- 现两页惰性构建，有限高度原生ListView独占滚动，ItemsStackPanel CacheLength=0.5，ContainerContentChanging只生成视口附近行，回收时清模板ContentPresenter；数据不变保留source/控件。快捷键按Bindings比较，LaTeX目录/usage变更Revision失效。去掉全行Button强持有，录入仅弱引用当前Button；离页/回收取消录入，冲突/保存失败恢复模型值。
- 最终Debug build/settings-lists-verified-build.log成功；真实WinUI设置全套5项149断言build/settings-lists-final-tests.log全过（生命周期、回收/不变复用、usage失效、scope冲突/持久化失败回滚、录入取消、导航/全7页样式）。最终截图增量2项50断言build/settings-lists-layout-final.log全过；PNG build/tests/settings-lists/{shortcuts,latex}-{1120,720}.png已目视检查。相同物理窗口最终首入约57/13ms，重入约6/5ms，初始仅8/4行；测量build/settings-lists-timing.log，旧值另存settings-lists-timing-before.log，数值有运行波动，不做机器相关耗时断言。
- 测试必须等真实行Loaded且已布局，不能只数模板容器；不能设置ItemContainer.Content替代数据（会出现有容器无行），须用共享DataTemplate的ContentPresenter。WinUI允许保留焦点/anchor项，只验其余回收释放和总量有界。PNG需原生动画完成及不透明根背景，勿据RTB过渡灰块修改产品。
- 停止点：本批实现/测试/交接同批本地提交（见HEAD）；无运行agents/build/test。不开agents/CUA、不自动鼠标/不关用户窗口，native串行≤4。后续按用户实际反馈或明确MSI请求继续；当前系统安装版尚未替换，不跟进旧流水线。先前样式8aabdf9c、导航df7c3c87、AI4a27c813基线见下文。

## 设置样式统一（8aabdf9c，历史）

- 用户要求统一设置页，尤其移除AI页整张表单大框。现删除Card包装，AI改无外框启用说明/服务/凭据/操作区；全部7页统一标题/说明、分节间距、输入/主按钮/状态与滚动规则。版本仍0.5.5，无MSI/推送/发布，不动设置schema、编辑器、CST和AI请求策略。
- 共享WinUI主题字典theme/Settings.xaml由App.xaml合并，绑定现有FoliaShell*/FoliaUi*主题颜色/字体，构造前使用Native资源别名；各页去掉硬编码灰/蓝/红brush。许可证列表继续虚拟化；主题预览窄窗口上下排列。SettingsViewSupport仅消费字典并生成语义控件。
- 验证：最终Debug build/settings-styles-final.log成功；完整生产WinUI应用宿主中3项97断言build/settings-styles-final-tests.log全过（7页标题、AI明暗/1120、720宽对齐/无框/主题色、控件生命周期、3轮返回导航）。Native PNG build/tests/settings-styles/{light,dark}-{1120,720}.png，已目视检查内容区。截图须渲染完整root，单独host缺祖先背景会呈灰；NavigationView选中指示使用Composition位移，RTB可能保留其原位置，不据此改原生控件。
- SettingsView现明确依赖应用共享主题字典，其lifetime测试移至tests/app/visual，使用正式App初始化；不在简化controller host注入补偿资源。真实启动/关闭复查build/settings-styles-startup.log。当前无运行agents/native build/test；不开agents/CUA、不自动鼠标、不关用户窗口，native串行≤4。
- 停止点：实现/共享资源/测试/本checkpoint同批提交（当前HEAD），后续按用户实际体验或明确MSI请求继续；不擅自跟进旧流水线/发布/安装。已完成导航基线df7c3c87、AI基线4a27c813见下文。

## 设置导航修复基线（df7c3c87，历史）

- 已完成用户要求：Folia菜单用直接“设置”入口替换“查看”，设置页原生NavigationView Back可返回正文；重复点击设置为幂等打开。根因是旧页隐藏Back、只剩View→Settings toggle。移除旧View子菜单/重复pane菜单/同步和无用toggle方法，标题栏文件/大纲按钮保留。版本仍0.5.5，无MSI/推送/发布。
- CloseSettings为必需callback，应用传弱MainWindow，SettingsView.Detach撤销Back订阅。Esc走Root未处理的KeyDown返回正文，先让下拉框/弹窗/快捷键录入处理，不用优先抢占的全局Esc accelerator。未修改CST、历史、文档内容。
- Debug成功build/settings-navigation-final-build.log。真实WinUI Automation菜单/Back反复3轮（含AI页、重复打开、正文/状态栏/原标签恢复）1测试24断言build/settings-navigation-tests-3.log；settings lifetime1测试11断言build/settings-navigation-lifetime.log；标题栏pane/窄窗口1测试26断言build/settings-navigation-panes.log，全部通过。测试按原生Closed事件等菜单完成关闭再打开，不模拟鼠标。
- 停止点：实现/测试/本checkpoint同批提交df7c3c87。该批结束时无运行agent/build/test；不开agents/CUA、不关用户窗口，native串行≤4。下步按用户反馈或明确MSI请求继续，不跟进旧流水线/擅自发布安装。AI已完成功能见下方4a27c813基线。

## AI输入预测基线（4a27c813，历史）

- 用户先不管0.5.5流水线，做Copilot式预测补全。Folia完全独立；设置→AI输入预测配置完整地址/协议/模型/API Key，默认关闭。不开agents/CUA、不自动鼠标/不关用户窗口，native串行≤4。没有真实模型联网、MSI、推送或新发布；版本仍0.5.5。
- 来源：MIT providers（Fairy ef60077797ddadc82ee934d2e9d80ce05cf67830）与nlohmann/json3.12.0均vendor，license/source-lock/sha256齐全，27协议源码hash核对；gitattributes锁定LF。无Fairy工程/运行/配置/凭据依赖。vendor已提交1c5617af，portable预算/缓存/fixtures提交09415f8c。
- 架构与使用：docs/inline-completion.md。src/completion只管有界上下文/prompt/LRU/proposal；WinUI独立controller/设置/WinHTTP async/凭据管理器。后台单worker+最新pending、防抖450ms、请求间隔≥1.5s、15s deadline、过期结果丢弃、无自动重试/无UI网络等待或join。同步WinHTTP跨线程close方案已替换，不恢复。
- 成本：读取block/line source views先限额再编码，4KiB prefix+1KiB suffix+2KiB preceding，默认128输出token/512码点；32项/256KiB内存LRU。10MiB文档不全文复制/序列化/上传。长段落前缀起点按256码点分段移动，帮助服务cache但不保证命中。按词接受/匹配输入沿用余下建议；CST语义配对或结构编辑改变位置/后继源码则作废重算。
- 灰字共用DirectWrite flow，正文/标题/引用/列表/任务/表格/代码/空文档/源码通用；主题muted色、原字号换行、无Markdown样式。所有建议字符及换行布局装饰均非Source（GeneratedPreview单独分类，caret仍在建议前）；不进CST/save/selection/print。Tab整段、Ctrl+Right按ICU词且不拆emoji/组合字符、Esc取消，原补全弹窗/snippet优先；接受走原Paste可逆事务，IME期间不请求。
- 验证：build/completion-core-final-2.log 11测试63断言（10MiB/Unicode预算、稳定prefix、LRU、3协议SSE）；build/completion-app-regression.log完整controller153测试19873断言通过。最终增量Debug build/completion-final-review-build.log成功，15测试1145断言build/completion-final-review-tests.log全部通过，覆盖async loopback HTTP/取消/错误、旧请求拒绝、缓存/typed-through/语义配对失效、Unicode词/Undo、明暗16容器实际layout/brush/caret/缓存失效/非Source映射、源码邻近行/反向选区/零全文操作。真实启动关闭build/completion-startup.log exit0；最终同项复查build/completion-final-startup.log。
- 停止点：应用批次为4a27c813；该批次验证结束时无agent/构建/测试在运行。后续按用户需求配置实际模型验证内容质量/延迟，或构建MSI；不冒称已有真实服务验证，不擅自跟进旧流水线/发布/安装。

## 0.5.5发布（用户暂不跟进流水线）

- 用户授权patch版本+1、推送并发布Release，目标0.5.5。六处版本入口已统一，release-notes/v0.5.5.md描述嵌套数学修复、括号编辑、UTF8界面；沿用下列已完成的Debug/core/controller验证。
- 沿用.github/workflows/release.yml，推master与新v0.5.5后监看CI构建/启动关闭/发布，下载正式MSI及sha256校验。不移动既有标签，不自动安装。
- master/v0.5.5已推送，发布commit023c6cc4446a5f2039c32d4afc52600efe5dfa73；CI41665272925/job126018787207进行中。下一步等CI发布成功，下载正式MSI及sha256核对后更新此处并提交交接。无agent/本地build句柄；不开agents/CUA、不动用户鼠标/窗口，native串行≤4。

## 核对mathbf希腊字母（89affb85，已完成）

- 用户怀疑mathbf{mu}未加粗是移植遗漏。已核对锁定MathJax4.1.3 ParseMethods.ts:113 lcGreek：mathStyle(char)||italic，不继承env.font；ucGreek才先用env.font。native parse_methods.cpp:88逐项一致，MathFont映射也一致。**无生产代码遗漏，不改上游语义**。
- 实际MathJax/NewCM与当前build/core/FoliaNativeMath.dll对照6式：mu、mathbf{mu}、mathbfit{mu}均1D707；boldsymbol{mu}=1D741；mathbf{m}=1D426；mathbf{Gamma}=1D6AA。六式SVG glyph path全部一致。证据build/tag-layout/greek-font-check/{source.json,source-*.svg,native-*.svg,source.cjs,compare.py}。初次桥接未传cancel按接口返回status1，补有效Cancellation后全部通过，无进程异常。
- 用户应使用boldsymbol{mu}得到粗斜体mu；当前仅核对记录，无构建/安装/版本/发布变更，无活跃句柄。后续按用户新要求接续；不用自动鼠标/agents/CUA。

## 数学括号编辑（b5c342c9，已完成）

- 用户要求基于匹配的补全/成对删除，参考VSCode，不能无脑插()。新增math_editor.SyntaxSession.PlanBracketEdit，复用容错TeX tokens/pairs，支持()[]{}、已有closer复用/越过、注释/转义排除、选区包裹与left/right。删任一已匹配括号去两端保内容。
- Command.typed_input区分直接输入与paste/snippet；Keyboard/CoreText接入（组合输入不干预，CoreText单字符删除回归语义Backspace/Delete）。EditorSession只通过CST/BlockSource定位数学内容，按content_to_source映射编辑；通用document_replace_text以一次原生块内源码事务记录前后选区，无全文重建。空内容不能走document_insert_text（会补分隔换行，且拒绝空replacement）；已用直接edit_inline/edit_block_source修正。
- 验证完成：最终Debug math-pairs-input-build.log成功；括号4项188断言通过math-bracket-fixed-tests.log；core全套math-pairs-core.log成功；应用controller全套math-pairs-app.log成功（含组合输入生命周期、原有数学/片段/滚动/撤销等回归）。首轮空内容3失败已由literal transaction修正，不复用会补换行/拒绝空替换的typing接口。
- 停止点：实现、测试和本交接一起本地提交（见HEAD），无运行agent/build/test句柄。没有安装/MSI/版本变更；后续按用户新反馈/打包请求接续。当前数学输入规则用于语义化公式编辑；源码视图继续其原有直接源码输入流程，未声称在源码视图启用括号规则。
- 继续遵守不开agents/CUA、不自动动鼠标、不关用户窗口、native串行≤4。

## 嵌套块公式与源码编码（e73fa69b，已完成）

- 用户报告引用块公式缺高亮/图标、光标错误及异机标题栏dirty点乱码，要求排查所有容器。共同根因：render_builder.append_flow_block把MathBlock降为只有TeX的InlineRenderItem，丢失raw_source/source_markers/content_to_source；现flow item共享原RenderBlockSpecial，调用同一BuildMathBlockText并MergeDisplayText，无按引用/列表等特判，无第二坐标/重复解析。代码块和CST行内数学已有完整独立投影，未改它们职责。
- WinUI项目增/utf-8；原CharacterSet=Unicode只影响API，不能保证UTF8源字面量的解码。包括关于页链接末尾↗、标题栏●等所有CPP字面量；URL本身ASCII正确。真实控件dirty.Text U+25CF、关于链接文字U+2197和NavigateUri合计1项6断言通过ui-unicode-final.log。测试run.ps1剥/TP源文件尾时也会误删随后/utf-8，已显式补齐，避免测试与产品编码不一致。
- 最终Debug成功nested-math-final-build.log；最终core全套nested-math-final-core.log成功；29项3064断言nested-math-final-app.log成功，含引用/列表/提示块真实renderer hit-test、展开/输入/Undo，及9类嵌套/空/不完整数学源码/图标/highlight/逐字符映射与零全文操作。原行内呈现9项316断言通过nested-math-inline-regression.log。共享pointer放在稀有semantic payload里，普通marker不扩大。
- 停止点：源码、回归测试和本交接一同本地提交（见HEAD）；无运行agents/build/test句柄，未构建MSI/安装/发布新版本。后续按用户新反馈/打包请求接续；不自动移动鼠标/不开agents/CUA、不关用户窗口，构建串行≤4。

## 0.5.4发布结果（历史）

- 用户确认“ok了”并授权推送、构建、发布0.5.4。版本入口（CMake、MSI默认、RC、manifest、设置页、README）统一0.5.4；release-notes/v0.5.4.md已准备。只做版本/发布资料更新，沿用下列已完成的功能验证。
- 发布沿用.github/workflows/release.yml：master提交后推v0.5.4，CI构建self-contained MSI、真实startup/close检查、SHA256、GitHub Release。既有0.5.3标签不移动。
- 已发布：38231c6a4b62513bd7b234fc565f2279b1b91303，CI40629886020成功；2026-11-03通过GitHub API核实Release与MSI/sha256存在。正式MSI53,693,825字节，API digest sha256:23ac23ba4c95f1d59758d0e8c69a1ee8bf72876b947f6b70d2e599a09d30952b；本轮未重新下载MSI。不改既有标签。

## 已完成的标签修复基线（9815cfcb，历史）

- 新反馈推翻“cloak已解决点击闪烁”的验收：按实际SDK SetWindowPos(SWP_SHOWWINDOW)测试稳定发生源窗失活；上轮Show(false)测试遗漏激活。before日志tab-activation-before.log。EnableWindow(false)仍未阻止失活，已撤回该尝试。
- 本轮修复：PreparedTabActivation UI线程局部CBT guard只在预备HWND上否决activate/focus，真正Transfer/Closed后RAII移除；保留WinUI拖动协议。另查SDK PopulateTabViewList包含AddButton，而DragTabWithinTabView只接受TabListView，边缘位置可刚合并又拆出；外部合并用同一list坐标边界、拒绝hidden/prepared窗口、转移后同步完成target layout。
- 验证：Debug成功tab-activation-guard-build.log；标签15项156断言通过tab-activation-final-transfer.log（含恢复正常窗口激活）；额外窗口定位/SetWindowPos成功返回检查1项6断言通过tab-activation-placement-check.log。完整visual的文件树6/95、chrome6/696通过；语言弹窗首次等待超时，定向重跑1/28通过tab-change-language-picker-check.log。
- Release/MSI成功tab-activation-msi.log，真实startup/close exit0。当前候选build/installer/bin/Folia-0.5.3-x64.msi，53,669,236字节，SHA256 e7076cebc04557585357faf964dc2aa0caee7dce40ab9c6e8e5c7bb051430271；邻接sha256已更新。未安装/推送/替换线上Release，版本保持0.5.3。
- 用户指出鼠标坐标模拟未命中并反对这种测试，已删除该测试/helper/临时运行脚本。超时不作为产品故障证据，**不再自动移动鼠标**；真实鼠标手势仍需人工复测，不声称已验收。不开agents/CUA、不关用户窗口，native串行≤4。
- 停止点：实现、测试及本交接一同本地提交（见HEAD）；无活跃agents/构建/测试句柄。下一步是候选人工拖动复测或按用户新反馈接续，不重跑已完成广泛检查。

## 上一轮已完成基线（历史，不替代当前checkpoint）

- 用户报告两个空窗口合并崩溃、合并后切标签闪窗、拉出空文档。本轮仅授权修复，版本保持0.5.3，不重新发布/移动既有标签；不开agents/CUA、不关用户窗口、native串行≤4。
- 新证据推翻此前“同TabViewItem迁移/NewWindowId可靠”的结论：实际DLL标记源码693f472aeaf89c6dff66256ee6fcd453e5d553b8的TabView.h/.cpp中，TabTearOutRequested.NewWindowId仅默认初始化从未赋值；TabViewListView缓存原parent且跨窗不刷新。SDK还在普通点击预建并Show/Hide窗口（官方issue10155）。源码证据build/tag-layout/TabView-installed.{cpp,h}、TabViewListView-installed.cpp、TabViewItem-installed.cpp。
- 已完成修复：TabItemsSource改为稳定boxed文档id，TabItemTemplate在每个窗口创建本地控件，不跨XamlRoot搬UI；原session/controller/history/cache仍原样转移。用args.Items查当前owner，每次gesture在DocumentTab记weak host、原move-loop的ExitedMoveSize清理，单页原窗口拖动也覆盖。预建窗口DWM cloak、不进switchers/noactivate，实际Transfer后才恢复展示。
- 初次测试的新代码startup崩溃已定向定位并修正：InputNonClientPointerSource与AppWindow一样不支持弱引用，auto_revoker会崩；改RAII强source/token订阅。tab-data-subscription-startup.log真实startup/close通过。不是用户原崩溃根因，不混淆。
- 已验：Debug最终构建tab-selection-boundary-build.log通过；完整visual **25项946断言**（tab-regression-final-visual.log、对应-stdout.log），3次真实startup/close全exit0（tab-regression-final-startup.log），真实多窗口激活/Unicode/相对路径/独立进程全过（tab-regression-final-activation.log）。包含未加载目标、空页合并/正常关闭源窗、同gesture反复合并拆出、click-only cloak/switchers、原会话/history/viewport、duplicate拒绝。
- 选中标签原生排序另有可复现失败：SelectionChanged→ActivateDocument→RefreshDocumentTabs反向修改集合，顺序无法一致（tab-selected-reorder-before.log超时）。已分离SelectDocumentTab（仅选择）与模型结构变化后的RefreshDocumentTabs；定向after 1项4断言通过，再跑上述全套成功。旧直接Transfer空页/未加载target本来也过，不能把它当原生协议已覆盖。
- 上轮代码22bea8fe及交接b0c4dccf仅本地提交。其MSI已被当前checkpoint候选替换，旧产物大小/散列不再用于交付。
- 用户真实安装版10:46崩溃记录0xc000027b、WER8000ffff；转储C:/Users/think book/AppData/Local/CrashDumps/Folia.exe.52828.dmp。缺stowed异常指针内存，未证明具体异常调用；不冒称确定根因。
- 0.5.3已正式发布且不改标签：35e56265，CI40577471717/job122447644967成功，Release https://github.com/water2004/Folia/releases/tag/v0.5.3 ，MSI53,722,497字节，SHA256 2ca50713200c8f5f5769c35b172ff5dbfac07b15fe2e686500a9c9625a889a35。历史全套基线core764/500069、platform84/4249、controller134/18718、visual19/875。

## 本轮实现与已解决问题

- 重载先清documentLoading再发通知，RAII保留异常/取消清理和loadSerial校验；不是CST或数学引擎问题。失败保留原文等5项61断言已验。
- 数学浮层与正文共享CreateFlow/PositionMath/PaintMath，按窗口可用宽度原字号自然换行，**不缩放、不内部滚动**。明确SVG intrinsic/paint尺寸契约修复片段重叠；单个vector mask/shadow effect，7主题mathPreview提供柔和阴影。用户原式与锁定MathJax4.1.3完全一致（native-3/source-3.svg）。
- 独立folia::math_editor：共享Tree-sitter runtime、容错增量TeX语法，UTF32 API/UTF8 tree/RAII；仅词法/括号/left-right/环境，不复制严格MML parser或宏求值，不加载字体。UI只用CST数学source span和SourceDisplayIndex转UTF16，无dollar扫描/第二坐标。9容器、Unicode、不完整TeX、编辑撤销零全文操作已验；原生6/14839、TexScanner对照1/36、生成可复现通过。外层未闭合dollar未单独覆盖。
- 括号保留用户指定**淡背景+细边框**，不改下划线/选区色/蓝色强调；主题editor字段，明暗像素测试已验。光标共享EditorFrameScheduler；静止只用单次blink timer，移动/fade持续帧，滚动直接跟随offset，真实selection/IME不变，尊重系统动画。
- 跨窗口现行设计见 docs/workspace-windows.md。同一session/controller/稳定数据item移动，UI控件留在各自窗口，保留dirty/history/selection/viewport/math cache。Presentation/RebindPresentation只换窗口callbacks，不能重置savedContentState/reloadState；process唯一session id，同路径不同会话拒绝。TransferTab按值持shared_ptr，不引用即将erase的map元素。
- 历史结论已被实际SDK源码推翻，不能按args.NewWindowId找目标；按gesture host跟随数据item，WinUI负责拖动结束后空源窗口关闭，不提前Close。AppInstance异步重定向支持普通启动同进程多窗口；标准PropertySet按值带绝对路径，key隔离exe完整路径。App::Current经自实现IXamlMetadataProvider（Application为composable base，不能get_self<IApplication>）。
- 真关闭异常：晚到PointerExited裸this访问已释放surface，0xc0000005。统一WindowEvent弱owner门控、分发期间持强owner、Closed后丢弃，renderer/frame同样受生命周期约束；证据tab-close-{focused,stack-focused}/run-1。不在pointer controller堆释放补丁，不能冒称旧0.5.1CI已证明同根因。
- 源码Undo/Redo行id漂移：过粗的历史块投影丢未变行，minimal_source_replacement只更新真实变化范围。保持块事务、不加源码快照/旧行缓存；10轮exact selection/line id与零全文操作通过。
- CoreText重绑后若surface仍聚焦且HWND foreground，显式恢复context，不能等待不会重发的GotFocus。500段长文转移后pump200ms验证viewport。
- controller与visual **编译串行**，共用PCH VC145.PDB时并行会C1041。激活测试独立候选目录保留Folia.exe/PRI原名（hardlinks/junctions），改exe名会坏PRI key，不是IPC丢参。LLDB缺liblldb.dll不再尝试，startup已有DbgHelp实时栈。

## 已确定边界

- 数学原生SVG已接入；Common共享布局、Scene所有权、SVG仅序列化，未来D2D消费同Scene。不恢复QuickJS/JS对象语义模拟/并行旧实现。
- MathJax4的38包加mediawiki-texvc别名包；lang/rang等101定义已验，v4同名优先。正式Source fixtures/锁文件不可当临时文件删。编号percentage/min-width已修；aligned内tag原版也拒绝，应放end{aligned}后。
- 字体按一次请求加载，统一批量接口；提前异步准备公式，滚动无同步IO，离屏取消，编辑预览仅内存。通用流程不特判有无公式。
- 编辑遵守AGENTS.md统一块树、块内source/CST、单一source selection、可逆事务；正常编辑零全文parse/serialize。marker图标只呈现语义，数学定界符删除两端保留内容。
- 文件树左/大纲右，标签标题栏同排，单击预览双击/编辑固定，无文件夹默认隐藏侧栏；完整矢量画布不恢复裁切PathIcon。滚动时钟已修无新证据不重查。
- controller正式用Debug；Release产品真实startup。Release console XAML宿主0xc000027b是已知限制，宿主旁支已撤回不重复/不冒称通过。

## 独立待办与界限

- Mermaid暂停。主题外置未全部完成：WorkspaceIcons.cpp的8个Fluent路径、WorkspaceChrome/FolderExplorer/MainWindow XAML样式及theme.ixx默认值；本轮未扩展。
- 已验历史基线CommonMark652/1304、GFM1428/2878、math977/546948；不重复广泛审计。现行模块整理已提交，历史查Git，不新增过程归档。
- 历史0.5.1首轮CI关窗异常无artifact，根因未证实；重跑40497361463成功发布；e23af71a增加后续CI失败dump/exe/PDB/log。
- 4个历史清理路径被自动审批拒绝，未重试不绕过：build/tests/winui/Release/controller、build/generated/themes、build/debug_winui_test.py、tools/themes（若仅pycache）。
