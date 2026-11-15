# 大文档内存：布局存储与源码投影

Folia 保留块树、完整块内 source、无损 CST 和精确滚动几何，通过减少
重复表示、无用对象以及屏外记录的固定大小来降低内存。现有
`MathSvgStore`、公式缓存的磁盘后备、视口读取和预览策略保持原样。
这些优化没有新增磁盘缓存，没有改变公式缓存的介质，也没有修改 CI。

本文衔接[大文档布局架构](document-scalability.md)。实现位于分支
[`perf/bounded-document-memory`](https://github.com/water2004/Folia/tree/perf/bounded-document-memory)，
已包含 master 的 `168426058a0135889ea7a9bd8807c7efc87be3e5`。

## 同一附件的测量结果

本轮附件 `markdown_stress_10MB_math.md` 为 10,485,760 字节。人工检查
只读取头部、中部和尾部合计 8 KiB；基准程序在进程内处理完整的新附件，
只输出尺寸、时间和计数，没有提交附件内容。此前列表压测附件未被打开。

基准使用实际 Markdown/core C++23 模块。它不运行 WinUI、DirectWrite、
公式 SVG 引擎或 GPU，因此下表是**可移植核心测量**，不能直接当作
Windows 应用从用户观察的 700+ MB 降到了某个数值。

同一机器、工具链、Release 参数和基准源文件，交替运行三个独立进程对。
基线生产代码为 `8675074f3fcf55ba7e724641c4a0317ef6c97f22`；优化代码为
[`6c086f8b`](https://github.com/water2004/Folia/commit/6c086f8b59fb885e4f46f60e2353b29b59de6bf5)。
数值取三个进程的中位数，MiB 按 1,048,576 字节换算。

| 阶段 | 基线在用堆 MiB | 优化后在用堆 MiB | 降幅 |
| --- | ---: | ---: | ---: |
| 解析和文档索引完成 | 317.48 | 254.56 | 19.82% |
| 所见即所得虚拟渲染投影完成 | 405.20 | 341.95 | 15.61% |
| 进入源码模式 | 645.28 | 520.16 | 19.39% |
| 源码显示投影完成 | 1,439.38 | 930.46 | 35.36% |

这里的在用堆是 glibc `mallinfo2().uordblks + hblkhd`，包含分配器开销，
不是所有对象请求字节的精确总和。完整流程的进程峰值 RSS 中位数从
1,399.41 MiB 降到 1,006.85 MiB；当前峰值发生在进入源码模式的过程中。
关闭文档后的在用堆约为 0.65 MiB，而分配器仍可能保留已经释放的页面，
所以关闭后的 RSS 不等于存活对象大小。实现没有调用 allocator trim，
也没有通过降低操作系统 working set 指标来制造内存收益。

附件产生 242,275 个块节点、207,665 个 inline CST 节点、118,663 个
富文本渲染片段和 390,592 个源码显示行。大量小对象的固定开销与重复
索引确实会超过原始文本自身，布局并非唯一的内存来源。

原始记录、编译环境、源代码指纹及验证范围位于
[`tools/document-scalability/memory-results`](../tools/document-scalability/memory-results)。
除了新附件，还包含 1 MiB 混合结构与独特公式编号的合成对照。它们只测量
核心数据结构，不测量公式缓存命中率或公式渲染耗时。

## 实现与保持的行为

### 屏外块只保留几何和索引元数据

旧 `EditorPreparedDocument::Block` 在每个片段中直接嵌入 display text、
多个 vector、caret 状态、layout/paint 指针以及
`optional<PreparedTable>`。释放元素后，这些容器对象和 optional 的
内嵌存储仍占据每条记录的空间；没有表格的块也承担表格容器的固定大小。

现在 `Block` 长驻保存实测高度、预览高度、owner、source/presentation
标识、失效状态和 intrinsic/applied flow context。绘制内容集中到
独立的 `VisualContent`，包括 DirectWrite layout、display/source 映射、
行数据、paint plan、图片、公式/图表预览和表格容器。

预测量仍逐片使用相同的真实布局路径。记录高度后释放整个视觉载荷，
屏外片段继续保留精确几何；总高度和滚动位置不退回字符宽度估算。
`Visuals()` 读取已释放块时返回共享的只读空状态，不为该块分配对象。
`EnsureVisuals()` 只在实际准备时创建载荷；已驻留且有效的布局继续复用。

每次冷准备增加一次载荷分配，预测量中的临时准备也承担这次分配和释放。
它没有增加全局测量次数，也没有改变宽度、主题、caret、completion 或
资源更新的失效规则。该常数成本及 Windows 进程的实际收益仍需原生测量。

原生回归要求 `sizeof(EditorPreparedDocument::Block) <= 128`，运行时输出
Block 和 VisualContent 的实际大小，并检查预测量后没有视觉载荷、
读取不物化、warm cache 身份不变、释放再物化后的高度/owner/flow 一致。
**128 字节是测试上限，不是本环境测出的 Windows sizeof。**

### 精确源码边界使用连续区间表示

代码块和公式块原来的 `content_to_source` 为每个字符边界分配一个
`size_t`。本附件有 5,235,696 个这样的边界，动态数组占 41,885,568 字节。
其中的连续位置可以直接用起点和长度表达。

`SourceOffsetMap` 保存线性或重复偏移的 run，第一段内嵌，额外段才分配。
位置高度离散、run 存储反而更贵时，退回 dense 表示。相同位置、HTML
实体、缩进造成的间隙和最后一个边界都保持精确值。该附件的 raw map
动态分配降为 0；map 对象自身仍有大小，并非没有任何存储。

顺序迭代缓存当前 run，保持 O(N) 遍历；随机位置和有序 map 的边界查询
直接定位 run。源码坐标仍然是唯一的编辑坐标，复制、裁切与重解析保持
独立值语义，没有引入借用旧 source 的悬空 view。

序列化时仍保留可写的 dense writer slots，以支持 HTML 片段交错输出和
first-emission-wins。只有 finalized owner map 才压缩，并逐 owner 释放
临时 dense 数组。源码投影索引按 run 转换原点，无须复制后逐字符减偏移。
公开的逆向查询仍按原顺序遍历，保留非单调 map、重复位置和 affinity
选择语义；已有要求单调 map 的编辑索引使用直接 run 搜索。

### 普通内容不分配未使用的附加对象

普通 `$...$` 的 delimiter 本来就是默认值。解析时不再为了重复写入该值
创建整个 `InlineCstSemantic`；非默认 delimiter 及需要语义数据的其他
节点照常保存。附件中的 semantic payload 从 79,109 个降为 5 个。
这是 CST 元数据优化，与公式缓存无关。

源码模式中，普通行不再为缺失的 language 分配 `RenderBlockSpecial`，
普通 Text item 也不再为默认可见性分配 `InlineRenderPayload`。
需要 language 的行仍保存它；文字内容、marker 显隐、样式、稳定行标识和
source 命中坐标均沿用原路径。局部编辑仍只更新受影响行。

### 解析临时缓冲在持久索引建立前结束生命周期

`parse_text` 按值接收 UTF-8 字符串并移动到 `ParseInput`，已有右值调用
不再多复制完整输入；左值调用仍取得独立输入副本。块树完成并持有自己的
source/CST/前缀后，提取尾部换行、元数据和诊断，再销毁 parser 的解码、
物理行和发现索引缓冲，最后构建持久文档索引、符号表与大纲。

索引交接处保留取消检查。持久对象不得引用已经销毁的 parser 缓冲。

## 性能与验证范围

新附件三个独立进程的时间中位数如下；原始记录同时保留所有波动样本。

| 核心阶段 | 基线 ms | 优化后 ms |
| --- | ---: | ---: |
| 解析和索引 | 1,576.16 | 1,533.22 |
| 初始虚拟投影 | 295.83 | 222.67 |
| 遍历并释放全部富文本投影片段 | 405.94 | 380.10 |
| 进入源码模式 | 3,932.00 | 3,077.22 |
| 源码显示投影 | 2,060.58 | 930.24 |
| 退出源码模式 | 187.91 | 63.07 |

首/中/尾位置的输入、撤销及两次增量投影均没有发生全文 parse、serialize
或文档索引扫描。三个位置的优化后中位数分别约为 0.061、0.035 和
0.012 ms。样本很少且没有 CPU 绑核，不应据此承诺每次按键的确定延迟。
三次基线解析本身在 1.48–2.62 秒之间波动。独特公式合成对照首次出现
源码进入耗时偏高后，额外补了两对交替样本，全部保留在结果中；三次中位数
为基线 538.38 ms、优化后 432.40 ms。

真实 Clang 21、libc++ `import std;` 和仓库 Boost.UT module 下，完整普通
核心 runner 通过 **949 tests / 680,405 assertions**。另行编译运行的
深层序列化 runner 通过 **2 tests / 2,005 assertions**，覆盖 50,000 层
引用和 1,000 层嵌套列表的精确文本及每个叶节点映射。

完整核心 runner 包括现有的编辑、源模式、历史、Unicode、渲染分片和几何
测试。新增回归覆盖连续大 map、随机有序映射的 dense oracle、CRLF、
HTML 实体/非单调输出、affinity、独立复制，以及源码行无用对象的释放。
普通 runner 不包含独立的官方 conformance/generated pathological 全套。
源码命中测试中的固定宽度 measurer 只验证坐标映射逻辑。

Windows/WinUI/DirectWrite 未在本环境运行。原生精确几何、滚动锚点、
显示一致性和实际打开/滚动耗时需要运行下面的应用测试；Linux 核心结果
没有替代这项验证。

## 复现

按[可移植核心工具链](../tests/libraries/core/README.md)配置，增加
`-DFOLIA_BUILD_SCALABILITY_BENCHMARK=ON`，然后运行：

```sh
cmake --build build/core-portable --target FoliaCoreTests FoliaMemoryProfile --parallel 4
build/core-portable/FoliaCoreTests
build/core-portable/FoliaMemoryProfile --input /path/to/markdown_stress_10MB_math.md
build/core-portable/FoliaMemoryProfile 1
build/core-portable/FoliaMemoryProfile 1 unique
python3 tools/document-scalability/summarize_memory.py
```

基线可使用另一个 checkout，通过 `-DFOLIA_SOURCE_ROOT=/path/to/baseline`
编译生产模块；基准源仍来自当前入口，因此两边使用同一程序。本轮基线
没有生产代码兼容补丁。独立深层测试的既有 CMake target 为
`FoliaDeepSerializerTests`。

Windows 从仓库根目录恢复依赖、构建 Release 应用后运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\setup.ps1
powershell -ExecutionPolicy Bypass -File .\build_app.ps1 -Configuration Release -SelfContained
powershell -ExecutionPolicy Bypass -File .\tests\app\run.ps1 -Configuration Release -SourceFilter test_document_layout_retention -TestFilter "document layout*" -TimeoutSeconds 600
```

测试使用实际应用对象、模块/PCH、运行时和 DirectWrite，日志位于
`build/tests/winui/`。同一文件包含输出 working set/private bytes 的
10 MiB 合成列表用例；它不自动等价于本轮数学附件的应用内存结果。
`tests/platform/run.bat` 不包含该应用层测试文件。
