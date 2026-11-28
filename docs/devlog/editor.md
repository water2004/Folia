# 编辑器与源码保真经验

## 保存改写 inline 源码或光标跳位

格式标记、转义、实体、空白、链接形式或未闭合语法在格式化/保存后发生改变，通常是语义节点被误当成原文，或光标被换算成全文序列化坐标。InlineDocument.source 保存权威原文，CST 是逐字符无损视图。编辑只在当前 owner 上应用 TextEdit，再重解析该节点的 CST。光标位置使用容器 id 和 source offset。规则见 [块内原文](../../src/markdown/model/inline_document.ixx)、[格式测试](../../tests/editing/test_document_format.cpp) 和 [仓库编辑契约](../../AGENTS.md)。

## Enter 或跨块编辑破坏结构

嵌套列表/引用、空段落和表格边界会暴露“编辑字符串后全文重解析”与结构操作混用的问题。Enter 应拆分当前 block owner。跨块删除应在树上合并或移除节点，并记录可逆事务。不要依赖节点显示顺序猜父容器，也不要用换行字符代替结构。回归见 [结构编辑测试](../../tests/editing/test_document_structure.cpp) 和 [边界测试](../../tests/editing/test_document_boundaries.cpp)。

## 选择/剪贴板移动后行或 owner 丢失

跨块选区可能包含对象、不可见目的地、空边界和表格单元。若只按可见文本长度构造替换范围，就可能丢块或造成偏移。保留结构事务和原 owner，只替换实际变化的 source，并检查 selection 与 undo/redo。历史修复见 [9afc1e7b](https://github.com/water2004/Folia/commit/9afc1e7b) 和 [83acd5bd](https://github.com/water2004/Folia/commit/83acd5bd)。当前随机保真与 selection 回归见 [结构测试](../../tests/editing/test_document_structure.cpp)。

## 把外部编辑器行为当成 Folia 契约

对固定的 [MarkText commit `43bd8b7`](https://github.com/marktext/marktext/commit/43bd8b77795fb27b1a9512737c000f7362031ea0) 的研究显示，Muya 将段落、列表项和引用作为可拆分、移动的树节点。Enter 根据容器关系决定结构，selection 定位到内容节点及其局部偏移。可借鉴树操作方式。Folia 的 source/CST、语法和边界行为仍以本仓库约定与测试为准，也无需持续追踪该上游。见 [文档模型约定](../../AGENTS.md) 和 [结构回归](../../tests/editing/test_document_structure.cpp)。
