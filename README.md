# PdbParser

从微软符号服务器下载并解析 **`ntoskrnl.exe`** 的 PDB 符号，查询任意内核函数（含未导出的私有符号）的地址，
并直接给出相对镜像基址的**内核偏移**。

适用于内核研究、驱动/反作弊分析、以及需要把「函数名」换算成「内核偏移」的场景。

## 工作原理

1. 用 `GetSystemDirectoryA` 定位当前系统的 `C:\Windows\System32\ntoskrnl.exe`；
2. 以 `SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols` 作为符号路径调用 `SymInitialize`，
   首次运行会自动从微软服务器下载对应的 PDB 并缓存到 `C:\Symbols`；
3. 用伪造的基址 `0x1000000` 把内核镜像通过 `SymLoadModuleEx` 挂进 DbgHelp（伪造非零基址是为了后续直接相减算偏移）；
4. 先用 `SymFromName` 精确查找；若未命中（部分私有符号查不到），再用 `SymEnumSymbols` 以 `*函数名*` 通配符枚举兜底；
   枚举过程中一旦遇到**完全相等**的符号名会立即停止，避免被其它相近符号抢走结果；
5. 输出 `符号地址` 与 `内核偏移 = 符号地址 - 0x1000000`，即函数在镜像内的偏移（RVA）。

## 编译

```bat
build.bat
```

`build.bat` 会用 `vcvars64.bat` 初始化 MSVC 环境，以 `/std:c++17 /utf-8 /EHsc /O2 /W4` 编译
`PdbParser.cpp`，并在成功后从 WDK Debuggers 目录拷贝 `dbghelp.dll`、`symsrv.dll`、`msdia140.dll`
到当前目录。链接的库是 `dbghelp.lib`（源码里用 `#pragma comment(lib, "dbghelp.lib")` 指定）。

### 前置条件

`build.bat` 顶部的三个路径是**本机硬编码**的，换机器需要改：

```bat
set "VSROOT=E:\Program Files\Microsoft Visual Studio"
set "KITS=E:\Windows Kits\10"
set "SDKVER=10.0.28000.0"
```

需要：

- Visual Studio（含 C++ 生成工具，`VC\Auxiliary\Build\vcvars64.bat` 存在）
- Windows SDK（含 `Lib\<版本>\um\x64\DbgHelp.Lib`）
- Windows SDK 的 Debuggers 组件 / WDK（含 `Debuggers\x64\dbghelp.dll`、`symsrv.dll`、`msdia140.dll`）

> 仓库里已经带了一份编译好的 `PdbParser.exe` 和上述 DLL，不想编译可以直接用。

## 使用

```bat
PdbParser.exe <函数名>
```

例如：

```bat
PdbParser.exe MmAllocateIndependentPages
```

输出形如：

```
[*] 正在加载内核镜像: C:\Windows\System32\ntoskrnl.exe
[*] 正在从微软服务器下载/解析 PDB 符号 (首次运行可能需要几秒)...

[+] 符号解析成功！
========================================
 函数名称: MmAllocateIndependentPages
 符号地址: 0x1C0xxxxx
 内核偏移: 0x1C0xxxxx
========================================
```

- **符号地址**：在伪基址 `0x1000000` 下的绝对地址；
- **内核偏移**：减去伪基址后的结果，即 `内核模块实际基址 + 该偏移 = 函数实际地址`。

未找到时提示：

```
[!] 未能在 PDB 中找到符号: <函数名> (可能拼写错误或该函数在新版内核已被移除)
```

## 附带的 UseDBH.bat

`UseDBH.bat` 是等价的「传统做法」参考脚本：直接调用 Windows SDK 自带的 `dbh.exe` 批量查询几个
`Mi*`/`Mm*` 函数，效果与 `PdbParser.exe` 一致，可以用来交叉验证结果。

```bat
set DBH="E:\Windows Kits\10\Debuggers\x64\dbh.exe"
set SYM_PATH=SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols
set TARGET=C:\Windows\system32\ntoskrnl.exe

%DBH% -s:%SYM_PATH% %TARGET% "x MmAllocateIndependentPages"
```

## 目录说明

| 文件 | 说明 |
|---|---|
| `PdbParser.cpp` | 全部源码 |
| `build.bat` | 一键编译 + 拷贝依赖 DLL |
| `UseDBH.bat` | 用 `dbh.exe` 查询内核偏移的参考脚本 |
| `PdbParser.exe` / `PdbParser.obj` | 已编译产物（便于直接使用） |
| `dbghelp.dll` / `symsrv.dll` / `msdia140.dll` | DbgHelp 与符号下载所需运行库 |

## 注意事项

- 需要**联网**（首次解析要下载 PDB）；符号缓存在 `C:\Symbols`。
- 查询的是**当前系统**的 `ntoskrnl.exe`，偏移随 Windows 版本/补丁号变化，跨版本不可通用。
- 控制台已通过 `SetConsoleOutputCP(CP_UTF8)` 切到 UTF-8，避免中文输出乱码；若在旧终端里看到乱码，
  先执行 `chcp 65001`。
