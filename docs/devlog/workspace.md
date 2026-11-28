# 工作区与标签经验

按症状查边界。每条都列出已确认的机制、保护它的当前测试，以及相似故障的第一检查点。提交链接是修复发生时的历史记录。实现以链接到的当前源码为准。

## 拖动标签或拆分窗口后，文档历史/选择串到另一视图

触发时，同一文档可以同时出现在多个编辑组/窗口。若把“文档”当成“视图”，拖动时重建 tab 会丢失组内选择与历史，或错误地把关闭动作应用到共享文档。修复把可移动的视图身份与文档身份分开，并让跨组/跨窗口传递保持原 view id。见 [工作区组模型](../../src/app-winui/workspace/WorkspaceEditorGroups.cpp)、[文档所有权](../../src/app-winui/workspace/WorkspaceDocuments.cpp) 与 [统一标签拖动提交](https://github.com/water2004/Folia/commit/237226da)。

应保持的规则如下。拖动迁移的是 view，内容、脏状态和历史归文档所有。先检查源/目标组的 selected view、文档 owner 和 view id，再检查传递后的焦点与关闭语义。覆盖见 [标签传递视觉测试](../../tests/app/visual/test_tab_transfer.cpp) 中 split、dirty view、shared document 与 detach 用例。验证限于 Windows UI 自动化。并非所有多窗口系统时序组合都覆盖。

## Agent 与编辑器打开同一临时预览，却出现两个标签

触发时，agent 引用文档和工作区打开动作各自创建了预览 tab，身份没有按共享文档协调。修复在 workspace 文档管理层共享预览，并在用户编辑或 pin 后转为保留标签。见 [WorkspaceDocuments](../../src/app-winui/workspace/WorkspaceDocuments.cpp)、[预览共享修复](https://github.com/water2004/Folia/commit/ac7cc87c)。

应保持的规则如下。预览属于工作区内的文档身份，不属于发起它的 UI 面板。排查重复 tab 时先核对路径/文档 id 和 preview/kept 状态。由 [workspace 文档测试](../../tests/app/test_workspace_documents.cpp) 及 [agent 预览视觉测试](../../tests/app/visual/test_agent_chat.cpp) 覆盖。视觉行为验证需要 Windows app runner。

## 从 Explorer 双击文件后，文件落在错误窗口或工作区

触发时，Shell activation 收到文件路径时，应用未先路由到拥有该文档/最近工作区的窗口。路径恢复和普通新开路径不一致。见 [窗口 activation 路由](../../src/app-winui/App.Windows.cpp)、[工作区文档查找](../../src/app-winui/workspace/WorkspaceDocuments.cpp) 与 [恢复正确工作区提交](https://github.com/water2004/Folia/commit/c858da00)。

应保持的规则如下。同一文档打开请求应送到其唯一 owner。无现存 owner 才按设置选窗口。先查 activation 参数、路径规范化结果和 owner 查找，再查焦点/显示。PowerShell activation 测试在 [test_activation.ps1](../../tests/app/test_activation.ps1)。这证明脚本覆盖的 Explorer/重复启动路径，不等同所有 Shell 启动环境。

## 拖入文件后，新标签没有落在指针下的编辑组

触发时，drop 事件只识别窗口级目标，没有把命中组传给异步打开流程。现在 drop handler 在接收时确定 editor group，再把它传到文件打开逻辑。窗口拖动目标的命中规则也共享该信息。见 [文件 drop 路由](../../src/app-winui/MainWindow.FileDrop.cpp)、[组拖动命中](../../src/app-winui/MainWindow.GroupDrag.cpp) 与 [按指针组打开修复](https://github.com/water2004/Folia/commit/930cada9)。

应保持的规则如下。命中目标应在异步操作前捕获，不能等 await 后用可能变化的 selected group。排查时先核对 drop 坐标→group 的映射和打开调用参数。 [标签传递视觉测试](../../tests/app/visual/test_tab_transfer.cpp) 有真实 storage file 拖放断言。需 Windows UI 环境。

## 点选标签后拖窗口，误进入系统 tear-out 或释放点不匹配

触发时，tab selection 会改变 native caption 的 data item。后续鼠标释放沿用旧拖动起点/宿主，原生系统把一次窗口移动误认成 tab tear-out。修复重选后再验证拖动起源，并按稳定 item 跟踪 native 手势。见 [标签传递控制](../../src/app-winui/MainWindow.TabTransfer.cpp)、[生命周期状态](../../src/app-winui/MainWindow.Lifecycle.h) 和 [起点重检提交](https://github.com/water2004/Folia/commit/776d3492)。

应保持的规则如下。gesture down/up 必须归属同一 host 和稳定 tab item。先检查 selection 后 caption hit region、data item 身份及 pointer release。测试位于 [test_tab_transfer.cpp](../../tests/app/visual/test_tab_transfer.cpp)，包含选标签后标题栏移动和 tear-out 防护。修复依赖 WinUI/native caption 集成，便携测试不能替代。

## 侧栏开合导致窗口跳动，阅读面右边缘闪空

触发时，侧栏过渡与窗口 resize 分别驱动，导致重复布局和阅读面宽度不连续。现实现把动画约束在窗口内，用 compositor motion 移动侧栏，并在状态转换执行一次编辑区 resize。见 [WorkspaceSidebarMotion](../../src/app-winui/workspace/WorkspaceSidebarMotion.cpp)、[窗口侧栏更新](../../src/app-winui/MainWindow.Workspace.cpp) 和 [动画提交](https://github.com/water2004/Folia/commit/64aba821)。

应保持的规则如下。一次开合只提交一个尺寸转换，阅读面在过渡期间持续铺满。先看 motion progress、resize 次数与右边界填充，而非仅检查动画最终位置。 [test_tab_transfer.cpp](../../tests/app/visual/test_tab_transfer.cpp) 覆盖单次 resize 和 compositor 边缘填充。性能结论仅针对这些 UI 回归观测，没有独立的帧时基准。

## 文件夹展开/刷新时，大目录行数暴涨或折叠后仍占用视图

触发时，直接把目录全量投影成常驻树节点会让大目录的 UI 节点和容器持续增长。当前 explorer 使用有界原生行、分页/按需展开，并在折叠时释放子节点。检查 [FolderExplorerController](../../src/app-winui/workspace/FolderExplorerController.cpp)、[Explorer view](../../src/app-winui/workspace/FolderExplorerView.cpp)，背景提交为 [bound native rows and stable lazy refresh](https://github.com/water2004/Folia/commit/0da4987f)。

应保持的规则如下。容器数应跟可见/当前页行数走，不随磁盘目录总项数无界增长。先观察节点数、分页切换后的 detached children 及折叠后的释放，再怀疑文件系统 I/O。大目录分页/折叠与懒展开分别见 [test_folder_explorer.cpp](../../tests/app/visual/test_folder_explorer.cpp)。现有测试造目录并测节点行为，不提供真实磁盘性能基准。
