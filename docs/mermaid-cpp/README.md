# Mermaid C++ 重构现状与接手指南

本次大量提交服务于一个目标：使用 C++23 重构 Folia 使用的 Mermaid 依赖，逐步以原生解析、布局和 SVG 输出替代现有 Rust 渲染依赖。不是翻译 Rust 后端，也不是实现 Mermaid 的全部浏览器生态。生产应用目前仍使用 Rust；本 PR 不包含最终依赖切换。

## 已完成并验证

行为依据是 Mermaid 12.2.0（`1fb7869866dd619168d840291c9c54b4f129758b`）及锁定的实际算法依赖，源码记录见 `source-lock.json`。原生代码不包含 JavaScript 运行时。

- 共用前端：源码预处理、frontmatter/YAML、指令、配置合并与会话隔离、图族识别、default/dark 主题以及请求级错误处理。
- 公共原生入口已覆盖 20 个图族：Packet、Pie、Flowchart、Sequence、Class、Quadrant、Radar、XY、Sankey、Treemap、Journey、Timeline、Gantt、Git graph、Mindmap、Block、Kanban、C4、Architecture、State。这里指约定案例和功能包完成，不表示上游所有语法与配置组合均兼容。
- Flowchart：经典形状、复合分组、样式、HTML/Markdown 标签、换行和文字测量；Dagre 路线与当前六例默认 ELK 路线已贯通。ELK 保留输入顺序、分层、交叉优化、端口、坐标、正交连线、复合图和组间距。
- Sequence：参与者、消息、激活、生命周期、注释及嵌套控制区域。Class：成员、关系、命名空间、注释、接口和自环；当前公共入口仍需显式 Dagre。
- 数据与时间图：保留各自解析与布局。包括 XY 坐标轴，Sankey 流带与渐变透明度，Treemap 分区，Gantt 日期/时轴，Git 分支/合并/提交标记，Journey/Timeline 标签，Block 嵌套网格，Kanban 卡片元数据。
- Mindmap 使用原版 CoSE；Architecture 使用独立版本的原版 fCoSE、约束及两次布局，并输出服务图标、组框、边、箭头和标签。State 已贯通 classic/Dagre 的基本状态、别名、转移文字、方向和复合状态案例。
- Windows 测量使用 DirectWrite。原生 DLL 返回拥有自身内存的 UTF-8 SVG、尺寸或错误；宿主承担编码、字体、队列/缓存等平台责任。
- ZenUML 已完成固定五例的独立解析、类型化数据、横向约束求解、递归纵向布局及 SVG；四种配置的原生/浏览器对照通过，但尚未接入公共入口，不能算作已可调用的第 21 个图族。

验证依据：[Windows 四配置全部通过](https://github.com/zhao0349/Folia/actions/runs/40795935250)，代码基线 `7b27d21059e6ad15eda6fab1d6f1189e35213405`。包括 standalone/integrated Debug/Release、原生及平台/宿主测试、原版观察与 SVG 对照。之后挑选到贡献分支的运行代码与该基线一致。

文字、绘制属性和图元数量保持严格比较；测量几何使用既有 1.5 单位边界，ELK 原版坐标使用 1e-4 数值精度。Mindmap 分开验证相同测量输入的布局及真实字体输入下的绘制，不能把不同输入的力导向收敛强行当作一致。

## 尚未完成

| 工作包 | 当前状态 | 下一步与退出条件 |
| --- | --- | --- |
| ZenUML 公共接入 | 五例 native/SVG 已过；公共接缝是候选补丁 | 注册官方外部 detector，接 `render_entry`、平台与 ABI；Windows Helvetica 的实际映射需与已验收 Arial 测量一致；通过同五例公共入口和请求隔离验证。 |
| Class ELK | 五例原版布局、最终端点标签观察及九列 fixture 已准备；后处理/共享接口未完成 | 提取共享 measured ELK scene，接端点标签避让和 Class painter，再验证五例、双主题十张 SVG。不得直接把未接缝模块加入自动 glob。 |
| ER | 八例 DB、两例错误和双主题 16 SVG 原版采样完成 | 实现类型化实体/属性/关系 DB，再按原版测量布局、端点标记与 SVG 接公共入口；先复用已需的 Dagre，不新增算法替代方案。 |
| 其他已保留需求 | Requirement 等仍未完成；State notes/concurrency/其他外观及更多既有图族行为未验收 | 先核对 Folia 实际需求和当前调用，不因上游支持就扩充范围。未获新授权前不从旧队列开启新图族。 |
| WinUI 与生产切换 | 仅有候选版预览、保存、主题和 PDF 的部分观察 | 完成新原生版本真实应用调用链、编辑刷新、尺寸、主题、PDF、GPU/DPI 等项目必要验收，再讨论切换 Rust 依赖。不能用静态 SVG 或 CI 代替真实应用验收。 |

`candidates/2026-10-28` 只保留未接入的 Class ELK、ZenUML 接缝及来源候选。不要重新应用已并入代码的旧 native-entry 补丁。其他开发者的历史归档仅供选取独立成果，不能整体覆盖当前已验证实现。

## 建议分工与执行顺序

1. **集成负责人**独占 `render_entry`、平台/ABI、CMake、工作流和 source-lock；先落实每个工作包的输入、返回值、测量接口及文件所有权。
2. **ZenUML 负责人**只收尾公共入口与 detector，保留独立语法树、横向约束和递归 Block 布局，不改用 Sequence 布局或按例硬编码坐标。
3. **Class ELK 负责人**完成共享 scene、端点标签后处理和 painter；与 Flowchart 共用算法层，但不可破坏已验收的六例 Flowchart。共享文件交由集成负责人落地。
4. **ER 负责人**从现有八例语法和 16 SVG 开始；先 parser/DB，再测量、布局、绘制，最后接入口。历史候选的未闭合属性块、旧入口签名必须核对，不能直接移植。
5. 每包只做代表性成功、必要常见失败和受影响回归；检查通过即小批提交，更新现状并释放所有权。完成这些收尾后，才由维护者确认下一批需求或宿主验收。

## 使用智能体 / vibe coding 的建议

- 开始先读本 README 与当前 HANDOFF，核对分支、HEAD、未提交改动、CI 和其他开发者所有权；不能把旧聊天中的 next 当作现状。
- 给每个智能体明确功能包、消费者、允许文件、依赖、固定案例、退出条件和交付格式。共享接口只保留一个写入负责人，独立模块可并行。
- 先观察锁定原版，再实现数据模型与原算法；文字必须真实测量。禁用字符数估宽、逐案例坐标补丁、放宽容差掩盖错误和另造通用布局引擎。
- 子任务交付应包含基线、改动文件、接缝补丁、实际执行的验证及仍缺的验收，不能只说“完成”。恢复上下文时先读记录，避免重复全面审计。
- 一次汇总同批失败后修复，保留原始日志；独立 SVG 检查即使前项失败也应给出结果。异常进程退出先停止扩大验证并定位。
- 不扩大到无关模块或假设性极端情况。当前收尾范围之外的功能需维护者重新确认。

## 构建与交付约定

使用 C++23 modules 和 `import std;`；核心不依赖 Windows API，平台测量放在 `src/platform`。测试使用仓库的 Boost.UT module 与 console runtime。核心/Windows 编辑测试分别使用 `build_test.bat`、`build_platform_test.bat`，独立 Mermaid 验证见 `tools/mermaid-cpp/CMakeLists.txt` 和 `.github/workflows/mermaid-cpp.yml`。

保留 Mermaid/MIT、ELK/EPL-2.0、D3 ISC/BSD、Cytoscape、RoughJS 和 ZenUML 等实际来源的许可证及 source-lock。生成物与恢复包留在忽略目录。源码准备脚本不代表产品依赖 JavaScript。

本 PR 只提交原生重构与验证成果，不自动合并、不发布、不切换生产依赖。
