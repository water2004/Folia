# 当前开发交接

恢复/压缩后先实际读本文。数学开发入口：[architecture](architecture.md)、
[详细约束](native-raii-architecture.md)、[文件图](native-raii-files.json)、[验收](native-raii-acceptance.md)。
只复查新变化；ignored候选用 `rg --no-ignore`。

## 当前 checkpoint（2026-10-27，目录与文档清理已完成）

- 用户当前要求：清理无用文件，根目录测试归tests，docs只放有用开发资料，不堆历史。原重构任务中的主题包归属要求仍待落实。不开agents/CUA、不关闭用户窗口；native串行≤4；版本0.4.1，不自动MSI/安装/推送/发布。
- 已提交：c6815be6废弃regexp工具；9a7d240d旧candidate patch；55ebb59f图片CPU/GPU所有权分离及math公共C header归src/math/include；f2de59dd独立folia::markdown库（原38模块+2数据文件，算法不重写，core单向依赖）；a1e267ad旧ignore例外；7046522e模块facade禁PCH。
- 已清理无用ignored构建产物13.60GiB+旧baseline worktree。当前app/core/参考源码/正式生成器依赖保留。明细build/cleanup-disk-{inventory,plan,result,summary}.json。
- 当前目录整理：6个batch入口归tests，WinUI runner为tests/app/run.ps1，console runtime为tests/support/windows_console_runtime.cpp；旧build_only、空drafts和已完成CST历史计划直接删除，未搬入历史目录。已更新CMake、工具、AGENTS和使用说明。
- 当前文档整理：103份无代码/结构化记录消费者的旧阶段报告和分工计划删除；其清单在build/cleanup-doc-{plan,result}.json。architecture.md改为当前开发指南，acceptance.md只保留正式验收规则；HANDOFF去历史流水账。详细数学约束、Source锁/fixture未删。
- 已验证当前代码：Debug应用构建cleanup-debug-build.log；新入口core763项499901断言、platform84项4249断言、encoding5项5断言、Debug app111项17628断言，均exit0。新入口CommonMark652项1304断言、GFM1428项2878断言、独立Markdown消费者3项49断言通过。日志cleanup-test-entry-*.log、cleanup-app-tests.log。新入口从D:/外部cwd调用仍正确定位仓库。
- 此前同产品代码验证：独立Markdown消费者3项49断言、数学977项546948断言；Release自包含构建markdown-release.log及真实启动markdown-library-startup.log/result.json通过（Ready/Success=true，exit0），只关闭自建测试子进程。当前未人工检查完整UI外观。
- **待完成主题要求**：WorkspaceIcons.cpp仍手写8个Fluent路径；WorkspaceChrome/FolderExplorer.xaml、MainWindow.Xaml有样式数据；core/rendering/theme.ixx与主题JSON重复默认值。产品尚未改。未接入的tools/themes生成器草稿已清掉，不能算主题完成。后续需确定主题包单一数据源、当前窗口的资源更新、MSI内置包替换/自定义包保留和切换测试；不要用全局可变主题或强制主题翻转修补资源更新。
- 目录整理已提交4f601e71；旧计划删除6d4c5bd3、旧阶段报告删除94a056bf。当前无live agent/build/test/诊断进程；候选应用仍为build/app-winui/bin/x64/{Debug,Release}/Folia.exe。md4c诊断因CommonMark无失败没有执行；新入口路径按同一仓库定位规则迁移。
- 最后4个未接入试验路径的删除被工具自动审批拦截（仅返回blocked by policy、无具体原因），未重试：build/tests/winui/Release/controller、build/generated/themes、build/debug_winui_test.py、tools/themes（若只剩__pycache__）。生产代码无依赖，保留不会进入MSI。不要声称这一批已删。
- 下一步：主题包归属任务仍未实现，按上面列出的实际硬编码位置继续；不重复已完成审计和Release宿主试验。此轮未构建MSI、安装、推送或发布。

## 已确定的边界（仅有新证据才重查）

- 原生数学已接入Folia，通过SVG显示；保留MathJax模块职责，Common完成数学布局，Scene拥有结果，SVG只序列化。不恢复QuickJS、JS对象语义模拟或并行旧实现。未来D2D消费同一Scene，本次不做D2D像素对照。
- TeX profile为MathJax4的38包加mediawiki-texvc旧别名包；lang/rang等101定义已验，v4同名定义优先。正式Source fixture和源版本锁不可当临时文件删掉。
- 数学按请求加载字体，一次请求可以是批量；单个接口归批量。文档公式提前异步准备，滚动无同步IO，已离屏需求取消；编辑预览只在内存。通用文档流程不按“是否有公式”另起路径。
- 编辑器以统一块树、块内完整source/CST、单一selection和可逆事务为准（AGENTS.md）；图标仅呈现语义，不能反过来决定编辑行为。平时隐藏marker、激活时显示，删除数学定界符去掉两端并保留内容。
- 文件树左、大纲右；标签与标题栏同一行，单击预览、双击/编辑固定；无文件夹默认隐藏文件侧栏。控件图标采用完整矢量画布，不能退回裁切的PathIcon；滚动时钟此前已由用户确认修好，不无证据重查。
- WinUI正式controller测试使用Debug。Release自包含产品用真实startup测试；Release console XAML宿主0xc000027b是已知测试宿主限制，失败的manifest/identity/Application试验全部撤回，不能重复这条旁支或声称它通过。
- 开发资料说明现行设计/使用/验收；不新增历史归档目录。旧过程在Git历史中查。HANDOFF只保留当前停止点、必要约束、验证界限和下一步。
