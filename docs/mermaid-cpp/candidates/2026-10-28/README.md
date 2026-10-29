# 未完成接缝

当前功能和验收边界见上级 README。旧 native-entry 已并入运行代码，不再保存重复补丁。

- class-elk.patch：当前五例/九列 fixture 和未接入实现；需要 class-elk-shared.patch，随后完成 painter/入口。
- zenuml-public.patch：未验收入口草案；还需外部 detector、Windows 字体映射及原版 source pins 集成。不要直接视为可应用完成品。
- zenuml-native-pins.json：原版布局、绘制和图标来源候选，整合前核验。

补丁使用 raw Git 字节，先在隔离工作区检查基线。当前代码已经通过验证的部分不重新应用。
