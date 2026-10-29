# Mermaid 当前交接

现状与下一步以 [README](README.md) 为准。本文件仅保存接手位置，不保留历史流水账。

- 验证基线：`7b27d210`，Windows 四配置运行 `40795935250` 全部成功。
- 当前交付：20 图族公共入口；ZenUML 五例 native/SVG 成功，尚未接公共入口。
- 待收尾：ZenUML detector/公共入口、Class ELK 共享接口与 painter、ER native 链。具体分工、案例和退出条件见 README。
- 未验收：完整 WinUI 调用与生产 Rust 依赖切换。禁止将部分宿主观察当作完成。
- `candidates/2026-10-28` 保存未接入补丁；Class fixture 已为九列，原函数提取器已支持 generator。先验证补丁基线再应用，禁止盲目按顺序套用。
- 没有正在执行的智能体任务；后续重新登记 owner 与实际构建句柄，避免重复工作。
- 上游 PR #1 保持开放，不自动合并。本轮仅更新代码、说明和分支整理。
- fork 保留 main 与唯一开发分支 feat/mermaid-cpp-port。后者为现有上游 PR 来源，承担 dev 角色；不要删除或改名导致 PR 关闭。
- 功能通过必要验证即提交；只因相关改动、实际回归或明确新需求重新验证，不重开已验收工作。
