# 当前开发交接

恢复/压缩后先实际读本文。数学开发入口：[architecture](architecture.md)、
[详细约束](native-raii-architecture.md)、[文件图](native-raii-files.json)、[验收](native-raii-acceptance.md)。
只复查新变化；ignored候选用 `rg --no-ignore`。

## 当前 checkpoint（2026-10-27，公式编号修复 / 0.5.1）

- 当前任务：用户报告长公式的 `\tag{4.9}` 与内容重叠，以及 `aligned` 内 `\tag{4.1}` 不渲染；修好后明确要求发布0.5.1。不开agents/CUA，不关闭用户窗口；native串行≤4，不安装本机。
- 根因已确认：MathJax完整宽度SVG用 `width="100%"` 和CSS `min-width`；应用MathJaxSvgParser将100%误读为100px且丢失最小宽度。修复在应用SVG边界用请求containerWidth和em解析外层viewport，归一化前写成绝对宽度；保留内层SVG、原版transform和data-mjx-viewBox，不改原生数学布局。
- 第二条原式在原版MathJax同样报 `\tag not allowed in aligned environment`；正确写法把tag移至end{aligned}之后，不在parser中改写用户TeX。两条原式与修正后的第二条共3例，native与锁定MathJax4.1.3输出逐字符一致（读文件统一CRLF），证据build/tag-layout/{source.cjs,render.py,inputs.json,source-*.svg,native-*.svg}。
- 本轮Debug应用构建通过（build/tag-layout/debug-build.log）；新增tests/app/test_math_svg_viewport.cpp，4项138断言通过（focused.log），覆盖百分比/最小宽度、16/24px字号、窄/宽容器、单个/批量、归一化实际尺寸、普通inline片段、aligned错误。完整Debug app回归通过：115 项、17766 断言，exit0，日志build/tag-layout/app-all.log。本地Release自包含重建成功，正常配置5次及空Assets/default配置5次startup/close均通过，Debug1次通过（build/tag-layout/startup-{release,clean,debug}）。CI异常尚未定位；不能用重跑通过代替根因结论。当前无live本地进程。
- 修复3b265efe、版本/说明e4b2dd4e7ed2be398da4ace1917036764f6acfb6已提交；master与annotated tag v0.5.1原子推送成功。CI40494779718失败：Release编译成功，真实关窗测试0xc0000005，RVA0x45328；MSI未发布，旧job122157347806无artifacts。用户要求修CI。当前新增失败时上传startup dump及同构建exe/PDB/log，准备在master通过workflow_dispatch重跑既有v0.5.1，不改标签/产品版本。
- 0.5.0已发布：5b9b45b507287119357570ba195e4641870205c4；CI40453942062成功，Release含Folia-0.5.0-x64.msi（53,693,825字节）及sha256。不重打旧标签。
- 上一轮架构/清理已完成并提交：独立folia::markdown目标和38模块+2数据、math公共C header归模块、图片CPU/GPU所有权分离、test入口归tests、移除过期工具/过程文档，详见Git历史。不要重做广泛审计。
- 基线验证：core763/499901、platform84/4249、encoding5/5、Debug app111/17628、CommonMark652/1304、GFM1428/2878、独立Markdown3/49、math977/546948；Release真实startup成功。最新新增回归以本轮记录为准。
- **未完成主题要求**：WorkspaceIcons.cpp的8个Fluent路径、WorkspaceChrome/FolderExplorer.xaml和MainWindow.xaml样式、core/rendering/theme.ixx默认值仍需归主题包。本次公式修复不处理；不宣称已完成。
- 最后4个无产品依赖的试验路径清理曾被工具自动审批拦截（仅blocked by policy），未重试：build/tests/winui/Release/controller、build/generated/themes、build/debug_winui_test.py、tools/themes（若只剩__pycache__）。不改路径绕过、不声称已删。此前已回收13.60GiB及旧baseline worktree。

## 已确定的边界（仅有新证据才重查）

- 原生数学已接入Folia，通过SVG显示；保留MathJax模块职责，Common完成数学布局，Scene拥有结果，SVG只序列化。不恢复QuickJS、JS对象语义模拟或并行旧实现。未来D2D消费同一Scene，本次不做D2D像素对照。
- TeX profile为MathJax4的38包加mediawiki-texvc旧别名包；lang/rang等101定义已验，v4同名定义优先。正式Source fixture和源版本锁不可当临时文件删掉。
- 数学按请求加载字体，一次请求可以是批量；单个接口归批量。文档公式提前异步准备，滚动无同步IO，已离屏需求取消；编辑预览只在内存。通用文档流程不按“是否有公式”另起路径。
- 编辑器以统一块树、块内完整source/CST、单一selection和可逆事务为准（AGENTS.md）；图标仅呈现语义，不能反过来决定编辑行为。平时隐藏marker、激活时显示，删除数学定界符去掉两端并保留内容。
- 文件树左、大纲右；标签与标题栏同一行，单击预览、双击/编辑固定；无文件夹默认隐藏文件侧栏。控件图标采用完整矢量画布，不能退回裁切的PathIcon；滚动时钟此前已由用户确认修好，不无证据重查。
- WinUI正式controller测试使用Debug。Release自包含产品用真实startup测试；Release console XAML宿主0xc000027b是已知测试宿主限制，失败的manifest/identity/Application试验全部撤回，不能重复这条旁支或声称它通过。
- 开发资料说明现行设计/使用/验收；不新增历史归档目录。旧过程在Git历史中查。HANDOFF只保留当前停止点、必要约束、验证界限和下一步。
