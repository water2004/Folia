# Mermaid C++ 交接

## 当前任务：测试系统重构（2026-11-24，已完成）

- Windows 主分支集成以重构分支 `f3b073b9` 为基线。统一入口的 core、conformance、tooling 三套在 MSVC 下通过 19/19 个 CTest 条目。修正 Canvas 测试的换行字节处理和正式语料目录的发现范围。464 个语料文件的已记录散列均匹配，结构检查及主工程 Release Mermaid DLL 构建通过。日志位于 `build/portable-test-merge/`。
- 在既有 `refactor/portable-test-system` 上完成；master 基线仍为 `2ba389aa34a5c2f23500dc552b9fab9c434a0cdd`。本次从 `c603f4db` 续做，所有完成批次均独立提交并立即推送，提交带 `[skip ci]`。本次续做未改 CI，master 与标签未动；此前分支已同步 Mermaid workflow 的 Python 比较入口/依赖，触发配置与 master 相同。分支 Actions 查询为 0 次运行。
- **最终统一入口通过**：`python tests/run.py --suite all --build-dir build/test-refactor/portable --jobs 4 --test-jobs 1`，七套 catalog、**26/26 CTest、0 failed/CTest skipped**。验证代码版本 `d378f543aff1398f4b36b8868c88c88d1017862d`，之后只补验收文档。Linux Clang/libc++ 21.1.8、CMake 4.3.4、Ninja，真实 Boost.UT/C++23 模块、单 native 构建并发 4；CTest 用时 54.24 秒。
- 完整原生结果：Core **1098 项/1305869 断言**；新增接线的既有 SDK-free policies **41/9096485**；Markdown 独立消费者 **4/61**；Completion **23/5562**；Math editor **7/15330**；Math **996/547261**；SVG/C ABI **83/2381**；Mermaid **274/959627**；真实 Math 集成 **5/205**，这些完整入口均无过滤/跳过。
- 所有活跃 portable Python 测试文件 **40/40** 已接入，实际执行 **2828 项**全部通过：Mermaid 比较器 2567、Math 工具 176、Markdown 工具 45、GFM runner 11、LaTeX 目录 7、测试系统 22。遗漏的 7 个 SDK-free editor policy 文件、4 个 Markdown consumer 用例和 LaTeX 目录检查均已补入统一入口；可选 md4c target 开启时也纳入聚合，默认关闭且本轮未运行。
- 数据与逻辑分离完成：`tests/math/fixtures` 无 `.inc/.hpp` 编译数据；最后八组 MathML/scene 的 **14326 个字段**通过旧 aggregate 与新 reader 独立逐位回环，8 个实际生成器 emitter 输出逐字节相同。最后 33 行 CSS 元数据/132 字段亦完整保留。Renderer、Lite、table 和 CommonMark projection 按职责拆分；排除 Windows/fixture 数据后，591 个通用 C++ 测试与支持文件最大 **577 行**。
- 永久完整性检查 `tests/tools/check_fixtures.py` 覆盖 **464 个文件、119059 条 TSV 记录**；SHA-256、字节、顺序、行数、缺失/新增均有失败控制。12 项破坏/边界测试通过。LF 文本、原始上游物理 CR 与 PNG/RGBA 二进制分别受 Git attributes 保护。历史19批迁移记录的43个原始blob/133个现存fixture哈希重新核对；明确不把此哈希汇编冒称重跑所有历史 Source 实验。
- 原始 **2811 个具名注册（含重复）全部保留，现有2816**。120个历史Windows-runner/support原始哈希仍保留：118无改，2个SDK-free测试各补一条确切import；只移除此唯一整行后必须恢复原哈希，新增负例确保不能隐藏断言/输入修改。production snippet模块也仅补selection import；实际Windows SDK测试未改。本轮独立重比mhchem 7960记录/22287 expected chunks零差异；完整Math较历史少1断言，是冗余constexpr数组计数检查由严格7960行loader替代，并非删例。
- JavaScript 测试工具职责收束为官方 MathJax/Mermaid/真实参考后端采样；原生进程编排、比较、RAII inventory、ledger、Source profile验收、parser faults、diagnostics与报告由Python承担。最后SVG paint真实对照 **76/76记录、82 SVG、547 paint、127可见性检查**通过；原/新验收246张PNG字节相同，禁用Node的离线replay也76/76通过。保持原容差、Source锁和来源独立性。
- CommonMark **652/652**严格HTML、CST/保存均过；GFM **1426/1426**输入/无损/精确保存检查执行，HTML为1395严格一致、11已审查的当前CommonMark差异、20明确不评分（smart/tagfilter/上游IGNORE），0未解决差异/缺例。完整21个病理输入及深层owner/50000quote/1000list检查通过。不能将GFM不评分项写成1426个严格HTML通过。
- 验收摘要见 [test-system-validation.json](../test-system-validation.json)，使用说明见 [tests/README.md](../../tests/README.md) 与各域README；完整日志 `build/test-refactor/resume-portable-all.log`、`build/test-refactor/portable/Testing/Temporary/LastTest.log`，JUnit和`run.json`同目录。JUnit成功输出有1024字节截断，断言总数取完整LastTest.log。
- 边界：按用户要求未运行Windows SDK/WinUI/DirectWrite；本任务不声称新完成旧375例Mermaid官方浏览器整图验收。历史Source命令缺旧capture/source path或要求Node24.13.0时仍严格失败，未改锁或把offline测试冒称fresh capture。生产资源生成器仍按现有构建使用Node。
- **本轮实现、统一回归与分批发布已完成，无继续实施的待办。** 所有agents已交回，无活动构建/测试句柄；最终文档发布后核对本地与远端同一HEAD、工作树干净。下一步仅按用户新反馈，不因恢复/压缩重开历史功能任务。

下方记录是本任务开始前已完成的功能基线，不能替代本轮测试结果。

## 历史检查点

- 2026-11-19：合入原生 Math SVG 绘制修复后，Windows 统一在构建时同步 Mermaid 使用的 Math DLL，覆盖导入库未变的增量构建。Mermaid Math、DLL 宿主测试与 22 项真实 MathJax CSS 公式对照通过。详见 [数学交接](../math-port/HANDOFF.md)。

- 2026-11-19：WinUI 的「常规 → 图片与图表」已接入独立 Mermaid 主题选择，通过 `FoliaNativeMermaidThemeName` 读取 DLL 的同一份主题清单，命名主题走现有 C ABI。切换主题会重建待渲染内容，源码主题仍优先。Windows Release 应用构建、Mermaid 工作线程 5 项和 WinUI 设置页定向测试通过，另验证图片按正文宽度缩放。日志 `build/media-settings-{build-final,mermaid-test,ui-test,image-test}.log`。

- 2026-11-19：`refactor/mermaid-modules-and-themes`（`26fd18e3`）已快进合入 `master`。集成提交 `c9ba1b17` 将 Python 限于可选的结构检查，在 Windows 按实际写入的 SVG 字节计算散列。禁用 Python 查找的独立 CMake 配置已通过。
- Windows MSVC Release 的结构、核心、DirectWrite、DLL 宿主、Math 集成测试 **5/5** 通过。DirectWrite 主题矩阵 **374/374** 生成有效 SVG，磁盘文件散列全匹配。官方浏览器 SVG 对照通过 **72 项**（State 32、Radar 16、XY 24），XY 另以浏览器测量复验 24 项。日志 `build/merge-mermaid-themes-{build,ctest,no-python,matrix}.log`，报告在 `build/mermaid-regression/windows-theme-merge*/`。本次没有执行完整 375 例官方 SVG 对照，亦没有运行 WinUI 界面验收。
- 分支基线 `daba4996`，最后的核心实现提交为 `a436adb3`，其后是复现工具、维护说明与验收记录。
- 原有 **342 个 C++ 源文件／片段** 按职责与图族归类，**144 个既有模块名全部保留**。当前 347 个源码文件、147 个模块接口。公共编排在 `api`，前端／配置／主题各自独立，图族在 `diagrams/<family>`，共享布局在 `layout/<engine>`。生成器、相对 include 与 MSVC `/bigobj` 路径已同步；结构检查注册到 CTest。说明见 [architecture.md](architecture.md)。
- 公共 `folia.mermaid`/`render_diagram` facade、11 种主题清单／名称反查、`ThemeOptions`/`make_theme_session` 已接入。未指定主题保留图族默认值，显式 default 与源码覆盖的优先级有测试；自定义变量保留原始输入，按请求隔离。Windows 类型化宿主与命名主题 C ABI 已补齐，旧 bool 接口的 default/dark + classic 行为保留。用法见 [themes.md](themes.md)。
- 主题完整图检查揭示并修复：Radar/XY 漏掉固定上游的默认主题合并，State 所有非顶层组合背景错误地使用同一 alternate 色。现在按上游规则合并缺省与逐层交替背景；既有主题生成数据不变。图族专属样式与节点顺序、并发 scope、多层嵌套均有回归。
- **Linux 原生构建与 CTest 2/2 通过：115 suites、274 tests、959627 assertions，0 failure/skip**。新增 19 项测试涵盖主题、facade、图族缺省和 State 深层背景。工具链 LLVM/libc++ 21.1.8、CMake 4.3.4、Ninja 1.13.2，libc++ 时区使用 `-fexperimental-library`，无 Windows SDK。日志 `build/mermaid-refactor-{build,ctest}-theme-fixes.log`。
- **34 图族 × 11 主题：374/374 有效原生 SVG，374/374 栅格化成功**，使用同一公共 facade、真实 Pango 1.56.3、librsvg 2.60.0/Cairo 1.18.4。10 个代表图族的主题对照已目视检查，最终 Radar/XY/State 再次检查。三处修复以外的 31 图族、341 个 SVG 与修复前逐字节一致。完整报告 `build/mermaid-themes/report.json`；可复现工具 `tools/mermaid-cpp/regression/render-themes.py`；持久验收摘要见 [theme-validation.json](theme-validation.json)。
- 每图族选择一份现有语料进行主题组合验证；该 374 组合不是旧的 375 例复杂语料。没有声称本轮执行官方完整 SVG/像素差分、Windows DirectWrite/WinUI、真实 Math DLL 集成测试。Windows 宿主回归已添加但未运行。Neo/Redux 暗色 Packet 的黑色位标、neo-dark State 的浅色备用背景等确属锁定官方预设，保持一致并在主题文档说明。
- 所有修复依据的上游文件均匹配现有 `source-lock.json`，不变更 Mermaid 12.2.0/commit `1fb7869866dd619168d840291c9c54b4f129758b` 或源码锁。固定源核查覆盖 Radar/XY/default merge、State 递归和样式、Packet 默认样式及暗色主题；证据散列保存在验收 JSON。
- 工作目录 `/workspace/scratch/daa11de45e84/Folia`；`source build/toolchain/env.sh` 后使用 `build/mermaid-refactor`，单 Ninja、并发≤4。最终 probe SHA256 `07c1200c3696c5669ab2aaaf7b4a85f4d689f5a19401a5427dcc133dce5380c3`。本轮实现和此环境验证已完成，无活动构建／测试／agent 任务；不重开已完成的目录、主题、字体或源码锁调查。

## 先前验收记录（历史）

- 2026-11-16：全图族回归已在 Windows 合入 `master`（合并提交 `540c1775`）。`4b970717` 让 State 说明与行内注释在词法层保留花括号，覆盖带 TeX 分组的节点、边和注释。`627b4c3c` 给独立 CI 构建接入 Math DLL，并按已锁定的 SHA-256 恢复上游源码换行字节。自动触发使用版本标签。
- Windows MSVC Release 的 Mermaid 核心、DirectWrite、DLL 宿主、Math 集成四套测试全部通过，Math SVG/C ABI 53 项通过。官方语法和 DirectWrite 完整原生语料均为 375/375。State 的 32 张 Edge SVG 对照通过，最大几何偏差 0.5 px，容差 1.5 px。独立 CMake 已用新 Math DLL 和导入库配置，登记了四套测试。日志见 `build/merge-mermaid-{ctest,math-bridge,corpus,state-comparison,standalone-configure}.log`，报告见 `build/mermaid-regression/windows-{native,official-prepare,state-browser}/`。下一步是完成全部 375 例的官方整图对照。

- 2026-11-16：全图族复杂回归位于独立分支 `feat/mermaid-comprehensive-regressions`，基线 `a368f9ea70dac36f235d9eca6bdb0be16ea82a5f`。代码与回归按来源、各图族修复、测量编排、ELK阶段、组件接线和验收工具分项提交；最终验证源码为 `06beebb47d8971f73e4f4b320bc05e76ceec16a1`，后续收尾提交仅文档。GitHub接口发布改变提交元数据，每个提交文件树均与本地验证版本逐项一致，原始本地验证编号与树散列保存在验收JSON。保持CI、原生Math实现、公式缓存与10MB压测文件不动。
- 固定 Mermaid **12.2.0** / `1fb7869866dd619168d840291c9c54b4f129758b`，ZenUML **3.49.2**。source-lock实际469文件全匹配，原417个已锁定文件哈希不变；原完整elk-worker保持固定来源。机器可读验收及每例SVG散列见 [regression-validation.json](regression-validation.json)，复验见 [regressions.md](regressions.md)。
- **34图族、375例、230个complex**，每族至少8例；53种Flowchart形状各两次覆盖。官方syntax/config **375/375**，生产完整原生渲染 **375/375**，没有skip/expectedFailure，没有删去失败的复杂输入。
- **最终核心255项/957780断言、真实Math集成5项/205断言全部通过，0跳过、正常退出0**。日志 `build/mermaid-regression/core-final-accepted.log`、`math-final-accepted.log`，汇总 `final-tests.json`。最终新增ER测试曾遗漏宿主initialize(default/classic)，现仅修测试初始化/诊断；原公式/多词标签输入保留，定向1项17断言通过，生产解析未因此放宽。
- 最终ELK对照全部通过：24个完整自环布局/5521断言；25个Greedy阶段/980断言；7个完整组件布局+14个阶段观察/1794断言；21个Mermaid方向/层级adapter布局/3702断言。算法容差仍 **1e-4**，SVG几何容差仍 **1.5px**。D3时间格式72个值、数值格式1794个值亦通过原版对照。
- ELK补齐自环预处理/恢复/side-slot路由、TWO_SIDED_GREEDY_SWITCH、共享弱连通组件DFS/packing、拆分前阈值和model-order身份。Greedy含真实交叉减少、端口重排、BARYCENTER后优化及48-bit RNG比较；SEPARATE孤点不采BARY/greedy RNG，INCLUDE原默认保留；node级自环配置不继承root。39/40/41节点阈值和DFS重排均有完整原版布局观察。
- Windows测量编排已提取到 `measured_render` / `TextMeasurer`，Windows仍提供原DirectWrite、Math服务和宿主宽度；Linux以真实Pango **1.56.3** 走同一生产C++管线，Zen另用实际Skia。没有按字符估宽或样例坐标特判。Windows/WinUI本轮未执行。
- 最终原生完整语料报告 `native-final/report.json`；Probe SHA-256 **d6932433fcc3440ec1cad64892bc9947849732958a9921c6360050679d473716**。原两例失败的Bang手绘和collapsed复杂自环均已按原输入修复，并在最终375例重跑中通过。
- **可执行的完整官方SVG证据共40例/3图族**：Info8/8、Packet8/8（257元素）检查完整SVG、真实fontconfig文件hash/index、geometry/paint/text，明确无栅格；Zen24/24采用实际原版renderer与同机Skia，296次真实测量，透明RGBA逐像素零差、视口和非空文字一致。最终Zen报告 `final-zenuml-canvas-native/comparison.json` 与上述Probe散列一致。
- **完整官方浏览器验收仍未完成：剩余335例/31图族**。本地Chromium启动socket EPERM；连接Edge任意测试页被URL安全策略拒绝且禁止间接变换载体绕过，已停止并完成CUA summary。不得将原生渲染、算法fixture或官方语法通过写成全部整图SVG通过；不得绕过浏览器限制。
- 最终正式总gate只提供Info/Packet实际捕获时，`final-partial-comparison.json` **16/375，browserCompared=0，officialSvgCompared=false，exit1**；其余359项均缺官方browser case。Zen24作为独立Canvas验收记录，不能伪装成browser capture。比较器74项自测通过，覆盖缺字/漏画/RGBA、CSS隐藏、视口、dx/dy及不能验证的文本效果拒绝。
- 工具链LLVM21.1.8/CMake4.3.4/Ninja；工作树 `/workspace/scratch/a4656ef82bb3/Folia`，构建目录 `build/mermaid-regression-native`。最后生产构建 `elk-final-build2.log`，测试增量构建 `er-test-config-build.log` 均正常成功；Math共享库以PIC构建，未改Math源码/缓存。始终单Ninja、并发≤4。全部agents已交回，无运行构建/测试/CUA句柄。
- 本环境可执行的实现与验收已完成，按用户要求提交推送独立分支。后续必需验收是在同一字体环境执行完整官方捕获/比较，以及Windows宿主验证；不能声称该剩余阶段已通过。不重开已完成的源码锁、字体、数学profile或ELK阶段调查，不改CI或创建发布。

## 数学支持基线（历史，已推送）

- 2026-11-16：`feat/mermaid-native-math` 已实现 Mermaid 内使用现有 C++ MathJax，基线 `922b2fdc`。六个功能提交已推送：`27ca37ac`（可移植构建）、`a7ace9a1`（Math SVG fragment）、`d8e5440e`（typed labels）、`729774bf`（Block/ER/Requirement/Use Case）、`31bcbb33`（Swimlane）、`65ea222c`（共享运行时与平台集成）；本交接、oracle 和验收文档随最后一批提交。未改 CI、既有公式缓存、版本或发布资产。
- 覆盖 Flowchart、Sequence、Mindmap、Class、State、Agentflow、Block、ER、Requirement、Use Case、Swimlane。TeX 在 Markdown/普通反斜杠处理前检测；普通文字用 DirectWrite 实测 advance，公式用 Math DLL 的精确 CSS 像素尺寸。布局副本共享不可变 MathLabel，Math fragment 结果不保留 Scene，没有静态复制字体数据。语法、配置和边界见 [math.md](math.md)。
- 全部真实 C++23 模块构建与回归通过：`FoliaMermaidTests` 220 项/937,106 断言；`FoliaMermaidMathTests` 5 组/229 断言；`FoliaMathSvgBridgeTests` 53 项/1,063 断言，全部正常退出。最终日志 `build/mermaid-math/final-{build,core-tests,native-tests}.log` 与 `build/mermaid-math-runtime/final-focused-tests.log`。12 份 native SVG 成功栅格化，代表图已目视检查。
- 与官方 MathJax 4.1.3、相同 NewCM profile 的 22 个公式，宽/高/基线实测最大差异 0，SVG 绘制树规范化后完全相同，2× RGBA 全部逐像素相同。记录与散列见 [math-validation.json](math-validation.json)；复现工具 `tools/mermaid-cpp/math-oracle/`。生产 Math DLL SHA-256 `b4d61e7c88724a8b30777bad563a99d65ca698dba156fbf29d20becc88e5ddf2`。
- 图布局仍锁定 Mermaid 12.2.0；该版数学用 KaTeX。官方 13.0.0-alpha.3 的 MathJax 插件已无 DOM 渲染 14 图 SVG/PNG，作为官方集成参考，不能混同 v12 整图坐标。旧 math scope 的 profile hash 首次提交即不匹配已提交 entry，已查清；新 oracle 单独锁实际未改 profile，旧 gate 未修改/未冒称通过。
- 验证限制：Windows DirectWrite/WinUI 与新增 Windows 宿主用例在此 Linux 环境未执行；Chromium socket EPERM，升级被自动审批拒绝，未绕过。公式精确一致不等于所有完整图通过浏览器验收。保留上游 Block 居中组合块标签、Dagre compound 标题 margin 等已有布局规则。
- 无运行构建/测试/agent 句柄。功能与可复现检查已完成，下一步由用户在 Windows 构建核对；不重开已完成的 profile、字体、模块导出调查，不额外启动 CI/发布。

## 已验证基线（历史）

- 2026-11-13：已移除 Mermaid 渲染入口的 Markdown 复制转义恢复。带转义的 Kavora 输入与 Mermaid 12.2.0 一样报语法错误，标准 Mermaid 版在默认 ELK 下正常渲染。核心、平台、DLL 三套测试和 20 张 Flowchart SVG 对照通过。可运行样例为 `build/mermaid-kavora/Kavora-ELK.md`。

- 2026-11-13：ELK 复合流程图支持跨子图反馈边。默认按 Mermaid 12.2.0 的子图投影确定反馈边，布局完成后恢复原箭头与端点标签方向。原生 ELK 在 `orientFeedbackEdges: false` 下也能路由固定端口造成的同层边。按上游顺序导入边及端口后，7 个 ELK 复合布局算例（412 断言）和 20 张 Flowchart SVG 对照通过。Class ELK、State ELK、Agentflow、ER、Requirement 的浏览器对照也通过。验收见 `build/mermaid-kavora/`。

- 2026-11-07：旧 Rust Mermaid 渲染器及其专用工具、验证归档已移除。桌面端使用 C++ Mermaid，usvg 负责 SVG 规范化。

- 2026-11-07：GitGraph 按锁定的 12.2.0 语法统一读取命令选项，支持紧贴冒号的值、引号转义、带引号的分支名及各命令的参数约束。31 个上游解析用例通过，亮暗主题 16 张 SVG 对照最大偏差 0.011 px，GitGraph 专页 35 个官方示例均可原生渲染。官方综合示例的 Folia Direct2D 成图见 `build/mermaid-official/10-git.png`。

- 2026-11-07：原生 Mermaid 已接入 Folia 0.7.0，SVG 规范化仍使用 usvg。本地 Release MSI 为 `build/installer/bin/Folia-0.7.0-x64.msi`，55,102,474 字节。MSI 产品版本与 EXE 产品版本均为 0.7.0，实际启动和正常关闭通过，原生 Mermaid SVG 视口和文字保留 24/24 通过。WiX 只有既有 ICE61 同版本升级提示。打包日志与校验见 `build/folia-070-msi.log`、`build/folia-070-msi-verification.json`。GitHub 自动构建由 `v*.*.*` 标签触发。

- 2026-11-07：锁定版本的 34 个图族都有通过的基础上游 SVG 对照。Packet、Pie 使用 jsdom；Railroad 四种语法合计一个图族。State 默认 ELK 已提交 `d8a150ad`，主工作区通过默认 ELK 34 张和显式 Dagre 32 张，ELK 最大几何偏差 0.5 px。Sol 独立验证 34 个图族默认配置，两种 State 写法和四种 Railroad 语法合计 76/76，SVG 规范化保留了逐项检查的 382 个文字元素。结果在 `build/mermaid-full-winui-gate/results.json`。
- ER、Requirement、Use Case、Swimlane、Venn、Event Modeling 使用实测结果输出普通 SVG 文字。对应提交 `4b3edae7`、`89c92b9b`、`95e2b2b5`、`0dea94ef`、`62c18bbe`、`a7c0af06`，对应上游对照 32、44、30、14、36、14 张。Venn 连续 CJK 断行复用 DirectWrite，`b40f4017`；Swimlane 对照补测实际文字范围，`bfe663f5`。
- WinUI 原生 Mermaid 接入已合入 `a00be23c`，ER 普通 SVG 标签已合入 `4b3edae7`。两处隔离验收快照已核对并保存在本地分支 `codex/archive-folia-mermaid-integration`、`codex/archive-er-portable`。宿主架构和复验命令见 [宿主集成](winui-integration.md)。
- Flowchart 已原生实现 53 种登记形状中的 44 种。文档虚线与无轮廓手绘文字边界 `84b3c7f4` 通过经典 20、Neo 16、手绘 24、额外形状 22 张对照。ELK 边界与端口 `8160060d` 通过 18 张。符号、存储、手绘形状分别有候选 `build/candidates/flowchart-symbols/`、`flowchart-storage-integration/`、`flowchart-handdrawn-organic/`。存储形状 `a42a2b68` 通过 64 张经典对照，覆盖 Dagre、ELK、两种主题和多行。Neo 与手绘尚待补测。其余 9 种符号候选在 `build/candidates/flowchart-symbol-integration/integration.patch`，有 36 张浏览器基准，下一步协调原生定向构建。
- 公共入口、DirectWrite、DLL 与 CI 基础覆盖 `4af512ff`，11 种主题 `8f3c9962`。Agentflow `c0abef0c` 的诊断和元数据已验证。ELK 容器端点 `48e2d1c2` 已通过 Agentflow 16 张及 State、Flowchart 和 Class ELK 回归。Agentflow 交叉线跳线 `ccc659c4` 通过 20 张对照，最大差异 0.773 px，另通过 Flowchart、State、Class ELK 回归。其余 ELK 图族尚未接入共用跳线路径。已验证的配置和样式范围以各图族基准为准。
- 2026-11-07 集成验收的核心、平台和 DLL 测试 3/3 通过（163.07 秒）。日志见 `build/mermaid-complete-ctest-final-release-baseline.log`，基准 SVG 在 `build/mermaid-complete-svg`。

## 边界

1. `src/mermaid/frontend/frontend.{ixx,cpp}` 识别图族、整理配置及主题。各图族模块解析并持有类型化数据。
2. `src/mermaid/api/render_entry.{ixx,cpp}` 统一准备和分发。核心保持标准 C++23。布局与 SVG 写出使用图族模块和已有共享算法。
3. `src/platform/text/mermaid_measurer.ixx` 负责 DirectWrite 测量；`mermaid_renderer.ixx` 将平台测量结果传给核心。新图族平台测试需加入 `tools/mermaid-cpp/CMakeLists.txt`。
4. `src/platform/text/mermaid_host.ixx` 和 `tools/mermaid-cpp/native-renderer.{h,cpp}` 提供宿主与 C ABI。JS 仅供上游对照脚本使用。

## 复验

参见 [构建说明](README.md)。本地对照使用 `.reference/mermaid-js` 的 `1fb7869866dd619168d840291c9c54b4f129758b` 和锁定的 Oracle 依赖。平台测试通过 `FOLIA_MERMAID_SVG_OUTPUT` 输出 SVG，再运行 `python tools/mermaid-cpp/compare-family.py FAMILY original.json native-svg-directory`。几何容差固定为 1.5 px，ELK 算子容差为 1e-4。布局使用实际文本测量。RoughJS 零粗糙度矩形允许控制点有差异，检查笔画几何、端点和绘制属性。

WinUI 的预览、编辑刷新、主题、尺寸和 PDF 验收见 [宿主集成](winui-integration.md)。普通 SVG 的规范化继续使用独立的 usvg DLL。
