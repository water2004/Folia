# Mermaid C++ 原生渲染模块说明与协作指南

本目录记录 Folia 的 Mermaid C++23 原生重构工程。该项目以官方 [`mermaid-js/mermaid`](https://github.com/mermaid-js/mermaid) **12.2.0**（commit `1fb7869866dd619168d840291c9c54b4f129758b`）为基准，在 `src/mermaid/` 与 `src/platform/text/` 下使用 C++23 modules 原生实现语法解析、布局计算、DirectWrite 文本测量与 SVG 渲染，用于替换现有的 `mermaid-rs-renderer` 依赖。各组件对应的上游源码版本与哈希记录在 [`source-lock.json`](source-lock.json) 中。

当前原生公共入口支持 28 个图族，语法、布局和外观覆盖以各图族的基准测试为准。C++ 库提供 `folia::mermaid` CMake 目标，桌面端仍使用原有 Rust 渲染链路。

---

## 一、已完成模块与图族覆盖

### 1. 公共前端与配置会话（`frontend` / `config_*` / `diagram_registry`）
- **源码预处理**：统一处理换行符与控制字符、提取前置 YAML frontmatter、解析并剥离 `%%{init: ...}%%` 指令、按阶段清理注释。
- **配置合并与主题计算**：通过 `yaml-cpp`（附带 JSON 转义补丁）与 `es-toolkit` 兼容合并语义归并配置树；基于 Khroma 颜色算法派生 `default` 与 `dark` 两套主题变量；通过 `ConfigSession` 提供请求级配置隔离。
- **图族识别与统一入口**：`DiagramRegistry` 按上游探测器优先级识别图族，由 `folia.mermaid.render_entry` 统一提供 `prepare_native_diagram`（解析与配置提取）和 `render_native_diagram`（注入测量结果并生成 SVG）。

### 2. 已接入公共渲染入口的 20 个图族
以下 20 个图族已接入 `render_entry` 与平台渲染层 `folia.platform.mermaid_renderer`，并通过与官方 Mermaid 12.2.0 浏览器渲染结果的 SVG 对照测试：

| 类别 | 已贯通图族 | 核心实现说明 |
| --- | --- | --- |
| 流程图与状态图 | **Flowchart**、**State** | Flowchart 支持经典节点几何、递归复合子图（subgraph）、`style`/`classDef` 样式优先级、HTML/Markdown 标签断行及边箭头标记，已同时打通 Dagre 与默认 ELK。State 的 classic/Dagre 路径支持复合状态、侧边注释和并发分区，各分区拥有独立的入口与出口。 |
| 交互与面向对象 | **Sequence**、**Class** | Sequence 支持参与者、消息箭头、激活框、生命周期、注释和嵌套控制块。Class 支持显式 Dagre 和默认 ELK，覆盖类成员、关系箭头、命名空间和多重性标签。 |
| 架构与层级图 | **Mindmap**、**Architecture**、**C4**、**Block**、**Kanban** | Mindmap 采用 `cose-bilkent` 力导向树布局；Architecture 采用 `fCoSE` 约束与双阶段布局，输出服务图标、分组框与有向边；C4 覆盖 Context/Container 等边界框与关系网格；Block 支持列跨度与嵌套网格；Kanban 支持分列卡片与元数据标签。 |
| 数据与统计图表 | **Packet**、**Pie**、**Quadrant**、**Radar**、**XY**、**Sankey**、**Treemap** | Packet 位域网格；Pie 扇区几何（`d3-shape`/`d3-path`）与图例排布；Quadrant 四象限坐标；Radar 极坐标与闭合 Catmull-Rom 样条；XY 柱状/折线图与线性/分带坐标轴（`d3-scale`）；Sankey 流量节点松弛与渐变连线（`d3-sankey`）；Treemap 层级面积切分（`d3-hierarchy` squarify）。 |
| 时间与序列图表 | **Gantt**、**Git graph**、**Journey**、**Timeline** | Gantt 日期解析、时长依赖排程（`dayjs`）与时间轴刻度（`d3-time`）；Git graph 分支车道、提交/合并/cherry-pick 符号与标签布局；Journey 任务评分与角色轨迹；Timeline 横向/纵向事件时间轴。 |

### 3. ZenUML 公共入口
- `src/mermaid/zenuml.{ixx,cpp}` 与 `zenuml_svg.{ixx,cpp}` 负责语法解析、横向约束、纵向布局和 SVG 输出。公共入口、Windows 文本测量和 DLL 调用均已接通，5 个基准案例通过浏览器 SVG 对照。

### 4. Requirement 公共入口
- `requirementDiagram` 的 classic 外观支持默认 ELK 和显式 Dagre。共 32 张浏览器 SVG 对照覆盖亮暗主题、节点和关系、HTML 与 SVG 标签、标题及标记。标签目前支持纯文本，富文本换行和任意 CSS 字体样式仍待补齐。

### 5. ER 公共入口
- `erDiagram` 的 classic 外观支持默认 ELK 和显式 Dagre。8 个案例的 32 张浏览器 SVG 覆盖亮暗主题、别名、属性和键、关系基数、非识别关系与方向。另有 2 个无效语法的解析对照。RoughJS 零粗糙度笔画的随机控制点按等价线段验证。

### 6. 其他图族

- **Info** 输出 Mermaid 12.2.0 全量版本号及主题文字颜色。
- **Tree View** 支持缩进与框线目录树、带引号的名称、描述、高亮和内置文件/文件夹图标。
- **Ishikawa** 支持多级原因、长标签换行与实测视口。经典外观的亮暗主题已通过浏览器对照。
- **Cynefin** 支持领域、分界、标签、徽章和过渡，浏览器对照使用固定随机种子。
- **Venn** 支持最多三个非空集合、交叠、标题和标签。classic 外观及 `default` / `dark` 通过对照。默认裸会话依上游选用 `redux-color`，原生公共主题层目前尚未移植该主题；DLL 请求会显式选择亮暗主题。

### 7. 平台文本测量与动态库接口（`src/platform/text/` / `tools/mermaid-cpp/`）
- **DirectWrite 文本测量（`mermaid_measurer.ixx`）**：在 Windows 平台解析 CSS `font-family` 候选列表，提供单行 advance/ink bounds 测量、按 Unicode 断行机会的文本折行以及 SVG 字体 ascent/descent 度量。
- **C ABI 导出（`native-renderer.h` / `mermaid_host.ixx`）**：`FoliaNativeMermaid.dll` 导出 `FoliaNativeMermaidRender` 与 `FoliaNativeMermaidFree`，接收 UTF-8 源码与亮/暗主题参数，返回由 DLL 分配的 UTF-8 SVG 字符串（或错误文本）及宽高尺寸。

### 8. 当前验证基线
- 迁移前的 PR 基线 `c128acc9` 中，GitHub Actions 工作流 [`.github/workflows/mermaid-cpp.yml`](../../.github/workflows/mermaid-cpp.yml) 在 Windows `standalone` / `integrated` × `Debug` / `Release` 四种配置下全部通过（上游 PR 检查见 [Actions Run #40820458232](https://github.com/water2004/Folia/actions/runs/40820458232)，Fork 仓库检查见 [Actions Run #40820482671](https://github.com/zhao0349/Folia/actions/runs/40820482671)）。
- 自动化验证覆盖：核心模块测试（`FoliaMermaidTests`）、DirectWrite 平台与 DLL 宿主测试（`FoliaMermaidPlatformTests`、`FoliaMermaidHostTests`）、主工程回归测试（`FoliaTests`、`FoliaWindowsEditorTests`）、同机 DirectWrite 与 Chromium 文本测量对照，以及各图族与官方 Mermaid 浏览器输出的 SVG 对照（实测几何容差 `1.5`，ELK 算子坐标容差 `1e-4`）。

---

## 二、待完成工作包

| 工作包 | 当前状态 | 下一步工作与完成标准 |
| --- | --- | --- |
| **4. 图族兼容性** | State 注释和并发分区已通过 24 张 SVG 对照。 | 继续验证未覆盖的语法、外观和主题。 |
| **5. WinUI 宿主集成与依赖切换** | 本轮保留现有 Rust 路径，等待单独安排 | 将最新 `FoliaNativeMermaid` 接入 WinUI 应用预览、编辑刷新、亮暗主题切换与 PDF 导出链路，完成桌面端实机验证后移除 `mermaid-rs-renderer` 依赖。 |

---

## 三、目录结构与构建说明

### 1. 目录结构
- `src/mermaid/`：C++23 平台无关核心模块（`.ixx` 模块接口与 `.cpp` 实现）。
- `src/mermaid/THIRD_PARTY_NOTICES.md`：第三方算法与上游源码许可证声明。
- `src/platform/text/`：Windows DirectWrite 文本测量（`mermaid_measurer.ixx`）、平台渲染桥接（`mermaid_renderer.ixx`）与宿主会话封装（`mermaid_host.ixx`）。
- `tests/mermaid_cpp/`：核心解析与布局单元测试（`FoliaMermaidTests`）及 TSV 基准数据（`fixtures/`）。
- `tests/platform/`：DirectWrite 平台测量与各图族 SVG 生成测试（`FoliaMermaidPlatformTests`）。
- `tools/mermaid-cpp/`：独立 CMake 工程、C ABI 动态库封装（`native-renderer.{h,cpp}`）、上游行为采样脚本（`*-oracle.mjs`、`*-browser-probe.mjs`）及 SVG 对照脚本（`compare-*-svg.mjs`）。
- [`docs/mermaid-cpp/HANDOFF.md`](HANDOFF.md)：当前检查点、模块边界与复验入口。
- [`docs/mermaid-cpp/elk-layout-boundary.md`](elk-layout-boundary.md)：ELK、CoSE/fCoSE 及 ZenUML 布局算法与上游依赖边界说明。

### 2. 本地构建与测试命令
- **运行主工程测试**：
  - 核心测试：`cmd /c tests\run_core.bat`
  - Windows 编辑器测试：`cmd /c tests\platform\run.bat`
- 从仓库根目录配置时，使用 `-DFOLIA_BUILD_MERMAID=ON` 生成独立 Mermaid 库和测试目标。该选项默认关闭。
- **独立构建并运行 Mermaid 测试（Windows + VS + Ninja）**：
  ```powershell
  cmake -S tools/mermaid-cpp -B build/mermaid-cpp -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build/mermaid-cpp --parallel 4
  ctest --test-dir build/mermaid-cpp --output-on-failure
  ```
- **运行 Node.js 上游对照脚本（需 Node.js 24 与 pnpm）**：
  ```powershell
  pnpm --dir tools/mermaid-cpp/oracle install --frozen-lockfile --ignore-scripts
  node tools/mermaid-cpp/zenuml-oracle.mjs
  node tools/mermaid-cpp/er-oracle.mjs
  ```

---

## 四、协作边界

各图族独立维护解析、布局、SVG 输出及基准测试。集成负责人维护 `render_entry`、Windows 测量桥和 CMake/CI。需要交接时，更新 [当前检查点](HANDOFF.md)。
