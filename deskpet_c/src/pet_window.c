/* ============================================================
 *  pet_window.c - 窗口创建 / 分层透明 / 消息循环 / 配置持久化
 *
 *  透明实现原理：
 *   1. 创建 WS_EX_LAYERED 窗口
 *   2. 用 32 位 DIB 作为离屏缓冲，GDI+ 绘制带 alpha 的图形
 *   3. UpdateLayeredWindow 把带 alpha 的位图贴到屏幕
 *      -> 实现真正的逐像素半透明，无黑边
 * ============================================================ */
#include "pet.h"

Pet g_pet;

int g_alwaysOnTop = 1;
int g_autoMove    = 1;
int g_soundOn     = 0;

/* 计时 */
static LARGE_INTEGER g_perfFreq;
static LARGE_INTEGER g_lastTick;

/* 配置 */
static wchar_t g_iniPath[MAX_PATH];

/* ============================================================
 *  配置读写
 * ============================================================ */
static void get_ini_path(void)
{
    if (g_iniPath[0]) return;
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t *slash = wcsrchr(exePath, L'\\');
    if (slash) *slash = 0;
    _snwprintf(g_iniPath, MAX_PATH, L"%s\\deskpet.ini", exePath);
}

void config_load(void)
{
    get_ini_path();
    g_alwaysOnTop = GetPrivateProfileIntW(L"DeskPet", L"AlwaysOnTop", 1, g_iniPath);
    g_autoMove    = GetPrivateProfileIntW(L"DeskPet", L"AutoMove",    1, g_iniPath);
    g_soundOn     = GetPrivateProfileIntW(L"DeskPet", L"Sound",       0, g_iniPath);

    g_pet.x = GetPrivateProfileIntW(L"DeskPet", L"X", -1, g_iniPath);
    g_pet.y = GetPrivateProfileIntW(L"DeskPet", L"Y", -1, g_iniPath);
}

void config_save(void)
{
    get_ini_path();
    wchar_t buf[32];

    wsprintfW(buf, L"%d", g_alwaysOnTop);
    WritePrivateProfileStringW(L"DeskPet", L"AlwaysOnTop", buf, g_iniPath);
    wsprintfW(buf, L"%d", g_autoMove);
    WritePrivateProfileStringW(L"DeskPet", L"AutoMove", buf, g_iniPath);
    wsprintfW(buf, L"%d", g_soundOn);
    WritePrivateProfileStringW(L"DeskPet", L"Sound", buf, g_iniPath);

    wsprintfW(buf, L"%d", g_pet.x);
    WritePrivateProfileStringW(L"DeskPet", L"X", buf, g_iniPath);
    wsprintfW(buf, L"%d", g_pet.y);
    WritePrivateProfileStringW(L"DeskPet", L"Y", buf, g_iniPath);
}

/* ============================================================
 *  创建离屏 32 位 DIB
 * ============================================================ */
static void create_backbuffer(Pet *p)
{
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = p->width;
    bmi.bmiHeader.biHeight      = -p->height;   /* 负数 = 自上而下 */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HDC screen = GetDC(NULL);
    p->memBmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen);

    p->memDC = CreateCompatibleDC(NULL);
    if (p->memBmp) p->oldBmp = (HBITMAP)SelectObject(p->memDC, p->memBmp);
}

static void destroy_backbuffer(Pet *p)
{
    if (p->memDC) {
        if (p->oldBmp) SelectObject(p->memDC, p->oldBmp);
        DeleteDC(p->memDC);
        p->memDC = NULL;
    }
    if (p->memBmp) {
        DeleteObject(p->memBmp);
        p->memBmp = NULL;
    }
}

/* ============================================================
 *  应用逐像素 alpha 到屏幕
 * ============================================================ */
void pet_apply_alpha(void)
{
    Pet *p = &g_pet;
    if (!p->hwnd || !p->memDC) return;

    HDC screen = GetDC(NULL);
    POINT src = { 0, 0 };
    POINT dst = { p->x, p->y };
    SIZE  size = { p->width, p->height };

    BLENDFUNCTION bf;
    bf.BlendOp             = AC_SRC_OVER;
    bf.BlendFlags          = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat         = AC_SRC_ALPHA;

    UpdateLayeredWindow(p->hwnd, screen, &dst, &size,
                        p->memDC, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, screen);
}

/* ============================================================
 *  重绘一帧
 * ============================================================ */
void pet_repaint(void)
{
    Pet *p = &g_pet;
    if (!p->hwnd || !p->memDC) return;
    render_frame(p);
    pet_apply_alpha();
}

/* ============================================================
 *  判断点是否在宠物身体范围内（用于点击穿透判断）
 *  窗口是矩形，但视觉上只有中间部分是宠物
 * ============================================================ */
static BOOL hit_test_pet(Pet *p, int mx, int my)
{
    (void)p;
    /* 宠物身体大致区域 */
    int cx = PET_W / 2;
    /* 覆盖"静止 + 跳到最高"两种情况的并集区域 */
    int bodyTop    = PET_H - PET_JUMP_MAX - 130;
    int bodyBottom = PET_H - PET_JUMP_MAX + 40;
    int halfW      = 72;

    if (my < bodyTop || my > bodyBottom) return FALSE;

    /* 椭圆近似判定 */
    double dx = (double)(mx - cx) / halfW;
    double dy = (double)(my - (bodyTop + bodyBottom) / 2.0)
                / ((bodyBottom - bodyTop) / 2.0);
    return (dx * dx + dy * dy) <= 1.05;
}

/* 让鼠标穿透透明区域 */
static void update_click_through(Pet *p, int mx, int my)
{
    LONG_PTR ex = GetWindowLongPtrW(p->hwnd, GWL_EXSTYLE);
    BOOL inside = hit_test_pet(p, mx, my);

    if (!inside) {
        if (!(ex & WS_EX_TRANSPARENT)) {
            SetWindowLongPtrW(p->hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
        }
        p->hover = 0;
    } else {
        if (ex & WS_EX_TRANSPARENT) {
            SetWindowLongPtrW(p->hwnd, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
        }
        p->hover = 1;
    }
}

/* ============================================================
 *  窗口过程
 * ============================================================ */
static LRESULT CALLBACK PetWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Pet *p = &g_pet;

    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        interact_on_drag_start(p, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;

    case WM_MOUSEMOVE:
        if (p->dragging) {
            interact_on_drag_move(p, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        } else {
            update_click_through(p, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        }
        return 0;

    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        int moved = abs(p->x - p->dragStartX) + abs(p->y - p->dragStartY);

        if (p->dragging) {
            if (moved < 6) {
                /* 位移很小 -> 视为点击 */
                p->dragging = 0;
                interact_on_click(p, mx, my);
            } else {
                interact_on_drag_end(p);
            }
        }
        return 0;
    }

    case WM_RBUTTONUP:
        interact_on_right_click(p, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;

    case WM_TIMER:
        if (wp == PET_TIMER_ANIM) {
            /* 计算 dt */
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            double dt = (double)(now.QuadPart - g_lastTick.QuadPart)
                        / (double)g_perfFreq.QuadPart;
            g_lastTick = now;
            if (dt > 0.1) dt = 0.1;   /* 防止卡顿后跳帧 */

            anim_update(p, dt);

            /* 位置变化时同步窗口 */
            POINT cur;
            GetCursorPos(&cur);
            RECT wr;
            GetWindowRect(hwnd, &wr);
            if (p->state == ST_WALK || p->dragging) {
                SetWindowPos(hwnd, NULL, p->x, p->y, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            /* 鼠标在窗口外时恢复非穿透检查 */
            POINT client = cur;
            ScreenToClient(hwnd, &client);
            if (!p->dragging) update_click_through(p, client.x, client.y);

            pet_repaint();
        } else if (wp == PET_TIMER_AI) {
            interact_ai_tick(p);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, PET_TIMER_ANIM);
        KillTimer(hwnd, PET_TIMER_AI);
        config_save();
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ============================================================
 *  创建窗口
 * ============================================================ */
BOOL pet_window_create(HINSTANCE hInst)
{
    Pet *p = &g_pet;
    ZeroMemory(p, sizeof(Pet));
    p->width  = PET_W;
    p->height = PET_H;

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = PetWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = PET_CLASS_NAME;

    if (!RegisterClassExW(&wc)) return FALSE;

    /* 位置：优先用配置，否则右下角 */
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (p->x < 0 || p->y < 0) {
        p->x = wa.right - PET_W - 60;
        p->y = wa.bottom - PET_H;
    }

    p->hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        PET_CLASS_NAME,
        PET_APP_NAME,
        WS_POPUP,
        p->x, p->y, PET_W, PET_H,
        NULL, NULL, hInst, NULL);

    if (!p->hwnd) return FALSE;

    create_backbuffer(p);
    anim_init(p);

    if (g_alwaysOnTop)
        SetWindowPos(p->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);

    ShowWindow(p->hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(p->hwnd);

    /* 计时器 */
    QueryPerformanceFrequency(&g_perfFreq);
    QueryPerformanceCounter(&g_lastTick);

    SetTimer(p->hwnd, PET_TIMER_ANIM, PET_TIMER_ANIM, NULL);
    SetTimer(p->hwnd, PET_TIMER_AI,   PET_TIMER_AI,   NULL);

    /* 开场白 */
    interact_say(p, L"你好，我是桌宠小橘！", 220);

    pet_repaint();
    return TRUE;
}

void pet_window_destroy(void)
{
    Pet *p = &g_pet;
    if (p->hwnd) {
        DestroyWindow(p->hwnd);
        p->hwnd = NULL;
    }
    destroy_backbuffer(p);
}
