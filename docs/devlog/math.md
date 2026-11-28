# 原生数学渲染经验

## 重排后公式闪回或被挤压

窗口或侧栏宽度变化触发重排时，按精确宽度键缓存会把仍有效的 SVG 当作失效，公式短暂回退源码。普通公式可按 `reusableWidthFrom` 契约复用几何。依赖宽度的编号或折行公式，应在新结果到达前保留上一份可画结果。不要猜 TeX 是否依赖宽度，也不要缩放公式。当前回归覆盖重排复用和编号公式保留，见 [数学架构](../math/architecture.md)、[编辑器准备测试](../../tests/app/test_math_preparation.cpp)。历史修复见 [907d8a5a](https://github.com/water2004/Folia/commit/907d8a5a)（v0.9.0 系列）。

## 独立 SVG 丢线或颜色

Array 规则或框线可能在共享 Scene 中正确，却在 SVG writer 的表格分支丢失 fill、dash、cap 或宽度。根因是序列化没有完整写出已计算的 Paint。应直接传递 Scene 中已完成的绘制属性，不要在宿主拼字符串或重新布局。当前 writer 和原生 paint 对照见 [SVG paint 测试](../../tests/math/output/svg/native_source_svg_paint.cpp)、[表格 Source 对照](../../tests/math/output/common/table/source.cpp) 与 [验证契约](../math/verification.md)。历史修复包括 [620bc080](https://github.com/water2004/Folia/commit/620bc080) 和 [c64d4b2f](https://github.com/water2004/Folia/commit/c64d4b2f)。这些历史结果未在本次重跑。

## 取消后崩溃或请求间串扰

批量或异步渲染在错误和取消边界最容易暴露借用对象活得比所有者久的问题。stop token 和 deadline 属于当前请求。Renderer/session 必须在同一线程创建、调用和销毁。C ABI cancellation handle 要一直存活到调用返回，SVG/Scene 结果则自行拥有数据。把临时 MML、glyph body 或宿主句柄放进不受控全局缓存，会造成请求串扰或悬空引用。约束见 [数学架构](../math/architecture.md)、[请求生命周期回归](../../tests/math/input/tex/request_lifetime.cpp)、[取消集成回归](../../tests/math/integration/math_cancellation_g147.cpp) 和 [取消请求类型](../../src/math/cancellation.ixx)。

## 不同数学用例只在其中一种对照中失败

源码行为、原生语义、布局几何、SVG 属性/字节、栅格像素和 WinUI 交互覆盖不同边界。只验证“渲染成功”不足以证明输出与上游一致。如果 writer 和本地对照都省略了 paint，两者仍可能同时通过。应按改动范围选择独立上游见证、原生测试和真实样式/宿主验证，并标明未执行的检查。参见 [数学验证契约](../math/verification.md)。
