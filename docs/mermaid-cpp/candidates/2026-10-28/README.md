# 待接入候选补丁说明（2026-10-28）

本目录保存 **Class 默认 ELK 布局** 的候选补丁。接入步骤与验证方法见上级目录的 [`HANDOFF.md`](../../HANDOFF.md) 与 [`README.md`](../../README.md)。

## 文件清单

| 文件名 | 内容说明 | 后续处理步骤 |
| --- | --- | --- |
| `class-elk-shared.patch` | 在 `src/mermaid/flowchart_elk.{ixx,cpp}` 中提取 `ElkMeasuredScene` 与 `layout_measured_elk_scene`，供 Flowchart 与 Class 共享 ELK 测量场景与端点标签输出。 | 先于 `class-elk.patch` 应用，确认既有 Flowchart ELK 6 个测试案例不回归。 |
| `class-elk.patch` | 新增 `src/mermaid/class_elk.{ixx,cpp}`、9 列基准数据 `tests/mermaid_cpp/fixtures/class-elk.tsv`、单元测试 `tests/mermaid_cpp/class_elk.cpp` 及 `tools/mermaid-cpp/class-elk-{cases,oracle,browser-probe}.mjs`。 | 在 `class-elk-shared.patch` 之后应用，随后在 `class_svg.{ixx,cpp}` 与 `render_entry.cpp` 中接入 `layout: elk` 渲染分支并验证 5 案例 × 双主题共 10 张 SVG。 |
| `sha256.json` | 上述补丁文件的原始字节 SHA-256 校验和。 | 应用前用于校验补丁完整性。 |

## 使用注意事项

1. 本目录配置了 `.gitattributes` 以保留补丁原始换行符。应用前请先执行 `git apply --check <patch>` 检查当前工作区基线是否匹配。
2. 库和测试的 CMake 文件分别自动收集 `src/mermaid/` 和 `tests/mermaid_cpp/` 的源文件。先应用 `class-elk-shared.patch` 提供 `layout_measured_elk_scene`，再应用依赖它的 `class-elk.patch`。
