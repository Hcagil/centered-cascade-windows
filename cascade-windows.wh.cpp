// ==WindhawkMod==
// @id              cascade-windows-personal
// @name            Centered Cascade Windows
// @description     Center app windows in taskbar icon order
// @version         1.9
// @author          Local custom mod
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lshell32 -lole32 -loleaut32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
Ordinary app windows already open are arranged into a centered diagonal cascade.
Windows follow their taskbar icons from left to right. Windows under one icon
stay together, oldest first. Dragging an icon reorders the cascade within two
seconds. The whole group stays centered. Closing or minimizing a window recenters;
restoring it adds it back. The Nahimic audio app, dialogs, tool windows, and
maximized windows are left alone. The steps shrink evenly when needed to keep
all windows on screen.
Window sizes are based on a 3000 x 2000 display and scaled for other monitors.
Disable the mod in Windhawk to stop it.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- width: 2500
  $name: Window width (pixels)
- height: 1550
  $name: Window height (pixels)
- stepX: 40
  $name: Shift each window right (pixels)
- stepY: 30
  $name: Shift each window down (pixels)
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <initguid.h>
#include <shellapi.h>
#include <appmodel.h>
#include <propkey.h>
#include <propsys.h>
#include <uiautomation.h>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
constexpr UINT kReloadSettings = WM_APP + 1;
constexpr UINT kDelayMs = 400;

struct Settings {
    int width, height, stepX, stepY;
};

Settings g_settings;
HANDLE g_thread = nullptr;
DWORD g_threadId = 0;
HWINEVENTHOOK g_showHook = nullptr;
HWINEVENTHOOK g_destroyHook = nullptr;
HWINEVENTHOOK g_minimizeHook = nullptr;
std::unordered_set<HWND> g_known;
std::unordered_map<HWND, ULONGLONG> g_pending;
std::vector<HWND> g_order;
ULONGLONG g_reflowAt = 0;
std::vector<std::wstring> g_taskbarOrder;
IUIAutomation* g_automation = nullptr;

BOOL CALLBACK FindTaskbarBridge(HWND hwnd, LPARAM result) {
    wchar_t className[128] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) &&
        lstrcmpW(className,
                 L"Windows.UI.Composition.DesktopWindowContentBridge") == 0) {
        *reinterpret_cast<HWND*>(result) = hwnd;
        return FALSE;
    }
    return TRUE;
}

std::vector<std::wstring> ReadTaskbarOrder() {
    std::vector<std::pair<int, std::wstring>> buttons;
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    HWND bridge = nullptr;
    if (taskbar) EnumChildWindows(taskbar, FindTaskbarBridge,
                                  reinterpret_cast<LPARAM>(&bridge));
    if (!bridge || !g_automation) return {};

    IUIAutomationElement* root = nullptr;
    IUIAutomationCondition* condition = nullptr;
    IUIAutomationElementArray* elements = nullptr;
    if (SUCCEEDED(g_automation->ElementFromHandle(bridge, &root)) && root &&
        SUCCEEDED(g_automation->CreateTrueCondition(&condition)) && condition &&
        SUCCEEDED(root->FindAll(TreeScope_Descendants, condition, &elements)) &&
        elements) {
        int count = 0;
        elements->get_Length(&count);
        for (int i = 0; i < count; ++i) {
            IUIAutomationElement* element = nullptr;
            if (FAILED(elements->GetElement(i, &element)) || !element) continue;
            BSTR id = nullptr;
            RECT rect = {};
            if (SUCCEEDED(element->get_CurrentAutomationId(&id)) && id &&
                wcsncmp(id, L"Appid: ", 7) == 0 &&
                SUCCEEDED(element->get_CurrentBoundingRectangle(&rect)) &&
                rect.right > rect.left) {
                buttons.emplace_back(rect.left, id + 7);
            }
            if (id) SysFreeString(id);
            element->Release();
        }
    }
    if (elements) elements->Release();
    if (condition) condition->Release();
    if (root) root->Release();
    std::stable_sort(buttons.begin(), buttons.end());
    std::vector<std::wstring> order;
    for (auto& [x, id] : buttons) order.push_back(std::move(id));
    return order;
}

std::wstring AppId(HWND hwnd) {
    wchar_t className[128] = {};
    GetClassNameW(hwnd, className, ARRAYSIZE(className));
    if (lstrcmpW(className, L"CabinetWClass") == 0)
        return L"Microsoft.Windows.Explorer";

    IPropertyStore* store = nullptr;
    if (SUCCEEDED(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store)))) {
        PROPVARIANT value = {};
        std::wstring id;
        if (SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID, &value)) &&
            value.vt == VT_LPWSTR && value.pwszVal) id = value.pwszVal;
        PropVariantClear(&value);
        store->Release();
        if (!id.empty()) return id;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    UINT32 length = 0;
    std::wstring id;
    if (GetApplicationUserModelId(process, &length, nullptr) ==
        ERROR_INSUFFICIENT_BUFFER && length) {
        id.resize(length);
        if (GetApplicationUserModelId(process, &length, id.data()) == ERROR_SUCCESS)
            id.resize(wcslen(id.c_str()));
        else id.clear();
    }
    CloseHandle(process);
    return id;
}

int Setting(const wchar_t* name, int fallback) {
    int value = Wh_GetIntSetting(name);
    return value >= 0 && value <= 10000 ? value : fallback;
}

void LoadSettings() {
    g_settings = {Setting(L"width", 2500), Setting(L"height", 1550),
                  Setting(L"stepX", 40), Setting(L"stepY", 30)};
}

BOOL CALLBACK RememberWindow(HWND hwnd, LPARAM) {
    if (IsWindowVisible(hwnd) && g_known.insert(hwnd).second) {
        g_order.push_back(hwnd);
    }
    return TRUE;
}

bool Eligible(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsZoomed(hwnd) ||
        IsIconic(hwnd) ||
        GetAncestor(hwnd, GA_ROOT) != hwnd || GetWindow(hwnd, GW_OWNER)) {
        return false;
    }
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((style & WS_CAPTION) != WS_CAPTION || (exStyle & WS_EX_TOOLWINDOW)) {
        return false;
    }
    wchar_t className[256];
    wchar_t title[256];
    if (!GetClassNameW(hwnd, className, ARRAYSIZE(className)) ||
        lstrcmpW(className, L"#32770") == 0 ||
        !GetWindowTextW(hwnd, title, ARRAYSIZE(title)) ||
        lstrcmpiW(title, L"Nahimic") == 0) {
        return false;
    }
    RECT rect;
    return GetWindowRect(hwnd, &rect) && rect.right - rect.left >= 300 &&
           rect.bottom - rect.top >= 200;
}

int Scaled(int value, int dimension, int reference) {
    return MulDiv(value, dimension, reference);
}

RECT Place(const RECT& monitor, const RECT& work, unsigned index,
           unsigned count) {
    const int monitorW = monitor.right - monitor.left;
    const int monitorH = monitor.bottom - monitor.top;
    const int workW = work.right - work.left;
    const int workH = work.bottom - work.top;
    const int width = std::min(Scaled(g_settings.width, monitorW, 3000),
                               std::max(300, workW - 40));
    const int height = std::min(Scaled(g_settings.height, monitorH, 2000),
                                std::max(200, workH - 40));
    const int dx = std::max(1, Scaled(g_settings.stepX, monitorW, 3000));
    const int dy = std::max(1, Scaled(g_settings.stepY, monitorH, 2000));
    const int gaps = std::max(1, static_cast<int>(count) - 1);
    // Even coordinates avoid one-pixel rounding in DPI-unaware apps at 200% scale.
    const int stepX = std::min(dx, (workW - width) / gaps) & ~1;
    const int stepY = std::min(dy, (workH - height) / gaps) & ~1;
    const int spanX = stepX * static_cast<int>(count - 1);
    const int spanY = stepY * static_cast<int>(count - 1);
    const int x0 = (work.left + (workW - width - spanX) / 2) & ~1;
    const int y0 = (work.top + (workH - height - spanY) / 2) & ~1;
    const int x = x0 + static_cast<int>(index) * stepX;
    const int y = y0 + static_cast<int>(index) * stepY;
    return {x, y, x + width, y + height};
}

void ArrangeAllWindows() {
    // A live scan repairs missed show/destroy events before assigning slots.
    EnumWindows(RememberWindow, 0);
    std::unordered_map<HMONITOR, std::vector<HWND>> byMonitor;
    for (HWND hwnd : g_order) {
        if (g_pending.contains(hwnd) || !Eligible(hwnd)) {
            continue;
        }
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        byMonitor[monitor].push_back(hwnd);
    }
    for (auto& [monitor, windows] : byMonitor) {
        MONITORINFO info = {sizeof(info)};
        if (!GetMonitorInfoW(monitor, &info)) {
            continue;
        }
        if (!g_taskbarOrder.empty()) {
            std::unordered_map<std::wstring, size_t> rank;
            for (size_t i = 0; i < g_taskbarOrder.size(); ++i)
                rank.emplace(g_taskbarOrder[i], i);
            std::stable_sort(windows.begin(), windows.end(), [&](HWND a, HWND b) {
                auto ia = rank.find(AppId(a));
                auto ib = rank.find(AppId(b));
                return (ia == rank.end() ? rank.size() : ia->second) <
                       (ib == rank.end() ? rank.size() : ib->second);
            });
        }
        for (size_t index = 0; index < windows.size(); ++index) {
            RECT target = Place(info.rcMonitor, info.rcWork,
                                static_cast<unsigned>(index),
                                static_cast<unsigned>(windows.size()));
            SetWindowPos(windows[index], nullptr, target.left, target.top,
                         target.right - target.left, target.bottom - target.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

void PlaceReadyWindows() {
    const ULONGLONG now = GetTickCount64();
    bool reflow = g_reflowAt && now >= g_reflowAt;
    if (reflow) {
        g_reflowAt = 0;
    }
    for (auto it = g_pending.begin(); it != g_pending.end();) {
        if (now < it->second) {
            ++it;
            continue;
        }
        HWND hwnd = it->first;
        it = g_pending.erase(it);
        reflow |= Eligible(hwnd);
    }
    if (reflow) {
        ArrangeAllWindows();
    }
}

void CALLBACK OnWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG objectId,
                         LONG childId, DWORD, DWORD) {
    if (!hwnd || objectId != OBJID_WINDOW || childId != CHILDID_SELF) {
        return;
    }
    if (event == EVENT_OBJECT_DESTROY) {
        if (g_known.erase(hwnd)) {
            g_order.erase(std::remove(g_order.begin(), g_order.end(), hwnd),
                          g_order.end());
            g_reflowAt = GetTickCount64() + 150;
        }
        g_pending.erase(hwnd);
    } else if (event == EVENT_OBJECT_SHOW && g_known.insert(hwnd).second) {
        g_order.push_back(hwnd);
        g_pending[hwnd] = GetTickCount64() + kDelayMs;
    } else if (event == EVENT_SYSTEM_MINIMIZESTART ||
               event == EVENT_SYSTEM_MINIMIZEEND) {
        g_reflowAt = GetTickCount64() + 150;
    }
}

DWORD WINAPI WatchWindows(void*) {
    // Monitor geometry and SetWindowPos use the same physical-pixel coordinate space.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(com))
        CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&g_automation));
    MSG message;
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    LoadSettings();
    g_taskbarOrder = ReadTaskbarOrder();
    EnumWindows(RememberWindow, 0);
    ArrangeAllWindows();
    g_showHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr,
                                OnWinEvent, 0, 0,
                                WINEVENT_OUTOFCONTEXT);
    g_destroyHook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY,
                                   nullptr, OnWinEvent, 0, 0,
                                   WINEVENT_OUTOFCONTEXT);
    g_minimizeHook = SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART,
                                    EVENT_SYSTEM_MINIMIZEEND, nullptr,
                                    OnWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    UINT_PTR timerId = SetTimer(nullptr, 0, 100, nullptr);
    if (!g_showHook || !g_destroyHook || !g_minimizeHook || !timerId) {
        Wh_Log(L"Could not start window watcher");
        if (g_showHook) UnhookWinEvent(g_showHook);
        if (g_destroyHook) UnhookWinEvent(g_destroyHook);
        if (g_minimizeHook) UnhookWinEvent(g_minimizeHook);
        if (timerId) KillTimer(nullptr, timerId);
        return 1;
    }
    ULONGLONG nextTaskbarCheck = GetTickCount64() + 2000;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_TIMER && message.wParam == timerId) {
            PlaceReadyWindows();
            if (GetTickCount64() >= nextTaskbarCheck) {
                nextTaskbarCheck = GetTickCount64() + 2000;
                auto order = ReadTaskbarOrder();
                if (!order.empty() && order != g_taskbarOrder) {
                    g_taskbarOrder = std::move(order);
                    ArrangeAllWindows();
                }
            }
        } else if (message.message == kReloadSettings) {
            LoadSettings();
            ArrangeAllWindows();
        } else {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    KillTimer(nullptr, timerId);
    UnhookWinEvent(g_showHook);
    UnhookWinEvent(g_destroyHook);
    UnhookWinEvent(g_minimizeHook);
    if (g_automation) g_automation->Release();
    if (SUCCEEDED(com)) CoUninitialize();
    return 0;
}
}  // namespace

BOOL Wh_ModInit() {
    g_thread = CreateThread(nullptr, 0, WatchWindows, nullptr, 0, &g_threadId);
    return g_thread != nullptr;
}

void Wh_ModSettingsChanged() {
    PostThreadMessageW(g_threadId, kReloadSettings, 0, 0);
}

void Wh_ModUninit() {
    PostThreadMessageW(g_threadId, WM_QUIT, 0, 0);
    WaitForSingleObject(g_thread, 2000);
    CloseHandle(g_thread);
    g_thread = nullptr;
}
