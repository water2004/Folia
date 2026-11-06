# 历史归档：早期 Rust Mermaid Direct2D 后端探索记录

## 状态说明（2026-10）

Folia 的 Mermaid 渲染重构已切换为基于官方 `mermaid-js/mermaid` 12.2.0 的 **C++23 原生实现方案**，最新架构说明、图族进展与交接文档见 [`../mermaid-cpp/README.md`](../mermaid-cpp/README.md) 与 [`../mermaid-cpp/HANDOFF.md`](../mermaid-cpp/HANDOFF.md)。

本目录（`docs/mermaid-native/`）下的文档仅作为早期在 `mermaid-rs-renderer` 基础上探索中间场景图（`DiagramScene`）与 Direct2D 渲染时的历史设计与验证归档保留，不再继续推进。

---

## 早期探索归档摘要（截至 2026-10-12 暂停时）

- **已完成的探索内容**：
  - 梳理了上游 23 个图族清单并建立了初始采样参考；
  - 定义了不可变的 `DiagramScene` / `SceneBuilder` 及几何、画刷、分组、裁剪、文本等中间表示类型；
  - 验证了 Direct2D 基础图元消费端与 11 个本地 WARP 测试用例；
  - 提取了共享尺寸与 `viewBox` 计算逻辑（`scene/compile/dimensions.rs`，commit `8b7a0a3`，见 `evidence/m5a-dimensions-acceptance.md`）。
- **暂停时的遗留边界**：
  - 各图族从 `Layout` 到 `DiagramScene` 的降级转换（lowering）及通用 SVG 消费端尚未接通；
  - 早期拆分的 `tiny-skia` CPU 光栅化、采样与混合管线（`m3b` ~ `m3j`）属于多余的范围扩张，已在 `work-scope-review.md` 中归档停用。
