// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

std::wstring exeName;
HINSTANCE hinstDLL;
CRITICAL_SECTION CriticalSection;
bool loopQueue = true;
bool  gInit = false;

#pragma pack(1)
typedef struct tagMsgOut {
	int codeChar;
	wchar_t strWindowName[128];
	wchar_t strProcessName[128];
} MsgOut, * pMsgOut;
#pragma pack()

std::queue<MsgOut> queueOut;

void dprintf(const char* pszFmt, ...){
	va_list arglist;
	va_start(arglist, pszFmt);
	CHAR szMsg[4096];
	vsprintf_s(szMsg, 4096, pszFmt, arglist);
	va_end(arglist);
	OutputDebugStringA(szMsg);
	return;
}

#define BUFSIZE 512


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		dprintf("DLL_PROCESS_ATTACH\n");
		break;
	case DLL_THREAD_ATTACH:
		dprintf("DLL_THREAD_ATTACH\n");
		break;
	case DLL_THREAD_DETACH:
		dprintf("DLL_THREAD_DETACH\n");
		break;
	case DLL_PROCESS_DETACH:
		dprintf("DLL_PROCESS_DETACH:%S\n", exeName.c_str());
		break;
	}
	return TRUE;
}

void CALLBACK timerProc(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime)
{
	HANDLE hPipe;
	BOOL   fSuccess = FALSE;
	DWORD  cbWritten;
	const wchar_t* lpszPipename = TEXT("\\\\.\\pipe\\cameyoNamedPipe");

	if (!queueOut.size()) {
		dprintf("queueOut is empty\n");
		return;
	}

	while (1) {
		hPipe = CreateFile(
			lpszPipename,   // pipe name 
			GENERIC_READ |  // read and write access 
			GENERIC_WRITE,
			0,              // no sharing 
			NULL,           // default security attributes
			OPEN_EXISTING,  // opens existing pipe 
			0,              // default attributes 
			NULL);          // no template file 

		if (hPipe != INVALID_HANDLE_VALUE)
			break;

		if (GetLastError() != ERROR_PIPE_BUSY) {
			dprintf("Could not open the pipe. GLE=%d\n", GetLastError());
			return;
		}

		if (!WaitNamedPipe(lpszPipename, 20000)) {
			dprintf("Could not open pipe: 20 second wait timed out.");
			return;
		}
	}
	dprintf("pipe connected\n");
	while (queueOut.size()) {
		auto msg = queueOut.front();
		fSuccess = WriteFile(hPipe, (void*)&msg, sizeof(MsgOut), &cbWritten, NULL);
		if (!fSuccess) {
			dprintf("WriteFile to pipe failed. GLE=%d\n", GetLastError());
			return;
		}
		queueOut.pop();
	}
	dprintf("queue drained\n");
	CloseHandle(hPipe);
}

extern "C" __declspec(dllexport) int NextHook(int code, WPARAM wParam, LPARAM lParam) {

	if (!gInit) {
		TCHAR szExeFileName[MAX_PATH];
		GetModuleFileName(NULL, szExeFileName, MAX_PATH);
		exeName = szExeFileName;
		size_t pos = exeName.find_last_of(L"\\");
		exeName = exeName.substr(pos + 1, exeName.length());
		dprintf("%S attached\n", exeName.c_str());
		SetTimer(NULL, 0, 1000, (TIMERPROC)&timerProc);
		gInit = true;
	}

	if (code < 0)
		return (LRESULT)CallNextHookEx(NULL, code, wParam, lParam);

	if (((MSG*)lParam)->message == WM_CHAR) {
		//MsgOut msg = { .codeChar = (int)((MSG*)lParam)->wParam, .strWindowName = L"Testing 123", .strProcessName = L"Testing 345" };
		MsgOut msg = { 0, };
		msg.codeChar = (int)((MSG*)lParam)->wParam;
		GetWindowText( ((MSG*)lParam)->hwnd, msg.strWindowName, sizeof(msg.strWindowName)-1 );
		queueOut.push(msg);
		dprintf("code=%u, wparam=%c, queueOut: %u\n", code, ((MSG*)lParam)->wParam, queueOut.size());
	}

	//if (((MSG*)lParam)->message == WM_QUIT){
		//dprintf("WM_QUIT\n");
	//}
	return (LRESULT)CallNextHookEx(NULL, code, wParam, lParam);
}