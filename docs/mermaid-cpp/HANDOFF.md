# Mermaid C++ 交接

## 当前检查点

- 2026-11-07：锁定版本的 34 个图族都有通过的基础上游 SVG 对照。Packet、Pie 使用 jsdom；Railroad 四种语法合计一个图族。State 默认 ELK 已提交 `d8a150ad`，主工作区通过默认 ELK 34 张和显式 Dagre 32 张，ELK 最大几何偏差 0.5 px。独立 DLL 的 78/78 全图族验证用的是上一版 State 显式 Dagre；整组默认配置需复跑。
- ER、Requirement、Use Case、Swimlane、Venn、Event Modeling 使用实测结果输出普通 SVG 文字。对应提交 `4b3edae7`、`89c92b9b`、`95e2b2b5`、`0dea94ef`、`62c18bbe`、`a7c0af06`，对应上游对照 32、44、30、14、34、14 张。Venn 的连续 CJK 断行仍在补测，复用 DirectWrite 的 Unicode 断行机会。
- Astra 的 Folia 接入补丁为 `build/candidates/folia-mermaid-integration.patch`，隔离工作区为同名目录。在主分支通过 `git apply --check`。最新独立原生 DLL 与候选的 usvg 规范化器通过 24 项验收，含 Info 400×100 视口和亮暗主题的六图族可见字形。见 `build/mermaid-complete-winui-svg-gate.log`。下一步在隔离工作区编译并验证 WinUI 预览、编辑刷新、主题和 PDF。主分支桌面端仍使用 Rust Mermaid。
- Flowchart 已原生实现 53 种登记形状中的 36 种。文档虚线与无轮廓手绘文字边界 `84b3c7f4` 通过经典 20、Neo 16、手绘 24、额外形状 22 张对照。ELK 边界与端口 `8160060d` 通过 18 张。符号、存储、手绘形状分别有候选 `build/candidates/flowchart-symbols/`、`flowchart-storage/`、`flowchart-handdrawn-organic/`，需按组接入。
- 公共入口、DirectWrite、DLL 与 CI 基础覆盖 `4af512ff`，11 种主题 `8f3c9962`。Agentflow `c0abef0c` 的诊断和元数据已验证，ELK 容器端点还未覆盖。已验证的配置和样式范围以各图族基准为准。
- 最新构建目录 `build/mermaid-relocation`，日志 `build/mermaid-complete-build.log`，原生 SVG `build/mermaid-complete-svg`。核心、平台、DLL 最近一次全量结果在 `build/mermaid-complete-ctest-latest.log`，编译最多四路。尚未推送 CI。

## 边界

1. `src/mermaid/frontend.{ixx,cpp}` 识别图族、整理配置及主题。各图族模块解析并持有类型化数据。
2. `src/mermaid/render_entry.{ixx,cpp}` 统一准备和分发。核心保持标准 C++23。布局与 SVG 写出使用图族模块和已有共享算法。
3. `src/platform/text/mermaid_measurer.ixx` 负责 DirectWrite 测量；`mermaid_renderer.ixx` 将平台测量结果传给核心。新图族平台测试需加入 `tools/mermaid-cpp/CMakeLists.txt`。
4. `src/platform/text/mermaid_host.ixx` 和 `tools/mermaid-cpp/native-renderer.{h,cpp}` 提供宿主与 C ABI。JS 仅供上游对照脚本使用。

## 复验

参见 [构建说明](README.md)。本地对照使用 `.reference/mermaid-js` 的 `1fb7869866dd619168d840291c9c54b4f129758b` 和锁定的 Oracle 依赖。平台测试通过 `FOLIA_MERMAID_SVG_OUTPUT` 输出 SVG，再运行同图族的 `compare-*-svg.mjs`。几何容差固定为 1.5 px，ELK 算子容差为 1e-4。布局使用实际文本测量。RoughJS 零粗糙度矩形允许控制点有差异，检查笔画几何、端点和绘制属性。

WinUI 切换另行验收预览、编辑刷新、主题、尺寸和 PDF 导出，随后才能移除 Rust 的 Mermaid 入口。普通 SVG 的规范化依赖独立保留。
