# MathJax 原生移植：当前交接

恢复/压缩后先实际读本文。架构、职责和详细验收见 [architecture](native-raii-architecture.md)、[files](native-raii-files.json)、[acceptance](native-raii-acceptance.md)。只复查新变化；ignored候选用 `rg --no-ignore`。

## 当前 checkpoint：图片粘贴与未保存关闭确认已完成（2026-10-26，未打包安装）

- 用户要求剪贴板图片落在MD同目录、新文档先保存，关闭有修改时保存/丢弃/取消。已本地提交：serializer新图片引用格式化 **38117303**；图片存储/粘贴、统一保存/关闭与正式测试 **ae14280a**。原目录提交57e85f0b保留；版本固定0.4.0，不自动推送/发布/安装，无子代理/Computer Use。
- ClipboardImageStore后台解码位图为唯一PNG；复制的图片文件按ContentType识别并异步保留原格式复制，同目录唯一名称不覆盖原图。Controller.Clipboard优先Bitmap，其次图片StorageItems（多图一个撤销事务），其余文本沿用普通粘贴。未命名文档先await保存，取消不插入/不写图片。相对引用由serializer生成，富文本/源码统一Command::Paste事务；失败/过期时清理仅本次已创建文件，Undo保留图片以支持Redo。未扫描Markdown/引入第二编辑模型。
- Controller.Save归并三个重复保存路径为awaitable bool；按钮、首次图片保存、关闭共用。后台编码/OpenTransactedWriteAsync成功提交后才mark saved，取消/失败不关闭。使用既有reload fingerprint作保存基线，后台加载后对编辑器表示计算一次基线，避免CRLF/块分隔格式误报；不增加每字符全文遍历或常驻全文副本。保存完成仍有新编辑则再次询问。
- AppWindow.Closing先Cancel并排队离开同步通知，再由强生命周期协程显示保存/不保存/取消ContentDialog，批准后Close；重复请求不叠加提示。打开/编码重开替换文档也确认，ExternalReload既有流程不变。异步操作捕获loadSerial+已提交documentSerial/选区/修订，替换中禁止Save/Paste，文档切换后旧结果不写入新文档。保存/提示回调复制到协程帧，Detach期间仍保持必要生命周期。
- **验收完成**：setup和Debug构建成功，最终WinUI完整 **68项/14971断言通过**（新clipboard6项/76、lifecycle10项/57）。覆盖实际PNG像素/相对路径/同目录、两模式Undo/Redo与零全文parse/serialize、首次保存取消/成功、多图文件复制、坏图片/过期请求/文本粘贴；关闭保存/取消/失败/新编辑重查、精确保存基线Undo/Redo、CRLF/视图切换、打开替换、重复关闭/Detach回调生命周期与替换中禁止Save/Paste。日志build/clipboard-close-{setup,app-build-final,app-final}.log；43793 exit0，无活动native/build/test句柄。
- **交付/停止点**：候选build/app-winui/bin/x64/Debug/FoliaClipboardPreview.exe；未启动实际UI、未安装/生成新MSI。测试使用真实DataPackageView/编码/存储和正式Controller，注入保存目标/提示结果，不改系统剪贴板；未做真实ContentDialog/FileSavePicker交互或窗口关闭按钮人工复验。图片是用户文档资源，非旧数学临时缓存；不自动删除Undo后的图片。下一步按用户要求试用/打包，不移动已发布0.4.0标签、不重开旧宽审。

## 历史：目录换行与正文同步已完成（2026-10-26，未打包安装）

- 用户任务：长目录标题自动wrap、目录跟随正文。实现本地提交 **57e85f0b**。版本保持0.4.0，不自动推送/发布；无子代理/Computer Use，不重开旧数学移植或Mermaid事项。
- SidebarController使用Wrap/不截断的TextBlock模板、禁用横向滚动及Stretch容器，层级padding独立于标题文字。保持虚拟化；按outline.content_key更新标题投影，滚动不重建列表。SyncViewport在draw后读取既有visual-to-source映射的顶部可见行，按renderModel.editable_index二分对应标题；不依赖标题文字匹配/不扫Markdown，不改正文选择、文档或历史。行边界采用可见行规则，避免鼠标命中上一空行而晚切章节；短尾章节在文档底部高亮末标题。
- 目录导航改用原生ItemClick（支持重复点击已高亮项），KeyUp保留方向/Home/End/Page导航；程序选中与用户激活分开，RAII guard避免同步重入。renderer新增Start/Nearest滚动对齐，原光标可见性默认Nearest不变；点击离屏嵌套标题先materialize所在flow，再按真实行几何校正到顶部。
- 验证：setup及最终Debug构建成功，**WinUI完整52项/14838断言通过**；新目录5项/71断言，覆盖真实模板中英/emoji长标题两宽度测量、重复/嵌套标题、滚动不改选择/源文、零全文parse/serialize、目录投影复用、标题编辑Undo、文档替换/Detach、源码模式及table/list/quote/math owner。日志build/outline-{setup,app-build-final,app-final}.log，59486 exit0。无core/platform源码改动，不重复旧宽测。
- **交付/停止点**：无活动native/build/test句柄。候选build/app-winui/bin/x64/Debug/FoliaOutlinePreview.exe，未启动/安装/生成MSI；测试通过LoadContent测量正式TextBlock模板，没有新人工UI/实际已加载目录容器外观检查。新改动不在已发布0.4.0安装包里；保持版本，不移动既有标签。下一步仅按用户反馈试用/打包，不再重跑完成的宽审。

## 已完成：0.4.0 已发布（2026-10-25，10-26核实）

- 用户接受公式SVG并授权发布0.4.0；版本/说明提交6d20710e已推送master，v0.4.0标签固定6d20710ee5084f7bfcc273f78bb4aa6a83fd1cd2。Release https://github.com/water2004/Folia/releases/tag/v0.4.0 已公开；既有CI https://github.com/water2004/Folia/actions/runs/40190865950 已完成发布，未移动标签。
- GitHub附件Folia-0.4.0-x64.msi **53,492,587 bytes**，digest SHA256 `85d0e1401ebf778de4754860fca327a969b3fa71f6011badd904591cc1b24470`；同名.sha256附件87 bytes。此前最终WinUI47项/14767断言通过：build/release-0.4.0-app-tests.log。未安装/覆盖用户窗口。新目录改动不在此已发布标签中。

## 历史：滚动修复与公式 SVG 外观（2026-10-25，均获用户认可）

- **用户已验证滚动修复**：“不用采了，我验证已经修复了”。第二次对照采样取消，未执行新UAC/PresentMon，不再采样。Release候选 `build/app-winui/bin/x64/Release/Folia.exe`（最后所见PID38752）由用户使用，不能关闭/覆盖；安装版仍旧0.3.2，尚未更新此候选。版本保持0.3.2，无推送发布/子代理/Computer Use；native一次一个，并发≤4。
- **已完成滚动实现**：EditorFrameScheduler使用系统合成时钟，先等合成tick再等GPU buffer；现代路径不再叠加独立刷新率timer，旧系统保留高精度timer。Start/Stop成对DRR boost覆盖惯性阶段；Normal优先级、单待处理帧、取消和异步让出保持。scroll_state内部offset/target/Snapshot用double，渲染边缘仍float，修复大坐标末段舍入停滞，惯性曲线不变。原实现新测试276断言失败，修复完整platform **83项/4230断言通过**：`build/scroll-inertia-{precision-before,platform-after}.log`。
- **滚动验收**：Debug构建及完整WinUI **47项通过**（`build/scroll-compositor-app-{build,final}.log`），Release自包含构建通过（`build/scroll-compositor-release-build.log`）。新增Detach取消clock/GPU等待、真实renderer连续输入/惯性停止/文档选择不变/零全文parse与serialize测试；后者默认生成CI文档，可用FOLIA_SCROLL_TEST_DOCUMENT只读指定≤16MiB文档。新clock回调约120/s只是回调率，不当作呈现FPS。无after呈现采样，仅用户实际验证。
- **现场证据/范围**：原整合.md只读338,898 bytes/6840行。已授权的首次20秒PresentMon得到活动段约50.6显示帧/s，间隔中位16.655ms/p95 33.369ms，GPU中位0.868ms；`build/scroll-actual-present-before{.csv,-summary.json}`。旧DwmTiming约60Hz，DCompositionWaitForCompositorClock实测8.33ms。真实文档早期Release renderer测量输入中位0.66–0.89ms/惯性0.43–0.44ms，但该版本未pump异步资源，仅CPU证据，不作FPS结论。
- **已撤回测试宿主试验**：Release console宿主缺合并manifest导致main激活异常；手工补manifest可跑单个真实文档测试，但完整UI Controls仍0xc000027b。已停止宽测；tools/run_winui_tests.ps1及tests/app/test_document_loading.cpp均恢复HEAD，无diff。不要重开此旁支。ignored `build/scroll-test-host-experiments.patch`、`build/scroll-release-perf-runner.ps1`仅保留失败试验，不能作正式通过。最终验证使用干净Debug正式宿主。
- **最新公式图标任务**：用户截图指行间$$太粗、像两枚货币符号/大方块。仅重新绘制DoubleDollar、MathBracketOpen/Close三个SVG：底块改扁平浅底、笔画1.7→1.3、收紧留白，7内置主题同步三个嵌入SVG；行内图标、CST/语义编辑、整对象命中、布局尺寸不改。tests/app/test_syntax_icons.cpp增加可选FOLIA_MATH_MARKER_PREVIEW原生离屏上下文预览。Debug构建成功，focused syntax icon **3项/10456断言通过**，`build/math-marker-refined-{build,tests}.log`；`build/math-marker-refined-preview.png`已实际查看（源码/上下两端图标），未安装给用户，视觉待确认。
- **停止点/下一步**：无活动native/build/test句柄（最新图标构建测试已结束）。7主题×3SVG共21份已逐一核对一致；滚动数值修复提交7bfef664、合成时钟提交759dd35b，本轮SVG/预览与记录在本文件所属提交。将原生预览给用户确认外观。保持正在使用的Release候选窗口，不强制退出、不重打覆盖该exe，不自动发布。此前CST编辑与数学渲染验收不重开。

## 历史：通用滚动调度回归（2026-10-25，已修复并安装）

- 用户反馈通用滚动似乎锁60帧，特别怀疑上次卡死修复；明确不按数学专属问题排查。新证据：e5c39fda把EditorFrameScheduler帧回调High改为Low；正式测试显示持续Normal UI任务让Low滚动帧等完全部64个任务才执行。不是已证明固定60帧锁，也没有现场呈现FPS。当前无运行中Folia，未操作UI/用户窗口。
- 修复仅把帧回调改Normal，保留显示刷新率/高精度计时/DXGI等待/单待处理帧/异步让出和嵌入通知合并；不恢复High或同步递归。新test_frame_scheduling.cpp覆盖普通任务与绘制相互及时运行及无GPU开销的调度节奏，旧输入公平性/延迟重绘测试仍通过。
- 验收：Debug成功；WinUI **45项/14153断言**全过。旧实现新测试失败：第一帧64/64；修复后1/64。无GPU开销的调度中位6.373ms、p95 6.586ms、约156.9回调/秒，**不是实际呈现FPS**。WMI报60不能单独当Folia锁帧证据；当前代码已按QDC物理刷新率选择。证据build/scroll-frame-{priority-before,app-build,app-tests}.log。无core/platform/math改动，不重复旧宽测。
- 本地修复提交`a80aec83`。Release/MSI构建成功并已同版本安装0.3.2，msiexec exit0；165个安装文件逐一哈希相符，settings字节不变（custom文件0）。MSI `build/installer/bin/Folia-0.3.2-x64.msi`，53,488,478 bytes，SHA256 `09D57E09242B334B8F3DE249EA7285B283A42A2A576301DF7FCCAEC3355A0748`，新ProductCode `{13BF8BD4-1154-4CDB-B8F1-B403134DF001}`。证据build/scroll-frame-{release-msi,msi-install}.log及{msi-properties,installed-payload-hashes,user-files-before}.json。安装前确认没有运行中Folia；没有启动/操作用户窗口。
- 停止点：本轮调度修复、正式测试与安装全部完成，无活动agent/build/test/install句柄（72650已exit0），版本0.3.2，无推送发布/子代理/Computer Use。下一步用户复测实际滚动；未采集实际呈现FPS，不把157回调/秒当屏幕FPS。若仍见稳定60需要现场呈现统计，不重开数学引擎。此前图标/波浪线修复保持，以下为历史。

## 历史：未闭合波浪线与行间公式图标（2026-10-25，已修复并更新安装）

- 新用户反馈：已安装版行间公式图标过小且相对TeX缩进；`1. 微观粒子质量~原子质量单位 $1u=1.6605655\timesz10^{-27}kg$`不渲染，删掉`~`正常。**根因与TeX无关**：inline parser已有Incomplete(~)内的正确InlineMath子节点，但render_builder的Incomplete分支把content整体原文输出。新正式测试在旧实现8处失败（~/~~ × paragraph/list/quote/bullet），修复后完整Core通过`build/math-marker-core-final.log`；只改CST→render递归，不在App扫描波浪线。
- 图标已修复：显示数学块明确设置Block placement，按主题行高×sizeEm分配方形图标，去除行内左右gap；行内图标维持原尺寸/间距。SyntaxIconRectangles改为直接读取排版对象metrics，不再从主题比例反推尺寸，使绘制/命中同源。源码、块缩进、语义编辑规则不改；主题仍schema2，docs/THEMES.md说明行间尺寸规则。
- 验证完成：Core **760项/499843断言**，WinUI **43项/14109断言**全部通过；Debug构建成功。两种块定界符×3字号×2宽度检查图标与TeX左沿一致、尺寸/整对象/源文不变；完整用户例子（含timesz）和移除~均走NativeMathSession得到SVG。日志`build/math-marker-core-final.log`、`build/math-marker-app-{setup,build,tests}.log`，9773/20640已exit0。旧用例8失败证据`build/math-marker-nested-before.log`。无代理/Computer Use；未改parser/数学引擎/源编辑/历史，不重跑旧移植宽审。
- 本地提交：CST投影与回归用例`ffb6e9a2`，行间图标布局与App测试`4dc6f85a`。
- MSI已构建并安装同版本0.3.2：`build/installer/bin/Folia-0.3.2-x64.msi`，53,508,958 bytes，SHA256 `9810AF1000803F2E28FBE75B3AE7B7EC376F3430D6693823DD0F1B6183C03654`；新ProductCode `{631F6F9D-A9FF-4895-9A33-D80A67332E88}`，msiexec exit0。165个已安装payload文件逐一哈希匹配，settings字节不变（本机custom文件0）。日志`build/math-marker-release-msi.log`、`build/math-marker-msi-install.log`，核验`build/math-marker-{msi-properties,installed-payload-hashes,user-files-before}.json`。安装版位于`%LOCALAPPDATA%/Programs/Folia/Folia.exe`；未启动或做本轮人工UI/真实IME复测。
- 停止点：无活动agent/build/test/install句柄（74657已exit0）；本轮实现/正式测试/安装核验全部完成并本地提交，版本保持0.3.2，无推送发布，无Computer Use。下一步用户试用已更新的安装版；不要重开TeX错误调查，原完整例子已经Native SVG测试通过。

## 历史：CST语义编辑、图标主题（2026-10-25，验收完成，MSI已安装）

- **固定规则**：全部所见即所得编辑按CST/块树语义；编辑层不依赖SVG、SyntaxIcon/SyntaxMarker或图标位置。公式定界符不可逐字符编辑，删除任一端成对去除、保留TeX为普通文本；ATX标题逐级降级。源码模式仍编辑原始源码。禁止子代理；版本0.3.2不变，不推送发布。
- **实现**：inline_source_edit查询完整CST定界符及移动边界；document_inline_format生成成对去格式TextEdit；document_block_edit将数学块通过树事务转换为段落；document_source_edit协调既有删除/移动入口；document_block_input按已解析标题等级退一级。未完成输入和code/math正文保持可编辑。没有扩大math原可编辑边界；block_source.ixx无diff，不得恢复已撤回候选。
- **显示根因已修复**：App按文字拼写选择图标曾丢掉CST完成状态，导致单独`*`图标化。现由render_builder消费CST种类/状态/范围给出图标身份，App只消费。显示数学块也使用block_source已有tokens，补齐两端SVG。删除593a9407引入的图标内部字符命中插值，恢复单个DirectWrite对象的整体命中；保持平时隐藏、点击出现、正文原位编辑与异步内存数学预览。
- **主题**：schemaVersion2单文件JSON包含15个SVG与sizeEm/gapEm，7主题更新；共享不可变资源、有限D2D缓存按主题/设备失效，绘制无文件IO。105个内嵌SVG与原资源逐一一致。v1自定义主题需按docs/THEMES.md升级格式，加载错误仍整份拒绝；不自动修改导入文件。
- **最终验证**：Core **759项/499811断言**、WinUI **41项/13919断言**、Windows editor **82项/3446断言**全部通过；Debug应用构建成功。日志`build/semantic-editing-{core-final,app-final,platform-final,app-build}.log`。覆盖CST完成/未完成、无图标的四反引号、嵌套格式、数学块根/quote/list、每字符编辑与Unicode、整体命中、复制粘贴、源码切换、精确Undo/Redo、零全文parse/serialize、主题像素/资源释放。未做本轮人工UI/真实IME复验；未改math引擎，不重复旧宽审。
- **MSI**：Generate-WixPayload只给打包themes顶层JSON添加精确文件名RemoveFile；正式隔离测试`tests/installer/test_theme_upgrade.ps1`实际同版本升级与repair均更新7主题，custom/settings字节不变，测试产品已卸载。证据`build/marker-theme-upgrade-test.log`，隔离目录`build/tests/theme-upgrade/3e9ae47f17c54b1d90e8a22ca3f35796`。未安装/卸载实际Folia，未生成正式MSI。
- **本地提交**：核心语义与回归测试 `5cfbe37c`；CST显示元数据 `1cface23`；App主题/显示 `7628deb3`；安装升级测试与最终checkpoint见本文件所属提交。全部本地保存，未推送。
- **MSI已按用户要求直接安装**：`build_msi.ps1 -Version 0.3.2`含setup恢复、自包含Release/WiX成功，并发≤4。产物`build/installer/bin/Folia-0.3.2-x64.msi`，53,492,574 bytes，SHA256 `C74854C9615783E1A2D4C11179DC1B6F63B2AF545723A25FAB4C5EF23F05EFC2`。msiexec静默同版本升级exit0，新ProductCode `{F2FA352F-229F-45D9-B566-3DC56559F11F}`。安装路径`%LOCALAPPDATA%/Programs/Folia/Folia.exe`；165个payload文件逐一SHA256相同（含7个schema2主题），已有settings字节不变，本机custom文件0。证据`build/marker-release-msi.log`、`build/marker-msi-install.log`、`build/marker-installed-payload-hashes.json`及`build/marker-install-*-before.json`。未启动安装版或做新人工交互验证。
- **停止点**：无活动agent/build/test/install句柄，87256已exit0。组合安装/核验脚本的写入被自动审批拒绝且未执行；随后清晰分开只读检查、直接msiexec安装及安装后哈希检查均获准并成功，没有重试被拒组合。无子代理、版本保持0.3.2、无推送发布。用户此前停止Computer Use后未再操作UI；旧Debug PID16448已不存在。下一步用户试用已安装版本。历史视觉基线593a9407、范围记录5ce20a98。

## 历史：SVG 外观与整对象排版（用户已接受，语义遗漏以上方为准）

- 用户否定继续给字体字形套背景，已选择保留 `*`/`**`/`~~`/`$` 等 Markdown 形状的成套 SVG；美观优先，统一尺寸、笔画、留白，保持平时隐藏/点击出现。禁止子代理、版本0.3.2不变，不自动推送发布。
- 用户再次纠正：图标必须是单一排版对象。已撤销每字符占位方案（窄列会拆图标），不再尝试给拆分方案补换行规则。采用一个IDWriteInlineObject覆盖完整marker范围；交互层通过既有display/source映射换算光标位置，source/CST/history不变。资源内嵌SVG、有限设备缓存；链接URL等文本不整段替换。
- 已实现13个原创SVG（强调/粗体/删除线/代码/数学定界符），统一24单位画布、1.7笔画、轻底色，成对共用资源。EditorSyntaxIcons只负责字形表/单个inline object/有限设备资源；排版与Painter消费同一几何。旧字符间距补偿、marker背景画框和专用brush已删除。链接URL/HTML及未收录拼写保持原始文本，不按任意字符替换。Core/parser/编辑事务不改。
- Debug构建成功；WinUI **36项/9738断言**、Windows editor **82项/3446断言**通过。包含整对象宽度/窄列不可分割、逐source位置命中/插入/撤销、Unicode、显隐、嵌套、斜体墨迹间距、真实SVG像素与资源重置。日志 `build/marker-icons-{atomic-build,all-app-tests,platform-tests}.log`；生产D2D渲染的深浅主题16/20/32 DIP样张 `build/syntax-icons-preview.png`。此前分占位/资源名带引号的失败候选不属于最终结果，诊断代码已删；不得恢复拆分占位或特殊换行补丁。
- 本地Debug界面已核对斜体/粗体/嵌套的对称、间距、点击显隐与无误选。预览PID16448、窗口24780394留着本轮76字符/修订1的未保存样例，不是用户文件。无活动agent/build/test；下一步用户验看外观，不自动发布。版本0.3.2不变。

## 历史：marker 样式与排版留白（被上方 SVG 方向替代）

- 用户认为上一版样式难看，并截图证明斜体正文与 marker 框重叠。只画框、未让排版保留空间的设计被否定；不得通过遮盖正文、绘制偏移或文本特判补救。marker-only/点击显隐/禁止子代理等边界仍有效。
- 实现仅在平台样式/DirectWrite排版/绘制层：字体恢复正体常规字重，SetCharacterSpacing只在 marker 首尾分配外间隙+内留白；绘制消费同一 HitTestTextRange 的 source endpoints/bidi 几何，扣除外间隙后画紧凑中性底色和轻底线，删除蓝色硬边框。样式集中于 EditorStyleSheet::MarkerStyle，不改 source/CST/选区/历史，不插入虚拟字符或控件。只有一套排版，光标/换行/命中自动使用已分配的间距。
- 验证中的发现：HitTestTextPosition 的字形run高度与 HitTestTextRange 的整行高度不同，不能按top相等匹配；已统一使用后者的文本范围，删除前者混用。中文模拟斜体伸出约0.234em，默认外间隙为marker字高0.30em（body约0.27em）；无按文本/字体名特判。失败候选日志 `build/marker-layout-{geometry-edges,focused-final,ink-details}.log` 保留，不把它们当最终验收。
- 已实际查阅 Zettlr `theme/editor.ts` 的标记字体/颜色处理及 Minimal 的弱化界面配色。正式测试覆盖 Segoe UI/Times New Roman/Consolas、12/16/32字号、中英文斜体实际墨迹边界和逐字符光标往返；完整 WinUI **33项/1090断言**通过，Debug构建成功（`build/marker-layout-{app-final,app-tests}.log`）。仅App样式/布局变化，Core/platform/math无改动，不重复宽测。Debug构建显式传 `-AssetsDirectory D:/md-cpp/src/app-winui/Assets`，实际主题加载正常，无源码路径改动。
- 界面已核对截图中的“斜体内容”：marker与正文之间有实际间隔，紧凑浅底/轻底线替代硬框；无误选。当前预览 PID62876 留着本轮生成样例（原始91字符/修订1），Computer Use 已检测到用户操作，停止继续输入。无活动agent/build/test；下一步用户验看新样式，不自动发布/改版本。自定义字体及深色主题未逐个交互验看。
- 代码/测试本地提交 `8bf67a4c`（5文件），未推送，版本0.3.2不变。

## 历史：marker-only 边界已完成，视觉与留白待上方改进

- 用户明确：**背景、轮廓和线条只属于 marker 本身，不属于被包裹的正文**；保留平时隐藏、点击编辑时显示。上轮把整段正文画框是错误实现，用户已否定，未提交。本轮已删除整段 StructureRange 投影和绘制，只使用现有 InlineStyleRange.marker。禁止子代理；数学保持正文原位编辑和异步内存预览。
- Web参考已实际查阅 SilverBullet `docs.silverbullet.md/API/syntax` 的 startMarkerClass/bodyClass/endMarkerClass 和 `client/styles/editor.scss` 的 sb-meta 独立样式（尤其删除线排除 marker）。只借鉴 marker/body 分离，原生 D2D 绘制 marker 淡底色与细边线，不引入 WebView/逐 span 控件。
- 实现：CST owning range 控制显隐；CST content_range 区分数学/链接/图片的定界符和内容，编辑时继续逐字显示原 source（含实体和转义），不改为语义文本。Painter 只绘制 marker=true 的范围；复用一支 brush，无整段装饰列表、额外控件、IO、解析或选区体系。标题 marker 同样按焦点显示；打印不画标记背景。
- 最终验收：Core **752项/499404断言**、platform **82项/3446断言**、WinUI **32项/775断言**全部通过，Debug应用构建成功。新增检查只允许 marker 进入装饰范围，正文/链接标题/TeX/图片alt不得进入；含跨行几何、Unicode、嵌套、实体源文保留、撤销及真实点击展开不误选。日志 `build/marker-only-{core-final,platform,app-tests,app-build-final}.log`。未改数学引擎，不重跑旧数学宽审。
- 界面已实际核验：阅读态正文无新增底色/轮廓；点击粗体仅两端星号有底色细线，中间文字无框且无选区。使用调试版默认回退主题（工具启动cwd非应用目录导致主题文件路径提示），未测自定义主题。旧测试窗口已关闭；当前新 Debug 窗口 PID8296 保留本轮生成的未保存演示文字供查看，无用户文件更改。无活动 agent/build。下一步为用户验看效果，不自动加正文容器、改版本或发布。
- 修正代码与正式测试已本地提交 `f1e0af64`（12文件，287新增/26删除），未推送；错误的整段装饰实现没有提交。版本保持0.3.2。
- 0.3.2 已发布，tag 固定 `e9b48e4a`；CI [40167476352](https://github.com/water2004/Folia/actions/runs/40167476352) 成功，公开/latest、MSI及checksum一致。`Folia-0.3.2-x64.msi` 53,485,235 bytes，SHA256 `9933ab6b2d2efaffb2c156dac081029f328db314721df120f59ed83b7b4b186a`，证据 `build/release-0.3.2-published.json`。未安装CI产物；已发布版本仍含旧的永久隐藏交互。本轮不改版本、不移动tag、不自动发布。

## 历史：0.3.2 发布过程（已完成）

- 用户已明确授权推送0.3.2，按既有tag触发release.yml发布；版本统一更新CMake/MSI默认/manifest/rc/About/README，新增release-notes/v0.3.2.md。此前不发布/固定0.3.1仅属上一阶段约束。
- 版本/XML/workflow/notes静态校验通过，master及新annotated tag已原子推送；v0.3.2固定在`e9b48e4a`。既有tag push流水线已触发：[40167476352](https://github.com/water2004/Folia/actions/runs/40167476352)。应用/Core/platform/math既有验收见下方。
- 发布已完成，最终验收以上方 checkpoint 为准。禁止移动已有 tag。

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
