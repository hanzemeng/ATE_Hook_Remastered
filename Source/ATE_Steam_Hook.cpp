// ATE_Steam_Hook.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include <psapi.h>
#include <tchar.h>
#include <windows.h>
#include <string>
#include <thread>
#include <fstream>


const wchar_t* m_targetWindowName = L"Muv-Luv Alternative Total Eclipse Ver 1.0.27";


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

    HWND targeHhwnd = FindWindow(NULL, m_targetWindowName);
    if (!targeHhwnd) {
        MessageBox(NULL, L"Target window not found!", L"Error", MB_OK);
        return 1;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(targeHhwnd, &pid);
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

    const char* dllPath = "ATE_Hook_Remastered.dll";
    SIZE_T dllPathSize = strlen(dllPath) + 1;

    LPVOID remoteMemory = VirtualAllocEx(hProcess, NULL, dllPathSize, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, remoteMemory, dllPath, dllPathSize, NULL);

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    LPTHREAD_START_ROUTINE loadLibraryAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, loadLibraryAddr, remoteMemory, 0, NULL);
    WaitForSingleObject(hThread, INFINITE);

    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return 0;
}
