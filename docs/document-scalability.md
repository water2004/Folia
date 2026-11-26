# 大文档：语义树、布局片段与资源生命周期

## 目标和基线

本次改造从 `5861fff47b5697c65949ba41cd3f4edd5256ef45` 开始，分支为
`architecture/bounded-document-layout`。目标是让一个包含大量项目的列表
不再成为不可分割的编辑、布局和资源保留单位，同时保留原有 Markdown
语义、逐字符源文本、CST、选择坐标、撤销/重做和精确滚动。

用户提供的附件为 10,485,760 字节。本次只在头部、中部和尾部各读取
2,048 字节，合计 6 KiB；不会将附件全文读入工具输出、解析器或基准。
压力测试自行生成同类列表，并另用不重复、嵌套、松散列表验证通用性。

## 根因：一个局部步骤不等于一条局部链路

当前 inline 编辑已经在叶子的 `InlineDocument` 上执行，不是每次调用
全文 Markdown 解析器。但是仍有几个粒度不一致的边界：

1. 源编辑更新祖先的容器语法元信息时，直接调用 `block_path(root, id)`，
   从根深度优先搜索；编辑列表末尾仍需访问大量无关节点。已有的
   `cached_block_locators` 没有被这条调用链使用。
2. 索引维修会递归重新登记发生移动的后续兄弟子树。一个列表的根位置
   改变，不意味着列表内部的每条 parent/child 边都改变。
3. `render_builder` 将整个顶层 List 展开到一个 `RenderBlock`。虚拟化、
   render key、失效索引以及 DirectWrite 准备都依赖这个单位。
4. 屏外回收本来已经存在，但一个横跨整个文档的列表永远与视口相交，
   其文本、显示/源坐标映射、DirectWrite layout、行数据、paint plan
   就无法局部回收。
5. 几何、失效和视口策略是纯 C++，却放在 Windows 平台目录，普通
   Linux 核心测试无法直接覆盖这条链路。

因此不能只增加一个“大文件阈值”、关掉预布局，或在列表内插入人为
Markdown 分隔符。需要解开语义所有权、布局粒度和缓存生命周期。

## 架构

### 1. 文档树仍是编辑的唯一真相

List、ListItem、Paragraph 等仍由统一块树表达。叶子保留逐字符的
`InlineDocument.source` 和 lossless inline CST。选区仍是
`container_id + source_offset + affinity`。分片不能修改源文本、改变
NodeId，也不能引入以完整文档字符串为坐标的第二套编辑模型。

常规文本输入只更新该叶子的源、CST及通往根的必要语法摘要。查找通过
验证过的 parent/child locator 进行；结构事务只维修实际改变的边和
导航区间。全文搜索、显式保存、初始解析是允许的全文操作，不能混入
每次键入路径。

### 2. 物理布局片段是派生数据

一个语义块可以对应多个 `RenderBlock`。布局片段沿语义安全的流边界
划分，保留原父容器的样式和必要上下文；不在任意 Unicode 字符位置
切开段落。片段由稳定源节点定位，携带开头/结尾信息，只有真正的语义
边界才应用外边距、外部 block gap 或容器收尾装饰。

`editable_fragment_index` 直接把 editable owner 映射到物理片段。
`editable_top_level` 继续表达文档语义，不再兼任物理缓存索引。
普通输入根据 owner 定位少量片段并只重建这些片段；未修改片段及其
测量结果保持有效。

当前每片最多包含 64 个完整 flow leaf。结构事务发布方向已经规范化的
`DocumentTreeDelta`，区分孩子数组的插入/删除与父节点属性变化；undo 的
delta 反向、逆序生成。可证明局部的结构变化只重新规划相邻片，溢出时
平衡切分，删除后在相邻片内合并。片的首尾稳定 leaf/owner ID 用于修复
路径；未变 source 不重新散列。中间插入仍需更新导航向量的后缀 rank。

`Move`、编号变化、未知语义影响等操作仍通过同一个 planner 扩大失效范围。
这不是另一套整块渲染器：无论失效多大，输出与缓存单位始终是相同的
物理片段。实际 planner 节点访问、正文散列及导航写入均有工作量计数。

必须同时覆盖完整模型和虚拟化模型的构建、增量更新、物化与释放，不能
保留“旧整块渲染”和“新分片渲染”两个行为不同的引擎。

### 3. 精确几何与完整视觉对象分离

滚动位置和总高度来自持久几何索引。字形、显示文本、命中映射、行列表、
DirectWrite layout 和 paint plan 是可重建的视觉工作集。

精确测量时逐片段使用实际文本测量器，记录高度后释放临时视觉对象。
几何索引保存高度和边距，通过前缀和树提供位置查找及局部高度更新。
滚动时只物化视口、适量预取范围和交互需要的片段。滚过全文不能使已访问
片段的重对象永久增长。

这不会消除初次解析/精确测量的 O(文档大小) 工作，而是消除 O(全文)
重型布局对象的同时驻留。宽度、字体或影响度量的样式变化确实需要重新
测量；不应把颜色变化或光标移动扩大为全文重排。

初次精确测量在已有文档加载后台执行，并支持停止令牌与进度。测量器仍
使用真实 DirectWrite、相同字体、宽度和 display projection；不以平均
字符宽度估算文字。它不创建 D3D/D2D 设备，发布到 UI 的是纯几何和源
标识。当前主题 revision 或宽度变化仍按已有失效策略重新测量；本次没有
宣称消除了这些会影响整份文档的重新排版成本。

几何、准备失效计划与视口调度已经移到 `src/core/rendering`。几何索引的
Fenwick 累计值改为 double，位置查询使用 O(log N) lower-bound；平台接口
仍按 DirectWrite 的 float DIP 暴露位置。离屏回收保留测量高度，丢弃
layout、行列表、display/source map 和 paint plan。原生 owner 索引也按
稳定片 ID 移动复用，尾部新增一片不会重新登记整个列表的所有 owner。

### 4. 同一语义流的共享排版上下文

把一个 DirectWrite flow 拆成数片时，还必须继承原来的全流规则。
例如，一个 image、公式或大字号 span 会让原 flow 使用 DEFAULT 行距；
全流是否为同级 heading 也影响基础字体。每片记录实际产生的 intrinsic
摘要，语义根保留摘要计数；只有共享策略发生变化时，才重新测量该根的
其余片。强制使用的共享策略不回写为 intrinsic，移除最后一个特殊内容
后可以恢复原策略。多片流不重复施加“整个块至少一行高”的最低高度。

嵌套 raw math/Mermaid 的非浮动编辑预览仍属于语义根尾部。它的几何位置
与源片分离，视口需要时只重建当前编辑 owner 的一个片来取得预览。
后台不把预览的 DirectWrite 对象带回 UI。浮动 inline 预览保持原路径。
PDF 分页也以语义根的 keep-together 边界为准，人工切片不成为新的分页
依据。

### 5. 交互与资源边界

- 保存、复制、搜索、跨片段选择和历史从文档模型取得源内容；它们不要求
  所选全文的字形对象驻留。
- caret、命中和 IME 需要精确几何时，定位并物化所属片段。
- 普通文本的测量结果在重复物化后应相同。异步图片、公式或图示资源有
  独立生命周期；尚未获得真实尺寸的资源不能伪装为“已精确测量”。
- 真实高度变化时，以可见内容的位置补偿滚动。用户主动滚动、跳转和
  caret reveal 仍遵循原有控制顺序。
- 打印/PDF 在同一模型上顺序物化和释放；不能为了打印另造一棵编辑树。

## 浏览器经验及适用边界

Chromium RenderingNG 将 DOM/layout object、不可变 fragment、display
list 和 compositor 数据分开。fragment 是布局结果，语义节点不必与
布局片段一一对应；未受影响的布局结果可以复用。这支持 Folia 保留
语义块树而改变物理布局粒度的方向。

`content-visibility: auto` 可跳过屏外子树布局，但通常使用
`contain-intrinsic-size` 提供的占位高度，并在真正布局后记住实测尺寸。
它并不等价于从首帧就知道任意内容的精确高度。Folia 要保留精确滚动，
就应将“测量过”与“保留全部布局对象”分开，而不是声称无须测量就能
知道真实高度。浏览器也仍有 DOM/CSS/文本本身的内存成本。

参考原始资料：

- [Chromium RenderingNG data structures](https://developer.chrome.com/docs/chromium/renderingng-data-structures)
- [LayoutNG](https://developer.chrome.com/docs/chromium/layoutng)
- [Chromium inline layout](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/inline/README.md)
- [content-visibility](https://web.dev/articles/content-visibility)
- [CSS Scroll Anchoring Level 1](https://www.w3.org/TR/css-scroll-anchoring-1/)

## 验证合同

测试不能只断言“调用了增量 API”。低层计数要覆盖实际 root DFS、索引
写入、inline 扫描字符、render source key 和片段重建量，防止隐藏的
全文遍历被指标漏报。

必须验证：

- 首/中/尾列表项的输入、删除、Enter、撤销和重做，源、CST、NodeId及
  选区一致；不发生全文 parse/serialize。
- 长列表前插入块，不重新登记列表内部未改变的 parent/child 边。
- 普通、有序、任务、嵌套、松散列表及引用/标注容器跨片段后，编号、缩进、
  空白、边界装饰和 owner 映射保持一致。
- 物化、释放再物化得到一致内容；全选不导致全文 layout 驻留。
- 精确几何与朴素累计结果一致；局部高度改变后，下方位置和总高度正确。
- 真实 C++23 module 构建，核心测试不依赖 Windows，也不将模块展开成
  拼接头文件来伪造可移植性。
- Windows 的 DirectWrite、WinUI、IME和像素布局验证单列，不能以 Linux
  上的模型测试冒充已完成 Windows 视觉验收。

## 非目标与需明确保留的成本

本次处理“大量列表项落在同一个顶层块”的问题。单个超长段落、不可分割
的超大表格或图片仍有自己的最小布局工作量；任意字符切片会破坏双向文字、
字形塑形和断行，不能用这种方式宣称任何输入都具有常数布局时间。

块树、CST和精确高度索引仍需 O(文档大小) 内存。现有 children/navigation
向量在中部插入时仍可能线性搬移；必须区分轻量索引移动、复制整棵子树、
重新解析全文和重新布局全文，不将它们统称为“常数时间编辑”。
真正改变引用容器物理行前缀的操作，仍需记录可精确撤销的语法
元数据；本次没有将所有此类 history payload 都改成常数大小。

## 实施与测量记录

实现按设计、可移植工具链、物理分片、编辑索引、精确几何生命周期、
结构 delta、语义分页、原生呈现兼容及测量证据分别提交。完整复现指令、
同机新旧基线的原始 JSONL 和汇总脚本在
[`tools/document-scalability`](../tools/document-scalability/README.md)。
最初仍包含全结构扫描的测量结果单独保留，没有被后续数据覆盖。

本地使用真实 Clang 21 C++23 modules、libc++ 标准库模块、CMake 4.3.4
和仓库 fork 的 Boost.UT module。完整核心回归已有 821 tests /
539,461 assertions 通过；随后补充的稳定语义根与分页回归通过对应
fragment/print gate。它们没有把 `.ixx` 展开成头文件，也没有使用假的
字体测量器来宣称原生精度。

构建 WinUI Release 后，可在本地运行 13 项 DirectWrite 布局回归：

```powershell
./tests/app/run.ps1 -Configuration Release -SourceFilter test_document_layout_retention -TestFilter 'document layout*' -TimeoutSeconds 600
```

这些测试覆盖高度、视口资源驻留、滚动锚点、owner 索引、共享行距和编辑预览。
原生 10 MiB 用例会输出 working set/private bytes，并检查测量后没有 layout 常驻。

Windows 本地使用 MSVC 19.50 和自包含 WinUI x64 Release 验证。
完整核心回归通过 824 项，DirectWrite 布局回归覆盖上述 13 项。
滚动锚点在换行前后采用相同的 DirectWrite 命中测试坐标，并检查
连续重绘时的滚动位置。
