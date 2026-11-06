/* ============================================================
 *  main.c - DeskPet 入口
 *  单实例检查 + 初始化 + 消息循环
 * ============================================================ */
#include "pet.h"

/* 单实例互斥体 */
static HANDLE g_hMutex = NULL;

static BOOL ensure_single_instance(void)
{
    g_hMutex = CreateMutexW(NULL, TRUE, L"DeskPet_SingleInstance_Mutex");
    if (!g_hMutex) return TRUE;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /* 已有实例在运行 */
        HWND old = FindWindowW(PET_CLASS_NAME, NULL);
        if (old) {
            /* 把已有实例提到前台 */
            SetForegroundWindow(old);
        }
        return FALSE;
    }
    return TRUE;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmdLine, int nCmdShow)
{
    (void)hPrev; (void)lpCmdLine; (void)nCmdShow;

    if (!ensure_single_instance()) {
        MessageBoxW(NULL, L"桌宠已经在运行中啦~", PET_APP_NAME,
                    MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    /* 高 DPI 支持（Win8.1+ 可用，失败不影响运行） */
    SetProcessDPIAware();

    srand((unsigned)GetTickCount());

    render_init();
    config_load();

    if (!pet_window_create(hInst)) {
        MessageBoxW(NULL, L"创建桌宠窗口失败！", PET_APP_NAME, MB_OK | MB_ICONERROR);
        render_shutdown();
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    pet_window_destroy();
    render_shutdown();

    if (g_hMutex) {
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
    }
    return (int)msg.wParam;
}
