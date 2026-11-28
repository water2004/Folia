# 渲染与 WinUI 经验

这些回归常是坐标、排版度量或合成边界不一致。先找症状对应的层，再看下列不变量。不要靠改容差或补偿坐标掩盖边界错误。

## 字形边缘模糊或一个源像素被放大成多个设备像素

触发时，editor surface 的 Direct2D 尺寸/缩放路径和呈现时的 DIP/设备像素比例不一致，内容经额外采样。修复让 editor surface 按一设备像素对应一源像素呈现，并记录 DPI 相关资源状态。见 [EditorRenderResources](../../src/app-winui/editor/rendering/EditorRenderResources.cpp) 和 [呈现缩放提交](https://github.com/water2004/Folia/commit/84babbc6)。

应保持的规则如下。surface 逻辑尺寸、target DPI 和呈现缩放必须是一致映射。先比对实际截图中文字像素宽度与已知 DirectWrite 度量，再查 DPI 变更时 target 是否重建。 [设备像素视觉测试](../../tests/app/visual/test_tab_transfer.cpp) 测定呈现字形。它只覆盖测试机 DPI/渲染栈，不代表所有缩放比例和显卡驱动。

## DirectWrite 重排后，滚动位置或 viewport anchor 突然跳动

触发时，缓存布局/保留几何更新后，anchor 恢复使用了不同的块坐标基准，行重排时滚动锚点偏离。修复在 document preparation pass 中按稳定的 viewport anchor 恢复，并补齐 retention 行为。见 [文档准备 pass](../../src/app-winui/editor/rendering/layout/EditorDocumentPreparationPass.cpp)、[保留测试](../../tests/app/test_document_layout_retention.cpp) 与 [anchor 修复提交](https://github.com/water2004/Folia/commit/cce91eab)。

应保持的规则如下。viewport 恢复依据稳定内容位置，而非旧像素 top。先检查重排前后的 anchor block/offset、viewport 高度及 prepared layout cache 命中。当前测试验证布局保留语义。不能证明所有长文档和实时 DirectWrite 字体替换场景。

## 行距设置改变后文本行高、表格行距或滚动距离一起失真

触发时，把字体 line height 和相邻行之间的 interline spacing 当成一个量，造成文本度量、段落间距、表格定位共用错误参数。修复将两种量独立传递，覆盖 editor 与 chat 的排版、绘制和滚动。见 [文本布局引擎](../../src/app-winui/editor/rendering/layout/EditorTextLayoutEngine.cpp)、[样式表](../../src/app-winui/editor/rendering/EditorStyleSheet.cpp) 与 [分离 spacing 提交](https://github.com/water2004/Folia/commit/00f33af2)。

应保持的规则如下。line height 描述单行文字排版盒，interline spacing 描述行间额外距离。先追踪用户设置到 DirectWrite layout、块高度、绘制基线和滚动估算的单位/字段。 [文档布局 retention 测试](../../tests/app/test_document_layout_retention.cpp) 与 [chat 视觉测试](../../tests/app/visual/test_agent_chat.cpp) 检查相关路径。未覆盖所有字体/排版 profile 组合。

## 标题里的星号/下划线变小，或标题内公式字号与正文相同

触发时，inline syntax marker 沿普通文字字号排版，或 math inline 使用固定正文大小，没有继承当前 heading 的字体上下文。历史修复分别见 [标题标记字号提交](https://github.com/water2004/Folia/commit/a3b253f5) 和 [标题数学字号提交](https://github.com/water2004/Folia/commit/fc4c12b3)。当前样式/inline layout 在 [EditorStyleSheet](../../src/app-winui/editor/rendering/EditorStyleSheet.cpp)、[内联布局](../../src/platform/windows/editor/layout/editor_inline_layout.ixx)。

应保持的规则如下。标记 glyph 和 inline object 使用所在块的字号/行高上下文。先比较 heading semantic style、marker run 和 math object 的 resolved font size。 [inline presentation 路径测试](../../tests/app/test_inline_presentation_paths.cpp) 覆盖语义呈现路径。若要确认所有字体与 heading level 的视觉像素效果，需 WinUI 实际捕获。该 portable/source 测试不等于跨 DPI 视觉验收。

## 折行处 caret 落在错误行，表格指针/绘制边界不一致

触发时，最近 caret 位置和可见文本 hit 被混为一谈，表格指针映射及 painter 又各自计算边界。当前改动把 wrapped caret 的命中/呈现与表格 pointer 和 paint 路径协调，见 [EditorPointerController](../../src/app-winui/editor/interaction/EditorPointerController.cpp)、[表格 pointer](../../src/app-winui/editor/interaction/EditorTablePointerController.cpp)、[表格 renderer](../../src/app-winui/editor/rendering/EditorTableBlockRenderer.cpp) 及 [修复提交](https://github.com/water2004/Folia/commit/e137f4f3)。

应保持的规则如下。hit-test 返回的文本位置必须由同一实际 layout 几何解释，nearest caret 不能冒充 visible text hit。先核对换行行矩形、表格单元 content rect 和 pointer transform，再看 painter clip。 [inline presentation paths](../../tests/app/test_inline_presentation_paths.cpp) 加强此回归。当前覆盖以有界交互场景为主，不能推出任意缩放/RTL 文本均已验证。

## 鼠标滚轮跨过标签区域后，滚动速度/方向不连续

触发时，tab region 与阅读视图分别消费 wheel，边界处手势路由和滚动曲线分裂。修复统一区域路由，并让 chat 阅读滚动使用阅读区距离曲线。见 [滚轮路由提交](https://github.com/water2004/Folia/commit/18a27cba)、[滚动控制器](../../src/app-winui/editor/interaction/EditorScrollController.cpp) 与 [scroll state tests](../../tests/platform/test_editor_scroll_state.cpp)。

应保持的规则如下。一个 wheel burst 应连续进入当前命中区域的滚动策略，反向输入立即反向，不应残留另一条区域的累计值。先检查 pointer hit region、滚轮单位/累积值与边界状态。平台状态测试验证曲线/方向逻辑。真实触控板和不同鼠标设备仍需 Windows UI 实测。
