# Mermaid C++ 交接

## 当前检查点

- 2026-11-16：正在 `feat/mermaid-comprehensive-regressions` 上执行全图族复杂回归，基于用户正在核验的数学支持分支 `a368f9ea70dac36f235d9eca6bdb0be16ea82a5f`。正在按独立任务分项提交，尚未推送，不能称完成或全部 SVG 通过。保持 CI、公式缓存与 10MB 压测文件不动。
- 工作树 `/workspace/scratch/a4656ef82bb3/Folia`。本任务是新环境；已恢复固定 `.reference/mermaid-js`、oracle 依赖、ut 子模块。已有源码锁仅通过 LF/CRLF 恢复精确原字节，不改旧 hash。新增引用来源由 root 统一追加到 source-lock。
- 语料 `tests/mermaid_regression/cases/*.json` 当前 34 图族、369 例以上（Block 还在补充），每族至少8例，包含完整官方原例、复杂组合与全部53种Flowchart形状的重复覆盖。`regression/corpus.mjs` 强制图族清单、复杂例、独立输入、来源/features，不允许 skip/expectedFailure。369例快照官方 syntax/config 全通过：`build/mermaid-regression/official-prepare-current/report.json`；最终改动后仍需重跑。
- Windows 实测编排已移到平台无关 `measured_render.{ixx,cpp}` 和 `TextMeasurer` 契约；Windows仍供DirectWrite与原Math服务。C4工作区宽度由宿主显式传入。`FoliaMermaidRegressionProbe` / `run-native.py` 在Linux用真实Pango，经同一生产管线输出SVG，不冒充官方浏览器验收。新增 probe `--remote-measurement` 供同机Skia精确比较，尚待最终编译。
- 工具链 LLVM21.1.8：`/workspace/scratch/a4656ef82bb3/toolchain/llvm21/usr/lib/llvm-21/bin/clang++`；CMake4.3.4 `.../toolchain/python/cmake/data/bin/cmake`；Ninja `.../toolchain/python/bin/ninja`。设置 `LD_LIBRARY_PATH=.../toolchain/llvm21/usr/lib/x86_64-linux-gnu:.../toolchain/llvm21/usr/lib/llvm-21/lib`。构建目录 `build/mermaid-regression-native`，并发≤4，任何时候只允许运行一个Ninja。
- 旧Ninja依赖日志损坏已隔离保存至 `build/mermaid-regression/dependency-recovery/`，recompact、串行class_svg重建与完整构建成功，非生产修复。special的 session2841 已正常退出。当前 root 独占编译锁：Math原运行时重建 session23081、日志 `build/mermaid-regression/math-pic-build.log`（首次链接因本机配置未启用PIC失败，已用CMAKE_POSITION_INDEPENDENT_CODE=ON重配），目录 `build/mermaid-regression-math`，无Math源码/缓存修改。完成后将已有Mermaid构建接入该 .so，统一编译最新符号/Block/Zen/ELK和数学集成测试。
- 首轮修复后核心243项、944922断言全过，无skip/异常退出：`build/mermaid-regression/core-tests-first.log`。此前全流程336/342中的6个失败（Agentflow、Architecture、Block、折叠Flowchart、空Gantt、Mindmap）已用新binary定向6/6通过：`previous-failures/report.json`。数据/时间族105例真实Pango全流程通过：`data-native/report.json`。Gantt5项735断言、Treemap3项5853断言（含1794官方D3格式fixture）通过。
- 严格Packet官方完整SVG 8/8、257个rect/text几何/文字/字体/paint对照通过，1.5px固定；实际fontconfig字体文件/hash/index一致，`rasterCompared:false`。报告 `packet-official/comparison.json`；统一捕获 `packet-capture/report.json` 8/8。Zen23例原版Skia生成SVG；透明RGBA逐像素目前21/23相同，剩余2个Markdown注释修复已写、待重建。不得提前称23例通过。
- 新增未编译改动：graph补齐9种Flowchart符号、Block边头和线型；special补Zen Markdown注释、C4 offset的JS整数语义；root补独立方向子图的ELK scope拆分 `flowchart_elk_scopes.inc`，复用既有compound算法。data正在用锁定官方ELK adapter函数生成16个固定测量盒的坐标对照；此为算法测试，容差1e-4，不能混同SVG字体验收。
- agents：graph拥有11结构族与其cases/tests；special拥有12特殊族；data拥有11数据/时间族、严格compare-svg与新的ELK方向oracle/test（不改生产ELK）；root拥有共享入口/测量/CMake/docs/source-lock及ELK方向实现。agents都不运行Ninja，等root协调。
- 完整SVG工具 `tools/mermaid-cpp/regression/` 的capture/compare按输入/输出hash和完整清单严格验收；缺项/过期/browser缺失均失败。比较器22项反例自测通过；仅Packet证据的全清单gate实际返回失败（8/369），没有假通过。新增源码锁旧hash未变，Block原始parser与新entry行尾已对齐旧rawFile锁。
- 环境限制：普通Chromium启动 `socket() EPERM`；已检查Browser技能与连接的Edge，任意data测试页被URL安全策略拒绝，明确禁止变换载体绕过。已停止该浏览器方案并发completion summary。不得绕过，不得将未运行的整图浏览器gate标绿；原版无DOM renderer/算法仍可执行，最终要明确未验收范围。
- 下一步：等Math session23081结束；统一新代码构建，先定向ELK坐标/符号/Block/Zen与真实数学集成，再完整native语料和核心；按实际快照更新记录，分批提交并推送新分支。官方整图浏览器未执行范围必须在交付中保留，不改CI。

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
