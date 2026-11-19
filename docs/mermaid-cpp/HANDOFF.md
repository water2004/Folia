# Mermaid C++ 交接

## 当前检查点

- 2026-11-19：从 `daba4996` 建立 `refactor/mermaid-modules-and-themes`，按用户要求分批提交并立即推送。此次目标是目录职责重构与可移植、类型化主题入口，保持 Mermaid 12.2.0 锁定版本、现有模块名称和渲染算法。
- 第一批（`816c1c34`）将公共 API、前端、配置、主题、文本与支持代码移入对应目录，生成数据随所属组件保存；同步修改生成器目标路径与 MSVC `/bigobj` 源文件路径。64 个移动文件在排除相对 include 路径后逐字节相同，所有本地 include 均可解析。
- 第二批将 Dagre、ELK、CoSE、fCoSE 与 RoughJS 几何按算法归类，模块名称、接口和算法保持不变。逐字节对照与本地 include 检查通过；Linux C++23 `import std` 工具链已就绪。
- 当前工作目录 `/workspace/scratch/daa11de45e84/Folia`，可移植工具链准备中；本轮尚未完成编译／运行验证。下一步：图族与布局模块归类、主题 API、Linux 构建与渲染验证。根代理负责共享接线及提交，主题代理负责新主题 API，构建代理负责工具链，架构代理负责结构约束与说明。

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

1. `src/mermaid/frontend.{ixx,cpp}` 识别图族、整理配置及主题。各图族模块解析并持有类型化数据。
2. `src/mermaid/render_entry.{ixx,cpp}` 统一准备和分发。核心保持标准 C++23。布局与 SVG 写出使用图族模块和已有共享算法。
3. `src/platform/text/mermaid_measurer.ixx` 负责 DirectWrite 测量；`mermaid_renderer.ixx` 将平台测量结果传给核心。新图族平台测试需加入 `tools/mermaid-cpp/CMakeLists.txt`。
4. `src/platform/text/mermaid_host.ixx` 和 `tools/mermaid-cpp/native-renderer.{h,cpp}` 提供宿主与 C ABI。JS 仅供上游对照脚本使用。

## 复验

参见 [构建说明](README.md)。本地对照使用 `.reference/mermaid-js` 的 `1fb7869866dd619168d840291c9c54b4f129758b` 和锁定的 Oracle 依赖。平台测试通过 `FOLIA_MERMAID_SVG_OUTPUT` 输出 SVG，再运行同图族的 `compare-*-svg.mjs`。几何容差固定为 1.5 px，ELK 算子容差为 1e-4。布局使用实际文本测量。RoughJS 零粗糙度矩形允许控制点有差异，检查笔画几何、端点和绘制属性。

WinUI 的预览、编辑刷新、主题、尺寸和 PDF 验收见 [宿主集成](winui-integration.md)。普通 SVG 的规范化继续使用独立的 usvg DLL。
