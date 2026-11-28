# Mermaid 经验

## 标签宽度随环境错误变化

CSS 字体族列表未经解析直接测量，或用字符数乘常数估宽，会让字体回退、Unicode、字号和折行改变最终节点几何。先用平台测量器解析出一个已安装字体，再进行 DirectWrite/Pango 实测。布局核心仍保持平台无关。当前路径见 [measured render](../../src/mermaid/api/measured_render.cpp)、[Windows 测量器](../../src/platform/text/mermaid_measurer.ixx)、[DirectWrite 测量](../../src/platform/text/dwrite_measurer.ixx) 和 [测量回归](../../tests/platform/mermaid_measurement.cpp)。

## 主题缺省错误或跨图污染

把未指定主题提前归一为 `default` 会覆盖部分图族自己的默认调色板。复用可变 ConfigSession，则可能让一张图的 frontmatter/init 配置影响下一张。保留“缺省”与显式 `default` 的区别，并为每次渲染创建独立会话副本。theme、look、layout 分别解析。见 [主题契约](../mermaid-cpp/themes.md)、[主题选项测试](../../tests/mermaid_cpp/themes/theme_options.cpp) 和 [主题 fallback 测试](../../tests/mermaid_cpp/themes/theme_renderer_fallbacks.cpp)。

## SVG 与官方对照通过，但浏览器行为仍不一致

原生 C++ 单测、完整 SVG 生成、官方 renderer 捕获/比较、真实字体栅格和 WinUI 交互分别验证不同内容。主题巡检的 11×34 单例组合只是 smoke matrix，不能代替完整图族语料的浏览器对照。报告应列明输入、上游锁定版本和实际执行的层级。修改上游 oracle 时，也要同步更新 SHA 锁与许可声明。见 [验收层级](../mermaid-cpp/regressions.md)、[来源锁](../../tools/mermaid-cpp/source-lock.json)、[许可声明](../../src/mermaid/THIRD_PARTY_NOTICES.md)。v0.9.0 固定历史见 [tag 源码树](https://github.com/water2004/Folia/tree/v0.9.0)。

## 新来源锁已更新，但 Oracle 仍写回旧位置

源文件搬迁时若只改 reader，生成/捕获脚本可能仍把结果写到旧路径。锁文件是工具读写的接口，不是静态文档附件。应一并检查所有 reader、writer 和许可声明链接，并保留被搬文件的字节散列。当前消费者见 [frontend oracle](../../tools/mermaid-cpp/frontend-oracle.mjs)、[source lock](../../tools/mermaid-cpp/source-lock.json) 与 [第三方声明](../../src/mermaid/THIRD_PARTY_NOTICES.md)。这是近期路径清理中发现的问题，本次没有重跑完整 Mermaid suite。
