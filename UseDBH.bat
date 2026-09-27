@echo off
chcp 65001 >nul
set DBH="E:\Windows Kits\10\Debuggers\x64\dbh.exe"
set SYM_PATH=SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols
set TARGET=C:\Windows\system32\ntoskrnl.exe

echo [*]正在批量提取内核函数偏移...
%DBH% -s:%SYM_PATH% %TARGET% "x MiGetPteAddress"
%DBH% -s:%SYM_PATH% %TARGET% "x MmAllocateIndependentPages"
%DBH% -s:%SYM_PATH% %TARGET% "x MmFreeIndependentPages"
pause