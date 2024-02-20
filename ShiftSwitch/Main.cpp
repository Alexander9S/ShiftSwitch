// Based on
// https://github.com/MikalaiR/LSwitch
// https://stackoverflow.com/questions/27720728/cant-send-wm-inputlangchangerequest-to-some-controls

#include <cstdio>
#include "Windows.h"

static constexpr bool b_use_logging = FALSE;
static constexpr int cc_log_message_max_length = 1024;
static constexpr int cb_trail_size = 3; // \r \n 0
static constexpr int cb_log_line_buffer_size = cc_log_message_max_length + cb_trail_size; // Buffer for ANSI chars
static HANDLE g_h_shutdown_event = nullptr;
static HHOOK g_h_low_level_keyboard_hook = nullptr;
static HANDLE g_h_log_file = nullptr;
static int g_n_state = 0;
static wchar_t g_wsz_message[cc_log_message_max_length];
static char g_sz_log_line[cb_log_line_buffer_size];

// ReSharper disable once CppParameterMayBeConst
void write_message_to_log_file(const wchar_t* pwsz_format, va_list varargs)
{
    vswprintf_s(
        g_wsz_message,
        cc_log_message_max_length,
        pwsz_format,
        varargs
    );
    const int cb_multi_byte_length = WideCharToMultiByte(
        CP_ACP,
        0,
        g_wsz_message,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );
    int cb_current_log_line_size = cb_multi_byte_length + cb_trail_size;
    if (cb_current_log_line_size > cb_log_line_buffer_size)
    {
        cb_current_log_line_size = cb_log_line_buffer_size;
    }
    memset(g_sz_log_line, 0, cb_current_log_line_size);
    // ReSharper disable once CppDeclaratorNeverUsed
    const int n_bytes_written = WideCharToMultiByte(
        CP_ACP,
        0,
        g_wsz_message,
        -1,
        g_sz_log_line,
        cb_current_log_line_size - cb_trail_size,
        nullptr,
        nullptr
    );
    strcat_s(g_sz_log_line, cb_current_log_line_size, "\r\n");
    DWORD dw_number_of_bytes_written = 0;
    WriteFile(
        g_h_log_file,
        g_sz_log_line,
        static_cast<DWORD>(strlen(g_sz_log_line)),
        &dw_number_of_bytes_written,
        nullptr
    );
}

void log_message(const wchar_t* pwsz_format...)
{
    if (g_h_log_file == nullptr)
    {
        return;
    }
    va_list varargs;
    va_start(varargs, pwsz_format);
    write_message_to_log_file(pwsz_format, varargs);
    FlushFileBuffers(g_h_log_file);
    va_end(varargs);
}

void exit_with_error(const wchar_t* pwsz_message)
{ // NOLINT(clang-diagnostic-missing-noreturn)
    MessageBox(
        nullptr,
        pwsz_message,
        L"Error",
        MB_OK | MB_ICONERROR
    );
    ExitProcess(1);
}

void CALLBACK shutdown_timer_callback(
    HWND h_wnd,
    UINT u_msg,
    UINT_PTR id_event,
    DWORD dw_time
)
{
    if (WaitForSingleObject(g_h_shutdown_event, 0) == WAIT_OBJECT_0)
    {
        PostQuitMessage(0);
    }
}

void reset_state_to_zero()
{
    g_n_state = 0;
}

void set_state_to_1()
{
    g_n_state = 1;
}

bool has_state_1()
{
    return g_n_state == 1;
}

/*
void switch_language()
{
    HWND h_wnd_target = GetForegroundWindow();
    AttachThreadInput(
        GetCurrentThreadId(),
        GetWindowThreadProcessId(h_wnd_target, nullptr),
        TRUE
    );
    const HWND h_wnd_thread_keyboard_focus = GetFocus();  // NOLINT(misc-misplaced-const)
    if (h_wnd_thread_keyboard_focus != nullptr)
    {
        h_wnd_target = h_wnd_thread_keyboard_focus;
    }
    if (h_wnd_target != nullptr)
    {
        PostMessage(
            h_wnd_target,
            WM_INPUTLANGCHANGEREQUEST,
            0,
            HKL_NEXT
        );
    }
}
*/

void set_next_keyboard_layout()
{
    HWND h_wnd_target = nullptr;
    const DWORD dw_foreground_thread_id = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    const DWORD dw_current_thread_id = GetCurrentThreadId();
    GUITHREADINFO gui_thread_info{};
    gui_thread_info.cbSize = sizeof gui_thread_info;
    const BOOL b_gui_thread_info_success = GetGUIThreadInfo(dw_foreground_thread_id, &gui_thread_info);
    AttachThreadInput(dw_foreground_thread_id, dw_current_thread_id, TRUE);
    const HWND h_wnd_thread_keyboard_focus = GetFocus();  // NOLINT(misc-misplaced-const)
    AttachThreadInput(dw_foreground_thread_id, dw_current_thread_id, FALSE);
    if (b_gui_thread_info_success)
    {
        if (gui_thread_info.hwndCaret != nullptr)
        {
            h_wnd_target = gui_thread_info.hwndCaret;
        }
        else if (gui_thread_info.hwndFocus != nullptr)
        {
            h_wnd_target = gui_thread_info.hwndFocus;
        }
        else if (h_wnd_thread_keyboard_focus != nullptr)
        {
            h_wnd_target = h_wnd_thread_keyboard_focus;
        }
        else if (gui_thread_info.hwndActive != nullptr)
        {
            h_wnd_target = gui_thread_info.hwndActive;
        }
    }
    else
    {
        h_wnd_target = h_wnd_thread_keyboard_focus;
    }
    if (h_wnd_target == nullptr)
    {
        h_wnd_target = GetForegroundWindow();
    }
    if (h_wnd_target != nullptr)
    {
        PostMessage(
            h_wnd_target,
            WM_INPUTLANGCHANGEREQUEST,
            INPUTLANGCHANGE_FORWARD,
            HKL_NEXT
        );
    }
}

LRESULT CALLBACK low_level_keyboard_hook_proc(
    const int n_code,
    const WPARAM w_param,
    const LPARAM l_param
)
{
    if (n_code == HC_ACTION)
    {
        const auto khs = reinterpret_cast<KBDLLHOOKSTRUCT*>(l_param); // NOLINT(performance-no-int-to-ptr)
        if (
            khs->vkCode == VK_LSHIFT
            || khs->vkCode == VK_RSHIFT
        )
            if (
                w_param == WM_KEYDOWN
                && (GetKeyState(VK_SHIFT) & 0x8000) == 0 // Shift
                && (GetKeyState(VK_CONTROL) & 0x8000) == 0 // Ctrl
                && (GetKeyState(VK_MENU) & 0x8000) == 0 // Alt
                && (GetKeyState(VK_LWIN) & 0x8000) == 0  // Left-Win
                && (GetKeyState(VK_RWIN) & 0x8000) == 0  // Right-Win
            )
                set_state_to_1();
            else if (
                w_param == WM_KEYUP
                && has_state_1()
            )
            {
                // log_message(L"vkCode=%ld", khs->vkCode);
                set_next_keyboard_layout();
                reset_state_to_zero();
            }
            else
                reset_state_to_zero();
        else
            reset_state_to_zero();
    }
    return CallNextHookEx(
        g_h_low_level_keyboard_hook,
        n_code,
        w_param,
        l_param
    );
}

int WINAPI WinMain(
    // ReSharper disable CppInconsistentNaming
    // ReSharper disable CppParameterNeverUsed
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd
    // ReSharper restore CppInconsistentNaming
    // ReSharper restore CppParameterNeverUsed
)
{
    g_h_shutdown_event = CreateEvent(
        nullptr,
        TRUE,
        FALSE,
        L"ShiftSwitchShutdownEvent"
    );
    if (g_h_shutdown_event == nullptr)
    {
        exit_with_error(L"CreateEvent()");
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (g_h_shutdown_event != nullptr)
        {
            SetEvent(g_h_shutdown_event);
        }
        goto QUIT;  // NOLINT(cppcoreguidelines-avoid-goto, hicpp-avoid-goto)
    }
    if (
        SetTimer(
            nullptr,
            0,
            500,
            shutdown_timer_callback
        ) == 0
    )
    {
        exit_with_error(L"SetTimer()");
    }
    g_h_low_level_keyboard_hook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        low_level_keyboard_hook_proc,
        GetModuleHandle(nullptr),
        0
    );
    if (!g_h_low_level_keyboard_hook)
    {
        exit_with_error(L"SetWindowsHookEx()");
    }
    if (b_use_logging)
    // ReSharper disable once CppUnreachableCode
    {
        g_h_log_file = CreateFile(
            L"ShiftSwitch.log",
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
    }
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (g_h_low_level_keyboard_hook != nullptr)
    {
        UnhookWindowsHookEx(g_h_low_level_keyboard_hook);
    }
    if (g_h_log_file != nullptr)
    {
        CloseHandle(g_h_log_file);
    }
QUIT:
    if (g_h_shutdown_event != nullptr)
    {
        CloseHandle(g_h_shutdown_event);
    }
    ExitProcess(0);
}
