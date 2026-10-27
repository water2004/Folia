# 原生数学模块开发指南

Folia 使用 `import folia.math;` / `folia::math::Renderer` 完成 TeX 到 SVG 的转换。
生产路径不运行 JavaScript。CJS 工具只用于离线生成字体/实体数据、锁定原版行为和生成对照样例。

## 模块职责

```text
TeX 请求（单项也是批量中的一项）
  → input/tex：宏包、解析器栈、会话状态
  → core/mml_tree：请求拥有的数学节点与属性
  → output/common：字体度量、伸缩、排版与数学坐标
  → output/scene：拥有完整布局结果的不可变值
  → output/svg：SVG 结构、数值拼写与转义
  → Folia 的 SVG 显示路径
```

| 目录 / 入口 | 职责 |
| --- | --- |
| `src/math/math.ixx`、`math.cpp` | 公共 API、请求组织与错误结果 |
| `src/math/input/tex/default_packages.*` | 生产和测试共同使用的宏包组装 |
| `src/math/input/tex/<package>/` | 对应 MathJax 宏包的语法与算法 |
| `src/math/core/mml_tree/` | MML 结构、属性继承、复制与所有权 |
| `src/math/output/common/` | 唯一的数学布局实现，按原版 wrapper 职责组织 |
| `src/math/output/scene/` | 已完成布局的存储和冻结，不建立第二套 wrapper |
| `src/math/output/svg/` | 消费 Scene，不重新计算分式、根式、表格或基线 |
| `src/math/fonts/` | 固定字体数据与按请求加载的字形资源 |
| `src/math/include/folia/math/native_renderer_bridge.h` | DLL 的 C ABI；不属于另一个公共头文件模块 |
| `src/platform/`、`src/app-winui/` | 后台请求、取消、缓存、窗口与图形 API |

未来 Direct2D 后端消费相同的 Scene；当前 Folia 接入的是 SVG 后端。
不能为新后端复制数学布局。源码文件以实际职责命名，不用 native/new/typed 名称保留竞争实现。

## 使用和生命周期

`Renderer` 拥有一个 TeX 会话，调用必须留在创建它的线程。
`renderBatch` 接受 `RenderInput` 序列，返回每项独立的结果或异常；单项便捷接口委托相同路径。
调用者提供的 TeX 视图在本次调用中有效，返回的 SVG 和 Scene 则独立拥有结果。

持久状态仅包括宏、标签、编号等 TeX 会话语义及小型只读索引。
MML、Common wrapper、排版临时数据和已解码字形由本次请求持有并通过 RAII 释放。
字体需求在请求内批量合并，不把字形内容留在跨请求缓存中。
返回的 Scene 不借用解析器、MML、wrapper 或请求缓冲区。

保留 MathJax 原有的语法和数学算法职责，以原生类型、明确枚举和类型化回调表达。
不恢复 Context/ObjectRef、通用 JS Value、反射属性堆或人工释放协议。

## 版本和对照

原版与字体版本由 [scope.json](scope.json) 及各生成器的 Source lock 固定。
默认组装包含 MathJax 4.1.3 的 38 个语法包，另有
[mediawiki-texvc 别名包](mediawiki-texvc.md)；v4 已有定义保持优先。
第三方来源与许可证见仓库的 `THIRD_PARTY_NOTICES.md`。

- 具体家族的所有权和布局约束：[native-raii-architecture.md](native-raii-architecture.md)。
- 修改后的验证要求：[native-raii-acceptance.md](native-raii-acceptance.md)。
- 正式生成工具及输入：[tools/math-port/README.md](../../tools/math-port/README.md)。
- 当前工作停止点：[HANDOFF.md](HANDOFF.md)。

这些资料描述当前开发约束；历史构建日志、分工单和阶段通过记录不作为当前实现的依据。
