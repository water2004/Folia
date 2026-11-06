# Mermaid C++ 交接

## 当前检查点

- 2026-11-07：34/34 个图族有通过的基础上游 SVG 对照（Packet、Pie 的基准运行于 jsdom）。Railroad 四种语法算一个图族，显式 `error` 和 `flowchart-elk` 不另计。独立 DLL 验证为 78/78（State 显式 Dagre），默认配置 33/34：上游默认 State 是 ELK，原生尚缺。`swimlane_router` 正补 State ELK，不能通过改默认值绕过。
- 已提交 11 种主题（`8f3c9962`）、Railroad/Error/ELK 别名（`ba77b050`）、Block 多方向箭头（`0bf7c4ba`）、Venn 多集合与手绘（`5218ace0`、`fea69ba4`）、Flowchart Neo 和 HTML 字符引用（`daeb2dcd`）、Flowchart 手绘（`8dd5ce06`、`132290c8`）以及会话初始化字体优先级（`20784f03`）。本地通过 Railroad 16、Error 2、Block 20、Venn 32、Flowchart Neo 8、手绘 Dagre 6 和 classic Dagre 118 / ELK 12 张 SVG 对照。
- 公共入口、DirectWrite 测量、DLL 与 CI 基础覆盖已提交 `4af512ff`。实测 SVG：Event Modeling 14；Use Case 30（新增富 HTML/SVG 标签，`80ecdaf1`）；Agentflow 8；Wardley 12；Swimlane 14（反向泳道、Georgia、多行，`71649ee1`；路由另有 119 组差分）；State 32；Requirement 44；Flowchart 手绘 10、曲线 24、额外节点 12。HTML 标签使用实测 advance，SVG 使用字形边界，共用族无关的测量接口。
- `build/mermaid-relocation` 两次全量 CTest 的核心、DirectWrite、DLL 都通过，最近 153 秒，见 `build/mermaid-complete-ctest-final.log`。此后增量构建和上述定向对照已通过。编译最多四路，日志 `build/mermaid-complete-build.log`，原生 SVG `build/mermaid-complete-svg`。尚未推送 CI。
- Flowchart 53 种上游登记形状已有 36 种原生实现。人物/云形 `2764169f`；文档/圆弧/括注 `324271ea`（经典 20、Neo 16、手绘 18，含原有用例）。ELK 实际边界与端口修正 `8160060d`，18 张对照通过。自定义虚线分段和手绘仅文字两项由 Sol 在 `build/candidates/sol-document-verification/` 和 `sol-geometry-review/` 验证候选。
- Agentflow 可观察诊断与元数据 `c0abef0c`，核心 7 项、10 张 SVG 通过；容器端点仍缺 ELK 路由。其他形状候选 `build/candidates/flowchart-storage/`、`flowchart-symbols/`、`flowchart-handdrawn-organic/`。下一步确认符号/存储与已发现的主题色、端点差异，再按组接入。State ELK 隔离构建通过 24/34 张，复合与并发布局、循环路由继续处理。
- Astra 的 Folia 接入补丁在 `build/candidates/folia-mermaid-integration.patch`，隔离工作区为同名目录。Sol 复核发现 usvg 丢弃 `foreignObject` 标签，以及 Info 缺省根尺寸导致内容框取代 400×100 视口。Astra 修订尺寸处理和标签保留用例。ER、Requirement、Use Case、Swimlane、Event Modeling、Venn 正改用已有测量结果输出普通 SVG 文字。接入验收还需默认 State、WinUI 预览和 PDF 回归。
- 当前工作区有两份 Flowchart 候选的组合修改，修复文档虚线分段和无轮廓手绘节点的文字边界。独立验证分别通过 24 张 SVG。组合构建撞到 State 的共享 ELK 文件处于编辑中，已请该负责人先恢复可编译状态。下一步统一构建并跑对应 Flowchart 定向对照，再按文件范围提交。

## 边界

1. `src/mermaid/frontend.{ixx,cpp}` 识别图族、整理配置及主题。各图族模块解析并持有类型化数据。
2. `src/mermaid/render_entry.{ixx,cpp}` 统一准备和分发。核心保持标准 C++23。布局与 SVG 写出使用图族模块和已有共享算法。
3. `src/platform/text/mermaid_measurer.ixx` 负责 DirectWrite 测量；`mermaid_renderer.ixx` 将平台测量结果传给核心。新图族平台测试需加入 `tools/mermaid-cpp/CMakeLists.txt`。
4. `src/platform/text/mermaid_host.ixx` 和 `tools/mermaid-cpp/native-renderer.{h,cpp}` 提供宿主与 C ABI。JS 仅供上游对照脚本使用。

## 复验

参见 [构建说明](README.md)。本地对照使用 `.reference/mermaid-js` 的 `1fb7869866dd619168d840291c9c54b4f129758b` 和锁定的 Oracle 依赖。平台测试通过 `FOLIA_MERMAID_SVG_OUTPUT` 输出 SVG，再运行同图族的 `compare-*-svg.mjs`。几何容差固定为 1.5 px，ELK 算子容差为 1e-4。布局使用实际文本测量。RoughJS 零粗糙度矩形允许控制点有差异，检查笔画几何、端点和绘制属性。

WinUI 切换另行验收预览、编辑刷新、主题、尺寸和 PDF 导出，随后才能移除 Rust 的 Mermaid 入口。普通 SVG 的规范化依赖独立保留。
