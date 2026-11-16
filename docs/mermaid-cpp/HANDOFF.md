# Mermaid C++ 交接

## 当前检查点

- 2026-11-16：独立分支 `feat/mermaid-comprehensive-regressions`，基线 `a368f9ea70dac36f235d9eca6bdb0be16ea82a5f`。远端同名分支仍在基线；本地已按来源、图族修复和共享算法分项提交，均尚未推送。工作树 `/workspace/scratch/a4656ef82bb3/Folia`，CI、公式缓存、10MB压测文件不动。
- 固定 Mermaid 12.2.0 / `1fb7869866dd619168d840291c9c54b4f129758b`；source-lock实际469文件全匹配，原417个已锁定文件哈希不变，证据 `build/mermaid-regression/source-lock-verification.json`。原版完整elk-worker已锁定，新ELK阶段不另换来源。
- 语料 **34图族、375例、230个complex**，每族至少8例；53种Flowchart形状各两次覆盖。禁止skip/expectedFailure、删除失败复杂输入或让原生输出覆盖官方期望。官方syntax/config **375/375**：`official-prepare-final/report.json`。
- 生产测量编排提取到 `measured_render` / `TextMeasurer`。Windows仍为原DirectWrite/Math服务；Linux probe用真实Pango走同一C++管线，Zen另用实际Skia。Windows/WinUI本轮未执行，C4宿主可用宽度显式传入。
- 工具链LLVM21.1.8、CMake4.3.4、Ninja在工作区外层toolchain；构建目录 `build/mermaid-regression-native`，始终单Ninja、并发≤4。最后构建session21110正常退出0，日志 `elk-final-build2.log`，包含所有最终ELK源码。没有Ninja在运行。
- **全核心最终重跑发现失败，已停止后续全量**：session13877 exit255，`core-final.log` 在 `table_math_labels.cpp:52` 的新增 `mermaid_er_relation_math_labels_preserve_source_for_math_provider` 中prepare_er_render失败。定向同例亦exit255（`er-math-focused-before.log`）。graph正排查ER解析/官方语法；其他C++freeze。未执行该串行任务后面的Math。旧首轮核心243项944922断言、旧真实Math5项205断言只能作先前记录，不是最终全通过。
- 最终ELK定向全部通过：Greedy **25例/980断言**（`greedy-final.log`）；components **7例完整几何+14个阶段观察，2项1794断言**（`components-final.log`）；direction/scope **21/21，3702断言**（`direction-final.log`）。原版自环 **24例/61节点/219边，5521断言**通过（`elk-self-loops-first.log`）。算法坐标容差保持1e-4。
- ELK实现补齐自环预处理/恢复/side-slot路由、TWO_SIDED_GREEDY_SWITCH、共享弱连通组件DFS/packing、拆分前阈值与model-order身份。Greedy含17个真降交叉、5个port重排和3个BARYCENTER后仍改善案例；SEPARATE孤点不消费BARY/greedy RNG，INCLUDE旧默认保留；阈值0强制，其余严格count<40。node级self-loop选项不继承root。
- 原生完整渲染首轮373/375；Bang相对弧手绘及collapsed复杂自环两例均已按原输入修复并定向通过（`symbol-repair/report.json`、`collapsed-selfloop-fixed/report.json`）。最终375须等ER定向修正后重跑。
- 完整官方SVG可执行证据：Packet8/8（257元素）、Info8/8，检查实际fontconfig文件hash/index、geometry/paint/text，无栅格；新官方捕获分别在 `final-official-packet` / `final-official-info`。Zen24/24经实际原版renderer与同机Skia透明RGBA逐像素零差、视口/非空文字相同，报告 `zenuml-canvas-native/comparison.json`，最终Probe重建后须刷新散列。不能当34族全量browser通过。
- 比较器最终 **74项自测通过**（6文字、8RGBA、12Packet、21Info、7视口、6visibility、14文本效果）。包含CSS/presentation、祖先display、可覆盖visibility、dx/dy及未建模效果拒绝；Windows Info自然视口改为实际browser读取但本环境未执行。Info-only正式总gate实测8/375失败，browserCompared0、officialSvgComparedfalse（`partial-gate-current.json`）。
- Browser限制：本地Chromium socket EPERM；连接Edge任意data测试页被URL策略拒绝并明确禁止变换载体绕过，已停止且完成CUA summary。**不得重试间接载体、不得把未执行的整图browser gate标绿**。Node真实原版无DOM renderer/算法对照可继续；剩余335例31族完整官方SVG视觉对照未执行。
- graph当前仅负责ER失败的定向排查；data/special已交回并freeze。special最新比较器文本效果修复随本独立工具提交保存。本批提交通用ELK处理阶段及其25/24例；组件/scope接线及其7+14/21例尚待下一批；随后准确docs/validation并push。不存在并发构建。
- 下一步：解决/核验ER单例，单Ninja重建必要目标；定向ER数学后全核心与真实Math；375原生；刷新Zen和Info/Packet实际SVG；分项提交剩余源码及准确验证清单；普通非force push新分支并核对远端。保持明确的未完成浏览器验收边界。

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
