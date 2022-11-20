#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <iostream>
#include <cstring>
#include <string>
#include <queue>
#include <functional>
#include <utility>

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#pragma comment(lib, "cpprest142_2_10")

#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

int getProcList(std::vector<std::wstring>& procToHook);

#pragma pack(1)
typedef struct tagMsgOut {
    int codeChar;
    wchar_t strWindowName[128];
    wchar_t strProcessName[128];
} MsgOut, * pMsgOut;
#pragma pack()


typedef struct
{
    OVERLAPPED oOverlap;
    HANDLE hPipeInst;
    TCHAR chRequest[BUFSIZE];
    DWORD cbRead;
    TCHAR chReply[BUFSIZE];
    DWORD cbToWrite;
} PIPEINST, * LPPIPEINST;

VOID DisconnectAndClose(LPPIPEINST);
BOOL CreateAndConnectInstance(LPOVERLAPPED);
BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);
VOID GetAnswerToRequest(LPPIPEINST);

VOID WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);
VOID WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);

HANDLE hPipe;



DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
    HANDLE hConnectEvent;
    OVERLAPPED oConnect;
    LPPIPEINST lpPipeInst;
    DWORD dwWait, cbRet;
    BOOL fSuccess, fPendingIO;

    hConnectEvent = CreateEvent( NULL, TRUE, TRUE, NULL);

    if (hConnectEvent == NULL) {
        printf("CreateEvent failed with %d.\n", GetLastError());
        return 0;
    }

    oConnect.hEvent = hConnectEvent;
    fPendingIO = CreateAndConnectInstance(&oConnect);

    while (1) {
        dwWait = WaitForSingleObjectEx( hConnectEvent, INFINITE, TRUE);
        printf("dwWait: %u\n", dwWait);
        switch (dwWait){
            case 0:
                if (fPendingIO){
                    fSuccess = GetOverlappedResult(
                        hPipe,
                        &oConnect,
                        &cbRet,
                        FALSE);
                    if (!fSuccess){
                        printf("ConnectNamedPipe (%d)\n", GetLastError());
                        return 0;
                    }
                }
                lpPipeInst = (LPPIPEINST)GlobalAlloc( GPTR, sizeof(PIPEINST));
                if (lpPipeInst == NULL){
                    printf("GlobalAlloc failed (%d)\n", GetLastError());
                    return 0;
                }

                lpPipeInst->hPipeInst = hPipe;

                lpPipeInst->cbToWrite = 0;
                CompletedWriteRoutine(0, 0, (LPOVERLAPPED)lpPipeInst);
                fPendingIO = CreateAndConnectInstance(&oConnect);
                break;
            case WAIT_IO_COMPLETION:
                break;
            default:{
                printf("WaitForSingleObjectEx (%d)\n", GetLastError());
                return 0;
            }
        }
    }
    return 0;
}

VOID WINAPI CompletedWriteRoutine(DWORD dwErr, DWORD cbWritten, LPOVERLAPPED lpOverLap)
{
    LPPIPEINST lpPipeInst;
    BOOL fRead = FALSE;

    // lpOverlap points to storage for this instance. 

    lpPipeInst = (LPPIPEINST)lpOverLap;

    // The write operation has finished, so read the next request (if 
    // there is no error). 

    if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))
        fRead = ReadFileEx(
            lpPipeInst->hPipeInst,
            lpPipeInst->chRequest,
            BUFSIZE * sizeof(TCHAR),
            (LPOVERLAPPED)lpPipeInst,
            (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);

    std::wcout << "reading:" << std::wstring(lpPipeInst->chRequest) << std::endl;

    if (!fRead)
        DisconnectAndClose(lpPipeInst);
}

// CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED) 
// This routine is called as an I/O completion routine after reading 
// a request from the client. It gets data and writes it to the pipe. 

VOID WINAPI CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead, LPOVERLAPPED lpOverLap)
{
    LPPIPEINST lpPipeInst;
    BOOL fWrite = FALSE;

    // lpOverlap points to storage for this instance. 

    lpPipeInst = (LPPIPEINST)lpOverLap;

    // The read operation has finished, so write a response (if no 
    // error occurred). 

    if ((dwErr == 0) && (cbBytesRead != 0)) {
        GetAnswerToRequest(lpPipeInst);

        fWrite = WriteFileEx(
            lpPipeInst->hPipeInst,
            lpPipeInst->chReply,
            lpPipeInst->cbToWrite,
            (LPOVERLAPPED)lpPipeInst,
            (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);
    }

    if (!fWrite)
        DisconnectAndClose(lpPipeInst);
}

// DisconnectAndClose(LPPIPEINST) 
// This routine is called when an error occurs or the client closes 
// its handle to the pipe. 

VOID DisconnectAndClose(LPPIPEINST lpPipeInst)
{
    if (!DisconnectNamedPipe(lpPipeInst->hPipeInst)){
        printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
    }

    CloseHandle(lpPipeInst->hPipeInst);

    if (lpPipeInst != NULL)
        GlobalFree(lpPipeInst);
}

// CreateAndConnectInstance(LPOVERLAPPED) 
// This function creates a pipe instance and connects to the client. 
// It returns TRUE if the connect operation is pending, and FALSE if 
// the connection has been completed. 

BOOL CreateAndConnectInstance(LPOVERLAPPED lpoOverlap)
{
    const wchar_t * lpszPipename = TEXT("\\\\.\\pipe\\cameyoNamedPipe");

    hPipe = CreateNamedPipe(
        lpszPipename,             // pipe name 
        PIPE_ACCESS_DUPLEX |      // read/write access 
        FILE_FLAG_OVERLAPPED,     // overlapped mode 
        PIPE_TYPE_MESSAGE |       // message-type pipe 
        PIPE_READMODE_MESSAGE |   // message read mode 
        PIPE_WAIT,                // blocking mode 
        PIPE_UNLIMITED_INSTANCES, // unlimited instances 
        BUFSIZE * sizeof(TCHAR),    // output buffer size 
        BUFSIZE * sizeof(TCHAR),    // input buffer size 
        PIPE_TIMEOUT,             // client time-out 
        NULL);                    // default security attributes
    if (hPipe == INVALID_HANDLE_VALUE)
    {
        printf("CreateNamedPipe failed with %d.\n", GetLastError());
        return 0;
    }

    // Call a subroutine to connect to the new client. 

    return ConnectToNewClient(hPipe, lpoOverlap);
}

BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
    BOOL fConnected, fPendingIO = FALSE;

    // Start an overlapped connection for this pipe instance. 
    fConnected = ConnectNamedPipe(hPipe, lpo);

    if (fConnected){
        printf("ConnectNamedPipe failed with %d.\n", GetLastError());
        return 0;
    }

    switch (GetLastError())
    {
        // The overlapped connection in progress. 
    case ERROR_IO_PENDING:
        fPendingIO = TRUE;
        break;

        // Client is already connected, so signal an event. 

    case ERROR_PIPE_CONNECTED:
        if (SetEvent(lpo->hEvent))
            break;

        // If an error occurs during the connect operation... 
    default:
    {
        printf("ConnectNamedPipe failed with %d.\n", GetLastError());
        return 0;
    }
    }
    return fPendingIO;
}

VOID GetAnswerToRequest(LPPIPEINST pipe)
{
    _tprintf(TEXT("[%d] %s\n"), (int)pipe->hPipeInst, pipe->chRequest);
    StringCchCopy(pipe->chReply, BUFSIZE, TEXT("Default answer from server"));
    pipe->cbToWrite = (lstrlen(pipe->chReply) + 1) * sizeof(TCHAR);
}

int main()
{
    std::vector<std::wstring> procToHook{ _T("notepad.exe"), _T("notepad++.exe"), _T("wordpad.exe"), _T("chrome.exe"), _T("firefox.exe") };
    getProcList(procToHook);

    // Finding target window
	HWND hwnd = FindWindow(NULL, _T("testing.txt - Notepad"));
	if (hwnd == NULL) {
		std::cout << "[ FAILED ] Could not find target window." << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}

	// Getting the thread of the window and the PID
	DWORD pid = NULL;
	DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
	if (tid == NULL) {
		std::cout << "[ FAILED ] Could not get thread ID of the target window." << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}

	// Loading DLL
	HMODULE dll = LoadLibraryEx(_T("cameyoprjagent.dll"), NULL, DONT_RESOLVE_DLL_REFERENCES);
	if (dll == NULL) {
		std::cout << "[ FAILED ] The DLL could not be found." << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}

    // "C" __declspec(dllexport) int NextHook(int code, WPARAM wParam, LPARAM lParam)
	HOOKPROC addr = (HOOKPROC)GetProcAddress(dll, "NextHook"); 
	if (addr == NULL) {
		std::cout << "[ FAILED ] The function was not found." << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}

    CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(ThreadProc), NULL, 0, 0);

	// Setting the hook in the hook chain
	HHOOK handle = SetWindowsHookEx(WH_GETMESSAGE, addr, dll, tid); 
    // Or WH_KEYBOARD if you prefer to trigger the hook manually
	if (handle == NULL) {
		std::cout << "[ FAILED ] Couldn't set the hook with SetWindowsHookEx." << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}

	// Triggering the hook
	PostThreadMessage(tid, WM_NULL, NULL, NULL);

	// Waiting for user input to remove the hook
	std::cout << "[ OK ] Hook set and triggered." << std::endl;
	std::cout << "[ >> ] Press any key to unhook (This will unload the DLL)." << std::endl;
	system("pause > nul");

	// Unhooking
	BOOL unhook = UnhookWindowsHookEx(handle);
	if (unhook == FALSE) {
		std::cout << "[ FAILED ] Could not remove the hook." << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}

	std::cout << "[ OK ] Done. Press any key to exit." << std::endl;
	system("pause > nul");
	return EXIT_SUCCESS;
}