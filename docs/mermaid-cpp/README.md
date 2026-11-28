# Mermaid C++ 原生渲染

Folia 的 Mermaid renderer 是平台无关的 C++23 核心，Windows 宿主通过平台文本测量与渲染桥接提供字体数据和 DLL 接口。行为基准为 Mermaid 12.2.0，commit `1fb7869866dd619168d840291c9c54b4f129758b`。版本和上游文件 SHA-256 由 [`source-lock.json`](../../tools/mermaid-cpp/source-lock.json) 锁定。

## 从这里开始

- [模块组织与平台边界](architecture.md)：源码职责、实际依赖、生成数据和扩展流程。
- [主题与公共 API](themes.md)：公共 C++ 入口、主题/配置优先级及 Windows C ABI。
- [数学标签](math.md)：数学渲染路径、配置和验证范围。
- [全图族回归](regressions.md)：语料约束、工具和验收层级。
- [ELK 与力导向布局边界](elk-layout-boundary.md)：算法来源及模块边界。
- [Windows 宿主集成](winui-integration.md)：宿主接线、打包和验证方法。

## 构建与测试

主工程以 `-DFOLIA_BUILD_MERMAID=ON` 启用 Mermaid 目标。独立工程位于 `tools/mermaid-cpp/`，C++23 模块工具链和 Math 运行库选项见下例；Windows SDK/DirectWrite 目标仅在 Windows 构建。

```sh
cmake -S tools/mermaid-cpp -B build/mermaid-cpp -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/mermaid-cpp --target FoliaMermaidTests --parallel 4
ctest --test-dir build/mermaid-cpp -R '^FoliaMermaid(Tests|Structure)$' --output-on-failure
python3 tools/mermaid-cpp/check-structure.py
```

独立 Linux 核心构建需支持 `import std` 的 C++23 标准库；真实文本度量回归还需 Pango/fontconfig。Windows 集成构建与 Math DLL 配置见 [架构说明](architecture.md) 和 [回归说明](regressions.md)。

## 维护约定

核心模块使用 C++23 modules 和 `import std;`，不引入 Windows/WinRT/Direct2D/DirectWrite 头文件。字体相关布局使用宿主提供的实测数据。新增上游来源时同步更新源码锁与 [第三方声明](../../src/mermaid/THIRD_PARTY_NOTICES.md)。生成数据必须由对应生成器更新；模块移动保持模块名稳定，并运行结构检查。

完整图的原生 SVG 生成、算法 fixture、官方 SVG/Canvas 对照和 Windows 宿主测试是不同的证据层级。报告应标明实际运行的层级、输入和代码版本；缺失的浏览器或平台证据不能推断为通过。
