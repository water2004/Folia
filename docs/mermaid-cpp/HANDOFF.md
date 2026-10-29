# Mermaid C++ 智能体交接与任务执行手册（HANDOFF）

> **致智能体（Agent）**：本文件是 Mermaid C++ 迁移工程的唯一权威任务指令单。请在开始修改任何代码前完整阅读本文件及 [`docs/mermaid-cpp/README.md`](README.md)，严格按照你所分配的 **Task ID（Task A ~ Task E）** 执行。

---

## 一、当前基线状态（Current Baseline）

- **开发分支**：`feat/mermaid-cpp-port`（对应上游 PR `water2004/Folia#1`，基于上游 `master` `38231c6a` Folia 0.5.4）。
- **最近验证通过提交**：`c128acc9e4e89856e34ac0ae620df6271e8d8793`（Windows `standalone`/`integrated` × `Debug`/`Release` 四配置 CI 全绿，运行记录：`40820458232` / `40820482671`）。
- **公共渲染入口已打通的 20 个图族**：
  `Packet`、`Pie`、`Flowchart`（Dagre + 默认 ELK）、`Sequence`、`Class`（显式 Dagre）、`Quadrant`、`Radar`、`XY`、`Sankey`、`Treemap`、`Journey`、`Timeline`、`Gantt`、`Git graph`、`Mindmap`、`Block`、`Kanban`、`C4`、`Architecture`、`State`（classic/Dagre）。
- **已完成但尚未接入公共入口的模块**：
  `ZenUML`（`third_party/mermaid-cpp/src/zenuml.{ixx,cpp}` 与 `zenuml_svg.{ixx,cpp}` 已通过独立单元测试与平台 SVG 对照）。
- **当前运行句柄**：无正在运行的后台构建或子智能体任务。

---

## 二、核心调用链路速查（Architecture Map）

当需要把一个新图族或新布局接入公共渲染入口时，完整的数据流经以下 5 个层次：

1. **前端识别与会话配置**
   - 文件：`third_party/mermaid-cpp/src/frontend.{ixx,cpp}`、`diagram_registry.{ixx,cpp}`
   - 函数：`prepare_diagram(source, session, limits)`
   - 作用：清理文本、提取 YAML frontmatter 与 `%%{init}%%` 指令，调用 `DiagramRegistry::detect` 识别图族 ID（如 `u"flowchart-v2"`、`u"classDiagram"`、`u"er"`、`u"zenuml"`），并生成合并后的 `effective_config` 与 `themeVariables`。
2. **核心渲染准备与分发入口（平台无关）**
   - 文件：`third_party/mermaid-cpp/src/render_entry.{ixx,cpp}`
   - 结构体：`NativeDiagram`（内部 `std::variant<...>` 持有各图族的 `*RenderInput`）
   - 函数 1：`prepare_native_diagram(source, session, limits)` —— 根据 `prepared->type` 调用对应图族的解析器，提取主题与配置，返回 `NativeDiagram`。
   - 函数 2：`render_native_diagram(input, ...各图族测量参数...)` —— 使用 `std::visit` 分发到对应图族的 `render_<family>_svg`，返回 `DiagramSvg{svg, width, height}`。
3. **Windows DirectWrite 文本测量层**
   - 文件：`src/platform/text/mermaid_measurer.ixx`
   - 类：`folia::platform::MermaidMeasurer`
   - 关键方法：
     - `resolve_family(std::u16string_view css_font_list)`：按逗号解析 CSS 字体列表并返回系统中第一个已安装的字体名（注意：`measure()` 只接受单个已解析的字体名，**不能**直接传入含逗号的 CSS 字体列表）。
     - `measure(text, font, preserve_spaces, styles, svg_bounds)`：返回 `MermaidTextMetrics{advance, ink_width, bounds_width, line_height, baseline}`。
     - `wrap(text, font, max_width, styles, break_spaces)`：按 DirectWrite 簇度量（cluster metrics）执行折行。
4. **平台测量与渲染桥接层**
   - 文件：`src/platform/text/mermaid_renderer.ixx`
   - 函数：`render_mermaid_svg(source, session, measurer, limits)`
   - 作用：先调用 `prepare_native_diagram`，再针对 `input->content` 中的具体图族类型调用 `measurer` 构造测量结构体或测量回调，最后调用 `render_native_diagram`。
5. **DLL 宿主与 C ABI 导出**
   - 文件：`src/platform/text/mermaid_host.ixx`、`tools/mermaid-cpp/native-renderer.{h,cpp}`
   - 函数：`FoliaNativeMermaidRender` / `FoliaNativeMermaidFree`（每次请求独立初始化 `default` 或 `dark` 的 `ConfigSession`）。

> ⚠️ **CMake 自动源文件收集警告**：
> `tools/mermaid-cpp/CMakeLists.txt` 对 `third_party/mermaid-cpp/src/*.{ixx,cpp}` 和 `tests/mermaid_cpp/*.cpp` 使用了 `file(GLOB_RECURSE ...)`，而 `tests/platform/*.cpp` 则是在 `tools/mermaid-cpp/CMakeLists.txt` 的 `FoliaMermaidPlatformTests` 目标中**显式逐个列出**的。
> - 往 `third_party/mermaid-cpp/src/` 或 `tests/mermaid_cpp/` 添加文件会**立即参与编译**，切勿放入半成品文件。
> - 往 `tests/platform/` 添加新平台测试文件时，**必须同步将其路径加入 `tools/mermaid-cpp/CMakeLists.txt` 的 `FoliaMermaidPlatformTests` 源文件列表中**。

---

## 三、待执行任务清单（Step-by-Step Playbooks）

### Task A：接入 ZenUML 公共渲染入口（优先级：高，难度：低）

- **目标**：将已实现的 `folia.mermaid.zenuml_svg` 接入 `frontend`、`render_entry` 与 `mermaid_renderer.ixx`，使 `zenuml` 成为可通过公共入口和 DLL 渲染的第 21 个图族。
- **已有基础与参考补丁**：
  - 核心与 SVG 实现已存在：`third_party/mermaid-cpp/src/zenuml.{ixx,cpp}`、`zenuml_svg.{ixx,cpp}`
  - 独立测试已存在：`tests/mermaid_cpp/zenuml.cpp`、`tests/platform/mermaid_zenuml.cpp`
  - 候选补丁草案：`docs/mermaid-cpp/candidates/2026-10-28/zenuml-public.patch`
  - 候选源码锁补充：`docs/mermaid-cpp/candidates/2026-10-28/zenuml-native-pins.json`
- **允许修改的文件**：
  - `third_party/mermaid-cpp/src/frontend.cpp`
  - `third_party/mermaid-cpp/src/render_entry.ixx`
  - `third_party/mermaid-cpp/src/render_entry.cpp`
  - `src/platform/text/mermaid_measurer.ixx`
  - `src/platform/text/mermaid_renderer.ixx`
  - `tests/mermaid_cpp/render_entry.cpp`
  - `tools/mermaid-cpp/native-renderer-test.cpp`
  - `docs/mermaid-cpp/source-lock.json`
- **精确执行步骤**：
  1. **阅读并核对候选补丁**：先查看 `docs/mermaid-cpp/candidates/2026-10-28/zenuml-public.patch`。该补丁已完成四处接线骨架：
     - 在 `frontend.cpp` 的 `prepare_diagram` 中调用 `registry.add_detector(u"zenuml", ...)` 注册以 `zenuml` 关键字开头的外部图族探测器；
     - 在 `render_entry.ixx` 中 `export import folia.mermaid.zenuml_svg;`，将 `ZenRenderInput` 加入 `NativeDiagram::content` 的 `std::variant`，并在 `render_native_diagram` 末尾增加参数 `const ZenSvgMeasure& zenuml_measure = {}`；
     - 在 `render_entry.cpp` 的 `prepare_native_diagram` 中增加 `if (prepared->type == u"zenuml")` 分支（调用 `parse_zenuml(prepared->code.cleaned)`），并在 `render_native_diagram` 中增加 `ZenRenderInput` 分支（调用 `render_zenuml_svg(content.document, zenuml_measure)`）；
     - 在 `src/platform/text/mermaid_renderer.ixx` 中构造 `ZenSvgMeasure zenuml_measure` 并传给 `render_native_diagram`。
  2. **修复关键坑点（Windows 下的 Helvetica 字体解析）**：
     - ⚠️ **坑点说明**：`ZenTheme` 的默认 `font_family` 是 `u"Helvetica, Verdana, serif"`。`zenuml-public.patch` 中直接把 `zenuml->theme.font_family` 传给了 `measurer.measure(...)`，这在运行期会直接报错 `"Resolved Mermaid font is not installed"`（因为 `measure()` 要求传入单个已安装的字体名，而不是逗号分隔列表；且 Windows 默认未安装 Helvetica，若直接调 `resolve_family` 会跳过 Helvetica 命中 Verdana，而 Chromium 在 Windows 上会将 Helvetica 映射为 `Arial`）。
     - **正确改法**：
       - 在 `src/platform/text/mermaid_measurer.ixx` 的 `resolve_family` 中，将 `lower == u"helvetica"` 映射为 `candidate = u"Arial"`（与 `sans-serif -> Arial` 相同）；
       - 在 `src/platform/text/mermaid_renderer.ixx` 构造 `zenuml_measure` 之前，先调用 `auto family = measurer.resolve_family(zenuml->theme.font_family); if (!family) return std::unexpected(family.error());`，然后在 lambda 内使用 `*family` 调用 `measurer.measure(text, {*family, static_cast<float>(size), bold ? 600u : 400u})`。
  3. **合并源码锁记录**：将 `docs/mermaid-cpp/candidates/2026-10-28/zenuml-native-pins.json` 中的条目合入 `docs/mermaid-cpp/source-lock.json`。
  4. **补充公共入口测试**：在 `tests/mermaid_cpp/render_entry.cpp` 和 `tools/mermaid-cpp/native-renderer-test.cpp` 中加入 `zenuml` 5 个固定案例（见 `tools/mermaid-cpp/zenuml-cases.mjs`）的公共入口调用与请求隔离断言，确认 `tests/platform/mermaid_zenuml.cpp` 通过公共入口生成的 SVG 与 `build/mermaid-cpp/zenuml-browser/original.json` 完全匹配。
  5. **清理已合入候选**：验证通过后，从 `docs/mermaid-cpp/candidates/2026-10-28/` 中移除已合入的 `zenuml-public.patch` 与 `zenuml-native-pins.json`，并同步更新 `candidates/2026-10-28/sha256.json` 与 `README.md`。

---

### Task B：完成 Class 图默认 ELK 布局与绘制（优先级：高，难度：中）

- **目标**：使 `classDiagram` 支持默认 `layout: elk` 路径（当前 `render_entry.cpp` 仅允许显式 `layout: dagre`），通过 5 个基准案例 × 双主题共 10 张 SVG 的浏览器对照测试，同时保证既有 Flowchart ELK 6 个案例不回归。
- **已有基础与参考补丁**：
  - 共享场景提取补丁：`docs/mermaid-cpp/candidates/2026-10-28/class-elk-shared.patch`
  - Class ELK 算子、后处理与测试补丁：`docs/mermaid-cpp/candidates/2026-10-28/class-elk.patch`
  - 已有的 Class 节点测量与绘制实现：`third_party/mermaid-cpp/src/class_svg.{ixx,cpp}`
- **允许修改的文件**：
  - `third_party/mermaid-cpp/src/flowchart_elk.{ixx,cpp}`
  - `third_party/mermaid-cpp/src/class_elk.{ixx,cpp}`（新增）
  - `third_party/mermaid-cpp/src/class_svg.{ixx,cpp}`
  - `third_party/mermaid-cpp/src/render_entry.cpp`
  - `tests/mermaid_cpp/class_elk.cpp`（新增）
  - `tests/mermaid_cpp/fixtures/class-elk.tsv`（新增）
  - `tests/platform/mermaid_class_elk.cpp`（新增，或扩展现有平台测试）
  - `tools/mermaid-cpp/class-elk-*.mjs`（新增）
  - `tools/mermaid-cpp/CMakeLists.txt`、`.github/workflows/mermaid-cpp.yml`
- **精确执行步骤**：
  1. **应用两个候选补丁**：
     ```bash
     git apply --check docs/mermaid-cpp/candidates/2026-10-28/class-elk-shared.patch
     git apply docs/mermaid-cpp/candidates/2026-10-28/class-elk-shared.patch
     git apply --check docs/mermaid-cpp/candidates/2026-10-28/class-elk.patch
     git apply docs/mermaid-cpp/candidates/2026-10-28/class-elk.patch
     ```
     - `class-elk-shared.patch` 在 `folia.mermaid.flowchart_elk` 中导出了 `ElkMeasuredScene`（包含 `std::vector<ElkLabelPosition> end_labels`）和 `layout_measured_elk_scene(...)`，供 Flowchart 与 Class 共用。
     - `class-elk.patch` 新增了 `folia.mermaid.class_elk` 模块（提供 `layout_measured_elk_class` 与端点标签避让后处理 `finish_class_elk_scene`）、9 列基准数据 `tests/mermaid_cpp/fixtures/class-elk.tsv`、单元测试 `tests/mermaid_cpp/class_elk.cpp` 以及 `tools/mermaid-cpp/class-elk-{cases,oracle,browser-probe}.mjs`。
  2. **跑通 `class-elk-oracle.mjs` 与 `FoliaMermaidTests`**：
     - 运行 `node tools/mermaid-cpp/class-elk-browser-probe.mjs` 生成 `build/mermaid-cpp/class-elk-browser/original.json`，再运行 `node tools/mermaid-cpp/class-elk-oracle.mjs` 校验 9 列 `class-elk.tsv`。
     - 编译并运行 `FoliaMermaidTests.exe "*class_elk*"`，确认 `layout_measured_elk_class` 与 `finish_class_elk_scene` 加上 `flowchart_elk_paint.inc` 中的端点裁剪后，所有节点、边路径、中心标签及起止端点标签（`startRight` / `endLeft`）坐标与 `class-elk.tsv` 在 `1e-4` 精度内一致。
  3. **在 `class_svg.{ixx,cpp}` 与 `render_entry.cpp` 接入 ELK 绘制路径**：
     - 在 `ClassSvgConfig`（`third_party/mermaid-cpp/src/class_svg.ixx`）中增加 `bool elk_layout = false;`。
     - 在 `third_party/mermaid-cpp/src/render_entry.cpp` 的 `u"classDiagram"` 分支中，将原本仅允许 `u"dagre"` 的检查放宽为允许 `u"dagre"` 或 `u"elk"`，并将 `c.elk_layout = (layout == u"elk");` 写入 `ClassSvgConfig`。
     - 在 `third_party/mermaid-cpp/src/class_svg.cpp` 的 `render_class_svg` 中：当 `config.elk_layout` 为 `true` 时，调用 `layout_measured_elk_class` 获取 `ClassElkScene`，对每条边保存裁剪前的原始端口坐标 `raw_endpoints`，执行与 Flowchart ELK 相同的矩形边界裁剪（`clip_elk_node_endpoints` / `elk_remove_short_marker_segment`），调用 `finish_class_elk_scene` 更新端点标签位置，最后使用 `rounded_elk_path`（圆角正交折线）替代 Dagre 的 `curveBasis` 输出边路径，并渲染对应的 `class="edgeTerminals"` 端点标签组。
  4. **验证双主题 10 张 SVG 并清理补丁**：
     - 确认 `compare-class-svg.mjs` 对 `class-elk-browser/original.json`（5 案例 × default/dark = 10 张 SVG）和既有 Dagre Class 案例全部通过，且 Flowchart ELK 测试不回归。
     - 从 `docs/mermaid-cpp/candidates/2026-10-28/` 移除已合入的 `class-elk*.patch`。

---

### Task C：实现 ER 图（`erDiagram`）原生解析、布局与 SVG 渲染（优先级：高，难度：中）

- **目标**：完成 `erDiagram` 的 C++23 原生模块并接入公共渲染入口，通过 8 个成功案例（双主题 16 张 SVG）与 2 个语法错误案例的验证。
- **已有基础（均已提交在仓库中，可直接复用）**：
  - 固定测试案例（8 成功 + 2 失败）：`tools/mermaid-cpp/er-cases.mjs`
  - 上游解析器 Oracle 脚本：`tools/mermaid-cpp/er-oracle.mjs`
  - 解析器基准 Fixture：`tests/mermaid_cpp/fixtures/er.tsv`
  - 浏览器 SVG 采样脚本：`tools/mermaid-cpp/er-browser-probe.mjs`
  - SVG 对照脚本：`tools/mermaid-cpp/compare-er-svg.mjs`
  - 上游源码哈希锁：`docs/mermaid-cpp/source-lock.json` 中的 `er` 条目（锁定 `erParser.ts`、`erDb.ts`、`erRenderer-unified.ts`、`erMarkers.ts`、`styles.ts` 及 `node_entities.ts`）
- **需要新增或修改的文件**：
  - 新增：`third_party/mermaid-cpp/src/er.ixx`、`third_party/mermaid-cpp/src/er.cpp`
  - 新增：`third_party/mermaid-cpp/src/er_svg.ixx`、`third_party/mermaid-cpp/src/er_svg.cpp`
  - 新增：`tests/mermaid_cpp/er.cpp`
  - 新增：`tests/platform/mermaid_er.cpp`（需同步加入 `tools/mermaid-cpp/CMakeLists.txt` 的 `FoliaMermaidPlatformTests` 列表）
  - 接线修改：`third_party/mermaid-cpp/src/render_entry.{ixx,cpp}`、`src/platform/text/mermaid_renderer.ixx`、`.github/workflows/mermaid-cpp.yml`
- **精确执行步骤**：
  1. **实现解析器与数据模型（`er.{ixx,cpp}`）**：
     - 对照 `tools/mermaid-cpp/er-oracle.mjs` 与 `tests/mermaid_cpp/fixtures/er.tsv` 的列格式，实现 `erDiagram` 语法解析：
       - 支持 `direction TB/BT/LR/RL`；
       - 支持实体声明、方括号别名 `ACCOUNT["User account"]`、属性块 `{ type name [PK|FK|UK] ["comment"] }`；
       - ⚠️ **必查边界**：属性块未闭合（如案例 `unclosed`：`erDiagram\nENTITY {\nint id`）或关系缺少目标实体（如案例 `missing-target`）时，**必须返回 `std::unexpected` 错误**，不能静默成功；
       - 支持四种基数端点（`ONLY_ONE` `||`、`ZERO_OR_ONE` `|o` / `o|`、`ONE_OR_MORE` `}|` / `|{`、`ZERO_OR_MORE` `}o` / `o{`）及实线（识别关系 `--`）/ 虚线（非识别关系 `..`）。
     - 编写 `tests/mermaid_cpp/er.cpp`，读取 `fixtures/er.tsv` 验证全部 8 个成功案例的实体、属性、关系与配置以及 2 个错误案例。
  2. **实现测量、Dagre 布局与 SVG 绘制（`er_svg.{ixx,cpp}`）**：
     - 对照锁定源码 `packages/mermaid/src/rendering-util/rendering-elements/shapes/erBox.ts`（`node_entities.ts`）与 `erMarkers.ts`：
       - 测量实体标题及属性表的三列（`type`、`name`、`keys`）或四列（含 `comment`）文本宽高；
       - 按 `entityPadding`、`diagramPadding`、`nodeSpacing`、`rankSpacing` 构造实体节点框，复用已有的 `folia.mermaid.dagre_rank` / `flowchart_svg` Dagre 管线计算节点坐标与边控制点；
       - 绘制实体外框、奇偶行背景（`attributeBackgroundColorOdd` / `attributeBackgroundColorEven`）、列分隔线、关系曲线、端点基数标记（crow's foot markers）以及关系标签背景框（`rect.er-label-background` / `foreignObject`）。
  3. **接入公共入口与 CI**：
     - 在 `render_entry.{ixx,cpp}` 中增加 `ErRenderInput` 与 `ErMeasure`（处理 `prepared->type == u"er"`），在 `src/platform/text/mermaid_renderer.ixx` 中完成 DirectWrite 文本测量桥接。
     - 新增 `tests/platform/mermaid_er.cpp` 输出 16 张 SVG，并在 `.github/workflows/mermaid-cpp.yml` 中加入 `er-browser-probe.mjs` 与 `compare-er-svg.mjs` 步骤。

---

### Task D：补齐 Requirement 图及剩余语法分支（优先级：中）

- **目标**：在 Task A/B/C 收尾后，对照 Folia 支持的图族清单实现 `requirementDiagram`（Requirement 图）的原生解析、Dagre 布局与 SVG 渲染，并按需补齐 State 注释/并发分隔等次要分支。
- **执行原则**：先编写 `tools/mermaid-cpp/requirement-{cases,oracle,browser-probe}.mjs` 锁定上游 Mermaid 12.2.0 行为与源码哈希（记入 `source-lock.json`），再实现 `requirement.{ixx,cpp}` 与 `requirement_svg.{ixx,cpp}`。

---

### Task E：WinUI 宿主集成与移除 Rust Mermaid 依赖（优先级：在图族收尾后执行）

- **目标**：将 Folia WinUI 桌面应用的 Mermaid 渲染链路从 `mermaid-rs-renderer` 切换至 `FoliaNativeMermaid` C++ 实现。
- **精确执行步骤**：
  1. 在 WinUI 渲染调用处接入 `folia.platform.mermaid_host` / `FoliaNativeMermaid`，复用现有的异步任务队列、缓存及普通 `usvg` 规范化管线。
  2. 在真实 Windows 桌面环境下验证：代表性图族渲染、编辑器实时修改刷新、亮/暗主题切换、尺寸自适应、PDF 导出及非法语法恢复。
  3. 桌面验收通过后，从 `usvg-normalizer` 的 `Cargo.toml` 与导出头文件中移除 `mermaid-rs-renderer` 及 Mermaid 专属 Rust 导出符号（保留普通 SVG 的 `usvg` 规范化能力），并运行 `tools/generate-third-party-notices.ps1` 更新根目录许可证声明。

---

## 四、构建与验证命令速查（Copy-Pasteable Commands）

```powershell
# 1. 安装 Oracle 依赖（首次或 lockfile 更新后执行）
pnpm --dir tools/mermaid-cpp/oracle install --frozen-lockfile --ignore-scripts

# 2. 配置并编译独立 Mermaid C++ 测试与动态库（Windows + VS + Ninja）
cmake -S tools/mermaid-cpp -B build/mermaid-cpp -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/mermaid-cpp

# 3. 运行 CTest 测试套件（含核心测试、DirectWrite 平台测试、DLL 宿主测试）
ctest --test-dir build/mermaid-cpp --output-on-failure

# 4. 单独运行某个核心测试套件（支持通配符过滤）
./build/mermaid-cpp/FoliaMermaidTests.exe "*zenuml*"
./build/mermaid-cpp/FoliaMermaidTests.exe "*class_elk*"

# 5. 生成平台测试 SVG 并与官方浏览器输出对比（以 ZenUML 和 ER 为例）
$env:FOLIA_MERMAID_SVG_OUTPUT = "build/mermaid-cpp/pie-svg"
./build/mermaid-cpp/FoliaMermaidPlatformTests.exe
node tools/mermaid-cpp/zenuml-browser-probe.mjs build/mermaid-cpp/zenuml-browser
node tools/mermaid-cpp/compare-zenuml-svg.mjs build/mermaid-cpp/zenuml-browser/original.json build/mermaid-cpp/pie-svg

# 6. 运行主工程核心测试与 Windows 编辑器回归测试
cmd /c build_test.bat 2>&1
cmd /c build_platform_test.bat 2>&1
```

---

## 五、智能体工作准则（Agent Guardrails）

1. **真实测量与原版算法**：所有需要文本尺寸的布局必须通过平台层传入真实 DirectWrite 测量结果，严禁使用“字符数 × 固定系数”估算字宽，严禁针对特定测试案例硬编码坐标偏移或调大比较容差（几何容差固定为 `1.5`，ELK 算子容差为 `1e-4`）。
2. **模块与平台隔离**：`third_party/mermaid-cpp/src/` 下的模块必须保持纯标准 C++23（使用 `import std;`），不得包含任何 Windows / DirectWrite / WinRT 头文件；平台相关代码一律放在 `src/platform/text/`。
3. **小步提交与原位更新交接**：每完成一个独立任务包并通过验证，立即提交代码，并直接在本文件顶部更新“当前基线状态”和对应任务的完成情况，不要在文档末尾堆叠历史流水账。
