# 原生数学模块验证要求

本文件规定修改后如何验收。一次旧构建的通过记录不代表后来代码通过；
具体当前结果和局限只记录在 `HANDOFF.md`，不在这里累积流水账。

## 架构和依赖

- 修改落在 MathJax 对应职责的现有 owner 中；新文件需有真实消费者和独立职责。
- 只有 Common 计算数学布局，Scene 保存完成的值，SVG 只序列化。
  宏包需要的子树度量复用同一 Common 实现；将来 D2D 也消费同一 Scene。
- 不出现 Context/ObjectRef、JS 反射对象协议、通用 Value 调用或人工对象退役机制；
  要检查传递依赖和实际所有权，不能只检查名字。
- 数学模块保持平台独立，生产模块使用 C++23 和 `import std;`。
- 修改构建注册时同时检查真实调用和源码中已无用途的旧实现。

## 所有权和会话

- MML 创建、链接、移除、替换、移动、克隆以及属性 own/default/inherited 优先级均有测试；
  节点不能成为自己的祖先，也不能错误引用另一个文档的节点。
- 正常返回、TeX 错误、异常和取消后，请求拥有的节点、wrapper、DOM 和临时字形恢复基线。
  使用真实生命周期/分配观测；不能靠放宽计数或重复清空持久容器掩盖所有权问题。
- 重复混合 Scene/SVG 请求必须没有逐请求增长。RSS 仅作为辅助证据，不能替代活对象检查。
- 宏、编号、标签、引用等有意持久的会话状态单独与原版对照。
  SVG/Scene 在后续请求、取消和 Renderer 销毁后仍然有效。
- 字形资源按请求加载，一次请求覆盖一个批次；批次内部复用，不在请求之外保留解码轮廓。
  观察批量的实际耗时和峰值占用，不能用常驻全部字体换取速度。

对应测试包括 `tests/math/request_lifetime.cpp`、`native_tex_session.cpp`、
原生字体测试和各 Common/Scene 家族测试。

## 与原版对照

- 参考输入来自固定版本的 MathJax、字体和独立 Source witness，不能由待测 C++ 算法生成期望值。
- 保留原有参数范围、分支、错误和 Unicode 输入；不以几个视觉样例代替完整家族对照。
- 比较包注册表、宏定义优先级、字符/定界符和 inline/display 结果。
  原版组装与别名扩展见 `mediawiki-texvc.md`。
- 比较完整 SVG 的结构、顺序、属性、数值拼写、资源顺序和内容。
  节点家族另检查布局/坐标；Folia 当前 SVG 接入不要求额外的 D2D 像素对照。
- Scene-only、SVG、单项和批量都要覆盖，并验证不同选项、缓存模式和相邻请求状态。
- 变更 fixture 前先复跑独立生成器，保留版本/哈希锁；未经解释的差异不能重录为新期望。

主要用例在 `tests/math/`，参考数据在其 `fixtures/`，离线工具见
[工具入口](../../tools/math-port/README.md)。

## Folia 接入

- UI 线程不做 TeX 排版或同步 SVG 文件读取；后台请求必须支持取消和文档代次隔离。
- 已离屏的公式不继续占据读取/显示请求；快速滚动不会让低价值回调淹没输入队列。
- 文档公式提前异步准备，编辑预览仅使用内存；不要另造“无公式文档”的特殊流程。
- 取消、窗口/文档关闭、重新打开、设备资源重建以及旧请求迟到结果均需覆盖。
- 控制器和缓存测试在 `tests/app/`；平台适配测试在 `tests/platform/`。

## 执行入口

使用已按仓库配置好的 CMake 构建目录，数学测试可单独构建和执行：

```powershell
cmake --build build/core-release --parallel 4 --target FoliaMathTests
.\build\core-release\FoliaMathTests.exe
```

核心、平台、编码、CommonMark/GFM、WinUI 和独立 Markdown 消费者的入口在
[tests/README.md](../../tests/README.md)。WinUI controller 使用已重新构建的 Debug 应用对象。
Release 自包含应用以真实入口的 startup 测试验证部署；当前 Release console XAML 宿主
存在初始化限制，不能将它的失败说成产品通过，也不要反复修改产品去适配该宿主。

原生构建串行执行，最多四个编译任务。Windows console 测试链接
`folia_console_test_runtime`，错误输出到 stderr，不弹 CRT 对话框。
异常退出后先停止宽测试，用有界的相关用例定位；编译成功、静态检查和只跑原版生成器
都不能冒充原生运行通过。按变更选择必要回归，记录实际退出码和验证边界。
