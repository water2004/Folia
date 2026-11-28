# 测试数据与迁移经验

## fixture 或 Gold 迁移后悄然变化

新 reader 能成功解析，不代表迁移保留了数据。行尾、记录顺序、重复项、空字段、二进制文件，以及缺失或新增文件都可能被静默规范化。固定文件清单和 SHA-256，并逐条比对迁移前后的记录。修改生成器时，还要比对 emitter 输出的原始字节。当前检查见 [fixture 完整性检查器](../../tests/tools/check_fixtures.py) 和 [fixture 测试](../../tests/tools/test_fixtures.py)。v0.9.0 的 Math fixture 外置和完整性校验修复见 [f2bdac1f](https://github.com/water2004/Folia/commit/f2bdac1f) 与 [c236142a](https://github.com/water2004/Folia/commit/c236142a)。这些提交不是新的验证结果。

## Windows 测试路径失效

测试从不同盘符、短临时目录或独立 build 目录运行时，如果路径逻辑假设固定 cwd 或编译器源码目录，就会找不到 fixture。诊断时区分语料根、运行目录和真实源码位置。统一 runner 应传入稳定根目录，fixture 中不要写机器绝对路径。当前入口见 [runner](../../tests/run.py) 和 [路径回归](../../tests/tools/test_fixtures.py)。历史修复见 [54dea548](https://github.com/water2004/Folia/commit/54dea548)。

## 测试被拆分/接线后注册数变少却仍通过

Suite 通过不代表所有既有测试都仍被收集。迁移测试入口时应对比注册清单，并把测试分类与输入数据分开。独立 reader/生成器检查可避免生产实现用自身结果证明正确。入口说明见 [测试指南](../../tests/README.md)，数据覆盖见 [fixture 检查器](../../tests/tools/check_fixtures.py)。历史上保护原注册的提交见 [bb6ad0fc](https://github.com/water2004/Folia/commit/bb6ad0fc)。

## 局部对照通过但报告声称完整验收

算法 fixture、官方捕获、SVG 比较、像素和 UI 测试覆盖不同层。报告应说明实际运行的检查、输入版本和未覆盖边界，不能从局部通过推断整个产品已通过验收。参见 [Mermaid 验收分层](../mermaid-cpp/regressions.md) 和 [数学验证](../math/verification.md)。
