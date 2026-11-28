# 开发资料

这里收录 Folia 的架构、模块契约和验证指南。排查曾经出现过的故障时，可以从文末的开发日志按症状查找。

## 仓库与测试

- 编辑模型、平台边界和原生渲染约束见 [仓库约定](../AGENTS.md)。
- 构建、验证和发布流程见 [贡献指南](../CONTRIBUTING.md)。
- 测试套件、平台测试和测试数据完整性检查见 [测试指南](../tests/README.md)。

## 编辑器与 Markdown

- [Markdown 解析与序列化 API](../src/markdown/README.md)
- [编辑器核心模块](../src/core/README.md)
- [大文档内存](document-memory.md)与 [布局](document-scalability.md)
- [编辑命令及验证范围](editing-completeness.md)
- [撤销事务与可逆编辑](undo-patches.md)
- [CommonMark 与 GFM 一致性](markdown-conformance.md)
- [表格交互](TABLE_INTERACTION.md)

## 工作区与 AI

- [工作区、窗口和页签](workspace-windows.md)
- [Agent 对话、模型和工具](agent-chat.md)
- [行内补全协议与编辑行为](inline-completion.md)

## 原生渲染

- [原生数学架构](math/architecture.md)与 [验证要求](math/verification.md)
- [原生 Mermaid 的架构、构建与验证](mermaid-cpp/README.md)

## 应用设置

- [设置与快捷键](SETTINGS.md)
- [主题包](THEMES.md)

## 用户指南

- [English README](../README.md)
- [简体中文 README](../README.zh-CN.md)

## 历史案例

[开发日志索引](devlog/README.md) 按症状汇集历史调查、架构决策和回归经验。
