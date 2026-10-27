# 当前开发交接

恢复/压缩后先实际读本文。数学开发入口：[architecture](architecture.md)、
[详细约束](native-raii-architecture.md)、[文件图](native-raii-files.json)、[验收](native-raii-acceptance.md)。
只复查新变化；ignored候选用 `rg --no-ignore`。

## 当前 checkpoint（2026-10-28，重载遮罩已修 / 跨窗口标签待实现）

- 当前授权任务两项：外部文件变更后一直显示“正在准备文档”；标签拖出成独立窗口、跨窗口移入。不开agents/CUA，不关闭用户窗口；native串行≤4，不自动改版本/安装。
- 重载根因：LoadDocumentAsync先setProgress(false)/documentChanged，随后LoadingGuard析构才清documentLoading，MainWindow同步查询IsLoading读到true；首次open外层刷新掩盖问题。修复显式完成状态后再发成功/取消通知，保留RAII异常退出及loadSerial校验；OpenFileAsync失败补结束进度通知。没有改CST、布局、数学引擎。
- 新增tests/app/test_document_reload.cpp及fixture观察UI回调时的IsLoading，覆盖监视器自动变更（纯文本/行内/行间公式）、手动刷新、失败保留原文、取消、异步open失败。旧代码5项19断言失败，修复后5项61断言通过；Debug应用构建通过，完整app120项17827断言通过。日志build/tag-layout/reload-{before,after,debug-build,app-all}.log。没有live本地构建/测试进程。
- 下一项跨窗口标签尚未修改产品：已确认当前WinUI提供CanTearOutTabs及4个原生tear-out事件；官方TabView/WinUI源码证据build/tag-layout/TabView.cpp。WinUI要求移动同一TabViewItem、管理多个Window，并用TabItemsChanged跟踪排序（tear-out模式不发TabDragCompleted）。必须转移原WorkspaceDocument/session（保留未保存、history/selection、source mode、viewport和math cache），更新控制器的窗口UI绑定且保留savedContentState/reloadState，不能重开文件或调用Attach重置存档基线。拟将window注册/生命周期归App，转移流程归专门MainWindow.TabTransfer.cpp，模型Extract/Insert归WorkspaceDocuments。需确认独立启动窗口跨进程的路由边界，不能默称已支持。
- 0.5.1已发布：修复3b265efe、版本e4b2dd4e（标签未改）；重跑CI40497361463/job122166085103全成功，Release已有Folia-0.5.1-x64.msi（53,681,537字节）和87字节sha256。最初CI40494779718在关窗测试0xc0000005/RVA0x45328，旧job无artifact；本地Release正常5次、空配置5次、立即关闭5次、Debug1次均通过，原始异常根因仍未确认。e23af71a已给后续CI增加失败dump/exe/PDB/log上传，不把重跑通过称作根因修复。
- 数学公式编号根因和修复已验：应用误将width=100%当100px并漏min-width，现以containerWidth/em确定外层viewport后归一化。3条原式/修正式与锁定MathJax4.1.3输出逐字符一致；aligned内tag原版也拒绝，需放end{aligned}后。回归4项138断言；证据build/tag-layout/{render.py,source.cjs,*svg,focused.log}。
- 旧架构/目录清理已提交：独立folia::markdown、math C header归模块、图片CPU/GPU所有权分离、入口归tests、移除过期工具和文档。基线core763/499901、platform84/4249、CommonMark652/1304、GFM1428/2878、math977/546948；不重复广泛审计。
- **未完成主题要求**：WorkspaceIcons.cpp的8个Fluent路径、WorkspaceChrome/FolderExplorer.xaml和MainWindow.xaml样式、core/rendering/theme.ixx默认值仍需归主题包；本轮不混入。
- 曾有4个无产品依赖的试验路径清理被自动审批拒绝（仅blocked by policy），未重试：build/tests/winui/Release/controller、build/generated/themes、build/debug_winui_test.py、tools/themes（若只剩__pycache__）。不绕过、不声称已删。此前回收13.60GiB及旧baseline worktree。

## 已确定的边界（仅有新证据才重查）

- 原生数学已接入Folia，通过SVG显示；保留MathJax模块职责，Common完成数学布局，Scene拥有结果，SVG只序列化。不恢复QuickJS、JS对象语义模拟或并行旧实现。未来D2D消费同一Scene，本次不做D2D像素对照。
- TeX profile为MathJax4的38包加mediawiki-texvc旧别名包；lang/rang等101定义已验，v4同名定义优先。正式Source fixture和源版本锁不可当临时文件删掉。
- 数学按请求加载字体，一次请求可以是批量；单个接口归批量。文档公式提前异步准备，滚动无同步IO，已离屏需求取消；编辑预览只在内存。通用文档流程不按“是否有公式”另起路径。
- 编辑器以统一块树、块内完整source/CST、单一selection和可逆事务为准（AGENTS.md）；图标仅呈现语义，不能反过来决定编辑行为。平时隐藏marker、激活时显示，删除数学定界符去掉两端并保留内容。
- 文件树左、大纲右；标签与标题栏同一行，单击预览、双击/编辑固定；无文件夹默认隐藏文件侧栏。控件图标采用完整矢量画布，不能退回裁切的PathIcon；滚动时钟此前已由用户确认修好，不无证据重查。
- WinUI正式controller测试使用Debug。Release自包含产品用真实startup测试；Release console XAML宿主0xc000027b是已知测试宿主限制，失败的manifest/identity/Application试验全部撤回，不能重复这条旁支或声称它通过。
- 开发资料说明现行设计/使用/验收；不新增历史归档目录。旧过程在Git历史中查。HANDOFF只保留当前停止点、必要约束、验证界限和下一步。
