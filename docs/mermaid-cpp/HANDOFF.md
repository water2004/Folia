# Mermaid C++ 交接

## 当前检查点

- 2026-11-07：锁定版本的 34 个图族都有通过的基础上游 SVG 对照。Packet、Pie 使用 jsdom；Railroad 四种语法合计一个图族。State 默认 ELK 已提交 `d8a150ad`，主工作区通过默认 ELK 34 张和显式 Dagre 32 张，ELK 最大几何偏差 0.5 px。Sol 独立验证 34 个图族默认配置，两种 State 写法和四种 Railroad 语法合计 76/76，SVG 规范化保留了逐项检查的 382 个文字元素。结果在 `build/mermaid-full-winui-gate/results.json`。
- ER、Requirement、Use Case、Swimlane、Venn、Event Modeling 使用实测结果输出普通 SVG 文字。对应提交 `4b3edae7`、`89c92b9b`、`95e2b2b5`、`0dea94ef`、`62c18bbe`、`a7c0af06`，对应上游对照 32、44、30、14、36、14 张。Venn 连续 CJK 断行复用 DirectWrite，`b40f4017`；Swimlane 对照补测实际文字范围，`bfe663f5`。
- Astra 的 Folia 接入补丁为 `build/candidates/folia-mermaid-integration.patch`，隔离工作区为同名目录。在主分支通过 `git apply --check`。最新独立原生 DLL 与候选的 usvg 规范化器通过 24 项验收，含 Info 400×100 视口和亮暗主题的六图族可见字形。见 `build/mermaid-complete-winui-svg-gate.log`。隔离 WinUI 和集成原生 DLL 均完成 Release 构建，`870c8ebf` 解决优化后的 COFF 节数限制。Release 两个 DLL 复验 76/76 与 24/24 通过，WiX 清单包含两个 DLL 和 17 份许可证及声明。桌面测试宿主在创建调度队列时返回 0x80040154，Astra 正在隔离复验预览、编辑刷新、主题和 PDF 绘制。主分支桌面端仍使用 Rust Mermaid。
- Flowchart 已原生实现 53 种登记形状中的 36 种。文档虚线与无轮廓手绘文字边界 `84b3c7f4` 通过经典 20、Neo 16、手绘 24、额外形状 22 张对照。ELK 边界与端口 `8160060d` 通过 18 张。符号、存储、手绘形状分别有候选 `build/candidates/flowchart-symbols/`、`flowchart-storage-integration/`、`flowchart-handdrawn-organic/`。存储候选有 64 张经典浏览器基准，尚未运行原生对照。
- 公共入口、DirectWrite、DLL 与 CI 基础覆盖 `4af512ff`，11 种主题 `8f3c9962`。Agentflow `c0abef0c` 的诊断和元数据已验证。ELK 容器直接连边仍缺失，已定位复合节点真实端口的默认位置与边界裁剪，候选 `build/candidates/agentflow-container-endpoints/candidate.patch` 已应用于工作区且可编译，定向测试在内部和外部同时连框的用例触发容器下标断言。已停止扩展验证，`swimlane_router` 正定位该最小用例。已验证的配置和样式范围以各图族基准为准。
- 最新构建目录 `build/mermaid-relocation`，日志 `build/mermaid-complete-build.log`，原生 SVG `build/mermaid-complete-svg`。核心、平台、DLL 最近一次全量 3/3 通过（133.62 秒），日志 `build/mermaid-complete-ctest-latest.log`；随后 Venn Unicode 增量平台 207 断言通过。编译最多四路，Agentflow 候选最多两路，WinUI 离屏复验最多两路。尚未推送 CI。

## 边界

1. `src/mermaid/frontend.{ixx,cpp}` 识别图族、整理配置及主题。各图族模块解析并持有类型化数据。
2. `src/mermaid/render_entry.{ixx,cpp}` 统一准备和分发。核心保持标准 C++23。布局与 SVG 写出使用图族模块和已有共享算法。
3. `src/platform/text/mermaid_measurer.ixx` 负责 DirectWrite 测量；`mermaid_renderer.ixx` 将平台测量结果传给核心。新图族平台测试需加入 `tools/mermaid-cpp/CMakeLists.txt`。
4. `src/platform/text/mermaid_host.ixx` 和 `tools/mermaid-cpp/native-renderer.{h,cpp}` 提供宿主与 C ABI。JS 仅供上游对照脚本使用。

## 复验

参见 [构建说明](README.md)。本地对照使用 `.reference/mermaid-js` 的 `1fb7869866dd619168d840291c9c54b4f129758b` 和锁定的 Oracle 依赖。平台测试通过 `FOLIA_MERMAID_SVG_OUTPUT` 输出 SVG，再运行同图族的 `compare-*-svg.mjs`。几何容差固定为 1.5 px，ELK 算子容差为 1e-4。布局使用实际文本测量。RoughJS 零粗糙度矩形允许控制点有差异，检查笔画几何、端点和绘制属性。

WinUI 切换另行验收预览、编辑刷新、主题、尺寸和 PDF 导出，随后才能移除 Rust 的 Mermaid 入口。普通 SVG 的规范化依赖独立保留。
