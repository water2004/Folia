# MathJax 原生移植：当前交接

恢复/压缩后先实际读本文。架构、职责和详细验收见 [architecture](native-raii-architecture.md)、[files](native-raii-files.json)、[acceptance](native-raii-acceptance.md)。只复查新变化；ignored候选用 `rg --no-ignore`。

## 当前 checkpoint：恢复旧测试清理（2026-10-23）

- **继续范围**：用户明确要求将剩余旧测试处理完再停；本轮完成全部剩余旧测试消费者及其失效工具引用，不再以小批进度结束。生产owner链、高CPU和Mermaid不扩展。按下方既有清单分组，Root保留架构/验收/集成/build/Git，Luna准备互不冲突的Source迁移与native覆盖候选。

- 已完成copy/copy_boundary（`15809d75`）与metadata（`45b2d38d`）：旧Cpp/mapper及专用工具退出，全部Source profiles/owners保留。copy既有proof的12pins相符、未重复执行；metadata Source32行与完整Cpp映射通过。原factory owner新增8kind×keepIds值语义，472断言通过，不模拟JS身份/可空attributes。
- 本轮math全测 **911测试/218824断言通过**，`build/math-port/copy-metadata-review/{final-build,all-math-tests}.log`；先前两个copy用例共50断言通过。仅新增测试，生产/应用/版本不变，parallel≤4。各批proof为`copy-consumer-retirement.json`、`copy-metadata-review/retirement-proof.json`。
- return_prototype已提交`0b05c59b`：两行Source检查通过，旧Cpp/mapper分支退出，普通155输入/profile/Cpp及生成字节完全保留。旧mapper原检查因历史optional→Property辅助类型漂移失败；完整Cpp经6类明确差异核对、全部输入/期待相符，见`return-prototype-review/{mapping-proof,retirement-proof}.json`，不是旧native执行。共享配置1303/10→1301/9；专用returns配置189/3→187/2并退对应2项原型mutation。最后构建`build.log`成功，沿用字节未变且全测通过的binary；无live build/agent，未重复全测。三个功能批次均本地提交，未推送/打包。
- returns/predicates已提交`ce0ab72d`：5Cpp/4mapper/4专用旧runner退出，Source32/155/75/111/7完整保留；32/155/111/7实际Source检查通过，75所属sparse299复用既有proof。全文映射仅明确的历史helper漂移。证明`finish-old-tests/returns/{source-proof,mapping-validation,retirement-proof}.json`，共享旧配置921/4。
- classes已提交`6f8622a5`：5Cpp及base gate退出，5个原oracle去掉Cpp发射死代码；Source3/432/14/9/28全量通过、profile字节不变。原Mo owner补真实双子row首尾form，1用例/2断言通过，`finish-old-tests/classes/{source-proof,retirement-proof}.json`及`native-final-build.log`。其他shared旧gate仅缩对应项（row_class实际28，旧候选误记32已纠正）。
- inheritance已验收：8Cpp、5mapper、5专用runner退出；3原oracle去Cpp发射，新增boundary结构化静态gold249cases/350values/7pins，其余profiles字节不变。boundary249、inherit-from9344、row6已完成Source proof复用；原inheritance六模式699/128/16/8/16/8实际通过。旧consumer54对应既有sparse299 proof。当前直接旧消费者 **16cpp/ixx+4inc**；`finish-old-tests/inheritance/{source-proof,promoted-proof,retirement-proof}.json`，无live build。
- **最新整合点**：已提交`4f4d86d7`继承、`b286ec03`真实数学覆盖、`8dcad201`family、`bf2e4b73`node/includes、`b2f974c9`operators。现在共100/122旧Cpp退出（host48/generic17尚待提交），旧class_binding测试import为0，5旧smoke/parent inc与numeric重复inc退出。Source与native职责记录在`evidence/old-{mml,host,generic}-test-retirement.json`；原profiles一字未改。build11成功；新增UTF5227、radix4、scan953、format1982、stack68、BBox572、priority195断言均通过。**全math正在运行**，句柄76178、`finish-old-tests/all-math-final.log`，不要并行重复。
- **精确下一步**：提交host/generic和新增native补充；parser19+input-host Source-only已promote并复用实际checks。Root发现Luna operator候选漏原8组Source观察、Mo仍读Cpp hash，因此未采用；`finish_family_source`正在`parser-references/operator-final`闭合原operator全组，`finish_host_and_owner_tests`正在`mo-final`闭合原Mo全组。最终22Cpp及map_handler_source.inc须在完整Source/native验收后退出。82专用旧tools待Root删除闭包；6混合工具+leaf/callable已promote，7个组合入口实际通过，日志`source-tool-integration-*`。callable原8/50 Source已静态保全，之前generic的“无Source”判断已纠正。无live build，仅上述全测。
- **活跃分工**：Luna`copy_retirement_review`仅收尾input-host/operator/Mo三组完整结构gold+实际Source检查；6.1Sol`finish_host_and_owner_tests`负责parser9项(`sol-a-final`)，`finish_inherit_operator_source`负责另10项(`sol-b-final`)；generic17 final包已齐（14实际Source checks、原profiles0改）；`finish_family_source`负责全局混合Source checker/失效runner闭包，leaf纯Source已ready（26Source/658helper+1direct，旧metadata不变）。Root只采用final包；此前Luna仅表达式清单不是gold。错误mask含None与input转义、operator顺序已定位为候选生成错误，未据此改生产。corrected mask hash5da2dc96…已集成。
- 完整范围：本轮开始时122未配置Cpp（279总Cpp、157活跃）；`finish-old-tests/{dispatch.json,inventory/all-unconfigured-tests.json}`逐项处置，不能凭inactive删。18Cpp已退出，104待整合。families/node最终包ready于各组`final/manifest.json`，operators于`final-manifest.json`；host-owner-final有8项native补充patch和48项逐case处置。Root下一步整合这些真实native缺口后集中构建验收；genericReferences由`finish_inherit_operator_source`继续闭合，parserReferences由Luna`copy_retirement_review`完成，`finish_family_source`继续全局失效工具引用候选。全部候选ignored，Root独占集成/build/Git。高CPU、Mermaid、生产owner链仍暂停。

## 历史：0.2.3运行高CPU，已捕获线程栈（2026-10-22）

- **当前优先级**：用户已要求“先暂时不管”高CPU/无响应问题，暂停继续采样、计数及修复；保留下方证据，待用户重启此项。当前转为评估Markdown解析器与md4c的规范对照/接入取舍，尚未授权或实施解析器替换。
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

## 旧迁移剩余工作（旧测试已恢复；生产owner链尚未处理）

1. **旧测试直接消费者仍有16个cpp/ixx +4个inc**（按class_binding直接导入文件，不是断言；未包含所有间接依赖）。完整本轮122项测试范围见顶部dispatch；旧consumer退出必须保留全部Source期待和实际数学覆盖。旧宽图`8ba6b468`是历史；当前窄清单见`finish-old-tests/inheritance/retirement-proof.json`，不重做生产宽审。
2. copy/copy_boundary/metadata、returns/predicates全组均已收尾，Source全量profiles/owners保留，见顶部与acceptance记录。剩余按dispatch分组继续。旧共享工具只是剩余配置，不是当前native验收路径。
3. 旧生产owner链仍需整体收尾：Fields → ContextBindings → ParserRuntime，values唯一调用仍fields.cpp:439。未配置不等于死代码；不能孤删/stub或新增双轨。`fe5eba6e` 按用户“全部提交”保存了原有4文件投影补丁（context_bindings.cpp/.ixx、mtable_bindings.cpp、semantics_bindings.cpp）；它们不在478活动源内，未重新验旧协议。**这是交接存档，不是继续扩展投影补丁的方向**，仍按RAII架构退出整条旧链。
4. 整个旧移植清理最终完成前，还要对剩余源码/工具/依赖做对应验收及完整完成审查；D2D不是本轮接入前提。本轮请求字体/批量/视口改动已获用户确认，不代表剩余旧迁移已验收。不要把既有Source-only检查或编译成功当作新的native运行验收。

## 固定边界与本机资料

- 保留MathJax的TeX/MML/Common家族职责；SVG和未来D2D消费同一完成布局，不引入平行算法、Scene家族或JS解释/对象语义层。旧原型/getter/rebind矩阵仅留离线Source来源记录。
- Root独占Git/CMake/build/native，一次一个build、parallel≤4。Windows console probe链接 `folia_console_test_runtime`；异常退出后只做有界诊断。小批提交，不reset，不覆盖接手人的改动。
- 已完成staging/执行proof不重复运行；512/513及verification旧256/257是host预算，不新增MathJax深度规则，不重新抽取旧对象协议测试。
- 之前6个dirty路径已按用户授权全部处理：4个旧binding源码保存于fe5eba6e；MathJaxSvgParser.cpp/SvgNormalizer.cpp只有行尾/状态差异，暂存后无内容改动。原备份仍在 `build/math-port/context-dirty-backup-20261021/`；不再存在“永远保护这6个未提交文件”的限制。
- **build/日志/候选/安装包均ignored，不随Git推送。** 远端接手者应按tracked脚本重建，不能假设本机候选存在；canonical Source profiles与owners在仓库中。live句柄只以上方当前checkpoint为准。
