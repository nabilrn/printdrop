#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <process.h>

#include "printdrop/qr.h"
#include "printdrop/receiver_session.h"
#include "printdrop/session_url.h"
#include "printdrop/win32_random.h"
#include "printdrop/win32_receiver_runtime.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#define PD_UI_RELAY_CAPACITY 256U
#define PD_UI_URL_CAPACITY 512U
#define PD_UI_SESSION_TTL_MS UINT64_C(300000)
#define PD_WM_RUNTIME_EVENT (WM_APP + 1U)
#define PD_WM_RUNTIME_DONE (WM_APP + 2U)

static const wchar_t pd_window_class[] = L"PrintDropWindowClass";
static const char pd_default_relay_base[] = "https://send.printdrop.app";

typedef enum pd_ui_state {
    PD_UI_CONNECTING = 0,
    PD_UI_READY,
    PD_UI_RECEIVING,
    PD_UI_VERIFYING,
    PD_UI_COMPLETE,
    PD_UI_FAILED
} pd_ui_state;

typedef struct pd_ui_event_message {
    pd_receiver_loop_event event;
    uint64_t received_bytes;
    uint64_t total_bytes;
} pd_ui_event_message;

typedef struct pd_app {
    HWND window;
    HANDLE worker;
    pd_receiver_session session;
    pd_qr_code qr;
    char relay_base[PD_UI_RELAY_CAPACITY];
    char public_url[PD_UI_URL_CAPACITY];
    wchar_t public_url_wide[PD_UI_URL_CAPACITY];
    wchar_t jobs_root[MAX_PATH];
    pd_ui_state state;
    pd_win32_receiver_runtime_result runtime_result;
    uint64_t received_bytes;
    uint64_t total_bytes;
    bool qr_valid;
} pd_app;

static bool pd_copy_bytes(char *destination, size_t capacity, const char *source, size_t length)
{
    if (destination == NULL || source == NULL || length + 1U > capacity) {
        return false;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}

static bool pd_load_relay_base(char output[PD_UI_RELAY_CAPACITY])
{
    DWORD length = GetEnvironmentVariableA("PRINTDROP_BASE_URL",
                                           output,
                                           (DWORD)PD_UI_RELAY_CAPACITY);
    if (length == 0U) {
        return pd_copy_bytes(output,
                             (size_t)PD_UI_RELAY_CAPACITY,
                             pd_default_relay_base,
                             sizeof(pd_default_relay_base) - 1U);
    }
    return length < (DWORD)PD_UI_RELAY_CAPACITY;
}

static bool pd_prepare_jobs_root(wchar_t output[MAX_PATH])
{
    static const wchar_t component[] = L"\\PrintDrop";
    wchar_t documents[MAX_PATH];
    size_t document_length;

    if (SHGetFolderPathW(NULL,
                         CSIDL_PERSONAL | CSIDL_FLAG_CREATE,
                         NULL,
                         SHGFP_TYPE_CURRENT,
                         documents) != S_OK) {
        return false;
    }

    document_length = wcslen(documents);
    if (document_length + (sizeof(component) / sizeof(component[0])) > (size_t)MAX_PATH) {
        return false;
    }

    memcpy(output, documents, (document_length + 1U) * sizeof(wchar_t));
    return wcscat_s(output, MAX_PATH, component) == 0;
}

static bool pd_build_session_prefix(const char *relay_base,
                                    char output[PD_UI_RELAY_CAPACITY])
{
    static const char suffix[] = "/s/";
    size_t length;
    size_t required;

    if (relay_base == NULL) {
        return false;
    }
    length = strlen(relay_base);
    while (length > 0U && relay_base[length - 1U] == '/') {
        --length;
    }
    required = length + (sizeof(suffix) - 1U) + 1U;
    if (length == 0U || required > (size_t)PD_UI_RELAY_CAPACITY) {
        return false;
    }

    memcpy(output, relay_base, length);
    memcpy(&output[length], suffix, sizeof(suffix));
    return true;
}

static bool pd_prepare_session(pd_app *app)
{
    char prefix[PD_UI_RELAY_CAPACITY];

    if (app == NULL || !pd_load_relay_base(app->relay_base) ||
        !pd_prepare_jobs_root(app->jobs_root)) {
        return false;
    }

    if (pd_receiver_session_create(&app->session,
                                   (uint64_t)GetTickCount64(),
                                   PD_UI_SESSION_TTL_MS,
                                   pd_win32_random_fill,
                                   NULL) != PD_RECEIVER_SESSION_OK) {
        return false;
    }

    if (!pd_build_session_prefix(app->relay_base, prefix) ||
        pd_receiver_session_build_url(&app->session,
                                      prefix,
                                      app->public_url,
                                      sizeof(app->public_url),
                                      NULL) != PD_SESSION_URL_OK ||
        pd_qr_encode_text(&app->qr, app->public_url) != PD_QR_OK) {
        pd_receiver_session_close(&app->session);
        return false;
    }

    if (MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            app->public_url,
                            -1,
                            app->public_url_wide,
                            (int)PD_UI_URL_CAPACITY) == 0) {
        pd_receiver_session_close(&app->session);
        return false;
    }

    app->qr_valid = true;
    app->state = PD_UI_CONNECTING;
    app->runtime_result = PD_WIN32_RECEIVER_RUNTIME_OK;
    return true;
}

static pd_app *pd_app_from_window(HWND window)
{
    return (pd_app *)(LONG_PTR)GetWindowLongPtrW(window, GWLP_USERDATA);
}

static void pd_post_runtime_event(void *context,
                                  pd_receiver_loop_event event,
                                  uint64_t received_bytes,
                                  uint64_t total_bytes)
{
    pd_app *app = (pd_app *)context;
    pd_ui_event_message *message;

    if (app == NULL || app->window == NULL) {
        return;
    }

    message = (pd_ui_event_message *)HeapAlloc(GetProcessHeap(),
                                               HEAP_ZERO_MEMORY,
                                               sizeof(*message));
    if (message == NULL) {
        return;
    }
    message->event = event;
    message->received_bytes = received_bytes;
    message->total_bytes = total_bytes;

    if (PostMessageW(app->window, PD_WM_RUNTIME_EVENT, 0U, (LPARAM)message) == 0) {
        HeapFree(GetProcessHeap(), 0U, message);
    }
}

static unsigned __stdcall pd_receiver_worker(void *context)
{
    pd_app *app = (pd_app *)context;
    pd_win32_receiver_runtime_result result;

    result = pd_win32_receiver_run_session(app->relay_base,
                                           &app->session,
                                           app->jobs_root,
                                           pd_post_runtime_event,
                                           app);
    if (app->window != NULL) {
        (void)PostMessageW(app->window, PD_WM_RUNTIME_DONE, (WPARAM)result, 0);
    }
    return 0U;
}

static bool pd_start_receiver(pd_app *app)
{
    uintptr_t thread;

    if (app == NULL || app->window == NULL) {
        return false;
    }
    thread = _beginthreadex(NULL, 0U, pd_receiver_worker, app, 0U, NULL);
    if (thread == 0U) {
        return false;
    }
    app->worker = (HANDLE)thread;
    return true;
}

static const wchar_t *pd_state_text(const pd_app *app)
{
    if (app == NULL) {
        return L"Unknown";
    }
    switch (app->state) {
    case PD_UI_CONNECTING:
        return L"Connecting to relay";
    case PD_UI_READY:
        return L"Ready - scan the QR code";
    case PD_UI_RECEIVING:
        return L"Receiving file";
    case PD_UI_VERIFYING:
        return L"Verifying SHA-256";
    case PD_UI_COMPLETE:
        return L"Complete - file saved";
    case PD_UI_FAILED:
        switch (app->runtime_result) {
        case PD_WIN32_RECEIVER_RUNTIME_REGISTRATION_ERROR:
            return L"Relay registration failed";
        case PD_WIN32_RECEIVER_RUNTIME_RELAY_ERROR:
            return L"Relay connection failed";
        case PD_WIN32_RECEIVER_RUNTIME_STORAGE_ERROR:
            return L"Could not write the file";
        case PD_WIN32_RECEIVER_RUNTIME_PROTOCOL_ERROR:
            return L"Transfer protocol failed";
        case PD_WIN32_RECEIVER_RUNTIME_ENDPOINT_ERROR:
            return L"Relay URL is invalid";
        default:
            return L"PrintDrop failed to start";
        }
    default:
        return L"Unknown";
    }
}

static void pd_draw_qr(HDC device_context, const pd_qr_code *qr, const RECT *area)
{
    const int quiet_modules = 4;
    int qr_size;
    int available_width;
    int available_height;
    int module_pixels;
    int rendered_size;
    int origin_x;
    int origin_y;
    int x;
    int y;
    HBRUSH black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);

    if (device_context == NULL || qr == NULL || area == NULL) {
        return;
    }
    qr_size = pd_qr_size(qr);
    if (qr_size <= 0) {
        return;
    }

    available_width = area->right - area->left;
    available_height = area->bottom - area->top;
    module_pixels = available_width / (qr_size + (quiet_modules * 2));
    if (available_height / (qr_size + (quiet_modules * 2)) < module_pixels) {
        module_pixels = available_height / (qr_size + (quiet_modules * 2));
    }
    if (module_pixels < 1) {
        return;
    }

    rendered_size = module_pixels * (qr_size + (quiet_modules * 2));
    origin_x = area->left + (available_width - rendered_size) / 2 + quiet_modules * module_pixels;
    origin_y = area->top + (available_height - rendered_size) / 2 + quiet_modules * module_pixels;

    for (y = 0; y < qr_size; ++y) {
        for (x = 0; x < qr_size; ++x) {
            if (pd_qr_get_module(qr, x, y)) {
                RECT module = {
                    origin_x + x * module_pixels,
                    origin_y + y * module_pixels,
                    origin_x + (x + 1) * module_pixels,
                    origin_y + (y + 1) * module_pixels,
                };
                FillRect(device_context, &module, black_brush);
            }
        }
    }
}

static void pd_draw_text(HDC device_context,
                         const wchar_t *text,
                         RECT *rect,
                         int height,
                         int weight,
                         COLORREF color,
                         UINT flags)
{
    HFONT font;
    HFONT previous;

    font = CreateFontW(-height,
                       0,
                       0,
                       0,
                       weight,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS,
                       L"Segoe UI");
    if (font == NULL) {
        return;
    }
    previous = (HFONT)SelectObject(device_context, font);
    SetTextColor(device_context, color);
    SetBkMode(device_context, TRANSPARENT);
    DrawTextW(device_context, text, -1, rect, flags);
    (void)SelectObject(device_context, previous);
    DeleteObject(font);
}

static void pd_draw_progress(HDC device_context, const pd_app *app, RECT area)
{
    HBRUSH border = (HBRUSH)GetStockObject(BLACK_BRUSH);
    HBRUSH fill = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RECT inside = area;
    RECT filled = area;
    double ratio = 0.0;
    wchar_t label[96];
    RECT label_area;
    unsigned int percent = 0U;

    if (app->total_bytes != UINT64_C(0)) {
        ratio = (double)app->received_bytes / (double)app->total_bytes;
        if (ratio > 1.0) {
            ratio = 1.0;
        }
        percent = (unsigned int)(ratio * 100.0 + 0.5);
    }

    FrameRect(device_context, &area, border);
    InflateRect(&inside, -2, -2);
    filled = inside;
    filled.right = filled.left + (int)((double)(inside.right - inside.left) * ratio);
    if (filled.right > filled.left) {
        FillRect(device_context, &filled, fill);
    }

    (void)swprintf_s(label,
                     sizeof(label) / sizeof(label[0]),
                     L"%u%%   %llu / %llu bytes",
                     percent,
                     (unsigned long long)app->received_bytes,
                     (unsigned long long)app->total_bytes);
    label_area = area;
    label_area.top = area.bottom + 8;
    label_area.bottom = label_area.top + 28;
    pd_draw_text(device_context,
                 label,
                 &label_area,
                 14,
                 FW_NORMAL,
                 RGB(90, 90, 90),
                 DT_LEFT | DT_SINGLELINE);
}

static void pd_paint(HWND window, pd_app *app)
{
    PAINTSTRUCT paint;
    HDC device_context;
    RECT client;
    RECT title;
    RECT qr_area;
    RECT status_area;
    RECT hint_area;
    RECT url_area;
    RECT path_area;
    RECT progress_area;
    HBRUSH background = (HBRUSH)GetStockObject(WHITE_BRUSH);
    bool show_qr;

    device_context = BeginPaint(window, &paint);
    GetClientRect(window, &client);
    FillRect(device_context, &client, background);

    title = client;
    title.left += 32;
    title.top += 24;
    title.bottom = title.top + 44;
    pd_draw_text(device_context,
                 L"PrintDrop",
                 &title,
                 30,
                 FW_BOLD,
                 RGB(20, 20, 20),
                 DT_LEFT | DT_SINGLELINE);

    qr_area.left = 32;
    qr_area.top = 92;
    qr_area.right = client.right / 2;
    qr_area.bottom = client.bottom - 52;
    show_qr = app != NULL && app->qr_valid &&
              (app->state == PD_UI_READY || app->state == PD_UI_RECEIVING ||
               app->state == PD_UI_VERIFYING);
    if (show_qr) {
        pd_draw_qr(device_context, &app->qr, &qr_area);
    } else {
        RECT placeholder = qr_area;
        pd_draw_text(device_context,
                     app != NULL && app->state == PD_UI_COMPLETE ? L"Transfer complete" :
                     app != NULL && app->state == PD_UI_FAILED ? L"Receiver unavailable" :
                                                                  L"Preparing QR code...",
                     &placeholder,
                     18,
                     FW_SEMIBOLD,
                     RGB(70, 70, 70),
                     DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    }

    status_area.left = client.right / 2 + 24;
    status_area.right = client.right - 32;
    status_area.top = 120;
    status_area.bottom = status_area.top + 72;
    pd_draw_text(device_context,
                 pd_state_text(app),
                 &status_area,
                 22,
                 FW_SEMIBOLD,
                 app != NULL && app->state == PD_UI_FAILED ? RGB(150, 25, 25) : RGB(20, 20, 20),
                 DT_LEFT | DT_WORDBREAK);

    hint_area = status_area;
    hint_area.top = status_area.bottom + 8;
    hint_area.bottom = hint_area.top + 54;
    pd_draw_text(device_context,
                 L"Scan with Android or iPhone. No login, WhatsApp, or cable required.",
                 &hint_area,
                 15,
                 FW_NORMAL,
                 RGB(90, 90, 90),
                 DT_LEFT | DT_WORDBREAK);

    url_area = hint_area;
    url_area.top = hint_area.bottom + 18;
    url_area.bottom = url_area.top + 54;
    if (app != NULL && app->public_url_wide[0] != L'\0') {
        pd_draw_text(device_context,
                     app->public_url_wide,
                     &url_area,
                     13,
                     FW_NORMAL,
                     RGB(80, 80, 80),
                     DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    }

    progress_area.left = status_area.left;
    progress_area.right = status_area.right;
    progress_area.top = url_area.bottom + 24;
    progress_area.bottom = progress_area.top + 14;
    if (app != NULL && (app->state == PD_UI_RECEIVING || app->state == PD_UI_VERIFYING ||
                        app->state == PD_UI_COMPLETE)) {
        pd_draw_progress(device_context, app, progress_area);
    }

    path_area.left = status_area.left;
    path_area.right = status_area.right;
    path_area.top = client.bottom - 72;
    path_area.bottom = client.bottom - 28;
    pd_draw_text(device_context,
                 L"Received files: Documents\\PrintDrop",
                 &path_area,
                 13,
                 FW_NORMAL,
                 RGB(110, 110, 110),
                 DT_LEFT | DT_WORDBREAK);

    EndPaint(window, &paint);
}

static void pd_apply_runtime_event(pd_app *app, const pd_ui_event_message *message)
{
    if (app == NULL || message == NULL) {
        return;
    }

    app->received_bytes = message->received_bytes;
    app->total_bytes = message->total_bytes;
    switch (message->event) {
    case PD_RECEIVER_LOOP_READY:
        app->state = PD_UI_READY;
        break;
    case PD_RECEIVER_LOOP_FILE_STARTED:
    case PD_RECEIVER_LOOP_PROGRESS:
        app->state = PD_UI_RECEIVING;
        break;
    case PD_RECEIVER_LOOP_VERIFYING:
        app->state = PD_UI_VERIFYING;
        break;
    case PD_RECEIVER_LOOP_COMPLETE:
        app->state = PD_UI_COMPLETE;
        break;
    case PD_RECEIVER_LOOP_FAILED:
        app->state = PD_UI_FAILED;
        break;
    default:
        break;
    }
}

static LRESULT CALLBACK pd_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    pd_app *app = pd_app_from_window(window);

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        app = (pd_app *)create->lpCreateParams;
        if (app == NULL) {
            return FALSE;
        }
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)app);
        return TRUE;
    }
    case PD_WM_RUNTIME_EVENT: {
        pd_ui_event_message *runtime_event = (pd_ui_event_message *)lparam;
        if (app != NULL && runtime_event != NULL) {
            pd_apply_runtime_event(app, runtime_event);
            InvalidateRect(window, NULL, TRUE);
        }
        if (runtime_event != NULL) {
            HeapFree(GetProcessHeap(), 0U, runtime_event);
        }
        return 0;
    }
    case PD_WM_RUNTIME_DONE:
        if (app != NULL) {
            app->runtime_result = (pd_win32_receiver_runtime_result)wparam;
            if (app->runtime_result != PD_WIN32_RECEIVER_RUNTIME_OK) {
                app->state = PD_UI_FAILED;
            }
            if (app->worker != NULL) {
                CloseHandle(app->worker);
                app->worker = NULL;
            }
            InvalidateRect(window, NULL, TRUE);
        }
        return 0;
    case WM_PAINT:
        pd_paint(window, app);
        return 0;
    case WM_SIZE:
        InvalidateRect(window, NULL, TRUE);
        return 0;
    case WM_DESTROY:
        if (app != NULL && app->worker != NULL) {
            CloseHandle(app->worker);
            app->worker = NULL;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command)
{
    WNDCLASSW window_class = {0};
    pd_app app = {0};
    HWND window;
    MSG message;

    (void)previous_instance;
    (void)command_line;

    SetProcessDPIAware();
    if (!pd_prepare_session(&app)) {
        MessageBoxW(NULL,
                    L"PrintDrop could not prepare a secure receive session.",
                    L"PrintDrop",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = pd_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = pd_window_class;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (RegisterClassW(&window_class) == 0) {
        pd_receiver_session_close(&app.session);
        return 1;
    }

    window = CreateWindowExW(0,
                             pd_window_class,
                             L"PrintDrop",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             780,
                             540,
                             NULL,
                             NULL,
                             instance,
                             &app);
    if (window == NULL) {
        pd_receiver_session_close(&app.session);
        return 1;
    }

    ShowWindow(window, show_command);
    UpdateWindow(window);

    if (!pd_start_receiver(&app)) {
        app.runtime_result = PD_WIN32_RECEIVER_RUNTIME_RELAY_ERROR;
        app.state = PD_UI_FAILED;
        InvalidateRect(window, NULL, TRUE);
    }

    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return (int)message.wParam;
}
