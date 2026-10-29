# ELK、CoSE/fCoSE 与外部图族布局边界说明

本文档说明 `mermaid-cpp` 中复杂图布局算法（ELK 分层与复合图、Cytoscape CoSE/fCoSE 力导向布局、ZenUML 序列布局）的上游基准版本、默认配置契约与 C++ 模块划分。

---

## 一、ELK 分层与复合图布局（`elkjs` 0.9.3）

### 1. 上游基准与许可证
- 锁定的 Mermaid `1fb7869866dd619168d840291c9c54b4f129758b` 使用 `elkjs` **0.9.3**（`lib/elk-worker.js`，由 Eclipse Layout Kernel 的 Java 实现经 GWT 编译生成）。
- ELK 相关原生模块遵循 **EPL-2.0** 许可证（见 `third_party/mermaid-cpp/ELK-LICENSE` 及 `THIRD_PARTY_NOTICES.md`），对应上游源码哈希记录于 `docs/mermaid-cpp/source-lock.json`。

### 2. Mermaid 侧的 ELK 适配器预设契约
Mermaid 的 `packages/mermaid/src/rendering-util/layout-algorithms/elk/render.ts` 定义了 `layout: elk` 的默认参数预设：

| 配置项 | 默认值 |
| --- | --- |
| 布局算法（`layout: elk`） | `elk.layered` |
| 层级分配（Layering） | `NETWORK_SIMPLEX` |
| 根图与容器节点放置（Node placement） | `BRANDES_KOEPF` |
| Brandes-Koepf 对齐方式 | `BALANCED` |
| 去环策略（Cycle breaking） | `DEPTH_FIRST` |
| 根图层级处理（Hierarchy handling） | `INCLUDE_CHILDREN` |
| 根图基础间距（Root spacing base） | `40` |
| 端口四周外边距（Port surrounding margin） | 每侧 `12` |

- 当配置指定命名预设时：`legacy` 使用 `GREEDY` 去环与 `NONE` 对齐；`modelOrder` 与 `depthFirst` 将根节点放置策略切换为 `NETWORK_SIMPLEX`。显式传入的 `config.elk` 键值优先于预设值。
- 图的实际方向（`TB`/`BT`/`LR`/`RL`）覆盖根节点的默认 `DOWN` 方向；复合分组内边距、子图标题约束以及实测标签包围盒由适配器在进入 ELK 管线前组装。

### 3. C++ 原生 ELK 模块划分
`third_party/mermaid-cpp/src/` 下按 ELK Layered 管线的实际处理阶段拆分为以下模块：

1. **去环与分层**：
   - `folia.mermaid.elk_cycle`：实现 `DepthFirstCycleBreaker`，按模型顺序与入度遍历节点并生成反馈边翻转计划（自环不参与去环）。
   - `folia.mermaid.elk_layering`：实现 `NetworkSimplexLayerer`，基于紧树（tight tree）与割值交换（cut/slack exchange）计算节点层级并执行连通分量平衡。
   - `folia.mermaid.elk_long_edges`：实现 `LongEdgeSplitter`，对跨层边插入虚拟节点（dummy nodes）并拆分边段。
2. **交叉最小化与端口排序**：
   - `folia.mermaid.elk_order`、`elk_order_constraints`、`elk_model_order`：实现 `PortListSorter`、重心启发式（`BarycenterHeuristic`）及 `LayerSweepCrossingMinimizer` 正反向交替扫描。
   - `folia.mermaid.elk_north_south`、`elk_north_south_layout`：处理南北向（North/South）端口虚拟节点准备、布局单元排序约束与坐标还原。
3. **坐标放置、正交路由与边还原**：
   - `folia.mermaid.elk_placement`：实现 Brandes-Koepf 四方向平衡坐标分配。
   - `folia.mermaid.elk_ports`、`elk_labels`、`elk_end_labels`：处理隐式点端口、边中心标签与边起止端点标签（HEAD/TAIL）的尺寸与位置。
   - `folia.mermaid.elk_routing`、`elk_restore`：执行正交边路由（共享上游 `java.util.Random` 确定性随机状态），并在路由完成后合并长边分段、还原翻转边方向。
4. **复合图（Compound Graph）与图族适配**：
   - `folia.mermaid.elk_compound_graph`、`elk_compound_ports`、`elk_compound_layout`、`elk_compound_restore`、`elk_direction`、`elk_boundary_layers`、`elk_boundary_layout`：处理跨层级边、外部端口、子图方向变换、复合图递归几何拼装与子图间距。
   - `folia.mermaid.flowchart_elk`：将实测后的 Flowchart 节点、子图与边接入复合 ELK 管线（已贯通公共入口并通过 6 个基准案例验证；Class 图的 ELK 适配见 `candidates/2026-10-28/class-elk*.patch`）。

---

## 二、CoSE 与 fCoSE 力导向布局（Mindmap 与 Architecture）

Mindmap 与 Architecture 分别依赖不同代际的 Cytoscape CoSE 布局库，二者在 `third_party/mermaid-cpp/src/` 中保持独立实现：

| 使用图族 | 上游算法包版本 | C++ 原生模块 | 核心特征 |
| --- | --- | --- | --- |
| **Mindmap** | `cytoscape-cose-bilkent` 4.1.0、`cose-base` 1.0.3、`layout-base` 1.0.2、`cytoscape` 3.34.0（MIT） | `cose_tree.{ixx,cpp}`、`mindmap.{ixx,cpp}`、`mindmap_svg.{ixx,cpp}` | 以 `quality: 'proof'` 运行平面树力导向布局。验证时分为“注入相同测量尺寸验证布局算子”与“使用 DirectWrite 实测字宽验证最终 SVG 绘制”两层。 |
| **Architecture** | `cytoscape-fcose` 2.2.0、`cose-base` 2.2.0、`layout-base` 2.0.1（MIT） | `architecture.{ixx,cpp}`、`architecture_constraints.{ixx,cpp}`、`architecture_measurement.{ixx,cpp}`、`architecture_svg.{ixx,cpp}` | 包含固定随机种子、分组相关的理想边长/弹性系数、对齐与相对位置约束求解，以及两次布局传递（two-pass fCoSE）。 |

---

## 三、ZenUML 外部图族集成（`@zenuml/core` 3.49.2）

- **上游版本与校验**：Mermaid 12.2.0 锁定 `@zenuml/core` **3.49.2**（MIT 许可证，Copyright 2022 Peng Xiao）。官方 npm 归档 `core-3.49.2.tgz` 的 SHA-512 为：
  `sha512-qeQMapuE0XETML2mRw2MIIrMCjcZBhRDSwW10//q8XQ+VyExzkAn39VCBwxIbbPdTBt6gJnaBBixF9A4hsIxcA==`
  其内置 `dist/zenuml.esm.mjs.map`（SHA-256：`c090e83b377121186edaa097c6e402562f72a9a694319c7a57cf3d06b4eba061`）包含全部 457 个上游源文件（含词法/语法分析器、横向坐标约束求解 `Coordinates`、纵向递归布局 `VerticalCoordinates`、`buildGeometry` 与 SVG 模板）。
- **C++ 原生实现**：`third_party/mermaid-cpp/src/zenuml.{ixx,cpp}`、`zenuml_svg.{ixx,cpp}` 与 `zenuml_assets.inc` 原生实现了语法解析、横向参与者间距约束求解、纵向消息/嵌套块递归布局及 SVG 输出，由平台层通过 `ZenSvgMeasure` 注入 DirectWrite 实测字宽。
