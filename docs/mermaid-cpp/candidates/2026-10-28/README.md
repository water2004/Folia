# 待接入候选补丁说明（2026-10-28）

本目录保存尚未合入主编译树的 **Class 默认 ELK 布局** 与 **ZenUML 公共渲染入口** 候选补丁及配套元数据。各任务的完整接入步骤与验证方法见上级目录的 [`HANDOFF.md`](../../HANDOFF.md) 与 [`README.md`](../../README.md)。

## 文件清单

| 文件名 | 内容说明 | 后续处理步骤 |
| --- | --- | --- |
| `class-elk-shared.patch` | 在 `third_party/mermaid-cpp/src/flowchart_elk.{ixx,cpp}` 中提取 `ElkMeasuredScene` 与 `layout_measured_elk_scene`，供 Flowchart 与 Class 共享 ELK 测量场景与端点标签输出。 | 先于 `class-elk.patch` 应用，确认既有 Flowchart ELK 6 个测试案例不回归。 |
| `class-elk.patch` | 新增 `third_party/mermaid-cpp/src/class_elk.{ixx,cpp}`、9 列基准数据 `tests/mermaid_cpp/fixtures/class-elk.tsv`、单元测试 `tests/mermaid_cpp/class_elk.cpp` 及 `tools/mermaid-cpp/class-elk-{cases,oracle,browser-probe}.mjs`。 | 在 `class-elk-shared.patch` 之后应用，随后在 `class_svg.{ixx,cpp}` 与 `render_entry.cpp` 中接入 `layout: elk` 渲染分支并验证 5 案例 × 双主题共 10 张 SVG。 |
| `zenuml-public.patch` | 将 `zenuml` 接入 `frontend.cpp`、`render_entry.{ixx,cpp}` 与 `src/platform/text/mermaid_renderer.ixx` 的候选接线补丁。 | 应用后需在 `mermaid_measurer.ixx` / `mermaid_renderer.ixx` 中先通过 `resolve_family` 将 Windows 下的 `Helvetica` 映射为 `Arial`，再传入 `measurer.measure()`。 |
| `zenuml-native-pins.json` | ZenUML 原生布局、SVG 绘制与图标资源对应的上游 `@zenuml/core` 3.49.2 源码哈希记录。 | 在完成 ZenUML 公共入口验证时合入 `docs/mermaid-cpp/source-lock.json`。 |
| `sha256.json` | 上述三个 `.patch` 文件的原始字节 SHA-256 校验和。 | 应用前可用于校验补丁完整性；补丁正式合入代码树并删除后同步更新或移除。 |

## 使用注意事项

1. 本目录配置了 `.gitattributes` 以保留补丁原始换行符。应用前请先执行 `git apply --check <patch>` 检查当前工作区基线是否匹配。
2. `tools/mermaid-cpp/CMakeLists.txt` 会自动扫描 `third_party/mermaid-cpp/src/` 与 `tests/mermaid_cpp/` 下的所有源文件。应用 `class-elk.patch` 前必须先应用 `class-elk-shared.patch`，否则 `class_elk.cpp` 会因缺少 `layout_measured_elk_scene` 声明而编译失败。
