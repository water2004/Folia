# 当前开发交接

恢复/压缩后实际读本文，只复查新变化。数学入口：[架构](architecture.md)、[所有权约束](native-raii-architecture.md)、[文件图](native-raii-files.json)、[验收](native-raii-acceptance.md)。ignored 候选用 `rg --no-ignore`。

## 当前 checkpoint（2026-10-28，0.5.3已发布）

- **0.5.3已正式发布**：版本/标签35e56265，master与v0.5.3已推送。现有CI40577471717/job122447644967全部成功（Release构建、真实startup/close、MSI、校验和、发布）。Release：https://github.com/water2004/Folia/releases/tag/v0.5.3 。MSI为53,722,497字节，已下载至build/releases/0.5.3并验证SHA256为2ca50713200c8f5f5769c35b172ff5dbfac07b15fe2e686500a9c9625a889a35，与发布附件一致。当前请求已完成；无待续构建/发布步骤，不移动标签。
- 本地提交：06fa7fb2重载、8d746851数学浮层、3409c654容错TeX语法、a53e526a主题、314f3e6b高亮/光标、344895cb会话转移、a28e5209源码历史、c42a924d原生标签跨窗口。已随0.5.3推送。
- 本轮五项全部实现：磁盘变更重载遮罩；数学高亮/括号；原字号长公式浮层/柔和阴影；平滑光标/渐变闪烁；标签拖出独立窗口/跨窗口移入。
- 最终全通过：core **764/500069**（tab-source-history-core-all.log），platform **84/4249**（tab-source-history-platform.log），controller **134/18718**（tab-transfer-app-final-all.log），真实WinUI **19/875**（tab-transfer-complete-visual.log）。日志均 build/tag-layout/。
- Debug构建 tab-input-focus-build.log 成功，候选 build/app-winui/bin/x64/Debug/Folia.exe。真实进程激活 tab-transfer-complete-activation.log 通过（Unicode/相对路径、同进程4窗口、显式独立进程、全部exit0）；startup正常关闭 tab-startup-final.log 3次、最终修改后 tab-input-focus-startup.log 1次均通过。没有手工鼠标拖动验证；Release/MSI已由本次CI验证。
- 无活跃agents、构建、采样句柄。不开agents/CUA，不关用户窗口，native串行最多4workers；测试只清理自己的进程。

## 本轮实现与已解决问题

- 重载先清documentLoading再发通知，RAII保留异常/取消清理和loadSerial校验；不是CST或数学引擎问题。失败保留原文等5项61断言已验。
- 数学浮层与正文共享CreateFlow/PositionMath/PaintMath，按窗口可用宽度原字号自然换行，**不缩放、不内部滚动**。明确SVG intrinsic/paint尺寸契约修复片段重叠；单个vector mask/shadow effect，7主题mathPreview提供柔和阴影。用户原式与锁定MathJax4.1.3完全一致（native-3/source-3.svg）。
- 独立folia::math_editor：共享Tree-sitter runtime、容错增量TeX语法，UTF32 API/UTF8 tree/RAII；仅词法/括号/left-right/环境，不复制严格MML parser或宏求值，不加载字体。UI只用CST数学source span和SourceDisplayIndex转UTF16，无dollar扫描/第二坐标。9容器、Unicode、不完整TeX、编辑撤销零全文操作已验；原生6/14839、TexScanner对照1/36、生成可复现通过。外层未闭合dollar未单独覆盖。
- 括号保留用户指定**淡背景+细边框**，不改下划线/选区色/蓝色强调；主题editor字段，明暗像素测试已验。光标共享EditorFrameScheduler；静止只用单次blink timer，移动/fade持续帧，滚动直接跟随offset，真实selection/IME不变，尊重系统动画。
- 跨窗口现行设计见 docs/workspace-windows.md。同一session/controller/TabViewItem移动，保留dirty/history/selection/viewport/math cache。Presentation/RebindPresentation只换窗口callbacks，不能重置savedContentState/reloadState；process唯一session id，同路径不同会话拒绝。TransferTab按值持shared_ptr，不引用即将erase的map元素。
- Native tear-out按args.NewWindowId找目标，WinUI负责拖动结束后空源窗口关闭，不提前Close。AppInstance异步重定向支持普通启动同进程多窗口；标准PropertySet按值带绝对路径，key隔离exe完整路径。App::Current经自实现IXamlMetadataProvider（Application为composable base，不能get_self<IApplication>）。
- 真关闭异常：晚到PointerExited裸this访问已释放surface，0xc0000005。统一WindowEvent弱owner门控、分发期间持强owner、Closed后丢弃，renderer/frame同样受生命周期约束；证据tab-close-{focused,stack-focused}/run-1。不在pointer controller堆释放补丁，不能冒称旧0.5.1CI已证明同根因。
- 源码Undo/Redo行id漂移：过粗的历史块投影丢未变行，minimal_source_replacement只更新真实变化范围。保持块事务、不加源码快照/旧行缓存；10轮exact selection/line id与零全文操作通过。
- CoreText重绑后若surface仍聚焦且HWND foreground，显式恢复context，不能等待不会重发的GotFocus。500段长文转移后pump200ms验证viewport。
- controller与visual **编译串行**，共用PCH VC145.PDB时并行会C1041。激活测试独立候选目录保留Folia.exe/PRI原名（hardlinks/junctions），改exe名会坏PRI key，不是IPC丢参。LLDB缺liblldb.dll不再尝试，startup已有DbgHelp实时栈。

## 已确定边界

- 数学原生SVG已接入；Common共享布局、Scene所有权、SVG仅序列化，未来D2D消费同Scene。不恢复QuickJS/JS对象语义模拟/并行旧实现。
- MathJax4的38包加mediawiki-texvc别名包；lang/rang等101定义已验，v4同名优先。正式Source fixtures/锁文件不可当临时文件删。编号percentage/min-width已修；aligned内tag原版也拒绝，应放end{aligned}后。
- 字体按一次请求加载，统一批量接口；提前异步准备公式，滚动无同步IO，离屏取消，编辑预览仅内存。通用流程不特判有无公式。
- 编辑遵守AGENTS.md统一块树、块内source/CST、单一source selection、可逆事务；正常编辑零全文parse/serialize。marker图标只呈现语义，数学定界符删除两端保留内容。
- 文件树左/大纲右，标签标题栏同排，单击预览双击/编辑固定，无文件夹默认隐藏侧栏；完整矢量画布不恢复裁切PathIcon。滚动时钟已修无新证据不重查。
- controller正式用Debug；Release产品真实startup。Release console XAML宿主0xc000027b是已知限制，宿主旁支已撤回不重复/不冒称通过。

## 独立待办与界限

- Mermaid暂停。主题外置未全部完成：WorkspaceIcons.cpp的8个Fluent路径、WorkspaceChrome/FolderExplorer/MainWindow XAML样式及theme.ixx默认值；本轮未扩展。
- 已验历史基线CommonMark652/1304、GFM1428/2878、math977/546948；不重复广泛审计。现行模块整理已提交，历史查Git，不新增过程归档。
- 历史0.5.1首轮CI关窗异常无artifact，根因未证实；重跑40497361463成功发布；e23af71a增加后续CI失败dump/exe/PDB/log。
- 4个历史清理路径被自动审批拒绝，未重试不绕过：build/tests/winui/Release/controller、build/generated/themes、build/debug_winui_test.py、tools/themes（若仅pycache）。
