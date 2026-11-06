# Mermaid C++ 交接

## 当前检查点

- 2026-11-07：34/34 个图族均已通过基础 SVG 对照。Railroad 四种语法算一个图族，显式 `error` 和 `flowchart-elk` 不另计。该数字衡量图族覆盖，单个图族的语法、外观、布局组合仍在补齐。WinUI 宿主切换另行验收。
- 已提交 11 种主题（`8f3c9962`）、Railroad/Error/ELK 别名（`ba77b050`）、Block 多方向箭头（`0bf7c4ba`）、Venn 多集合与手绘（`5218ace0`、`fea69ba4`）、Flowchart Neo 和 HTML 字符引用（`daeb2dcd`）、Flowchart 手绘（`8dd5ce06`、`132290c8`）以及会话初始化字体优先级（`20784f03`）。本地通过 Railroad 16、Error 2、Block 20、Venn 32、Flowchart Neo 8、手绘 Dagre 6 和 classic Dagre 118 / ELK 12 张 SVG 对照。
- 公共入口、DirectWrite 测量、DLL 与 CI 基础覆盖已提交 `4af512ff`。实测 SVG：Event Modeling 14；Use Case 30（新增富 HTML/SVG 标签，`80ecdaf1`）；Agentflow 8；Wardley 12；Swimlane 14（反向泳道、Georgia、多行，`71649ee1`；路由另有 119 组差分）；State 32；Requirement 44；Flowchart 手绘 10、曲线 24、额外节点 12。HTML 标签使用实测 advance，SVG 使用字形边界，共用族无关的测量接口。
- `build/mermaid-relocation` 两次全量 CTest 的核心、DirectWrite、DLL 都通过，最近 153 秒，见 `build/mermaid-complete-ctest-final.log`。此后增量构建和上述定向对照已通过。编译最多四路，日志 `build/mermaid-complete-build.log`，原生 SVG `build/mermaid-complete-svg`。尚未推送 CI。
- Flowchart 53 种上游登记形状已有 25 种原生实现。人物/云形及共用局部几何（测量盒、可见范围、轮廓、裁剪数据）已提交 `2764169f`；最新对照文件 `build/mermaid-cpp/extra-shapes-flowchart-browser/original.json`。`swimlane_router` 的另外 11 种文档/圆弧/括注已通过 24 张隔离对照，正在从 `build/candidates/flowchart-doc-shapes/` 接入公共绘制，暂由该代理独占 Flowchart 共享文件。
- 其余候选位于 `build/candidates/flowchart-storage/`（`wardley_port`）、`flowchart-symbols/`（`agentflow_port`）。下一步先构建文档组并复验完整 SVG，再接入候选，保持同一布局/裁剪/绘制路径。`venn_complete` 另查已实现形状的手绘；`railroad_parser` 负责 Agentflow 可观察诊断。未验证的外部资源注册等仍不计入完成。

## 上一轮验证

- 2026-11-06。本地从 `4d71b92c` 继续推进，`folia::mermaid` 公共入口现有 28 个图族。根工程默认关闭 `FOLIA_BUILD_MERMAID`，WinUI 继续使用 Rust。切换需单独安排。
- 本地已提交 ZenUML 公共入口 `00a56ea2`、Class 默认 ELK `80445c35`、Requirement 解析与渲染 `acca239c` / `82a18683`、ER 解析与渲染 `a24ef49e` / `02e66776`。
- 以锁定的 Mermaid 12.2.0 浏览器输出为基准，ZenUML 5 张、Class ELK 10 张、Class Dagre 66 张、Flowchart ELK 12 张、ER 32 张和 Requirement 32 张通过。ER 和 Requirement 的样例均覆盖默认 ELK、显式 Dagre 和两套主题。Requirement 标签已覆盖纯文本，富文本排版和自定义 CSS 字体样式仍缺。
- State 的侧边注释与并发分区已提交（`ae58933a`、`c4c671a5`），解析 oracle 通过 13 个成功案例与 2 个语法错误；亮暗主题 24 张 SVG 对照通过。
- 第二批五个图族均已通过亮暗主题对照，共 46 张 SVG。Info 6 张、Tree View 12 张、Ishikawa 8 张、Cynefin 8 张、Venn 12 张。各图族独立提交，共用公共入口。Tree View、Ishikawa 和 Cynefin 共用 SVG 文本测量。
- 最终本地 Windows Debug 构建在 `build/mermaid-relocation`，最多四路。核心、DirectWrite 平台和 DLL 三套 CTest 通过，日志为 `build/mermaid-next-ctest-final.log`。基准产物在 `build/mermaid-cpp/*-browser`，原生 SVG 在 `build/mermaid-next-svg`。新增对照已写入 CI，验收结果为本地实测。
- 后续边界见 [图族覆盖](README.md)。WinUI 接入留待单独安排。

## 边界

1. `src/mermaid/frontend.{ixx,cpp}` 识别图族、整理配置及主题。各图族模块解析并持有类型化数据。
2. `src/mermaid/render_entry.{ixx,cpp}` 统一准备和分发。核心保持标准 C++23。布局与 SVG 写出使用图族模块和已有共享算法。
3. `src/platform/text/mermaid_measurer.ixx` 负责 DirectWrite 测量；`mermaid_renderer.ixx` 将平台测量结果传给核心。新图族平台测试需加入 `tools/mermaid-cpp/CMakeLists.txt`。
4. `src/platform/text/mermaid_host.ixx` 和 `tools/mermaid-cpp/native-renderer.{h,cpp}` 提供宿主与 C ABI。JS 仅供上游对照脚本使用。

## 复验

参见 [构建说明](README.md)。本地对照使用 `.reference/mermaid-js` 的 `1fb7869866dd619168d840291c9c54b4f129758b` 和锁定的 Oracle 依赖。平台测试通过 `FOLIA_MERMAID_SVG_OUTPUT` 输出 SVG，再运行同图族的 `compare-*-svg.mjs`。几何容差固定为 1.5 px，ELK 算子容差为 1e-4。布局使用实际文本测量。RoughJS 零粗糙度矩形允许控制点有差异，检查笔画几何、端点和绘制属性。

WinUI 切换另行验收预览、编辑刷新、主题、尺寸和 PDF 导出，随后才能移除 Rust 的 Mermaid 入口。普通 SVG 的规范化依赖独立保留。
