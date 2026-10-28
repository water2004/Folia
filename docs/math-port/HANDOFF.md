# 当前开发交接

恢复/压缩后实际读本文，只复查新变化。数学入口：[架构](architecture.md)、[所有权约束](native-raii-architecture.md)、[文件图](native-raii-files.json)、[验收](native-raii-acceptance.md)。ignored 候选用 `rg --no-ignore`。

## 当前 checkpoint（2026-10-28，发布0.5.4）

- 用户确认“ok了”并授权推送、构建、发布0.5.4。版本入口（CMake、MSI默认、RC、manifest、设置页、README）统一0.5.4；release-notes/v0.5.4.md已准备。只做版本/发布资料更新，沿用下列已完成的功能验证。
- 发布沿用.github/workflows/release.yml：master提交后推v0.5.4，CI构建self-contained MSI、真实startup/close检查、SHA256、GitHub Release。既有0.5.3标签不移动。
- 本提交后下一步推送master及v0.5.4，监看CI及下载校验正式产物，完成后在此更新run/commit/散列。当前无agent/构建句柄；不开agents/CUA、不自动移动鼠标、不关用户窗口，native串行≤4。

## 已完成的标签修复基线（9815cfcb，历史）

- 新反馈推翻“cloak已解决点击闪烁”的验收：按实际SDK SetWindowPos(SWP_SHOWWINDOW)测试稳定发生源窗失活；上轮Show(false)测试遗漏激活。before日志tab-activation-before.log。EnableWindow(false)仍未阻止失活，已撤回该尝试。
- 本轮修复：PreparedTabActivation UI线程局部CBT guard只在预备HWND上否决activate/focus，真正Transfer/Closed后RAII移除；保留WinUI拖动协议。另查SDK PopulateTabViewList包含AddButton，而DragTabWithinTabView只接受TabListView，边缘位置可刚合并又拆出；外部合并用同一list坐标边界、拒绝hidden/prepared窗口、转移后同步完成target layout。
- 验证：Debug成功tab-activation-guard-build.log；标签15项156断言通过tab-activation-final-transfer.log（含恢复正常窗口激活）；额外窗口定位/SetWindowPos成功返回检查1项6断言通过tab-activation-placement-check.log。完整visual的文件树6/95、chrome6/696通过；语言弹窗首次等待超时，定向重跑1/28通过tab-change-language-picker-check.log。
- Release/MSI成功tab-activation-msi.log，真实startup/close exit0。当前候选build/installer/bin/Folia-0.5.3-x64.msi，53,669,236字节，SHA256 e7076cebc04557585357faf964dc2aa0caee7dce40ab9c6e8e5c7bb051430271；邻接sha256已更新。未安装/推送/替换线上Release，版本保持0.5.3。
- 用户指出鼠标坐标模拟未命中并反对这种测试，已删除该测试/helper/临时运行脚本。超时不作为产品故障证据，**不再自动移动鼠标**；真实鼠标手势仍需人工复测，不声称已验收。不开agents/CUA、不关用户窗口，native串行≤4。
- 停止点：实现、测试及本交接一同本地提交（见HEAD）；无活跃agents/构建/测试句柄。下一步是候选人工拖动复测或按用户新反馈接续，不重跑已完成广泛检查。

## 上一轮已完成基线（历史，不替代当前checkpoint）

- 用户报告两个空窗口合并崩溃、合并后切标签闪窗、拉出空文档。本轮仅授权修复，版本保持0.5.3，不重新发布/移动既有标签；不开agents/CUA、不关用户窗口、native串行≤4。
- 新证据推翻此前“同TabViewItem迁移/NewWindowId可靠”的结论：实际DLL标记源码693f472aeaf89c6dff66256ee6fcd453e5d553b8的TabView.h/.cpp中，TabTearOutRequested.NewWindowId仅默认初始化从未赋值；TabViewListView缓存原parent且跨窗不刷新。SDK还在普通点击预建并Show/Hide窗口（官方issue10155）。源码证据build/tag-layout/TabView-installed.{cpp,h}、TabViewListView-installed.cpp、TabViewItem-installed.cpp。
- 已完成修复：TabItemsSource改为稳定boxed文档id，TabItemTemplate在每个窗口创建本地控件，不跨XamlRoot搬UI；原session/controller/history/cache仍原样转移。用args.Items查当前owner，每次gesture在DocumentTab记weak host、原move-loop的ExitedMoveSize清理，单页原窗口拖动也覆盖。预建窗口DWM cloak、不进switchers/noactivate，实际Transfer后才恢复展示。
- 初次测试的新代码startup崩溃已定向定位并修正：InputNonClientPointerSource与AppWindow一样不支持弱引用，auto_revoker会崩；改RAII强source/token订阅。tab-data-subscription-startup.log真实startup/close通过。不是用户原崩溃根因，不混淆。
- 已验：Debug最终构建tab-selection-boundary-build.log通过；完整visual **25项946断言**（tab-regression-final-visual.log、对应-stdout.log），3次真实startup/close全exit0（tab-regression-final-startup.log），真实多窗口激活/Unicode/相对路径/独立进程全过（tab-regression-final-activation.log）。包含未加载目标、空页合并/正常关闭源窗、同gesture反复合并拆出、click-only cloak/switchers、原会话/history/viewport、duplicate拒绝。
- 选中标签原生排序另有可复现失败：SelectionChanged→ActivateDocument→RefreshDocumentTabs反向修改集合，顺序无法一致（tab-selected-reorder-before.log超时）。已分离SelectDocumentTab（仅选择）与模型结构变化后的RefreshDocumentTabs；定向after 1项4断言通过，再跑上述全套成功。旧直接Transfer空页/未加载target本来也过，不能把它当原生协议已覆盖。
- 上轮代码22bea8fe及交接b0c4dccf仅本地提交。其MSI已被当前checkpoint候选替换，旧产物大小/散列不再用于交付。
- 用户真实安装版10:46崩溃记录0xc000027b、WER8000ffff；转储C:/Users/think book/AppData/Local/CrashDumps/Folia.exe.52828.dmp。缺stowed异常指针内存，未证明具体异常调用；不冒称确定根因。
- 0.5.3已正式发布且不改标签：35e56265，CI40577471717/job122447644967成功，Release https://github.com/water2004/Folia/releases/tag/v0.5.3 ，MSI53,722,497字节，SHA256 2ca50713200c8f5f5769c35b172ff5dbfac07b15fe2e686500a9c9625a889a35。历史全套基线core764/500069、platform84/4249、controller134/18718、visual19/875。

## 本轮实现与已解决问题

- 重载先清documentLoading再发通知，RAII保留异常/取消清理和loadSerial校验；不是CST或数学引擎问题。失败保留原文等5项61断言已验。
- 数学浮层与正文共享CreateFlow/PositionMath/PaintMath，按窗口可用宽度原字号自然换行，**不缩放、不内部滚动**。明确SVG intrinsic/paint尺寸契约修复片段重叠；单个vector mask/shadow effect，7主题mathPreview提供柔和阴影。用户原式与锁定MathJax4.1.3完全一致（native-3/source-3.svg）。
- 独立folia::math_editor：共享Tree-sitter runtime、容错增量TeX语法，UTF32 API/UTF8 tree/RAII；仅词法/括号/left-right/环境，不复制严格MML parser或宏求值，不加载字体。UI只用CST数学source span和SourceDisplayIndex转UTF16，无dollar扫描/第二坐标。9容器、Unicode、不完整TeX、编辑撤销零全文操作已验；原生6/14839、TexScanner对照1/36、生成可复现通过。外层未闭合dollar未单独覆盖。
- 括号保留用户指定**淡背景+细边框**，不改下划线/选区色/蓝色强调；主题editor字段，明暗像素测试已验。光标共享EditorFrameScheduler；静止只用单次blink timer，移动/fade持续帧，滚动直接跟随offset，真实selection/IME不变，尊重系统动画。
- 跨窗口现行设计见 docs/workspace-windows.md。同一session/controller/稳定数据item移动，UI控件留在各自窗口，保留dirty/history/selection/viewport/math cache。Presentation/RebindPresentation只换窗口callbacks，不能重置savedContentState/reloadState；process唯一session id，同路径不同会话拒绝。TransferTab按值持shared_ptr，不引用即将erase的map元素。
- 历史结论已被实际SDK源码推翻，不能按args.NewWindowId找目标；按gesture host跟随数据item，WinUI负责拖动结束后空源窗口关闭，不提前Close。AppInstance异步重定向支持普通启动同进程多窗口；标准PropertySet按值带绝对路径，key隔离exe完整路径。App::Current经自实现IXamlMetadataProvider（Application为composable base，不能get_self<IApplication>）。
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
