#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <string>

// 链接 dbghelp 库
#pragma comment(lib, "dbghelp.lib")

struct SymbolEnumContext {
    const std::string* target = nullptr;
    std::string matchName;
    DWORD64 matchAddress = 0;
    bool found = false;
};

BOOL CALLBACK SymbolEnumProc(PSYMBOL_INFO pSymbol, ULONG /*symbolSize*/, PVOID userContext) {
    SymbolEnumContext* context = static_cast<SymbolEnumContext*>(userContext);

    if (!context->found) {
        context->found = true;
        context->matchName = pSymbol->Name;
        context->matchAddress = pSymbol->Address;
    }

    // 如果找到精确匹配就停止，避免被通配符命中的其它相近符号抢走结果
    if (context->target && pSymbol->Name == *context->target) {
        context->matchName = pSymbol->Name;
        context->matchAddress = pSymbol->Address;
        return FALSE;
    }

    return TRUE;
}

// 打印错误信息的辅助函数
void PrintError(const std::string& message) {
    std::cerr << "[!] " << message << " 错误代码: " << GetLastError() << std::endl;
}

void PrintSymbolResult(DWORD64 pseudoBase, const std::string& symbolName, DWORD64 address) {
    DWORD_PTR offset = (DWORD_PTR)(address - pseudoBase);

    std::cout << "\n[+] 符号解析成功！" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << " 函数名称: " << symbolName << std::endl;
    std::cout << " 符号地址: 0x" << std::hex << address << std::endl;
    std::cout << " 内核偏移: 0x" << std::hex << std::uppercase << offset << std::endl;
    std::cout << "========================================" << std::endl;
}

int main(int argc, char* argv[]) {
    // 控制台按 UTF-8 解码，避免中文输出在默认 GBK 代码页下显示乱码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " <函数名>" << std::endl;
        std::cout << "示例: " << argv[0] << " MmAllocateIndependentPages" << std::endl;
        return -1;
    }

    std::string symbolName = argv[1];

    // 1. 获取当前系统内核文件的路径 (通常是 C:\Windows\System32\ntoskrnl.exe)
    char systemPath[MAX_PATH];
    GetSystemDirectoryA(systemPath, MAX_PATH);
    std::string kernelPath = std::string(systemPath) + "\\ntoskrnl.exe";

    std::cout << "[*] 正在加载内核镜像: " << kernelPath << std::endl;

    // 2. 初始化符号引擎
    // 与 dbh 保持一致的选项组合，包含 AUTO_PUBLICS 和延迟加载
    SymSetOptions(
        SYMOPT_CASE_INSENSITIVE |
        SYMOPT_UNDNAME |
        SYMOPT_DEFERRED_LOADS |
        SYMOPT_LOAD_LINES |
        SYMOPT_EXACT_SYMBOLS |
        SYMOPT_ALLOW_ABSOLUTE_SYMBOLS |
        SYMOPT_AUTO_PUBLICS
    );

    HANDLE hProcess = GetCurrentProcess();
    const char* symbolSearchPath = "SRV*C:\\Symbols*https://msdl.microsoft.com/download/symbols";
    if (!SymInitialize(hProcess, symbolSearchPath, FALSE)) {
        PrintError("符号引擎初始化失败");
        return -1;
    }

    // 设置符号下载缓存路径（自动创建 C:\Symbols 并从微软服务器下载）
    SymSetSearchPath(hProcess, symbolSearchPath);
    LoadLibraryA("symsrv.dll");

    // 3. 将 ntoskrnl.exe 加载到符号引擎中
    // 注意：这里的基址我们伪造一个非零值（0x1000000），方便后续直接相减算偏移
    DWORD64 pseudoBase = 0x1000000;

    HANDLE hFile = CreateFileA(kernelPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        PrintError("无法打开 ntoskrnl.exe 文件");
        SymCleanup(hProcess);
        return -1;
    }

    std::cout << "[*] 正在从微软服务器下载/解析 PDB 符号 (首次运行可能需要几秒)..." << std::endl;
    DWORD64 moduleBase = SymLoadModuleEx(
        hProcess,           // 进程句柄
        hFile,              // 文件句柄
        kernelPath.c_str(), // 模块路径
        "ntoskrnl",         // 模块别名
        pseudoBase,         // 伪造的基地址
        0,                  // DllSize 传 0，让 DbgHelp 从 hFile 读取 PE 头
        NULL,               // 特殊修改参数
        0                   // 标志
    );
    CloseHandle(hFile);

    if (moduleBase == 0) {
        PrintError("加载内核模块符号失败");
        SymCleanup(hProcess);
        return -1;
    }

    // 4. 检索目标符号信息
    // 分配足够空间存放 SYMBOL_INFO 结构体及后面的名字缓冲区
    ULONG64 buffer[(sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR) + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    if (SymFromName(hProcess, symbolName.c_str(), pSymbol)) {
        PrintSymbolResult(pseudoBase, pSymbol->Name, pSymbol->Address);
    } else {
        // 5. 部分私有符号无法由 SymFromName 直接命中，用通配符枚举兜底
        SymbolEnumContext enumContext;
        enumContext.target = &symbolName;

        std::string pattern = "*" + symbolName + "*";
        SymEnumSymbols(hProcess, moduleBase, pattern.c_str(), SymbolEnumProc, &enumContext);

        if (enumContext.found) {
            PrintSymbolResult(pseudoBase, enumContext.matchName, enumContext.matchAddress);
        } else {
            std::cout << "[!] 未能在 PDB 中找到符号: " << symbolName << " (可能拼写错误或该函数在新版内核已被移除)" << std::endl;
        }
    }

    // 6. 清理资源
    SymUnloadModule64(hProcess, moduleBase);
    SymCleanup(hProcess);
    return 0;
}
