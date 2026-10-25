# MathJax 原生移植：当前交接

恢复/压缩后先实际读本文。架构、职责和详细验收见 [architecture](native-raii-architecture.md)、[files](native-raii-files.json)、[acceptance](native-raii-acceptance.md)。只复查新变化；ignored候选用 `rg --no-ignore`。

## 当前 checkpoint：0.3.2 发布进行中（2026-10-25）

- 用户已明确授权推送0.3.2，按既有tag触发release.yml发布；版本统一更新CMake/MSI默认/manifest/rc/About/README，新增release-notes/v0.3.2.md。此前不发布/固定0.3.1仅属上一阶段约束。
- 远端已fetch，master无新提交，本地10项已验收提交待推送；工作开始时干净，v0.3.2本地tag不存在。应用/Core/platform/math的最终验收见下方，不重复宽审。
- 下一步：版本/发布配置校验→提交→master与v0.3.2标签推送→等待既有GitHub Release CI成功及MSI/checksum验收。禁止移动已有tag。无本地build或agent。

## 历史验收：编辑体验、WinUI生命周期与math优化（2026-10-25）

- 用户约束保持：**不使用数学局部面板，正文原位数学编辑和异步内存预览保留**；source/CST/selection/history架构不变。版本仍0.3.1，无推送/发布/MSI。旧生产owner链和Mermaid继续暂停，无活动agent/build/native句柄或待落候选。
- UX：完整格式标记隐藏、工具栏显示格式状态，未完成语法可见；4DIP物理拖选阈值阻止公式展开后静止点击误选。代码块去整框、修独立Code/Math定位、语言搜索显式提交、修函数/预处理/模板高亮。补全采用内置Acrylic，复用当前行并在接受/关闭时释放。
- WinUI：设置快捷键ComboBox事件曾强捕获自身，重建后不能释放；真实旧对象测试两次均失败，去捕获后11断言全过。补全身份复用/关闭weak引用释放25断言通过。未改变设置页架构；首次打开后6页仍保留，按需页创建未展开。**没有证明整App常驻内存下降的MB数**。
- 最终验证：Core **752项/499404断言**，Windows editor **82项/3446断言**，math **972项/536146断言**，WinUI app **28项/557断言**全部通过。WinUI Debug最终构建成功，应用`build/app-winui/bin/x64/Debug/Folia.exe`，与build/core数学DLL哈希一致。日志均在ignored `build/editing-perf/`：`core.log`、`platform-final.log`、`math-final-full.*.log`、`app-build-settings-final.log`、`app-tests-complete.log`。测试runner部署normalizer到EXE旁，避免源码fallback假覆盖，支持`-TestFilter`。未做人工界面观感、真实IME输入或用户原文档复测。
- 本机Debug同输入对照：Common默认表/catalog约-39%，4公式×20完整render中位1184.7→1096.2ms（约-7.5%）；SVG64glyph local/none及512 none约-18..23%，512 local无明显变化，四场景各6份完整SVG逐字节一致。Cache512×32成员登记中位24403→2868µs。trim30000调用旧12–14s/新41–50ms仅局部微基准。字体局部查询也减少分配；不把这些数值当整App或Release倍数。
- 明确否定：ParseOptions批量删除候选实测更慢，生产与新增专属测试已精确撤回，ignored `rejected-tex-list/`和日志保留否定证据，不再次引入。上下标去每次\d正则保留简单等价实现，96新增断言及全测通过，但整compile中位仅约1%，不称大热点收益。补全1000×8行刷新中位3057→2586ms，private均稳定，不称常驻内存减少。
- 提交：`1ed3cd67` trim；`b1be2b26` 请求字体；`82a51cb8` Math owner集合；`1a631266` 拖选/真实SVG测试；`b155f576` 补全生命周期；`3b987cc1` **包含编辑UI/格式/代码块及Common默认表（标题仅写后者，已有UI暂存被一并提交，按实际diff验阅）**；`0ba16482` SVG；`293fa021` script数字；`754b067b` 设置控件生命周期。本轮未重写历史。基准、旧obj和日志仅ignored证据，正式测试全部tracked；build/core临时CMAKE_PROJECT_INCLUDE已移除。下一工作以用户新要求为准，不自动发布或重开历史迁移审计。

## 已完成：0.3.1 已发布（2026-10-25）

- 用户明确要求推送并按既有CI/CD发布0.3.1，包含 `9a72e432` 打开线程修复和 `e5c39fda` 数学滚动修复。统一更新CMake/MSI默认/manifest/rc/About及README版本，新增 `release-notes/v0.3.1.md`。
- 版本/XML/workflow/notes静态检查通过；固定标签 `v0.3.1` 指向 `92ac9f54`，原tag push流水线 [40119208893](https://github.com/water2004/Folia/actions/runs/40119208893) 全部成功。[Release v0.3.1](https://github.com/water2004/Folia/releases/tag/v0.3.1) 已公开并设为latest，两次修复与版本提交均已推送。
- 附件 `Folia-0.3.1-x64.msi` **53,493,427 bytes** 和同名 `.sha256` 已上传，校验文件与GitHub附件digest一致：`fe0099f3ff4a48b8a0426cb71d7f9a6d5ed4e54895bb46292d11989eff77e053`。验收记录 `build/release-0.3.1-published.json`。发布无剩余步骤，无活动agent/native/build句柄；未安装或交互试用CI产物。
- 现有应用与平台验收已完成，见下方；本次只改版本与说明，不重复宽审。用户补充窗口也拖不动：与UI线程被持续绘制占用吻合，但没有当前现场堆栈或原文档标题栏拖动实测，不把已有测试宣称为完整现场复现。

## 已完成：快速拖动公式文档的输入停滞修复（2026-10-25）

- 用户重新要求排查，确认高CPU、滚动条/点击停滞，十几二十分钟后有时恢复；本轮未发现运行中的Folia，未取新现场堆栈，不把历史38588样本当成当前复现。只沿渲染调度/公式取消链调查，旧生产owner链及Mermaid移植继续暂停。
- 已确认并修复：FrameScheduler持续以High优先级提交重帧，普通UI回调测试被推迟至第12帧结束；Render退出的deferredInvalidate又同步调用Render，使帧预算不能让出UI。现均以Low优先级异步补帧，重绘和embedded完成通知各合并至一个待处理回调，弱生命周期保护复用原invalidation state。
- 公式重复工作也已修复：离屏布局原先会Render排队，帧末取消，取消完成仍触发刷新。新Lookup仅借用已有geometry并登记原owner，不排渲染/读取；只有visible band启动补算，pending公式回到视口会重提，取消不发完成通知。原SVG按当前视口后台读取/离屏丢弃、预览只在内存、请求字体/批量/Common布局均未改变。
- 新正式测试 `tests/app/test_math_scrolling.cpp` 覆盖调度公平性、递延补帧不递归、取消通知、inline/display离屏查询与owner保留、400公式20次视口替换后停止重绘及文档/选择不变。WinUI Debug构建成功；app **11项/52断言**、Windows editor **80项/3429断言**通过。隔离替换原3个公式Cpp后，新增math6项有3项失败（取消多通知、两类离屏任务）；原调度/递延测试失败记录也保留。日志 `build/math-scroll-{app-build,tests-before,tests-after,feedback-before.stdout,platform}.log`。
- 最终连续10轮app复验全部通过：**110项/520断言**，逐轮记录 `build/math-scroll-repeat-summary.log`，日志 `build/tests/winui/Debug/scroll-final-*.stdout.log`。快速切换已完成时允许零补帧，要求输入回调得到执行、重绘停止及无递归；专门的递延测试则强制渲染中请求新帧并验证让出UI。修复提交 `e5c39fda` 已随顶部0.3.1发布；无活动agent/native/build句柄、候选或剩余检查。尚未用用户原文档复测十几分钟停滞或实际标题栏拖动；不展开旧生产owner链或Mermaid移植。

## 已完成：偶发打开跨线程错误已修复（2026-10-24）

- 新任务来自用户截图“打开失败：应用程序调用一个已为另一线程整理的接口”。`LoadDocumentAsync` 的表面未就绪32ms等待及读文件变化100ms重试，均在 `resume_after` 后遗漏返回UI；进入这些时序后会从线程池访问renderer/界面回调。已在入口捕获 `apartment_context`，两处等待后显式返回，再检查取消/生命周期。未更改数学算法、文档模型、版本号或重新发布0.3.0。
- 正式应用测试 `tests/app/test_document_loading.cpp` 使用真实DispatcherQueue和生产控制器/Session/Renderer，检查延迟就绪（普通/公式文档）、取消、取消后重开及已就绪路径。`tools/run_winui_tests.ps1` 复用app对象与相同Boost.UT模块编译配置，console runtime已链接；产物仅在 `build/tests/winui/`，不进入MSI。
- 验收：setup及WinUI Debug编译链接通过；新测试5项/22断言通过，连续10次50项/220断言全部通过。隔离使用修复前 `81bed653` 控制器的同套测试5项有4项失败并检出错误线程；没有改回工作区源码。日志 `build/open-thread-{app-build,winui-tests,before.stdout}.log` 和 `build/tests/winui/Debug/repeat-*.stdout.log`。100ms文件变化重试分支已按相同规则修复，未强制制造该分支；未交互操作用户安装版。
- 本修复提交 `9a72e432` 已随顶部0.3.1发布；无活动agents/build/native句柄或待处理候选。后续高CPU滚动问题的修复见上方，旧生产owner链/Mermaid继续暂停，不重新宽审。

## 已完成：0.3.0 已发布（2026-10-24）

- 用户明确要求发布 **0.3.0**，此前固定0.2.3的约束属于历史。本次不修改数学运行时或重开暂停事项。
- [Release v0.3.0](https://github.com/water2004/Folia/releases/tag/v0.3.0) 已公开并设为latest；[CI 39980955488](https://github.com/water2004/Folia/actions/runs/39980955488) 全部成功。原标签固定在 `36b0bd43`，CI修复位于master `ffae1abd`；通过既有流水线的按标签重跑入口发布，实际checkout与标签一致且属于master。
- 附件 `Folia-0.3.0-x64.msi` **53,497,523 bytes** 与同名 `.sha256` 均已上传；校验文件与GitHub附件digest一致：`84a26dbbc1c7cff609a9df698b44f5d65d1a78af031325298f1891c05a323bfc`。验收记录 `build/release-0.3.0-published.json`。未安装或交互试用本次CI产物。
- 前两次CI缺锁定字体npm和实体TS输入，已修复恢复步骤。隔离恢复的84个字体Cpp/inc及实体inc与已测本机构建逐字节一致，未改Source锁或期待值；日志 `build/release-0.3.0-{font-restore,clean-font-generation,clean-entity-generation}.log`。
- 本次发布无剩余步骤，无活动agents、本地native/build句柄或待处理候选。后续仅响应新任务；以下已完成验收及暂停事项保持原结论。

## 已完成：旧测试全部完成（2026-10-23）

- **范围已完成**：本轮122个未配置旧Cpp全部逐项处理；有效数学行为先迁入正式typed owner，JS对象/回调/描述符规则只留离线Source记录。现在159个math Cpp/ixx全部正式配置，旧class_binding测试import为0，7个根目录旧inc退出；82个专用旧runner/mapper/helper退出，顶层工具无缺失测试路径引用。生产owner链、高CPU、Mermaid继续暂停。
- **最终验收**：build13成功（parallel≤4）；最终math **965测试/272734断言通过**，Core **581测试/43936断言通过**。日志`build/math-port/finish-old-tests/{native-combined-build-13,all-math-final-combined,core-final}.log`；`final-inventory.json`记录122/122、159/159、零旧import/缺失引用、生产源改动0。所有native/build已结束，无live句柄。
- **Source保全**：原profiles字节不变；唯一旧Cpp gold静态解码为结构化数据，不复制Cpp正文、不从新运行重定期待。完成所有原Source组，operator8组/13536观察及完整Unicode域、Mo16组/2342369查询均只读通过；leaf26/658+1、callable8/50遗漏已补。七个重构组合入口实际通过。拒绝了只查pristine字典的早期候选；实际采用`tools/operator-final`及`parser-references/mo-final`。详细native覆盖与历史metadata差异见acceptance。
- **字节锁**：37个规则下的profile与Git blob逐字节相同，原有Source evidence改动0。Mo完整新gold仅末尾原始为CRLF，使用`-text`并仅对该新文件重新应用Git过滤规则后保留原始字节；没有修改任何期待值或既有profile。最终核验`finish-old-tests/final-git-proof.json`，代码/Source提交为`80b09f98`，工作区已核验干净。
- **可交接资料**：tracked `evidence/old-test-retirement-index.json`将原122个Cpp逐一映射到唯一职责记录；`old-{foundation,mml,host,generic,parser}-test-retirement.json`记Source/native职责与hash，`old-host-test-static-semantic-values.json`保留手写gold，`old-test-tool-retirement.json`列82个失效工具。原canonical Source owners和profiles可在远端复跑，ignored候选/日志不是交付依赖。
- **提交/停止点**：分批保存，native最终补充`a9e018cd`、parser全组`106a9822`、旧发射器`2051e723`、旧runner`23090a3b`；最后的Source入口/交接记录见Git最新提交。旧u编码器及10文件死引用已退出，Source AST/pins不变，所有引用链helper实际载入通过。最终工具检查零缺失require、零旧Cpp名称引用。所有agents/build/test均已结束，本轮无剩余旧测试事项；不自动展开暂停的生产owner链。无推送、MSI或版本变更；固定0.2.3。

## 历史：0.2.3运行高CPU，已捕获线程栈（2026-10-22）

- **当时优先级（历史）**：用户当时要求“先暂时不管”高CPU/无响应问题，暂停继续采样、计数及修复。2026-10-25已按新请求恢复对应调用链排查，当前结果以上方checkpoint为准；保留下方历史证据。
- **新证据/当前任务**：用户报告Folia卡死及高CPU，要求捕获堆栈。已对运行中安装版PID38588采样8次：`build/hang-diagnostics/folia-38588-sample-{1..3}.txt`及`followup-{1..5}.txt`；工具 `capture.ps1`/`capture-followup.ps1` 用Windows线程/DbgHelp API，finally恢复线程，未关闭目标进程。安装Folia.exe与本地Release SHA256相同（D1CD68A92469CEF99412287AE2FE2F3AF1E52745CD253703C6DC0C0B96E176D1），App PDB可解析；Release Math DLL没有匹配PDB，内部仅模块偏移/最近导出名，不能把最近导出名当真实函数。
- **恢复前后已抓到**：用户随后确认突然恢复响应。新增 `unresponsive-{1..5}.txt` 五次样本：1为math完成回调→Invalidate→Render→DrawDocument→EditorEmbeddedContentUpdater::Update:84→BlockPreparer::Prepare:745→DirectWrite文本测量；2/3在EndDraw；4/5主线程已回到GetMessageW、math线程条件变量空闲等待且累计CPU未再增加。随后Responding=true（窗口标题“折笺 — 整合.md”）。因此是长时间UI计算/绘制后恢复，**不可宣称永久死锁或已证明无限循环**；下述请求/取消反馈仍待证明。目标未关闭/未改代码，无live采样工具。
- **已确认采样结果**：主线程62912持续在D2D EndDraw/DXGI Present；全部8次均经math完成通知→Invalidate→MainWindow::RenderEditorSurface→Render，部分同时出现Render退出RAII再次同步Invalidate→Render。followup1→5主线程累计增加4046.875ms，math线程18220增加3437.5ms。math线程在RenderBatch、取消catch清理renderer和条件变量等待之间变化，证据不支持单个公式一直不返回。GPU驱动线程35848也有工作。
- **尚未证明的原因**：优先检查预取/embedded范围提出math请求，DrawDocument末尾按actual visible取消，取消完成仍发completion→embeddedGeneration变化→pendingMath刷新重提的循环；以及Render析构同步补帧导致UI不让出消息循环。代码支持这些风险，但采样尚未证明具体重复的key/owner。精确下一步是对这一窄链增加请求/取消/刷新计数或可复现测试，不重开字体/旧移植宽审。当前仅采样与读源码，未改生产代码、未构建/重打包；无live agent/build，PID仅为采样时事实。

- **用户确认**：2026-10-22 用户确认当前版本无误，要求整理提交；本轮功能已收尾。不将此确认解释为具体内存数值测量或整个旧移植清理完成。
- **用户边界**：只有一种批量实现，单个=1项；字体正文/轮廓/解码对象仅属本批请求，结束即释放，不跨请求LRU。数学线程与轻量引擎持续驻留。普通视口流程不变、禁止HasMath分类/全文视觉排版；滚动UI零同步IO、离屏任务取消；编辑预览异步且仅内存。固定**0.2.3**并允许同版本更新，不回退/擅自bump。Mermaid及旧移植清理暂停。
- **实现已提交**：视口取消/移除8MiB历史SVG缓存 `b8ded311`，测试 `8b7a8028`；请求字体 `3248adc2`、Source验收 `a6e0a005`；Core C ABI批量及字体职责整理 `2d591d14`；Folia有界批量/驻留轻量引擎 `3205ccab`；启动批次/未提交请求取消/视口测试 `0fc6b705`。无reset/推送/发布。Root独占Git/build，parallel≤4、math pool2。
- 字体职责：`newcm_font_data.*`只读POD表/索引/元数据，`newcm_font_module_provider.*`拥有本批NewcmFontRequest及去重/解码；Renderer仅保留共享只读元数据和解析器，每批建/释放FontDataBundle，Scene只持用到的resource。数据为DLL文件支持的只读表，无全量堆展开、SQLite/独立fontpack。Common/SVG借用同请求，不引入第二套数学算法。
- 统一批量已贯通Renderer→C ABI→NativeMathSession→`MathJaxRenderer.Batch.cpp`；原单项helper委托1项批次。每项结果/异常隔离，超时从该项开始算。Folia32公式/16块有界提交等待，单owner不拆；staged Batch析构清掉未提交请求。FinishPreparation/ReleaseSession过渡策略已删除，引擎持续驻留。
- **最终Core验收**：构建 `font-request-owner-core-final-build.log`；math全测 `font-request-owner-all-math-tests.log`6.64s通过；Core全测 `font-request-core-tests.log`通过。Source `font-request-owner-source-compare.log` **14,568,049值0差异**（24,307 glyph/642,872段/238,666 aliases）。比较器已分别严格检查既有semantic resourceID和SVG cacheID；旧eager-map测试helper改为实际lookup，原33号glyph/BBox期待不变，不重新调查这两项已解决差异。
- **最终内存探针** `font-request-owner-memory-batch.log`：引擎持续存活，64×16=1024公式；private idle4,165,632→重复后5,582,848→取消/错误后5,754,880 bytes，重复一致/取消/恢复通过，1.92s。旧全量引擎仅构造曾约增加112MiB；这是隔离测量，**不是Folia GUI回到128MB的证明**。用户实际基线是首次native运行时版 `2d58ded9`；同文档/同内存列的GUI对照仍待提供文档并实测。
- **最终App验收**：`font-request-release-msi.log` Release构建成功；worker `font-request-release-worker-final.log`1.76s PASS：预览文件IO0、引擎驻留、离屏SVG0，100次视口替换取消107 Read、实际1 Read+1当前native render，abandoned batch无遗留pending/文件。layout `font-request-release-layout-final.log`1.68s PASS：512不同公式32批，同owner80公式3批、entries完整；4100重复/普通节点及15行稳定断行通过。
- **当前MSI包含上述最终代码**：`build/installer/bin/Folia-0.2.3-x64.msi`，53,366,438 bytes；SHA256 `C8D29D3B708B67C76238C58AE0A36643B36CDC299A1A92A538C3F6A16188F23C`。`font-request-release-deployment.json`记录148文件逐个hash相符、Core DLL相符、probe/旧JS排除、同版本升级开启且ProductCode更新。安装清单重生成一致；验包用Windows PowerShell，pwsh7的排序差异只改变XML顺序。WiX有预期ICE61同版本覆盖warning。用户已确认无误；agent未安装/GUI试用/断电实测，未推送/发布。
- **此前交付状态**：请求字体/批量/视口代码、测试、MSI及用户确认已完成，验包记录 `11594681`、用户确认 `6f680548`；当前高CPU新证据按上方checkpoint继续。旧迁移清理保持暂停。build、日志、验包JSON/MSI均ignored，远端需按tracked脚本重建。

## 历史：0.2.3前两次包

版本提交71dbd654曾未经用户要求升0.2.3；用户已安装，现固定此版本。同名旧包先为53,350,054 bytes/SHA256 `39645DCE4ABE67D2DEB16B857E48C28DBA786FA3CF467E2F2B623E3F8CE5B416`（ee5cc224记录），随后被上方视口修复包覆盖；旧hash不能用于当前文件。源码/原验包JSON可追溯，不恢复8MiB SVG历史缓存或销毁重建完整引擎的过渡策略。

## 历史：0.2.2安装包（已被当前源码替代）

- `e1b66948`启动回收、`230c1430`版本、`5942bade`验包记录；未推送/发布。0.2.2自包含MSI `build/installer/bin/Folia-0.2.2-x64.msi`，53,337,766 bytes，SHA256 `2134773981B611CE18FE471739B19B64E60C3BF03EAFCE3A2115A418CC3C30E7`。148文件解包hash已验，两个probe EXE与旧JS资产排除；证明`math-cache-cleanup-release-deployment.json`。不要用旧MSI验当前异步流程。
- 0.2.2的全文块预排版/额外离屏renderer已由当前源码移除，不重新恢复。更早0.2.1推送至origin/master `2d58ded9`；当前不擅自推送/打包。原生接入仍为TeX→request-owned MML→原Common共享布局→immutable Scene→SVG→Folia，整个旧移植清理尚未完成。
- MathSvgStore仍沿用TEMP/FoliaMath/{GUID}.svgstore、独占非继承/DELETE_ON_CLOSE；普通退出/进程结束关闭句柄删除，断电遗留下次后台回收，不复用半写内容。缓存不含源文档，不提供未保存编辑恢复。此前删除旧probe小目录被自动审批拦截，保留`build/math-port/worker-probe/Debug/cache-reclaim-9f3496cb771f4cc395a1bbd80711917d`，不要为清它绕过审批；不影响产物。

## 旧迁移剩余工作（旧测试已完成；生产owner链仍暂停）

1. **旧测试消费者为0，迁移清单无剩余项**。本轮122个旧Cpp及7个旧根目录inc全部处理，159个保留Cpp/ixx全部配置。所有Source期待和真实数学覆盖保留，见顶部验收与tracked逐项职责记录。不要恢复旧Context测试框架，或重做已完成Source宽审。
2. copy/copy_boundary/metadata、returns/predicates、classes、inheritance、families、node、operators、host、generic、parser均已收尾。未来新增数学测试放现有typed owner；离线Source owner不再发射这些旧Cpp。旧共享runner及失效配置已退出。
3. 旧生产owner链仍需整体收尾：Fields → ContextBindings → ParserRuntime，values唯一调用仍fields.cpp:439。未配置不等于死代码；不能孤删/stub或新增双轨。`fe5eba6e` 按用户“全部提交”保存了原有4文件投影补丁（context_bindings.cpp/.ixx、mtable_bindings.cpp、semantics_bindings.cpp）；它们不在478活动源内，未重新验旧协议。**这是交接存档，不是继续扩展投影补丁的方向**，仍按RAII架构退出整条旧链。
4. 整个旧移植清理最终完成前，还要对剩余源码/工具/依赖做对应验收及完整完成审查；D2D不是本轮接入前提。本轮请求字体/批量/视口改动已获用户确认，不代表剩余旧迁移已验收。不要把既有Source-only检查或编译成功当作新的native运行验收。

## 固定边界与本机资料

- 保留MathJax的TeX/MML/Common家族职责；SVG和未来D2D消费同一完成布局，不引入平行算法、Scene家族或JS解释/对象语义层。旧原型/getter/rebind矩阵仅留离线Source来源记录。
- Root独占Git/CMake/build/native，一次一个build、parallel≤4。Windows console probe链接 `folia_console_test_runtime`；异常退出后只做有界诊断。小批提交，不reset，不覆盖接手人的改动。
- 已完成staging/执行proof不重复运行；512/513及verification旧256/257是host预算，不新增MathJax深度规则，不重新抽取旧对象协议测试。
- 之前6个dirty路径已按用户授权全部处理：4个旧binding源码保存于fe5eba6e；MathJaxSvgParser.cpp/SvgNormalizer.cpp只有行尾/状态差异，暂存后无内容改动。原备份仍在 `build/math-port/context-dirty-backup-20261021/`；不再存在“永远保护这6个未提交文件”的限制。
- **build/日志/候选/安装包均ignored，不随Git推送。** 远端接手者应按tracked脚本重建，不能假设本机候选存在；canonical Source profiles与owners在仓库中。live句柄只以上方当前checkpoint为准。
