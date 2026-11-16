# Mermaid 全图族回归

回归输入位于 `tests/mermaid_regression/cases/`，每个图族一个 JSON 文件。
它们与核心单元测试共存：单元测试定位语法、数据库、排程、几何和格式化规则；
这里的完整图验证公共入口、字体测量、布局与最终 SVG 能否一起工作。
生产代码与回归驱动共用 `folia.mermaid.measured_render`，Windows 的
`folia.platform.mermaid_renderer` 仅提供 DirectWrite、原生数学服务和宿主可用宽度。

## 本次执行结果

本分支的最终源码通过核心 **255项／957,780断言**、真实Math集成
**5项／205断言**，全部 **375个输入** 均通过官方语法准备和完整原生渲染。
具体源码、二进制、语料、fixture和SVG散列保存在
[`regression-validation.json`](regression-validation.json)。

**完整官方SVG验收尚未全部完成。** 本环境已验证Info8张、Packet8张完整SVG，
以及ZenUML24张实际官方renderer／同机Skia的逐像素零差；其余335张、31个图族
需要可运行浏览器的环境。本地Chromium启动失败，连接浏览器的测试页又被URL策略
拒绝，因此没有把缺少的官方证据算作通过。Windows DirectWrite／WinUI也未执行。

正式总比较器只收到Info／Packet捕获时，明确以16／375、退出码1失败，
`officialSvgCompared` 保持false。ZenUML的24张Canvas证据另行记录。
下方命令保留完整375例的验收要求，不会因为缺少浏览器而退化成语法检查。

## 固定基准与覆盖要求

Mermaid 基准为 **12.2.0**，提交
`1fb7869866dd619168d840291c9c54b4f129758b`；外部 ZenUML 基准为
`@zenuml/core` **3.49.2**。算法、官方文档样例的来源与源码散列保存在
[`source-lock.json`](source-lock.json)，Node 依赖使用
`tools/mermaid-cpp/oracle/pnpm-lock.yaml`。

`corpus.mjs` 检查固定的 34 图族清单，每族至少 8 个不同输入，并要求多个复杂例
（只展示版本号的 Info 除外）。每例包含稳定 id、复杂度、覆盖特征、上游来源和
原始 Mermaid 源码；可选 `config` 表示正常初始化配置。源码中的 frontmatter
和 `init` 指令按产品中的优先级处理。没有 skip 或 expectedFailure 字段。
不能删除失败的合法复杂例、修改期望来迎合原生输出，或用重复输入增加数量。

覆盖包括完整官方示例，以及嵌套分组、跨组反馈、长标签、多行与 Unicode、
方向和布局选择、类与样式、不同箭头、数值极值、排程依赖等组合。
Flowchart 的 53 个登记形状分别进入普通文字和多行／Markdown 的连接图，
并轮换四个方向。Railroad 在同一图族内覆盖 IR、EBNF、ABNF、PEG 四种语法。
Architecture 未注册图标的问号是官方的资源缺失行为，不代表外部图标包已注册。

### 当前覆盖矩阵

共有 **375 例**，其中 **230 例复杂组合**。Info 只显示版本信息，8例覆盖主题、
字体和配置不变量，不把字体大小不生效的等价结果算作新的视觉能力。
既有数学集成测试和下述算法阶段测试另行统计，不拿它们补足完整图数量。

| 图族 | 完整图输入 | 复杂组合 |
| --- | ---: | ---: |
| agentflow | 8 | 5 |
| architecture | 9 | 5 |
| block | 10 | 7 |
| c4 | 8 | 7 |
| class | 10 | 5 |
| cynefin | 8 | 7 |
| er | 9 | 6 |
| eventmodeling | 15 | 5 |
| flowchart | 27 | 23 |
| gantt | 11 | 8 |
| gitgraph | 12 | 9 |
| info | 8 | 0 |
| ishikawa | 8 | 7 |
| journey | 8 | 5 |
| kanban | 8 | 6 |
| mindmap | 8 | 5 |
| packet | 8 | 5 |
| pie | 8 | 6 |
| quadrant | 8 | 5 |
| radar | 8 | 5 |
| railroad | 18 | 5 |
| requirement | 8 | 4 |
| sankey | 8 | 6 |
| sequence | 10 | 7 |
| state | 8 | 4 |
| swimlane | 13 | 12 |
| timeline | 8 | 6 |
| tree-view | 13 | 5 |
| treemap | 16 | 14 |
| usecase | 8 | 3 |
| venn | 11 | 6 |
| wardley | 21 | 9 |
| xy | 10 | 7 |
| zenuml | 24 | 11 |

运行以下命令查看当前准确数量、每族特征和输入文件散列：

```sh
node tools/mermaid-cpp/regression/corpus.mjs
```

## 验收分层

| 检查 | 实际检查的内容 | 成功不能证明的内容 |
| --- | --- | --- |
| `prepare-official.mjs` | 原版 Mermaid 的语法、配置和数据库入口；原版 ZenUML 的 AST | 原生渲染成功、整图外观一致 |
| `FoliaMermaidTests` | 原生解析、数据模型、布局与官方算法 fixture | 宿主字体或 WinUI 的最终显示 |
| `run-native.py` | 全部输入经生产 C++ 测量与渲染路径生成 SVG | 与官方 SVG 一致 |
| `capture-official.mjs` | 锁定官方 renderer 生成完整 SVG | 原生结果一致 |
| `compare-svg.mjs` | 全语料 SVG、可见文字、绘制属性和实际栅格比较 | 未执行的平台 UI 交互 |
| `compare-zenuml-canvas.mjs` | 原版 ZenUML 与原生 renderer 使用同一个真实 Skia 字体引擎，比较 SVG 文字、视口及逐像素结果 | DirectWrite／浏览器跨平台字体一致 |

完整验收必须保留每例结果，逐项核对语料散列和 SVG 散列。
缺失样例、旧语料报告、生成失败、缺少浏览器或比较失败都会使总检查失败；
部分成功不会把 `officialSvgCompared` 标为 true。
当前实际执行结果与限制见 [`regression-validation.json`](regression-validation.json)
和 [`HANDOFF.md`](HANDOFF.md) 的当前检查点，
不要把历史基础样例的通过记录当作本语料的验收。

## 构建与原生回归

按 [README](README.md) 恢复依赖和配置支持 C++23 modules 的构建目录。
回归驱动是普通控制台程序，不需要启动 WinUI：

```sh
cmake --build build/mermaid-cpp --target FoliaMermaidTests FoliaMermaidRegressionProbe --parallel 4
ctest --test-dir build/mermaid-cpp -R '^FoliaMermaidTests$' --output-on-failure
python tools/mermaid-cpp/regression/run-native.py --probe build/mermaid-cpp/FoliaMermaidRegressionProbe.exe
```

驱动在 Windows 和 Linux 均需要 Python 3.11 或更高版本。Windows 直接使用
生产 DirectWrite；Linux 另需系统 Pango、PangoCairo、PyGObject 和 fontconfig。例如 Debian 环境可用
`/usr/bin/python3`，probe 路径省略 `.exe`。它通过有界的 JSON 协议提供真实
字体、glyph bounds 与 Unicode 断行机会，不估算字符宽度。JSON 使用显式
Unicode 转义，补充平面字符不会被 YAML 转义规则误拒绝。

结果默认写入 `build/mermaid-regression/native/`，包括每张 SVG、逐例错误、
使用的字体引擎、输入散列和 probe 二进制散列。排查时可传
`--family flowchart`、`--case flowchart-collapsed-subgraph` 或 `--prepare`。
这些定向结果不能替代全语料结果。异常进程退出必须停止广跑，先定向诊断。

原生数学仍通过现有 Math 动态库接入。若要同时运行既有数学集成回归，
按 README 配置 `FOLIA_NATIVE_MATH_RUNTIME` 后构建、运行
`FoliaMermaidMathTests`；不复制字体数据，不改变公式缓存。

## 官方 SVG 和整图比较

在有可用 Chromium 或 Edge 的开发环境执行：

```sh
pnpm --dir tools/mermaid-cpp/oracle install --frozen-lockfile --ignore-scripts
node tools/mermaid-cpp/prepare-zenuml-oracle.mjs
node tools/mermaid-cpp/regression/prepare-official.mjs
node tools/mermaid-cpp/regression/capture-official.mjs --browser=/absolute/path/to/browser
node tools/mermaid-cpp/regression/compare-svg.mjs --official build/mermaid-regression/official-svg --native build/mermaid-regression/native --browser /absolute/path/to/browser
```

也可以用 `FOLIA_BROWSER` 指定浏览器。捕获器固定 1600×1200 的可用屏幕和
UTC 时区；原生驱动也显式传入 1600 CSS px 的可用宽度。应在同一台机器、
相同已安装字体下捕获与比较，使用同一份未修改的语料。
Venn 的随机初值按已有锁定 oracle 固定，避免把随机布局差异当作实现差异。
Gantt 样例显式给定日期或时间范围，并关闭动态 today marker；另保留空轴回归。

比较器检查视口、可见 grapheme 的内容、位置、字号、字重、斜体与颜色，
再在 2× 分辨率比较实际栅格。几何容差固定 **1.5 CSS px**，不会因样例复杂
而扩大；ELK 算子测试仍是 **1e-4**。SVG resource id 不必相同，但实际绘制属性
和图像必须通过。局部缺失笔画、小标记、颜色变化、透明度变化不能被整图
平均误差稀释。比较器自身有丢字、漏画、近邻平行线和透明像素的反例测试：

```sh
node tools/mermaid-cpp/regression/compare-svg.mjs --self-test
```

Packet 和 Info 的官方 renderer 不读取 DOM 文本几何，因此 Linux 上可以独立
运行完整 SVG 比较：逐项核对 rect/text、CSS、文字和实际 fontconfig 文件及 face index。
这条路径明确记录 `rasterCompared: false`，没有把 jsdom 当作字体或浏览器引擎。
可用 `capture-official.mjs --family=packet --output=...` 或 `--family=info` 单独捕获；
每次 `--family` 只接受一个图族。全图比较仍要求其余图族全部存在，不能用这两个
图族的报告宣称总验收成功。

## ZenUML 的真实 Canvas 对照

原版 `renderToSvg` 支持 OffscreenCanvas。专用捕获器通过锁定的
`@napi-rs/canvas` 提供真实 Skia 测量，显式断言没有进入按字符估宽的 fallback。
原版解析、横纵向定位、SVG 输出均保留；附加导出仅用于观察内部模型。
同一个字体引擎也响应原生 probe 的文本测量请求，排除跨引擎字体误差后，
完整位图逐像素比较，不使用宽松总误差比例。

```sh
node tools/mermaid-cpp/regression/capture-zenuml.mjs
node tools/mermaid-cpp/regression/compare-zenuml-canvas.mjs --probe=/absolute/path/to/FoliaMermaidRegressionProbe
```

几何、字体观测、官方 SVG、原生 SVG、PNG 和逐像素差异统计保存在对应的 ignored build
目录。该对照能定位嵌套控制块、返回消息和注释布局问题；它作为明确命名的
Canvas 验收存在，不冒充未执行的 Windows 或浏览器验收。

## 增加或修复回归

先保留能复现问题的官方合法输入，再添加复杂组合覆盖；通过原版入口核实其
含义，修复负责该语义的解析、测量或布局模块。不要在回归驱动中重写输入，
也不要按 case id 或源码片段补坐标。
算法 fixture 必须由锁定官方实现生成，提交后默认检查模式只比较、不重写。
ELK 的新增验证保留完整处理语义：24个自环布局，25个Greedy Switch阶段输入，
7个完整组件布局及14个组件阶段观察，21个Mermaid方向／层级adapter布局。
组件例包括原模型顺序、DFS存储顺序、共享随机状态、孤点及39／40／41节点阈值。
Greedy例检查实际交叉减少、端口顺序和RNG状态，不仅检查生成成功。
对应 `elk-self-loops`、`elk-greedy-switch`、`elk-components`、
`elk-components-layout`、`elk-directed-scopes` 的 `*-oracle.mjs` 可独立复验。
`er-regression-oracle.mjs`、`gantt-format-oracle.mjs`、
`treemap-value-oracle.mjs` 和 `zenuml-regression-oracle.mjs` 是本轮新增的定向入口。
最后重新构建、运行受影响核心测试和全语料渲染，再进行官方 SVG 对照。
所有命令均可在本地执行；不要求修改 GitHub CI。
