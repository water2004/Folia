# Mermaid C++ 原生渲染模块说明与协作指南

本目录记录 Folia 的 Mermaid C++23 原生重构工程。该项目以官方 [`mermaid-js/mermaid`](https://github.com/mermaid-js/mermaid) **12.2.0**（commit `1fb7869866dd619168d840291c9c54b4f129758b`）为基准，在 `third_party/mermaid-cpp/` 与 `src/platform/text/` 下使用 C++23 modules 原生实现语法解析、布局计算、DirectWrite 文本测量与 SVG 渲染，用于替换现有的 `mermaid-rs-renderer` 依赖。各组件对应的上游源码版本与哈希记录在 [`source-lock.json`](source-lock.json) 中。

当前工程处于阶段性交付状态：公共配置前端、DirectWrite 测量桥接、动态库 C ABI 以及 **20 个图族**的公共渲染入口均已贯通并通过 CI 验证；剩余图族收尾及 WinUI 宿主切换正在推进中（Folia 桌面端当前仍使用原有 Rust 渲染链路）。

---

## 一、已完成模块与图族覆盖

### 1. 公共前端与配置会话（`frontend` / `config_*` / `diagram_registry`）
- **源码预处理**：统一处理换行符与控制字符、提取前置 YAML frontmatter、解析并剥离 `%%{init: ...}%%` 指令、按阶段清理注释。
- **配置合并与主题计算**：通过 `yaml-cpp`（附带 JSON 转义补丁）与 `es-toolkit` 兼容合并语义归并配置树；基于 Khroma 颜色算法派生 `default` 与 `dark` 两套主题变量；通过 `ConfigSession` 提供请求级配置隔离。
- **图族识别与统一入口**：`DiagramRegistry` 按上游探测器优先级识别图族，由 `folia.mermaid.render_entry` 统一提供 `prepare_native_diagram`（解析与配置提取）和 `render_native_diagram`（注入测量结果并生成 SVG）。

### 2. 已接入公共渲染入口的 20 个图族
以下 20 个图族已完整接入 `render_entry` 与平台渲染层 `folia.platform.mermaid_renderer`，并通过与官方 Mermaid 12.2.0 浏览器渲染结果的 SVG 对照测试：

| 类别 | 已贯通图族 | 核心实现说明 |
| --- | --- | --- |
| 流程图与状态图 | **Flowchart**、**State** | Flowchart 支持经典节点几何、递归复合子图（subgraph）、`style`/`classDef` 样式优先级、HTML/Markdown 标签断行及边箭头标记，已同时打通 Dagre 路线与默认 ELK 分层/复合图路线；State 已打通 classic/Dagre 下的基础状态、别名、转换标签、方向与复合状态。 |
| 交互与面向对象 | **Sequence**、**Class** | Sequence 支持参与者、消息箭头、激活框、生命周期创建/销毁、注释及 `loop`/`alt`/`opt`/`par` 等嵌套控制块；Class 支持类成员、可见性/静态/抽象修饰符、关系箭头、命名空间、注释、棒棒糖接口（lollipop）及自环（当前走显式 Dagre 路线，默认 ELK 路线收尾中）。 |
| 架构与层级图 | **Mindmap**、**Architecture**、**C4**、**Block**、**Kanban** | Mindmap 采用 `cose-bilkent` 力导向树布局；Architecture 采用 `fCoSE` 约束与双阶段布局，输出服务图标、分组框与有向边；C4 覆盖 Context/Container 等边界框与关系网格；Block 支持列跨度与嵌套网格；Kanban 支持分列卡片与元数据标签。 |
| 数据与统计图表 | **Packet**、**Pie**、**Quadrant**、**Radar**、**XY**、**Sankey**、**Treemap** | Packet 位域网格；Pie 扇区几何（`d3-shape`/`d3-path`）与图例排布；Quadrant 四象限坐标；Radar 极坐标与闭合 Catmull-Rom 样条；XY 柱状/折线图与线性/分带坐标轴（`d3-scale`）；Sankey 流量节点松弛与渐变连线（`d3-sankey`）；Treemap 层级面积切分（`d3-hierarchy` squarify）。 |
| 时间与序列图表 | **Gantt**、**Git graph**、**Journey**、**Timeline** | Gantt 日期解析、时长依赖排程（`dayjs`）与时间轴刻度（`d3-time`）；Git graph 分支车道、提交/合并/cherry-pick 符号与标签布局；Journey 任务评分与角色轨迹；Timeline 横向/纵向事件时间轴。 |

### 3. ZenUML 独立模块（待接入公共入口）
- `third_party/mermaid-cpp/src/zenuml.{ixx,cpp}` 与 `zenuml_svg.{ixx,cpp}` 已完成语法解析、横向约束求解、递归纵向 Block 布局与 SVG 输出，5 个基准案例在 Windows 四配置下均已通过原生测试与浏览器 SVG 对照，仅剩 `render_entry` 公共入口接线与字体别名映射收尾。

### 4. 平台文本测量与动态库接口（`src/platform/text/` / `tools/mermaid-cpp/`）
- **DirectWrite 文本测量（`mermaid_measurer.ixx`）**：在 Windows 平台解析 CSS `font-family` 候选列表，提供单行 advance/ink bounds 测量、按 Unicode 断行机会的文本折行以及 SVG 字体 ascent/descent 度量。
- **C ABI 导出（`native-renderer.h` / `mermaid_host.ixx`）**：`FoliaNativeMermaid.dll` 导出 `FoliaNativeMermaidRender` 与 `FoliaNativeMermaidFree`，接收 UTF-8 源码与亮/暗主题参数，返回由 DLL 分配的 UTF-8 SVG 字符串（或错误文本）及宽高尺寸。

### 5. 当前验证基线
- 代码分支已变基至上游 `master` `38231c6a`（Folia 0.5.4），GitHub Actions 工作流 [`.github/workflows/mermaid-cpp.yml`](../../.github/workflows/mermaid-cpp.yml) 在 Windows `standalone` / `integrated` × `Debug` / `Release` 四种配置下全部通过（上游 PR 检查见 [Actions Run #40820458232](https://github.com/water2004/Folia/actions/runs/40820458232)，Fork 仓库检查见 [Actions Run #40820482671](https://github.com/zhao0349/Folia/actions/runs/40820482671)）。
- 自动化验证覆盖：核心模块测试（`FoliaMermaidTests`）、DirectWrite 平台与 DLL 宿主测试（`FoliaMermaidPlatformTests`、`FoliaMermaidHostTests`）、主工程回归测试（`FoliaTests`、`FoliaWindowsEditorTests`）、同机 DirectWrite 与 Chromium 文本测量对照，以及各图族与官方 Mermaid 浏览器输出的 SVG 对照（实测几何容差 `1.5`，ELK 算子坐标容差 `1e-4`）。

---

## 二、待完成工作包

| 工作包 | 当前状态 | 下一步工作与完成标准 |
| --- | --- | --- |
| **1. ZenUML 公共入口接入** | `zenuml` / `zenuml_svg` 核心与平台测试已通过；入口草案见 `candidates/2026-10-28/zenuml-public.patch` | 在 `frontend.cpp` 注册 `zenuml` 探测器，接入 `render_entry` 与 `mermaid_renderer.ixx`；处理 Windows 下 `Helvetica` 到 `Arial` 的字体映射；通过 5 个基准案例的公共入口渲染与请求隔离测试。 |
| **2. Class 默认 ELK 路线** | 5 个案例（双主题 10 张 SVG）的浏览器采样与 9 列 `class-elk.tsv` 已就绪；候选补丁见 `candidates/2026-10-28/class-elk*.patch` | 应用 `class-elk-shared.patch` 与 `class-elk.patch`，复用 `flowchart_elk` 的共享测量场景接口，完成端点标签（cardinality/role）避让后处理，并在 `class_svg.cpp` 与 `render_entry.cpp` 接入 `layout: elk` 路径，通过 10 张 SVG 对照。 |
| **3. ER 图（`erDiagram`）** | 8 个成功案例、2 个语法错误案例的 Oracle（`er-oracle.mjs`）与双主题 16 张浏览器基准 SVG（`er-browser-probe.mjs`）已就绪 | 新增 `er.{ixx,cpp}` 与 `er_svg.{ixx,cpp}`，实现实体/属性/主键外键/基数关系解析、Dagre 布局适配、关系端点标记（crow's foot）绘制及公共入口接线，通过 `compare-er-svg.mjs` 16 张 SVG 对照。 |
| **4. 其余图族与特性补齐** | Requirement 图等尚未开始；State 注释/并发分隔及部分存量图族次要语法分支待补齐 | 对照 Folia 实际支持的图族范围，优先实现 Requirement 图的原生解析、布局与 SVG 输出，并按需补齐存量图族的常见语法分支。 |
| **5. WinUI 宿主集成与依赖切换** | 独立候选分支已完成早期编译与初步预览验证 | 将最新 `FoliaNativeMermaid` 接入 WinUI 应用预览、编辑刷新、亮暗主题切换与 PDF 导出链路，完成桌面端实机验证后移除 `mermaid-rs-renderer` 依赖。 |

> **候选补丁说明**：[`candidates/2026-10-28/`](candidates/2026-10-28/README.md) 目录保存了尚未接入主编译树的 **Class ELK** 与 **ZenUML 公共入口** 补丁。由于 `tools/mermaid-cpp/CMakeLists.txt` 对 `third_party/mermaid-cpp/src/*.{ixx,cpp}` 和 `tests/mermaid_cpp/*.cpp` 使用了 `GLOB_RECURSE` 自动收集源文件，请在确认共享接口与模块实现配套完整后再将新源文件放入上述目录。

---

## 三、目录结构与构建说明

### 1. 目录结构
- `third_party/mermaid-cpp/src/`：C++23 平台无关核心模块（`.ixx` 模块接口与 `.cpp` 实现）。
- `third_party/mermaid-cpp/THIRD_PARTY_NOTICES.md`：第三方算法与上游源码许可证声明。
- `src/platform/text/`：Windows DirectWrite 文本测量（`mermaid_measurer.ixx`）、平台渲染桥接（`mermaid_renderer.ixx`）与宿主会话封装（`mermaid_host.ixx`）。
- `tests/mermaid_cpp/`：核心解析与布局单元测试（`FoliaMermaidTests`）及 TSV 基准数据（`fixtures/`）。
- `tests/platform/`：DirectWrite 平台测量与各图族 SVG 生成测试（`FoliaMermaidPlatformTests`）。
- `tools/mermaid-cpp/`：独立 CMake 工程、C ABI 动态库封装（`native-renderer.{h,cpp}`）、上游行为采样脚本（`*-oracle.mjs`、`*-browser-probe.mjs`）及 SVG 对照脚本（`compare-*-svg.mjs`）。
- [`docs/mermaid-cpp/HANDOFF.md`](HANDOFF.md)：面向智能体（Agent / Vibe Coding）的分步任务手册与精确执行规范。
- [`docs/mermaid-cpp/elk-layout-boundary.md`](elk-layout-boundary.md)：ELK、CoSE/fCoSE 及 ZenUML 布局算法与上游依赖边界说明。

### 2. 本地构建与测试命令
- **运行主工程测试**：
  - 核心测试：`cmd /c build_test.bat`
  - Windows 编辑器测试：`cmd /c build_platform_test.bat`
- **独立构建并运行 Mermaid 测试（Windows + VS + Ninja）**：
  ```powershell
  cmake -S tools/mermaid-cpp -B build/mermaid-cpp -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build/mermaid-cpp
  ctest --test-dir build/mermaid-cpp --output-on-failure
  ```
- **运行 Node.js 上游对照脚本（需 Node.js 24 与 pnpm）**：
  ```powershell
  pnpm --dir tools/mermaid-cpp/oracle install --frozen-lockfile --ignore-scripts
  node tools/mermaid-cpp/zenuml-oracle.mjs
  node tools/mermaid-cpp/er-oracle.mjs
  ```

---

## 四、多人远程协作分工建议

1. **共享接线文件（建议由集成负责人统一维护）**
   - 涉及文件：`third_party/mermaid-cpp/src/render_entry.{ixx,cpp}`、`third_party/mermaid-cpp/src/frontend.cpp`、`src/platform/text/mermaid_renderer.ixx`、`tools/mermaid-cpp/CMakeLists.txt`、`.github/workflows/mermaid-cpp.yml`、`docs/mermaid-cpp/source-lock.json`。
   - 多人并行开发时，各图族负责人可在各自模块内完成解析、布局与 `render_<family>_svg` 接口，并提交最小入口补丁由集成负责人合入，避免多人同时修改 `render_entry` 产生冲突。
2. **独立功能包认领（可并行推进）**
   - **ZenUML 公共入口收尾**：基于 `candidates/2026-10-28/zenuml-public.patch` 完成探测器注册、字体别名解析与入口测试。
   - **Class ELK 布局收尾**：基于 `candidates/2026-10-28/class-elk-shared.patch` 与 `class-elk.patch` 完成 `class_elk` 模块与 `class_svg` ELK 路径绘制，验证 Class ELK 5 个案例且保持 Flowchart ELK 6 个案例不回归。
   - **ER 图原生实现**：基于 `er-cases.mjs` 与 `fixtures/er.tsv` 独立开发 `er.{ixx,cpp}` 与 `er_svg.{ixx,cpp}`，通过 16 张双主题 SVG 对照。
3. **使用智能体（Vibe Coding）协作**
   - 若使用智能体辅助开发，请将根目录 [`AGENTS.md`](../../AGENTS.md) 与 [`docs/mermaid-cpp/HANDOFF.md`](HANDOFF.md) 作为上下文入口提供给智能体。`HANDOFF.md` 中已按任务拆解了精确的文件清单、接口签名、实现要点与验证命令。
