# Mermaid 模块组织

`folia::mermaid` 是平台无关的 C++23 库。目录按职责和图族组织；
`folia.mermaid.*` 模块名是代码依赖的稳定标识，不随物理路径改变。
这次整理保留现有解析、布局、主题计算和 SVG 输出算法，也保留锁定的
Mermaid 12.2.0 行为基准。

## 源码归属

| 目录 | 职责 | 维护规则 |
| --- | --- | --- |
| `src/mermaid/api/` | 公共渲染入口、图族分发和测量编排 | 负责组装图族与宿主服务；图族算法留在图族目录 |
| `src/mermaid/frontend/` | 源码预处理、frontmatter、指令解析和图族识别 | 不执行宿主字体测量或平台渲染 |
| `src/mermaid/config/` | 配置值、schema、合并、清理和会话隔离 | 配置树只有一套实现；生成数据位于 `generated/` |
| `src/mermaid/themes/` | 主题名称、颜色派生和主题变量 | 上游派生的预设位于 `presets/`，颜色名表位于 `generated/` |
| `src/mermaid/diagrams/<family>/` | 一个图族的解析、模型、布局适配和 SVG 输出 | 新图族及其辅助文件放在自己的目录；跨图族复用显式表达 |
| `src/mermaid/layout/elk/` | ELK 分层、复合图、端口、排序和路由算法 | 保留阶段顺序、随机状态和既有几何精度 |
| `src/mermaid/layout/dagre/` | Dagre 排名、排序和坐标分配 | 布局适配使用现有图模型 |
| `src/mermaid/layout/cose/`、`layout/fcose/` | 力导向布局与约束算法 | 区分通用布局和图族适配的责任 |
| `src/mermaid/rendering/rough/` | 共享手绘笔画和填充算法 | 不依赖操作系统绘图 API |
| `src/mermaid/text/` | 普通／数学标签、SVG 文字和 `TextMeasurer` 契约 | 宿主提供真实字体度量；核心编排只消费测量结果 |
| `src/mermaid/support/` | 通用词法、数字转换和 HTML 字符引用 | 保持小而明确的用途，不收纳图族专有代码 |

`src/mermaid/` 根目录只保留构建入口、依赖补丁、许可证和说明。
`.ixx` 与相应 `.cpp` 放在同一职责目录；同一模块的多个实现单元也应有明确归属。
目录拆分不会新增第二套运行路径或对外模块名的兼容副本。

### 模块与文件不是同一层接口

使用方继续写 `import folia.mermaid.render_entry;` 等模块导入，
不通过文件路径包含模块接口。CMake 的 `folia::mermaid` 目标递归收集
接口和实现，导入图决定编译顺序。不要为每个图族维护一份重复的总入口，
也不要为了目录整齐把所有模型和算法合并到一个巨大的接口。

`.inc` 是其所属实现中的文本片段，不是独立模块。部分现有片段仍由多个
图族共享，例如 Flowchart 的绘制样式、ELK 路径和形状辅助函数。
这些片段留在提供者的图族目录，调用方使用明确的相对路径。
不添加覆盖所有源码子目录的全局 include 搜索路径；这样依赖来源可见，
同名文件也不会因搜索顺序而被误用。

## 实际依赖与平台边界

主要调用顺序是：宿主提供请求配置和测量服务，前端提取图族及局部配置，
图族解析并建立模型，测量编排取得真实尺寸，布局和 SVG 模块生成结果。
配置会话负责每次请求的主题与配置隔离。

目录表达维护责任，不承诺所有导入都严格沿一条分层方向。现有适配关系包括：

| 依赖 | 原因 |
| --- | --- |
| `dagre_rank` → `flowchart` | Dagre 适配继续使用已经定义的流程图模型 |
| `fcose_constraints` → `architecture_constraints` | 原生 fCoSE 约束使用架构图的约束表示 |
| `fcose_architecture` → `architecture_measurement` | 架构图适配将实测图元尺寸交给力导向布局 |
| `flowchart_svg` 实现 → `flowchart_elk` | SVG 编排可选择 ELK；适配接口复用流程图的布局结果类型 |
| State、Class、ER、Requirement、Use Case → Flowchart 辅助绘制片段 | 复用既有路径、样式和图形算法 |

因此，不能仅因为目录名称而增加会拒绝这些既有适配的层级规则。
将来若要抽取公共图模型或绘制模块，应作为独立的语义重构，保留算法
fixture 与完整图回归，并确认没有扩大接口或复制数据模型。

Windows 专用代码仍位于 `src/platform/text/`：`mermaid_measurer` 提供
DirectWrite，`mermaid_renderer` 提供宿主桥接，`mermaid_host` 封装 DLL 会话。
核心目录不能引入 Windows、WinRT、Direct2D 或 DirectWrite 头文件。
`TextMeasurer` 是平台无关契约；Linux 回归通过真实 Pango 提供同一契约的数据，
不按字符数估算宽度。数学标签继续通过现有数学服务获取 SVG 和精确尺寸。

## 生成数据与上游来源

| 输出目录／文件 | 生成或检查工具 |
| --- | --- |
| `config/generated/config_schema_data.inc` | `tools/mermaid-cpp/config-oracle.mjs` |
| `config/generated/config_defaults.inc`、`config_callbacks.inc` | `tools/mermaid-cpp/session-oracle.mjs` |
| `themes/generated/theme_color_names.inc` | `tools/mermaid-cpp/theme-oracle.mjs` |
| `themes/presets/theme_*.inc` | `tools/mermaid-cpp/generate-themes.mjs` |

所有输出路径均相对 `src/mermaid/`。生成器与消费方必须同时指向这些位置，
避免下一次生成又在根目录产生第二份数据。不要手工修改生成结果以通过测试；
先核实锁定的上游输入与生成步骤。新增上游依赖时更新
[`source-lock.json`](source-lock.json) 和
[`THIRD_PARTY_NOTICES.md`](../../src/mermaid/THIRD_PARTY_NOTICES.md)。
许可证留在根目录，供既有 Windows 分发规则收集。

## 扩展与验证流程

1. 确定改动归属。图族语法和几何放入相应 `diagrams/<family>/`；
   通用配置、主题、文字或布局变化放入对应目录。只有公共编排接线进入 `api/`。
2. 保持模块名稳定。在 `.ixx` 中声明接口，在同目录 `.cpp` 中实现；
   使用 `import std;`，通过显式模块导入表达依赖。
3. 若新增图族，在现有识别、准备、测量和渲染入口接线，避免另建旁路。
   新增共享片段时明确其所有者和所依赖的局部类型。
4. 新增能暴露语义风险的测试，优先复用 `tests/mermaid_cpp/` 的模块测试、
   官方算法 fixture，以及 `tests/mermaid_regression/cases/` 的完整图输入。
5. 执行结构检查、构建和受影响测试；布局、字体或绘制行为变化还要执行
   相关完整图及官方 SVG 对照。Windows 宿主变化另需平台测试。

源码结构检查只依赖 Python 3，可从任意工作目录执行：

```sh
python3 tools/mermaid-cpp/check-structure.py
```

它检查根目录没有散落源码、源码位于已定义职责目录、模块接口唯一、
实现与导入能找到接口、局部 include 能解析、核心没有平台头文件，以及
生成器输出位置与已提交数据一致。它不会代替 C++ 编译，也不会把未执行的
渲染对照当作通过。

独立 CMake 工程位于 `tools/mermaid-cpp/`，支持 Windows 和具备 C++23
`import std` 工具链的 Linux。具体构建命令见 [README](README.md)，
真实字体测量、完整语料与官方比较见 [回归说明](regressions.md)。

```sh
cmake --build build/mermaid-cpp --target FoliaMermaidTests FoliaMermaidRegressionProbe --parallel 4
ctest --test-dir build/mermaid-cpp -R '^FoliaMermaidTests$' --output-on-failure
python3 tools/mermaid-cpp/regression/run-native.py --probe build/mermaid-cpp/FoliaMermaidRegressionProbe
```

最后一条命令在 Linux 上需要具备 PyGObject、Pango/PangoCairo 和 fontconfig
的 Python；某些环境应使用 `/usr/bin/python3`。原生 SVG 生成成功与官方 SVG
对照成功分别记录；未执行的 WinUI／DirectWrite 验证同样应明确说明。
