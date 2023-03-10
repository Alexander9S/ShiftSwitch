// https://github.com/MikalaiR/LSwitch

#include <stdio.h>
#include <windows.h>

static const bool USE_LOG_FILE = FALSE;
static const int LOG_MESSAGE_MAX_LENGTH = 1024 * 10;
static HANDLE g_hShutdownEvent = NULL;
static HHOOK g_hLowLevelKeyboardHook = NULL;
static HANDLE g_hLogFile = NULL;
static int g_nState = 0;

void WriteMessageToLogFile(const wchar_t* pwszFormat, va_list varargs)
{
    wchar_t wszMessage[LOG_MESSAGE_MAX_LENGTH];
    vswprintf_s(
        wszMessage,
        LOG_MESSAGE_MAX_LENGTH,
        pwszFormat,
        varargs
    );
    const int cbMultiByteLength = WideCharToMultiByte(
        CP_ACP,
        0,
        wszMessage,
        -1,
        NULL,
        0,
        NULL,
        FALSE
    );
    const int cbLogLineBufferSize = cbMultiByteLength + 3;
    char* pszLogLine = new char[cbLogLineBufferSize];
    memset(pszLogLine, 0, cbLogLineBufferSize);
    const int nBytesWritten = WideCharToMultiByte(
        CP_ACP,
        0,
        wszMessage,
        -1,
        pszLogLine,
        cbMultiByteLength,
        NULL,
        FALSE
    );
    strcat_s(pszLogLine, cbLogLineBufferSize, "\r\n");
    DWORD dwNumberOfBytesWritten = 0;
    WriteFile(
        g_hLogFile,
        pszLogLine,
        strlen(pszLogLine),
        &dwNumberOfBytesWritten,
        NULL
    );
    delete [] pszLogLine;
}

void LogMessage(const wchar_t* pwszFormat...)
{
    if (g_hLogFile == NULL)
        return;
    va_list varargs;
    va_start(varargs, pwszFormat);
    WriteMessageToLogFile(pwszFormat, varargs);
    FlushFileBuffers(g_hLogFile);
    va_end(varargs);
}

void ExitWithError(const wchar_t* pwszMessage)
{
    MessageBox(
        NULL,
        pwszMessage,
        L"Error",
        MB_OK | MB_ICONERROR
    );
    ExitProcess(1);
}

void CALLBACK ShutdownTimerCallback(
    HWND hWnd,
    UINT uMsg,
    UINT_PTR idEvent,
    DWORD dwTime
)
{
    if (WaitForSingleObject(g_hShutdownEvent, 0) == WAIT_OBJECT_0)
        PostQuitMessage(0);
}

void ClearState()
{
    g_nState = 0;
}

void SetState1()
{
    g_nState = 1;
}

bool HasState1()
{
    return g_nState == 1;
}

void SwitchLanguage()
{
    HWND hWndTarget = GetForegroundWindow();
    AttachThreadInput(
        GetCurrentThreadId(),
        GetWindowThreadProcessId(hWndTarget, NULL),
        TRUE
    );
    const HWND hWndThreadKeyboardFocus = GetFocus();
    if (hWndThreadKeyboardFocus)
        hWndTarget = hWndThreadKeyboardFocus;
    if (hWndTarget)
        PostMessage(
            hWndTarget,
            WM_INPUTLANGCHANGEREQUEST,
            0,
            HKL_NEXT
        );
}

LRESULT CALLBACK LowLevelKeyboardHookProc(
    int nCode,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* ks = (KBDLLHOOKSTRUCT*) lParam;
        // LogMessage(L"vkCode=%ld", ks->vkCode);
        if (
            ks->vkCode == VK_LSHIFT
            || ks->vkCode == VK_RSHIFT
        )
            if (
                (wParam == WM_KEYDOWN)
                && ((GetKeyState(VK_SHIFT) & 0x8000) == 0) // Shift
                && ((GetKeyState(VK_CONTROL) & 0x8000) == 0) // Ctrl
                && ((GetKeyState(VK_MENU) & 0x8000) == 0) // Alt
                && ((GetKeyState(VK_LWIN) & 0x8000) == 0)  // Left-Win
                && ((GetKeyState(VK_RWIN) & 0x8000) == 0)  // Right-Win
            )
                SetState1();
            else if (
                (wParam == WM_KEYUP)
                && HasState1()
            )
            {
                SwitchLanguage();
                ClearState();
                // return 1;
            }
            else
                ClearState();
        else
            ClearState();
    }
    return CallNextHookEx(
        g_hLowLevelKeyboardHook,
        nCode,
        wParam,
        lParam
    );
}

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
)
{
    g_hShutdownEvent = CreateEvent(
        NULL,
        TRUE,
        FALSE,
        L"ShiftSwitchShutdownEvent"
    );
    if (g_hShutdownEvent == NULL)
        ExitWithError(L"CreateEvent()");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        SetEvent(g_hShutdownEvent);
        goto QUIT;
    }
    if (
        SetTimer(
            NULL,
            0,
            500,
            ShutdownTimerCallback
        ) == 0
    )
        ExitWithError(L"SetTimer()");
    g_hLowLevelKeyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardHookProc,
        GetModuleHandle(0),
        0
    );
    if (!g_hLowLevelKeyboardHook)
        ExitWithError(L"SetWindowsHookEx()");
    if (USE_LOG_FILE)
        g_hLogFile = CreateFile(
            L"ShiftSwitch.log",
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(g_hLowLevelKeyboardHook);
    if (g_hLogFile != NULL)
        CloseHandle(g_hLogFile);
QUIT:
    CloseHandle(g_hShutdownEvent);
    ExitProcess(0);
}
