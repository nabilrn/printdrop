#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static const wchar_t pd_window_class[] = L"PrintDropWindowClass";

static LRESULT CALLBACK pd_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        RECT client_rect;
        HDC device_context = BeginPaint(window, &paint);
        GetClientRect(window, &client_rect);
        DrawTextW(
            device_context,
            L"PrintDrop\n\nFoundation build\nNative receiver is ready for implementation.",
            -1,
            &client_rect,
            DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command)
{
    WNDCLASSW window_class = {0};
    HWND window;
    MSG message;

    (void)previous_instance;
    (void)command_line;

    window_class.lpfnWndProc = pd_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = pd_window_class;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (RegisterClassW(&window_class) == 0) {
        return 1;
    }

    window = CreateWindowExW(
        0,
        pd_window_class,
        L"PrintDrop",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        480,
        NULL,
        NULL,
        instance,
        NULL);

    if (window == NULL) {
        return 1;
    }

    ShowWindow(window, show_command);
    UpdateWindow(window);

    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return (int)message.wParam;
}
