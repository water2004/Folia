# 开发资料

这里保留当前实现的设计、约束、使用方式和可复现的验证方法。
已完成的任务清单、临时分工、逐次构建结果和旧实现进度不归档在这里；
需要查阅过去的过程时使用 Git 历史。

| 主题 | 入口 |
| --- | --- |
| 仓库约束、块树与编辑坐标 | [AGENTS.md](../AGENTS.md) |
| Markdown 可复用模块 | [src/markdown/README.md](../src/markdown/README.md) |
| 编辑与核心模块 | [src/core/README.md](../src/core/README.md) |
| 撤销事务 | [undo-patches.md](undo-patches.md) |
| CommonMark / GFM 对照 | [markdown-conformance.md](markdown-conformance.md) |
| 测试入口与职责 | [tests/README.md](../tests/README.md) |
| 原生数学架构 | [math-port/architecture.md](math-port/architecture.md) |
| 主题包 | [THEMES.md](THEMES.md) |
| 设置与快捷键 | [SETTINGS.md](SETTINGS.md) |
| 表格交互 | [TABLE_INTERACTION.md](TABLE_INTERACTION.md) |

`math-port` 中的 Source 锁和结构化参考数据仍有工具消费者，不能按文件年代删除。
唯一的工作连续性记录是 `math-port/HANDOFF.md`，仅保留当前停止点、必要约束和下一步。
