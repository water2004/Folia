# 构建与发布包

本文记录 MSI 与 portable ZIP 的可复用故障边界。发布仍按仓库既有约定本地准备，不在此引入 CI 或外部发布流程。

## 包能安装，但 portable 缺资源或带入了开发文件

**先看** [DeploymentAssets.props](../../src/app-winui/DeploymentAssets.props)、[Get-DeploymentFiles.ps1](../../tools/Get-DeploymentFiles.ps1)、[Generate-WixPayload.ps1](../../tools/Generate-WixPayload.ps1)。应用资源、MSI 与 ZIP 若各自维护文件清单，就会出现某包漏主题/许可，或将用户数据、私有文件、构建产物一并发布。当前 MSBuild 与两个包共用部署资源清单，打包器只选列出的资产。程序文件另过滤调试和探测文件。

规则如下。新增运行时资产时先更新共享清单，确认两种包都取同一份资源。不要在 MSI/ZIP 脚本里复制维护资源列表。用 [test_release_payload.py](../../tests/installer/test_release_payload.py) 比较 MSI 解包和 ZIP 的完整清单、重复项、必要 DLL、逐文件内容与校验和。[test_theme_upgrade.ps1](../../tests/installer/test_theme_upgrade.ps1) 还验证用户文件被排除。背景：[aa192472](https://github.com/water2004/Folia/commit/aa192472)。

## 覆盖升级后内置主题未更新，或用户设置/自定义主题消失

**先看** [Generate-WixPayload.ps1](../../tools/Generate-WixPayload.ps1) 中组件和 `RemoveFile` 的生成，以及 [Package.wxs](../../installer/Package.wxs) 的数据目录和卸载条件。MSI 组件规则可能把修改过的内置文件视为用户文件而保留。反过来，对整个 Assets 目录做清理又可能删除用户设置、自定义主题、命令或聊天记录。普通卸载也不应隐式删除个人数据。

规则如下。内置文件按产品更新，用户数据按用户所有。升级/修复应替换内置主题与数学命令目录，同时保留自定义项和用户设置。删除数据必须是完整卸载时的明确选择。验证真实 MSI 的同版本修复、升级、普通卸载、可选全清理：[test_theme_upgrade.ps1](../../tests/installer/test_theme_upgrade.ps1)、[test_install_directory.ps1](../../tests/installer/test_install_directory.ps1)。历史修复：[fb0d2c11](https://github.com/water2004/Folia/commit/fb0d2c11)、[6e41e539](https://github.com/water2004/Folia/commit/6e41e539)。

## portable 从别的目录启动时找不到资源，或资源跑到当前目录

**先看** [AssetPaths.cpp](../../src/app-winui/storage/AssetPaths.cpp)、[AssetPaths.h](../../src/app-winui/storage/AssetPaths.h)、[build_app.ps1](../../build_app.ps1) 的 portable 编译选项。portable 的资源路径应相对于可执行文件目录，而不是调用方当前工作目录。安装版则必须仍使用其配置的数据位置。工作目录和应用/用户数据目录混用会让快捷方式、命令行启动及临时目录运行得到不同结果。

规则如下。保持路径选择的构建配置边界。测试便携包时使用 ZIP 解压目录作为程序位置，并显式传入不同工作目录。启动测试脚本支持指定 `WorkingDirectory`：[test_startup.ps1](../../tests/app/test_startup.ps1)。相关打包边界在 [build_portable.ps1](../../build_portable.ps1) 与 [d19c435a](https://github.com/water2004/Folia/commit/d19c435a)。

## 发布文件名、资源版本和可执行文件版本彼此不一致

**先看** [Get-FoliaVersion.ps1](../../tools/Get-FoliaVersion.ps1)、[build_portable.ps1](../../build_portable.ps1)、[build_msi.ps1](../../build_msi.ps1)。仅改一个版本字段会导致包名、MSI ProductVersion、文件属性或清单版本分叉。portable 构建会读取 EXE 的 ProductVersion 并与期望版本比对，发现不符即失败。

规则如下。版本只从版本清单读取，构建入口传同一值，并保留构建时版本一致性检查。查看 [05b65dca](https://github.com/water2004/Folia/commit/05b65dca)，该提交把版本读取收敛到单一入口，并为包生成 SHA-256 sidecar。

## 包可解压，但换机器后启动失败或二进制带有开发机路径

**先看** [test_release_payload.py](../../tests/installer/test_release_payload.py) 的 `path_signatures` / `check_paths` 和 [build_portable.ps1](../../build_portable.ps1) 的打包后启动检查。编译器可能把仓库、用户目录或额外指定目录编码进二进制。只检查源码树或“能生成 ZIP”无法发现。sidecar 存在也不代表内容吻合，ZIP 成员还需读取以触发 CRC 检查。

规则如下。对 MSI 和 ZIP 分别验证 sidecar SHA-256、包清单、文件字节、ZIP CRC，并检查仓库/用户/指定开发路径签名。portable 启动测试应针对实际打好的 ZIP 解压副本运行。测试入口见 [tests/README.md](../../tests/README.md) 的 installer/release 验证段落以及 [test_release_payload.py](../../tests/installer/test_release_payload.py)。

## “启动测试通过”被误读为整段用户流程都正常

**先看** [test_startup.ps1](../../tests/app/test_startup.ps1) 里的 `Folia.StartupComplete`、子进程异常捕获和正常关闭判定。这个探针从真实 GUI 入口启动一个全新、无参数子进程，确认主窗口构造激活后请求正常关闭。它能覆盖早期启动异常和缺失运行文件，但不验证打开文档、编辑、更新或卸载流程。

规则如下。将启动探针视为包的早期启动门槛，其他生命周期行为由对应隔离测试验证。发生异常时先用该探针的有界运行结果定位，再扩展检查。portable 入口默认运行它，只有显式 `-SkipStartupTest` 才跳过：[build_portable.ps1](../../build_portable.ps1)。
