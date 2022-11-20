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

int loopNamedPipe()
{
	HANDLE hPipe;
	BOOL   fSuccess = FALSE;
	DWORD  cbToWrite, cbWritten, dwMode;
	const wchar_t* lpszPipename = TEXT("\\\\.\\pipe\\cameyoNamedPipe");

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

		if (GetLastError() != ERROR_PIPE_BUSY){
			dprintf("Could not open the pipe. GLE=%d\n", GetLastError());
			return -1;
		}

		if (!WaitNamedPipe(lpszPipename, 20000)){
			dprintf("Could not open pipe: 20 second wait timed out.");
			return -1;
		}
	}
	dprintf("pipe connected\n");

	dwMode = PIPE_READMODE_MESSAGE;
	fSuccess = SetNamedPipeHandleState( hPipe, &dwMode, NULL, NULL);

	if (!fSuccess){
		dprintf("could not switch the pipe to the message mode, GLE=%d\n", GetLastError());
		return -1;
	}

	while (loopQueue) {
		if (!TryEnterCriticalSection(&CriticalSection)) {
			dprintf("CRITICAL: *** failed to enter CriticalSection ***\n");
			Sleep(100);
			break;
		}

		if (!queueOut.size()) {
			LeaveCriticalSection(&CriticalSection);
			Sleep(100);
			continue;
		}

		auto msg = queueOut.front();
		//dprintf("wparam=%c, queueOut: %u\n", msg.codeChar, queueOut.size());
		//cbToWrite = sizeof(int);
		//memcpy((void*)&msg1, data, sizeof(MsgOut));

		fSuccess = WriteFile( hPipe, (void*)&msg, sizeof(MsgOut), &cbWritten, NULL);

		if (!fSuccess) {
			dprintf("WriteFile to pipe failed. GLE=%d\n", GetLastError());
			return -1;
		}
		queueOut.pop();
		LeaveCriticalSection(&CriticalSection);
	}

	CloseHandle(hPipe);

	return 0;
}

void hookThread() {

	if (!InitializeCriticalSectionAndSpinCount( &CriticalSection, 0x00000400 )) {
		dprintf("failed - InitializeCriticalSectionAndSpinCount\n");
		return;
	}

	TCHAR szExeFileName[MAX_PATH];
	GetModuleFileName(NULL, szExeFileName, MAX_PATH);
	exeName = szExeFileName;
	int pos = exeName.find_last_of(L"\\");
	exeName = exeName.substr(pos + 1, exeName.length());
	dprintf("%S attached\n", exeName.c_str());

	try {
		loopNamedPipe();
	}
	catch (std::exception const& ex) {
		dprintf("exception loopNamedPipe() STL: %s", ex.what());
	}

	dprintf("INFO: Terminating thread\n");
	//std::wstring message = L"Hooked into " + exeName + L" (PID " + std::to_wstring(GetCurrentProcessId()) + L")";
}

HANDLE hookThreadHandle = NULL;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		//hookThreadHandle = CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(hookThread), NULL, 0, 0);
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
		//loopQueue = false;
		//WaitForSingleObject(hookThreadHandle, INFINITE);
		//DeleteCriticalSection(&CriticalSection);
        break;
    }
    return TRUE;
}

void CALLBACK f(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime)
{
	dprintf("Hello\n");
}

extern "C" __declspec(dllexport) int NextHook(int code, WPARAM wParam, LPARAM lParam) {

	if (!gInit) {
		SetTimer(NULL, 0, 1000, (TIMERPROC)&f);
		gInit = true;
	}

	SetTimer(NULL, 0, 1000 * 60, (TIMERPROC)&f);
	if ( code >= 0 && ((MSG*)lParam)->message == WM_CHAR) {
		if (TryEnterCriticalSection(&CriticalSection)){
			//GetWindowText
			MsgOut msg = { .codeChar = (int)((MSG*)lParam)->wParam, .strWindowName = L"Testing 123", .strProcessName = L"Testing 345" };
			//msg.codeChar = ((MSG*)lParam)->wParam;
			queueOut.push(msg);
			dprintf("code=%u, wparam=%c, queueOut: %u\n", code, ((MSG*)lParam)->wParam, queueOut.size() );
			LeaveCriticalSection(&CriticalSection);
		}
		else{
			dprintf("CRITICAL: failed to enter CriticalSection in the hook\n");
		}
	}
	return (LRESULT)CallNextHookEx(NULL, code, wParam, lParam);
}






	

