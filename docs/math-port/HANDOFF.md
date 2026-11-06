# 当前开发交接

恢复/压缩后实际读本文，只复查新变化。数学入口：[架构](architecture.md)、[所有权约束](native-raii-architecture.md)、[文件图](native-raii-files.json)、[验收](native-raii-acceptance.md)。ignored 候选用 `rg --no-ignore`。

## 当前 checkpoint（2026-11-06，Responses 与推理力度输入）

- 0.6.4 生产补全代码无 DeepSeek 域名或模型名分流。Responses 现在用标准纯文本消息表示补全示例的助手回复，见 `f149e0e6`。WinHTTP 保留短错误码、类型和参数名，错误响应正文最多读取 8 KiB、等待至多一秒。截图中的 OpenAI 400 尚无上游错误详情或真实 OpenAI 复测，不能确认实际拒绝原因。
- OpenAI `GET /models` 无推理档位字段。继续只读取上游公开能力信息。手填推理力度时用 WinUI `TextSubmitted` 明确保留文本，显示选项来自上游，留空走服务默认。
- Debug 已构建。HTTP 本地回环 21 项 251 断言及加载 5 项 22 断言通过，日志 `build/completion-responses-http.log`。WinUI 设置 9 项 131 断言通过，日志 `build/tests/winui/Debug/window/reasoning-focus-all-stdout.log`，覆盖手填、焦点切换与保存。测试通过 WinUI 焦点 API，未重现用户的物理输入序列。无运行构建或测试句柄。修复在本地提交，已发布的 0.6.4 MSI 不含本轮改动。
- 已从 `482b5d53` 重建本地 x64 MSI `build/installer/bin/Folia-0.6.4-x64.msi`，包含 Responses 与手填失焦修复。MSI 与 EXE 版本均核实为 0.6.4，53,911,538 字节，SHA-256 `84c4107602585ed28f82a8e6673bcd0cde2e40ff0f73394350be1e0595b49524`。日志 `build/folia-064-responses-msi.log`，打包跳过额外启动测试，WiX 只有既有同版本升级警告。构建已结束，本地包与公开发布资产的校验值分别记录。

## 0.6.4 已发布（历史）

- 0.6.4 包含自 0.6.3 起的模型列表与手动配置、运行时补全提示词、接续公式和代码块的局部解析修复。版本元数据和英文说明 `release-notes/v0.6.4.md` 随发布提交。LaTeX 目录一致性检查和 7 项目录测试已通过。
- `v0.6.4` 指向 `527ec902caa1d7a29e0dc5f9d7612088f8864d2f`，已与 master 推送。Actions [42180268451](https://github.com/water2004/Folia/actions/runs/42180268451) 因账户账单锁定未启动构建。按用户确认，沿用该标签，用 `build_msi.ps1 -Version 0.6.4` 本地构建并发布 [Folia v0.6.4](https://github.com/water2004/Folia/releases/tag/v0.6.4)。发布 ID `436215960`。
- 公开发布的 x64 MSI 为 53,936,114 字节。MSI 与 EXE 版本均为 0.6.4，真实启动及正常关闭通过，WiX 只有既有同版本升级警告。SHA-256 `8e89e3a784f8b3ecd3f093e206a612756ba4429eb8e5765ee2be1c3eec84a1b4` 已与 GitHub 公开资产 digest 和校验文件核对。日志 `build/folia-064-msi.log`，发布核验 `build/folia-064-release-verification.json`。
- 发布工作已结束，无运行构建或上传句柄。保留现有标签和发布资产，等下一项用户请求。

## 局部结构编辑计划重构完成（发布前验证，历史）

- 用户要求整理d8b1961e维护性问题，已完成：`document_block_reparse.ixx`将只读规划与事务应用拆开。`BlockReparsePlan`持有解析后blocks及parser-local光标映射；规划不改文档/ID/选区；应用才分配文档ID、统一记录树编辑，不重解析Markdown。普通字符输入复用同一规划/应用，SingleBlock与Fragment是命令语义范围，不是旧新兼容路径。
- `document_paste.ixx`移除boolean预解析/serialize检查、内部namespace访问、重复结构事务追加。接续段落保留预先解析的plan，raw块先做既有source插入再规划，最终同一应用入口。结构判断/局部往返验证只在结构模块，仍block-local source/CST+source/tree单事务，无全文parse/serialize、第二坐标或AI字符特判。源码批与此记录本地提交，版本0.6.3，未推送/发布/安装/MSI。
- 验收均通过：`build/completion-plan-core.log` **15项274断言**（paste过滤），增加只读规划不改source/IDs/selection及`block_fragment_parses == 1`；计数器在markdown instrumentation/parse_block_fragment，用于实际解析次数约束。`completion-plan-reclass.log` **3/27**、`projection.log` **1/212**、`incomplete.log` **1/28**（后二日志同completion-plan-前缀），验证共享普通输入路径。`build/completion-plan-controller.log` **21/314+loading5/22**，root/quote/list实际AI Accept、重载渲染类型与精确Undo/Redo沿用上轮正式测试。本轮未调用API/模拟鼠标/重跑广泛数学或全部核心。
- setup成功`build/completion-plan-setup.log`，Debug成功`build/completion-plan-debug.log`，exe **build/app-winui/bin/x64/Debug/Folia.exe**。编译≤4/native串行，无运行本任务build/test/agent/CUA句柄。**已安装Release尚未包含d8b1961e及本次重构**，下一步按用户试用或明确MSI要求打包，不能把Debug成功当安装已更新。
- 已修问题背景：d8b1961e解决接受`$$`后的补全时仅解析新增文字、把闭合符错当开头吞掉后文。接续现以owner完整源码识别，loaded raw闭合+正文也可拆兄弟块；前次core14/262与controller21/314见`completion-structure-*`，本轮以上更新日志优先，勿重复旧调查。
- prompt独立运行时配置，397851b6已同步repo/Debug/Release Assets及`%LOCALAPPDATA%/Folia/Assets/completion.json`，修改后进程须重开。真实API历史样例不是结构接受验收，不能代替本轮controller测试。现有**build/installer/bin/Folia-0.6.3-x64.msi**仍旧51.4MiB（115c1ee8，`build/folia-063-runtime-completion-msi.log`），没有新JSON/结构修复/重构，用户要MSI时必须重新构建0.6.3。不查看已完成发布CI。

## 补全协议/上下文系统性修复完成（历史基线）


- 用户反馈近处上文无效、首换行/公式参数异常、关思考后invalid edit，要求系统性修正，**不逐例加prompt规则且不标记Markdown/代码/TeX，由模型推断**。已按此完成并提交c421df12，版本0.6.3，Debug已更新；后续MSI状态以顶部checkpoint为准。不开agents/CUA/OS输入注入，native串行≤4。
- 代码根因与最终边界：旧oldText/newText JSON迫使模型精确复述源尾并转义TeX，严格schema拒绝；prompt把下文放到当前正文前且分两条user消息；邻近只取3 owner/line，Source行被拼成双换行，输出硬限4行。现由客户端定义同线32-scalar源尾，模型只回同一marker包裹的原文，**无JSON/old echo/模型坐标/旧格式fallback**。空响应abstain，空frame删除，边界名不出现在输入源中；原文首换行、TeX反斜杠保留，仅CRLF/CR→LF，字符限512、无4行限。客户端trim公共尾使复制右源不进事务/不推caret；沿用diff预览、word接受、既有Paste源/树单事务，不加语法修补。
- 上下文按阅读顺序在单一user消息连续呈现，稳定之前文本仍在活跃区之前，右文本不再仅为cache移到前面。无环境类型标签；根据实际可用源码由模型判断。邻近按预算可超过3行/块，Source模式真实单换行，复用cached editable order与views，工作量有界，无全文parse/serialize。completion模块已移除无用JSON include path。
- 验收全过：**completion-contract-portable-final.log 21项4629断言**（含300次Unicode右源/caret性质）；**completion-contract-controller-final.log 20项280**、**completion-contract-http.log 20项237**（三协议单user消息原文顺序）、**completion-contract-interaction.log 6项82**、**completion-contract-layout.log 2项1056**（controller/HTTP/layout各另loading5/22）。**completion-contract-debug.log**成功，**completion-contract-startup.log**两次真实启动/正常关闭exit0。测试覆盖raw TeX/首换行/5行/empty/delete/frame collision、超3邻近/Source换行、公式参数/源尾复用/部分接受/精确Undo和零全文操作。
- 验证限制：**未调用真实上游模型/修改用户key，不宣称已验证实际模型预测质量或cache ratio**。Tex测试首轮2次fullserialize来自测试自身TextUtf8比对，现已隔离计数区间，产品接受/Undo均0，不当作架构回归重查。
- **历史基线**：实现及测试提交c421df12；当时未重打MSI，现有同名MSI已由顶部构建checkpoint更新，不再使用旧包说明。

## 标准协议与手填配置完成（历史基线）


- 用户最新要求已完成：删除DeepSeek域名/路径/认证特判，按所选API协议+Base URL获取模型；模型及思考档位均可下拉或手填。候选只来自上游标准能力，缺失时明确提示可手填，不猜模型名/不预设档位。此要求取代5cbdfd6f的provider适配。用户确认0.6.3发布完成且不查看CI；当前仍0.6.3，本轮只本地提交，不推送/发布/MSI/安装。不开agents/CUA/OS输入注入，native串行≤4。
- 网络边界：三个协议统一Base URL + suffix，保留显式路径/query，各自标准认证，无host remap/fallback请求。OpenAI Models只基本ID信息，无标准档位字段，忽略私有effort.supported_levels；Messages读取capabilities.effort显式supported及标准分页。模型GET无需预先选模型/生成预算。生产completion/settings已rg无DeepSeek/supported_levels。
- 输入边界：两个原生ComboBox IsEditable，模型显示ID/name仅tooltip；Text就是保存值。思考手填不加入上游候选，空值/服务默认不发参数。候选更新和模型切换保留显式effort，刷新保留手填model/effort即使列表遗漏。只在用户手填未匹配项时SelectedIndex=-1再设Text，避免WinUI retemplate把它覆盖成默认项；实际内部TextBox→Save/清空已验。vendor ModelValidation移除固定力度枚举，只验32字符ASCII token，三协议依然用各自标准字段，具体值交服务判断；source-lock保留upstream hash并记录局部变更。原credential snapshot/弱交付/取消语义保留。
- 正式验收全过：setup/Debug与最终**standard-manual-selection-build.log**成功；**standard-manual-http.log 19项225断言**（另loading5/22），正式run.ps1 **standard-manual-ui-verified.log 8项125断言**，**standard-manual-width-wheel.log 2项96断言**（含可编辑/不可编辑closed wheel），**standard-manual-startup.log**两次真实启动/正常关闭exit0。明暗720/1120原生PNG已检查；27项vendor hashes匹配、中英资源key集合一致。无真实上游请求/用户key修改，不重复不相关数学/编辑广泛测试。
- 测试记录：临时增量脚本曾在125断言全过/result文件0后因Process.ExitCode读取方式返回失败；正式run.ps1缓存Handle并Refresh已复核exit0，不是产品崩溃。临时脚本也已修成同样方式，但不替代正式验收日志。UI测试只通过原生属性/automation peer/生产Click，未注入OS输入。
- **停止点/下一步**：本批源码、正式测试、docs与本checkpoint一起本地提交（见HEAD）；Debug已更新，无运行native/build/test/agent句柄。后续按用户体验或明确发布要求继续；不再查看已完成发布，不移动tag。标准依据见docs/inline-completion.md的OpenAI/Anthropic官方模型schema链接。

## 0.6.3发布（用户确认完成，历史）

- 用户确认发布已完成且不要再查看；版本commit **aceafbb507557d7d32d8d136c84bbe546138fae9**、annotated v0.6.3已推远端。英文notes与本地Release/MSI预检成功；不声称本轮另查正式CI资产，不移动tag、不安装。

## 多窗口IME与引用公式验收（已完成，历史）

- 用户两项请求均已完成代码/正式测试：窗口A→B→A后输入法不可切换；引用空$$正文光标偏到引用竖线。分批本地提交 **6c514ffc**（输入焦点）与 **3ef97d92**（数学源码几何），本checkpoint随最后文档提交。版本仍0.6.2，本轮未推送/新发布/MSI/安装，不移动旧tag。不开agents/CUA/OS输入注入，native串行≤4。
- IME责任边界：FocusEnter只允许本UI线程GetActiveWindow所属且surface有XAML焦点；注册/撤销CoreText.FocusRemoved清focused_；MainWindow激活用weak排到WinUI焦点转移完成后，重查active HWND/当前编辑控件/设置状态，失活立即Leave；InitializeTextInput同条件。无强制中英/布局模式、无每键重建context、无焦点抢占重试循环。原后台迟到Enter能改变前台实际TSF document，before2断言失败，修后正式4项58通过。
- IME验证限制：只是慢速激活来回旧版也能过；测试通过ITfThreadMgr.GetFocus比较真实系统文档身份，覆盖迟到通知、12次只激活窗口、快速排队、原生TextBox不被抢回。**未发送Shift，不能声称完整手动复现/验证用户输入法转换体验**。不要重试peer.NotifyFocusEnter等FocusRemoved（正常peer切换不发该事件）。不要用GetForegroundWindow代替thread-active HWND（隐藏启动/进程未全局前台是另一语义，曾使测试GetFocus空，UT fatal后已聚焦修正，不是产品CRT崩溃）。依据见docs/workspace-windows.md与官方GetActiveWindow/FocusRemoved文档。
- 数学责任边界：core Math flow item携带typed continuation_indent_columns；UI按该布局元数据为raw source续行追加BoundaryDecoration，不修改source、不识别Markdown容器。DisplayPositionForSource保留firstGreater fallback后继续找精确Source，避免先行layout spacing的较大offset遮掉开头定界符；保留既有prefix/suffix/preview/affinity语义，不泛改LineIndexFor过滤。原quote/nested/list/callout父内容列4断言失败；最终root/quote/nested/list/task/footnote/callout、双affinity、hit-test、输入/Undo/Enter均过。列表比较同层续段的逻辑列，不用bullet字形advance代表缩进；root math end处Enter新建后续段落是既有结构语义，不强迫它匹配math frame padding。
- 最终**quote-empty-math-debug.log Debug成功**；**focus-quote-core.log 768项500632**、**quote-empty-math-platform.log 93项4521**全过。**focus-quote-test_*.log**：math_inline_editing8/735、math_bracket_editing5/1331、inline_presentation_paths10/419、completion_layout2/1056（各另loading5/22）；visual text_input_focus4/58、tab_transfer21/244、completion_interaction6/82全过。**focus-quote-startup.log**两轮真实启动/正常关闭exit0。无需重复已过广泛测试。
- **停止点/下一步**：Debug已更新，全部代码/测试/文档已分批本地提交，工作区应干净，无运行build/test/agent句柄。后续按用户实际Shift体验或新的发布要求继续；不重开旧宏包/布局审计，不跟进已完成0.6.2流水线。无真实模型/key使用，0.6.2正式资产核对见下方基线。

## 补全交互与设置布局验收（已完成，历史）

- 用户反馈取代0.6.2的“TeX/snippet抑制AI”结论。本地提交**b172e699**：移除屏蔽，可见AI拥有Tab/CtrlRight/Esc；TeX保留arrows/Enter，无AI时Tab回TeX/snippet，ShiftTab回snippet。AI接受关闭过期TeX列表；hint中英同步实际职责。选区替换输入也安排AI（正式snippet测试原requests0已修），同offset异affinity用is_caret。当前参数插入保留导航，跨参数范围改写结束旧snippet。无新坐标/全文parse/serialize路径。
- 本地提交**7435fa86**修复7个设置页宽窗口偏右：页面MaxWidth+Stretch居中是根因。共享PageViewport factory加载主题DataTemplate，native ContentPresenter.Content承载页面，左起列上限1008含margin，移除页面自身960上限；列表仍有限视口。不要恢复曾试过的NavigationView.ContentTemplate普通Binding（UIElement缺对应DataContext使页面空白），已撤掉。测试使用嵌套native viewport避免Windows工作区1602宽度限制；before 7页1800/2400均偏移，after **settings-width-accepted.log 1项84全过**。
- 最终Debug **settings-width-debug-final.log**成功。正式回归：**completion-interaction-platform-final.log 92项4511**；**interaction-final-test_*.log**中completion_interaction6/82、settings_styles3/78、settings_lists2/50、settings_control_lifetime1/13、settings_navigation1/24、inline_completion17/223、latex_completion4/27216（controller各另loading5/22）全部通过。**interaction-final-startup.log**两轮真实启动/正常关闭exit0。原生PNG build/tests/settings-styles/{light-1120,dark-720}.png已目视检查。无需重复已过广泛测试。
- 验证边界：新补全测试使用真实MainWindow/TeX popup/生产按键路由与fixture transport，含inline/display/quote、Enter/Tab/CtrlRight/Esc、snippet、精确undo与零全文操作；无真实模型/用户key调用，无物理鼠标注入。不开agents/CUA，native串行≤4。
- **停止点**：本轮代码/测试/文档已分批本地提交；本checkpoint随最后文档提交。工作区应干净，无运行native/test/agent句柄。Debug已更新；版本仍0.6.2，本轮未推送/新发布/MSI/安装，不移动旧tag。下一步按用户体验反馈或明确发布要求继续。0.6.2正式发布核对已完成，见下段，不再跟进旧CI。

## 0.6.2已发布（完成基线）

- master/tag **a11031e1521f8ef73c01c7492d4fc094cea4c75b**，CI **41807522121 success**；https://github.com/water2004/Folia/releases/tag/v0.6.2 。发布说明全英文，归一CRLF后与release-notes/v0.6.2.md精确匹配。正式MSI下载build/releases/v0.6.2/，ProductVersion **0.6.2**、**53907228 bytes**、SHA256 **b17724d0063cbcf80472e6d65ef7d43c31488420f2ab4bc008ce1756157c7268**，匹配sidecar与GitHub digest，verification.json记录。未安装，不移动tag。
- 本地MSI预检build/folia-062-msi.log成功，Release真实启动/正常关闭exit0；本地MSI53907215 bytes与正式CI不同构建，不混hash。仅既有WiX AllowSameVersionUpgrades ICE61 warning、0error。

## AI编辑与流式请求验收基线（已完成，历史）

- 本轮功能均已实现：AI以有界 `oldText/newText` 编辑提案复用/替换后缀，diff预览与一次Paste范围事务、部分接受/撤销；统一TeX→snippet→AI键盘归属，$0释放Tab；修复长ghost光标行归属；仅可见标签按下允许SDK撕出，标题空白手势禁止；稳定前后文放active区域之前；解析器括号边界支持 `\left({}\right)` 内层补全/删除；HTTP按15秒无数据计时，可用数据及时读取，协议终止立即完成。
- 本轮已分批本地提交：**d0cb0614** ghost光标；**ae9fb608** Edit/JSON/diff/cache；**34a4a25e** 三协议终止；**2e717403** HTTP无活动超时；**2778f5db** 嵌套括号；**2e755061** 有界前后文；**b876ebc6** 窗口手势；**fc048c7a** 编辑事务/预览/统一按键。本checkpoint随最后文档提交保存。版本仍0.6.1，未推送/新发布/MSI/安装，不移动旧tag。不开agents/CUA/鼠标模拟，native串行≤4。
- 接口/边界：core Paste新增可选block-local范围，保留原selection_before、语义结构粘贴和一次事务；不临时SetSelection，不增加全文parse/serialize或第二坐标。diff仅显示层，复用字仍Source，删除字muted划线。TeX/snippet活跃时抑制AI请求/交付/接受。capture仅取附近3个前/后owner或SourceLine，4:1:1:1总预算不增加，当前块固定行/256字符锚点；仅声明prompt稳定，不承诺真实服务端cache ratio。
- math syntax增加BracketBoundary，由Tree-sitter恢复节点提供glyph/whole syntax，删除raw前缀扫描；外层闭合边界不能被恢复中的内层借用。**inline-edit-math-syntax-accepted.log 7项15330**过；app新3例亦过。不重开已验数学宏包审计。
- 正式验收：**inline-edit-core.log 768项500632**、**inline-edit-platform.log 91项4473**过。最新portable **inline-edit-completion-final-tests.log 18项3722**过（含新CRLF生成文本归一和有界行查找）。最终分组 **inline-edit-final-test_*.log**：inline_completion17/221、HTTP14/96、layout2/1056、latex4/27216、math_bracket5/1331、control_lifetime4/62（controller各另loading5/22）；visual tab_transfer21/244、workspace_chrome7/762全部过。Debug **inline-edit-accepted-build.log**成功，**inline-edit-startup.log**两轮正常关闭exit0。
- 最后两个cpp变更也已验收：**inline-edit-current-build.log** 增量Debug成功；**inline-edit-current-test_inline_completion.log 17项221**、**inline-edit-current-test_completion_http.log 14项96**（各另loading5/22）全过；**inline-edit-current-startup.log** 两轮正常启动/关闭exit0。vendor source-lock全部**27项hash**匹配。无运行build/test/agent句柄，无真实API调用/凭据改动。
- 验证限制：无物理鼠标拖动或真实服务端cache采样；窗口测试走生产gesture入口/真实XAML几何/SDK状态，未注入输入。HTTP全loopback，包括持续reasoning超过短idle、DONE后保持连接、无响应超时；WinHTTP官方依据 https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpquerydataavailable 。docs/inline-completion.md与workspace-windows.md已更新。
- **停止点/下一步**：本轮请求已完成，Debug已更新，全部代码/正式测试/文档已分批提交；以用户实际体验反馈或新的发布要求为下一步，不重复已过测试，不追旧流水线。本轮未做物理拖动人工复测，保留该验证限制。既有ignored诊断目录因清理审批拒绝保留，不重试。

## 0.6.1已发布（完成基线）

- master/tag发布commit **3d13b5d5b152159dc632c9363daef721b104bada**均已推送；CI **41772307089成功**，Release https://github.com/water2004/Folia/releases/tag/v0.6.1。正式MSI **53870364 bytes / ProductVersion0.6.1**，SHA256 **0f0e0cd0dbfbe33045579fb6099f3d23a81980d1ba1f5f5a7c989b16861bfeff**，同时匹配sidecar与GitHub digest。build/releases/v0.6.1/verification.json记实测结果；未安装。本地build/folia-061-msi.log也成功（Release真实startup正常退出，WiX既有ICE61 warning/0error）。不再跟进旧CI，不移动tag。

## 数学补全与0.6.1开发验证（历史）

- 用户要求补齐数学补全，完成后推送发布0.6.1；此前Enter两批本地提交d2dde9c7/c1e583e4一并发布。六处版本与release-notes已改0.6.1，尚未推送/tag。不开agents/CUA/鼠标模拟，native串行≤4。
- 原266项目录扩大为1769项：锁定MathJax4.1.3原生注册表对应1567个命令/别名、70环境，tools/latex-completion/{registry,methods,environments,templates}.json明确职责；generate.py输出静态Assets/latex/commands.json，运行时不构造数学引擎。tools/math-port/tex-package-conformance.cjs只导出registry metadata，不再写UI目录；既有native fixture未改变。
- 查询改partial_sort索引，仅复制最终候选；UI最多32项、视口仍8行。Python生成一致性及7项单测已过。既有补全Undo/Redo实测20失败：先SetSelection临时选区且参数选择在事务外；现通用ReplaceSourceText命令带TextSpan/after selection，复用document_replace_text一次事务，Keyboard不再先改选区。build/latex-completion-app-accepted.log **4项27216断言+loading5项22**全部通过，含全部1769 snippet/精确查询/参数导航、10容器历史、3嵌套多行环境、70环境与代表性宏包实际native SVG。empheq只支持left/right选项、indentalign需要l/c/r参数，模板按源码修正，无引擎改动。
- 标准Debug成功build/latex-completion-debug-final.log。core-final **768项500632**、platform **88项4275**、popup **4项62+loading5项22**、settings虚拟化 **2项50**全过。原生registry-focused真实执行1项**9607断言**（build/latex-completion-registry-accepted.log；UT颜色码需去除才能统计，不要再误判全skip）。70环境实际SVG无错误，未改native引擎。
- tools/Generate-WixPayload精确替换内置commands.json且排除commands.user，isolated安装/同版本升级/repair全过build/latex-completion-installer.log，自定义命令/usage/settings/custom theme字节保留。brackets **5项1304**、inline completion **13项127**（各另有loading5项22）全过；真实startup两轮正常关闭exit0，日志build/latex-completion-{brackets,inline,startup}.log。
- 目录/生成/CI数据检查批**f93995aa**，编辑事务/查询批**012ef4b4**，installer批**6e41e539**均已本地提交；版本批待提交。正在build/folia-061-msi.log做Release/MSI预检（exec句柄见最新工具输出），之后提交版本、推master/v0.6.1、按release.yml等待发布并下载核对正式MSI/hash/version。已fetch远端master无新增提交，v0.6.1不存在，无agents/CUA。不再重复已过测试。

## 公式块Enter修复已完成（历史基线）

- 用户反馈$$→Enter→括号与字母行为不同，根因创建$$\n$$没有正文行。**已从创建语义修正为opening\n\nclosing**（Dollar/Bracket/fenced math/code/tilde及菜单插入），光标直接位于真实空行；普通输入仅在CST确实缺正文边界时补分隔，已有空行不追加。源码与CST仍一套位置，所有输入/撤销走既有TextEdit事务。
- 源码批已本地提交 **d2dde9c7**：payload_shell保留BlockSourceKind（空storage记录grammar，不复制正文）修复重做后误按FencedCode解析；Task续行不计checkbox，编辑元数据与SerializerVisitor共用markdown.container_source.list_continuation_prefix，Writer消费policy.nonempty_prefix。3任务marker×4raw块及18容器/数学定界符组合覆盖精确保存重载、Undo/Redo、括号与Unicode输入，正常编辑零全文parse/serialize。
- 呈现批与本记录同批提交（见HEAD）：InlineItemsEndPosition/BuildDisplayInlineText保留EditorDisplayPosition.kind/affinity，prefix末端承载空Source边界、suffix不生成可编辑留白行；SpansFromMapping保留真实行尾而不让装饰拉回source范围；显式空Text有Source ZWSP锚点。DrawFlowDecorations直接消费DirectWrite实际锚点行几何计算代码语言栏命中框，移除预设行高推测。无数学/quote/list特判、新坐标、临时dump或鼠标模拟。
- 最终验证均在build：**math-empty-body-core-accepted.log 767项500621断言**，**math-empty-body-platform-final.log 88项4275断言**，**math-empty-body-inline-final-accepted.log 38项1902断言**（AI/格式/实际Math caret/代码空行），**math-empty-body-brackets-accepted.log 数学5项1304断言+loading5项22**，**math-empty-body-code-final.log code7项106断言+loading5项22**，全部通过。**math-empty-body-renderer-build.log 标准Debug成功**；**math-empty-body-startup.log 两轮真实启动/正常关闭exit0**。未重复广泛上游数学审计。
- 验证界限：全量controller合跑math-empty-body-app-final.log触发既有60秒总时限（滚动性能阶段），不称全集通过；随后按受影响文件分组保持原时限全部通过。最终通过日志以上一行所列为准；旧failed/diagnostic日志留ignored build，不代表待办。
- **无运行agent/build/test句柄，无本轮待完成功能项**。Debug已更新；版本仍0.6.0，**本轮未MSI/安装/推送/发布**。下一步按用户反馈或打包要求继续，不因压缩重复已完成测试/审计。不开agents/CUA/自动鼠标，native串行≤4；允许正常关闭用于构建但保留未保存提示。旧ignored清理被审批拒绝，不重试绕过。

## 0.6.0发布完成（历史）

- **master与v0.6.0均已推送并发布**：[Release](https://github.com/water2004/Folia/releases/tag/v0.6.0)，发布commit **819bc1f617572df3f795c5c3afe28a8b39a63203**。既有CI **41719399300 / job126207857479** 全部成功，包含自包含MSI构建、真实启动/正常退出、SHA256与公开发布。全部此前AI/设置/页签本地提交都已包含。最后只提交/推送本交接结果，不移动tag。
- 正式资产已下载至**build/releases/v0.6.0/**，只读核实MSI ProductVersion **0.6.0**、53862094 bytes，SHA256 **d363588eb9ca552df8fece48808186b058b059600ece85aa3a276508da514dda**，同时匹配随附.sha256和GitHub asset digest；verification.json记录结果。未安装。此前本地预检build/folia-060-msi.log也成功，Release EXE0.6.0.0/0.6.0，startup exit0；WiX ICE61是既有AllowSameVersionUpgrades警告，0 errors。不要混淆本地预检与正式CI产物的hash。
- 核心修复提交**2b36da6e**：通用insert_text按FencedCode CST补缺失正文换行，paste同一路径；空code正文Source锚点，header归现有BoundaryDecoration，AddBlockLines跳过纯装饰行且检查terminal Source边界。无新增Chrome/Control enum或引用/列表特判，临时friend/dump已清。普通/closed/tilde/quote/list双affinity42断言通过；不重开已验方案。
- AI功能提交**673a4f94**：上下文1–64 KB（1000 bytes/KB），output16–4096 tokens，默认7/128；三档prompt主动程度独立模型reasoning，native状态环/Ready/Empty/BudgetExhausted/Failed，generation取消、限额空不缓存。**用户要求额度用尽只留短提示**。点击获取读取GET /models的data[].id→effort.supported_levels，按API顺序有界去重、协议过滤；不预设/猜型号/发生成试探。字段可省略，初始只服务默认及已保存值。Model/base/protocol变化与Unloaded取消、弱UI丢旧结果，共享ReadHttp保持异步WinHTTP与边界。本轮仅loopback fixtures，未真实调用上游；API来源 https://api-docs.deepseek.com/api/list-models/。
- 全设置页ChoiceBox直接SDK ComboBoxT覆写：闭合不消费wheel交父ScrollViewer，展开用原生行为；预算普通TextBox/from_chars，不再NumberBox或缺失样式。schema5、文档docs/SETTINGS.md与docs/inline-completion.md已同步。UI fixtures获取时用fixture-key，**Save前清PasswordBox，绝不能改用户凭据**。Folia保持独立，无Fairy工程/配置依赖。
- 验收完成（均在build）：completion-decoration-core-tests.log **765项500165断言**，completion-decoration-platform-tests.log **85项4256**，completion-portable-tests.log **13项74**；folia-060-completion-tests.log **26项1273**（metadata/取消/限额/围栏accept/undo），folia-060-presentation-tests.log **14项338**，folia-060-ui-tests.log **35项1241**（settings/wheel/tab/chrome），全部通过。明暗720/1120原生截图已检查；folia-060-debug-build.log成功；folia-060-startup.log两轮真实启动/正常退出0，folia-060-activation.log Unicode/relative路径、多窗口/独立实例/正常退出成功。无需重复这些审计或测试。
- **无运行agent/build/test/下载句柄，无待完成功能项**。下一步按用户新反馈继续。不开agents/CUA/自动鼠标，native串行≤4；用户允许正常关闭Folia供构建但保留未保存提示。旧ignored候选/诊断清理被自动审批拒绝，不重试绕过。发布使用.github/workflows/release.yml，不另建发布路径。

以下为历史记录；旧的运行句柄、待办和发布状态均由上方当前checkpoint取代。
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
