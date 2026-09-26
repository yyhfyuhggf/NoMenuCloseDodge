# NoMenuCloseDodge（源码 / 云编译用）

这是 SKSE 插件 **NoMenuCloseDodge** 的源码仓库。它的作用是修掉：

> TK Dodge RE + 手柄：用 B 键关闭菜单（物品栏 / 对话 / 地图 / 日志…）后，
> 角色会莫名翻滚一下。

原理：TK Dodge RE 的 `SprintHandlerHook::ProcessButton()` 把「冲刺键短按」的
抬起事件当作闪避；关菜单那一按恰好符合这个条件。而 `TKRE::canDodge()` 里要求
`ControlMap::IsMovementControlsEnabled()` 为真，所以本插件在菜单关闭后 0.30 秒内
把「移动」控制临时关掉，闪避就被否决了（与已下架的 Nexus mod 172881 思路一致）。

## 怎么得到 DLL

* 仓库自带 GitHub Actions 工作流 `.github/workflows/build.yml`：
  每次 push / 手动触发都会编译一次，产物在构建页面的 **Artifacts →
  `NoMenuCloseDodge-MOD`** 里。
* 本地编译：Windows 上装好 Visual Studio 2022「使用 C++ 的桌面开发」后，
  用同目录外层的 `build.ps1` 一键编译。

## 安装

把下载到的 `NoMenuCloseDodge-MOD.zip` 解压得到 `SKSE\Plugins\NoMenuCloseDodge.dll`，
用 MO2「从文件安装」或直接放进游戏 `Data` 目录。

* 前置：SKSE64、Address Library for SKSE Plugins（TK Dodge RE 本身也需要）。
* 日志：`文档\My Games\Skyrim Special Edition\SKSE\NoMenuCloseDodge.log`
* 可选设置：`NoMenuCloseDodge.ini` → `BlockDurationMs = 300`（屏蔽时长，毫秒）

## 许可

依赖 CommonLibSSE-NG（GPL-3.0-or-later）。本仓库为自行编写的补丁实现，
不包含已下架 mod 的任何文件；病因分析基于 TK Dodge RE 的公开源码（MIT）。
