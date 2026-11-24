# Mermaid C++ 原生渲染模块说明与协作指南

本目录记录 Folia 的 Mermaid C++23 原生渲染模块。该项目以官方 [`mermaid-js/mermaid`](https://github.com/mermaid-js/mermaid) **12.2.0**（commit `1fb7869866dd619168d840291c9c54b4f129758b`）为基准，在 `src/mermaid/` 与 `src/platform/text/` 下实现语法解析、布局计算、DirectWrite 文本测量与 SVG 渲染。各组件对应的上游源码版本与哈希记录在 [`source-lock.json`](source-lock.json) 中。

原生公共入口覆盖锁定版本的 34 个图族，各有原生 SVG 与上游渲染结果的对照。Packet 和 Pie 的对照在 jsdom 中运行，其余图族按各自的测试脚本运行。Railroad 的四种语法视为同一图族；显式 `error` 是内部诊断图。语法、布局和外观覆盖以各图族的基准测试为准。C++ 库提供 `folia::mermaid` CMake 目标，Folia 0.7.0 的桌面端使用原生 Mermaid，生成的 SVG 继续由 usvg 规范化供 Direct2D 与 PDF 使用。

---

## 一、已完成模块与图族覆盖

### 1. 公共前端与配置会话（`frontend` / `config_*` / `diagram_registry`）
- **源码预处理**：统一处理换行符与控制字符、提取前置 YAML frontmatter、解析并剥离 `%%{init: ...}%%` 指令、按阶段清理注释。
- **配置合并与主题计算**：通过 `yaml-cpp`（附带 JSON 转义补丁）与 `es-toolkit` 兼容合并语义归并配置树；基于 Khroma 颜色算法派生 11 种内置主题变量；通过 `ConfigSession` 提供请求级配置隔离。
- **图族识别与统一入口**：`DiagramRegistry` 按上游探测器优先级识别图族，由 `folia.mermaid.render_entry` 统一提供 `prepare_native_diagram`（解析与配置提取）和 `render_native_diagram`（注入测量结果并生成 SVG）。

### 2. 流程、架构、数据与时间图族
以下图族已接入 `render_entry` 与平台渲染层 `folia.platform.mermaid_renderer`，并有与官方 Mermaid 12.2.0 渲染结果的基础 SVG 对照记录。新增复杂语料的验收状态单独记录在当前交接中：

| 类别 | 已贯通图族 | 核心实现说明 |
| --- | --- | --- |
| 流程图与状态图 | **Flowchart**、**State** | Flowchart 支持经典节点几何、递归复合子图（subgraph）、`style`/`classDef` 样式优先级、HTML/Markdown 标签断行及边箭头标记，已同时打通 Dagre 与默认 ELK。State 支持默认 ELK 和显式 Dagre，覆盖复合状态、侧边注释、并发分区和循环。 |
| 交互与面向对象 | **Sequence**、**Class** | Sequence 支持参与者、消息箭头、激活框、生命周期、注释和嵌套控制块。Class 支持显式 Dagre 和默认 ELK，覆盖类成员、关系箭头、命名空间和多重性标签。 |
| 架构与层级图 | **Mindmap**、**Architecture**、**C4**、**Block**、**Kanban** | Mindmap 采用 `cose-bilkent` 力导向树布局；Architecture 采用 `fCoSE` 约束与双阶段布局，输出服务图标、分组框与有向边；C4 覆盖 Context/Container 等边界框与关系网格；Block 支持列跨度与嵌套网格；Kanban 支持分列卡片与元数据标签。 |
| 数据与统计图表 | **Packet**、**Pie**、**Quadrant**、**Radar**、**XY**、**Sankey**、**Treemap** | Packet 位域网格；Pie 扇区几何（`d3-shape`/`d3-path`）与图例排布；Quadrant 四象限坐标；Radar 极坐标与闭合 Catmull-Rom 样条；XY 柱状/折线图与线性/分带坐标轴（`d3-scale`）；Sankey 流量节点松弛与渐变连线（`d3-sankey`）；Treemap 层级面积切分（`d3-hierarchy` squarify）。 |
| 时间与序列图表 | **Gantt**、**Git graph**、**Journey**、**Timeline** | Gantt 日期解析、时长依赖排程（`dayjs`）与时间轴刻度（`d3-time`）；Git graph 分支车道、提交/合并/cherry-pick 符号与标签布局；Journey 任务评分与角色轨迹；Timeline 横向/纵向事件时间轴。 |

### 3. ZenUML 公共入口
- `src/mermaid/diagrams/zenuml/zenuml.{ixx,cpp}` 与 `zenuml_svg.{ixx,cpp}` 负责语法解析、横向约束、纵向布局和 SVG 输出。公共入口、Windows 文本测量和 DLL 调用均已接通，5 个基准案例通过浏览器 SVG 对照。

### 4. Requirement 公共入口
- `requirementDiagram` 的 classic 外观支持默认 ELK 和显式 Dagre。共 44 张浏览器 SVG 对照覆盖亮暗主题、节点和关系、HTML 与 SVG 标签、标题及标记。已验证强调、显式换行和实测的样式换行；任意 HTML/CSS 不在此覆盖范围内。

### 5. ER 公共入口
- `erDiagram` 的 classic 外观支持默认 ELK 和显式 Dagre。8 个案例的 32 张浏览器 SVG 覆盖亮暗主题、别名、属性和键、关系基数、非识别关系与方向。另有 2 个无效语法的解析对照。RoughJS 零粗糙度笔画的随机控制点按等价线段验证。

### 6. 其他图族

- **Info** 输出 Mermaid 12.2.0 全量版本号及主题文字颜色。
- **Tree View** 支持缩进与框线目录树、带引号的名称、描述、高亮和内置文件/文件夹图标。
- **Ishikawa** 支持多级原因、长标签换行与实测视口。经典外观的亮暗主题已通过浏览器对照。
- **Cynefin** 支持领域、分界、标签、徽章和过渡，浏览器对照使用固定随机种子。
- **Venn** 支持多集合布局、交叠、标题和标签，以及有种子的手绘轮廓和填充。已对照 28 张 SVG，含四集合交叠和五集合分离。DLL 请求显式选择亮暗主题。
- **Railroad** 的 IR、EBNF、ABNF 和 PEG 语法共用原生语法树及测量布局，16 张 SVG 对照通过。
- **Agentflow** 复用 Flowchart 几何和 ELK，另提供自己的语法、嵌套流程、节点类型与主题色。
- **Swimlane** 按泳道组织节点和标题，布局与正交路由分离，路由后处理沿用上游的阶段顺序。
- **Event Modeling** 支持命名空间、帧、关系、注释和结构化数据块，使用各元素实际字体度量。
- **Wardley** 支持演化阶段、锚点、组件和管线、关系、策略标记、加速器和标注。
- **Use Case** 支持用例、参与者、系统边界、分组、注释和关系，支持默认 ELK 与显式 Dagre。
- **显式错误图** 使用上游诊断 SVG 图形。普通语法失败通过原生入口的错误结果报告。

### 7. 平台文本测量与动态库接口（`src/platform/text/` / `tools/mermaid-cpp/`）
- **可移植测量编排（`measured_render.{ixx,cpp}`）**：公共 source-to-SVG 管线通过 `TextMeasurer` 接收宿主实测字体数据。Windows 应用与跨平台完整图回归使用同一套编排、布局和 SVG 输出；核心模块不依赖 Windows。
- **DirectWrite 文本测量（`mermaid_measurer.ixx`）**：在 Windows 平台解析 CSS `font-family` 候选列表，提供单行 advance/ink bounds 测量、按 Unicode 断行机会的文本折行以及 SVG 字体 ascent/descent 度量。
- **C ABI 导出（`native-renderer.h` / `mermaid_host.ixx`）**：`FoliaNativeMermaid.dll` 导出 `FoliaNativeMermaidRender` 与 `FoliaNativeMermaidFree`，接收 UTF-8 源码与亮/暗主题参数，返回由 DLL 分配的 UTF-8 SVG 字符串（或错误文本）及宽高尺寸。

### 8. 数学标签

`$$...$$` 通过现有 C++ MathJax 生成纯 SVG，普通文字和公式的真实尺寸共同参与布局。
实现不引入浏览器或 JS 运行时，不复制数学字体数据，也不改变现有公式缓存。
图族范围、配置、源码示例、版本区别及严格 SVG/位图对照见 [数学标签](math.md)。

### 9. 主题与宿主 API

`import folia.mermaid;` 提供一步 source-to-SVG 渲染、11 种主题清单、
类型化 `ThemeOptions`、独立主题会话和变量自定义。源码主题覆盖沿用原有优先级。
使用方式与 C ABI 见 [主题与公共 API](themes.md)。

### 10. 当前验证基线
- 迁移前的 PR 基线 `c128acc9` 中，GitHub Actions 工作流 [`.github/workflows/mermaid-cpp.yml`](../../.github/workflows/mermaid-cpp.yml) 在 Windows `standalone` / `integrated` × `Debug` / `Release` 四种配置下全部通过（上游 PR 检查见 [Actions Run #40820458232](https://github.com/water2004/Folia/actions/runs/40820458232)，Fork 仓库检查见 [Actions Run #40820482671](https://github.com/zhao0349/Folia/actions/runs/40820482671)）。
- 自动化验证覆盖：核心模块测试（`FoliaMermaidTests`）、DirectWrite 平台与 DLL 宿主测试（`FoliaMermaidPlatformTests`、`FoliaMermaidHostTests`）、主工程回归测试（`FoliaTests`、`FoliaWindowsEditorTests`）、同机 DirectWrite 与 Chromium 文本测量对照，以及各图族与官方 Mermaid 浏览器输出的 SVG 对照（实测几何容差 `1.5`，ELK 算子坐标容差 `1e-4`）。

### 11. 全图族复杂回归

`tests/mermaid_regression/cases/` 为 34 个图族保留从基础到复杂的独立输入，
每族至少 8 例，包含官方完整示例与复杂组合。`FoliaMermaidRegressionProbe`
经生产测量管线输出 SVG；官方捕获器与严格比较器逐例检查输入及输出散列、
字体、几何、文字、绘制属性和实际栅格。缺项或未执行的浏览器验收不会被标为通过。
运行命令、固定基准、验证分层与真实 Canvas 对照见 [全图族回归](regressions.md)。

---

## 二、待完成工作包

| 工作包 | 当前状态 | 下一步工作与完成标准 |
| --- | --- | --- |
| **4. 图族兼容性** | 34 个图族通过基础 SVG 对照；各自语法、外观和主题覆盖程度不同。 | 继续补充复杂标签、外观组合、曲线和需要额外注册资源的图形。浏览器回调依赖另行设计宿主边界。 |
| **5. WinUI 宿主集成与依赖切换** | 已接入 Folia 0.7.0。Release 预览、编辑刷新、亮暗主题、PDF 导出和 WiX 文件清单均有验收记录。 | 参见 [宿主集成](winui-integration.md) 和 [交接](HANDOFF.md)。 |

---

## 三、目录结构与构建说明

### 1. 目录结构
- `src/mermaid/`：C++23 平台无关核心。公共编排在 `api/`，前处理在 `frontend/`，配置在 `config/`，主题在 `themes/`，各图族在 `diagrams/<family>/`，共享布局在 `layout/<engine>/`；文本、绘制和基础工具分别在 `text/`、`rendering/`、`support/`。完整归属和实际依赖见 [模块组织](architecture.md)。
- `src/mermaid/THIRD_PARTY_NOTICES.md`：第三方算法与上游源码许可证声明。
- `src/platform/text/`：Windows DirectWrite 文本测量（`mermaid_measurer.ixx`）、平台渲染桥接（`mermaid_renderer.ixx`）与宿主会话封装（`mermaid_host.ixx`）。
- `tests/mermaid_cpp/`：核心解析与布局单元测试（`FoliaMermaidTests`）及 TSV 基准数据（`fixtures/`）。
- `tests/mermaid_math/`：通过现有 Math DLL 执行真实公式的可移植集成测试（`FoliaMermaidMathTests`）。
- `tests/platform/`：DirectWrite 平台测量与各图族 SVG 生成测试（`FoliaMermaidPlatformTests`）。
- `tools/mermaid-cpp/`：独立 CMake 工程、C ABI 动态库封装（`native-renderer.{h,cpp}`）、上游行为采样脚本（`*-oracle.mjs`、`*-browser-probe.mjs`）及 Python 图族 SVG 比较入口 `compare-family.py`。JavaScript 负责官方样本和浏览器观测采集，Python 负责原生进程编排与验收。
- [`docs/mermaid-cpp/HANDOFF.md`](HANDOFF.md)：当前检查点、模块边界与复验入口。
- [`docs/mermaid-cpp/elk-layout-boundary.md`](elk-layout-boundary.md)：ELK、CoSE/fCoSE 及 ZenUML 布局算法与上游依赖边界说明。

### 2. 本地构建与测试命令
- **运行主工程测试**：
  - 核心测试：`cmd /c tests\run_core.bat`
  - Windows 编辑器测试：`cmd /c tests\platform\run.bat`
- 从仓库根目录配置时，使用 `-DFOLIA_BUILD_MERMAID=ON` 生成独立 Mermaid 库和测试目标。该选项默认关闭。
- **集成构建并运行 Mermaid 测试（Windows + VS + Ninja）**：
  完成根目录 `setup.ps1` 的既有依赖恢复，在支持 `import std` 的 VS 开发者终端中运行：
  ```powershell
  cmake -S . -B build/core -G Ninja -DCMAKE_BUILD_TYPE=Release -DFOLIA_BUILD_MERMAID=ON
  cmake --build build/core --target FoliaMermaidTests FoliaMermaidMathTests FoliaMermaidPlatformTests FoliaMermaidHostTests FoliaMathSvgBridgeTests --parallel 4
  ctest --test-dir build/core -R "^FoliaMermaid(Tests|MathTests|PlatformTests|HostTests)$" --output-on-failure
  ./build/core/FoliaMathSvgBridgeTests.exe
  ```
- **使用已有 Math DLL 独立构建**：
  ```powershell
  cmake -S tools/mermaid-cpp -B build/mermaid-cpp -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DFOLIA_NATIVE_MATH_RUNTIME="$PWD/build/core/FoliaNativeMath.dll" `
    -DFOLIA_NATIVE_MATH_IMPORT_LIBRARY="$PWD/build/core/FoliaNativeMath.lib"
  cmake --build build/mermaid-cpp --parallel 4
  ctest --test-dir build/mermaid-cpp --output-on-failure
  ```
  Math DLL 需与当前源码配套，包含新 fragment C ABI。构建会把它放在 Mermaid DLL
  和对应测试程序旁边。Linux 的纯核心构建不依赖 Windows；若提供
  `FOLIA_NATIVE_MATH_RUNTIME=/absolute/path/libFoliaNativeMath.so`，也会构建真实数学
  集成测试。独立工具链需要 CMake 4.3、Ninja、Python 3（结构检查）和支持 C++23 `import std` 的标准库。
- **Linux 独立构建（已验证 LLVM/libc++ 21.1.8）**：
  安装匹配版本的 Clang、`clang-scan-deps`、libc++（含标准库模块与
  experimental 库）、CMake 4.3 和 Ninja 后执行：
  ```sh
  cmake -S tools/mermaid-cpp -B build/mermaid-cpp -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++-21 \
    -DCMAKE_CXX_FLAGS="-stdlib=libc++ -fexperimental-library"
  cmake --build build/mermaid-cpp \
    --target FoliaMermaidTests FoliaMermaidRegressionProbe --parallel 4
  ctest --test-dir build/mermaid-cpp --output-on-failure
  ```
  libc++ 21 的时区支持需要 `-fexperimental-library`。此构建不需要
  Windows SDK；没有提供 Math 共享库时，不登记真实 Math 集成测试。
  `FoliaMermaidStructure` 与核心测试均可在 Linux 运行。
- **生成官方上游样本（需 Node.js 24 与 pnpm）**：
  ```powershell
  pnpm --dir tools/mermaid-cpp/oracle install --frozen-lockfile --ignore-scripts
  node tools/mermaid-cpp/zenuml-oracle.mjs
  node tools/mermaid-cpp/er-oracle.mjs
  ```
- **比较已保存的图族 SVG**：安装 [比较器依赖](../../tools/mermaid-cpp/regression/comparison/README.md) 后，从仓库根目录运行 `python tools/mermaid-cpp/compare-family.py FAMILY original.json native-svg-directory`。`FAMILY` 选择图族，后两个参数分别指向同图族的官方捕获和原生 SVG 目录；使用 `--browser` 或 `FOLIA_BROWSER` 选择实际浏览器。比较保留原有字体、几何、文字和绘制规则。

---

## 四、协作边界

各图族独立维护解析、布局、SVG 输出及基准测试。集成负责人维护 `render_entry`、Windows 测量桥和 CMake/CI。需要交接时，更新 [当前检查点](HANDOFF.md)。
