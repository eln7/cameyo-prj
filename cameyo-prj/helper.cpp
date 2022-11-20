#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <psapi.h>
#include <string>
#include <queue>
#include <functional>
#include <utility>


void PrintProcessNameAndID(DWORD processID, std::vector<std::wstring> & procToHook)
{
    TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
    std::wstring test(szProcessName);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
        PROCESS_VM_READ,
        FALSE, processID);

    bool found = false;

    if (NULL != hProcess){
        HMODULE hMod;
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)){
            GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
        }

        found = (std::find(procToHook.begin(), procToHook.end(), szProcessName) != procToHook.end());

        _tprintf(TEXT("%s  (PID: %u) ---- %s\n"), szProcessName, processID, found?L"hook":L"not to hook");
        CloseHandle(hProcess);
    }
}

int getProcList(std::vector<std::wstring> & procToHook )
{
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        return 1;
    }

    cProcesses = cbNeeded / sizeof(DWORD);

    for (i = 0; i < cProcesses; i++){
        if (aProcesses[i] != 0){
            PrintProcessNameAndID(aProcesses[i], procToHook);
        }
    }

    return 0;
}