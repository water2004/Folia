# Contributing to Folia

This guide covers the source tree, local builds, verification, and release packaging. For installation and everyday use, see the [English README](README.md) or [简体中文 README](README.zh-CN.md).

## Build the Windows app

Use Windows 10 or 11, Visual Studio with the Desktop development with C++ workload, CMake, Ninja, Rust/Cargo, and PowerShell. The app build needs an MSVC toolchain with C++23 module support.

```powershell
git submodule update --init --recursive
powershell -ExecutionPolicy Bypass -File .\setup.ps1
powershell -ExecutionPolicy Bypass -File .\build_app.ps1 -Configuration Debug
```

`setup.ps1` restores NuGet and native dependencies, including the pinned ICU runtime for charset detection and transcoding. Use `-Configuration Release -Clean` for a clean Release build. Application binaries go to `build/app-winui/bin/<platform>/<configuration>/`. Keep generated outputs and restored packages out of commits. [WinUI project](src/app-winui/Folia.vcxproj)

## Find the right layer

```text
Markdown source
    └─ block tree with exact per-block inline source and lossless inline CST
         └─ platform-neutral render model
              └─ Windows layout and editor interaction
                   └─ WinUI 3 shell and Direct2D/DirectWrite rendering
```

The core uses C++23 modules and stays platform-independent. Block structure and exact inline source own editing state. Windows APIs and presentation live in the platform and application layers. Use the repository instructions for the editing and testing invariants. [Repository instructions](AGENTS.md) · [Source layout](src/README.md) · [Core](src/core/README.md) · [Windows platform](src/platform/README.md) · [WinUI app](src/app-winui/README.md)

For subsystem work, use the established contracts and verification guides. [Development docs](docs/README.md) · [Table interaction](docs/TABLE_INTERACTION.md) · [Native math](docs/math/architecture.md) · [Math verification](docs/math/verification.md) · [Native Mermaid](docs/mermaid-cpp/README.md) · [Development log](docs/devlog/README.md)

## Run tests

Use the portable runner to list suites or select the suites relevant to the change.

```sh
python tests/run.py list
python tests/run.py --suite core
python tests/run.py --suite all
```

Use the Windows wrappers for the core, deterministic editor, and encoding suites. The Boost.UT runners accept positional test-name filters.

```powershell
cmd /c tests\run_core.bat "*inline*"
cmd /c tests\platform\run.bat
cmd /c tests\encoding\run.bat
powershell -ExecutionPolicy Bypass -File .\tests\app\run.ps1 -Configuration Debug
```

The app tests reuse the application’s compiled objects. Build the same configuration before running them, and add `-Visual` to run tests against a real WinUI window. For toolchain setup, suite ownership, fixtures, and reports, see the [test guide](tests/README.md).

## Package a release locally

The distribution version comes from the [app manifest](src/app-winui/Package.appxmanifest). Build both Windows x64 packages and run the payload verification from the repository root.

```powershell
powershell -ExecutionPolicy Bypass -File .\build_msi.ps1
powershell -ExecutionPolicy Bypass -File .\build_portable.ps1
$version = & .\tools\Get-FoliaVersion.ps1
$msi = "build/installer/bin/Folia-$version-x64.msi"
$zip = "build/portable/Folia-$version-x64-portable.zip"
python tests/installer/test_release_payload.py $msi $zip
```

The scripts produce an MSI at `build/installer/bin/Folia-<version>-x64.msi` and a ZIP at `build/portable/Folia-<version>-x64-portable.zip` with matching SHA-256 files. Both package the Windows App SDK runtime. After checking startup and the payloads, manually attach the packages and their checksums to the GitHub release.

### Installation and upgrade behavior

The per-user MSI defaults to `%LOCALAPPDATA%\Programs\Folia` and remembers the selected folder on upgrade. For an unattended installation, set `INSTALLFOLDER` to choose another path. Its Assets directory is `%LOCALAPPDATA%\Folia\Assets`, resolved at runtime from the compile-time `{LocalAppData}\Folia\Assets` token. The MSI registers Folia as an available handler for `.md` and `.markdown`.

Upgrades replace installer-managed resources, including built-in themes, and preserve user data. Standard uninstall removes MSI-owned files and leaves user settings and custom themes. The optional full cleanup in the uninstall wizard also removes user resources, commands, and saved AI credentials. For silent uninstall, use `FOLIAREMOVEUSERDATA=1` to request full cleanup.

The portable edition uses an `Assets` folder beside `Folia.exe` for settings and custom themes. API keys use Windows Credential Manager. [Assets locations and theme upgrades](docs/THEMES.md)

### Optional native math probe

Run this against the already-built self-contained Release app to check that the deployed math DLL starts correctly.

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\math-port\winui-math-worker-probe.ps1 -Configuration Release -SkipBuild -SelfContained
```

The probe reuses the existing application objects and deployed math DLL. Its output stays in `build/`.
