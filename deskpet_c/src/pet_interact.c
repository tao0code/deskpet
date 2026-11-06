/* ============================================================
 *  pet_interact.c - 交互逻辑
 *  拖拽 / 点击 / 右键菜单 / 气泡说话 / 自主 AI
 * ============================================================ */
#include "pet.h"

/* ---------- 菜单命令 ID ---------- */
#define IDM_JUMP        1001
#define IDM_SLEEP       1002
#define IDM_WAKE        1003
#define IDM_TALK        1004
#define IDM_TOPMOST     1005
#define IDM_AUTOMOVE    1006
#define IDM_CENTER      1007
#define IDM_RESET       1008
#define IDM_EXIT        1099

static int rand_range(int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + rand() % (hi - lo + 1);
}

/* ============================================================
 *  让宠物说话
 * ============================================================ */
void interact_say(Pet *p, const wchar_t *text, int frames)
{
    if (!text) return;
    wcsncpy(p->bubble, text, 255);
    p->bubble[255] = 0;
    p->bubbleFrames = frames > 0 ? frames : 160;
}

/* ============================================================
 *  左键单击
 * ============================================================ */
void interact_on_click(Pet *p, int x, int y)
{
    (void)x; (void)y;
    static const wchar_t *click_words[] = {
        L"哎呀！别戳我啦~", L"嘻嘻，好痒！", L"你好呀！(＾▽＾)",
        L"我在呢，有什么事吗？", L"再摸一下嘛~", L"呜…被发现了",
        L"你戳到我的脸了！", L"今天心情不错哦~"
    };
    interact_say(p, click_words[rand_range(0, 7)], 180);
    anim_set_state(p, ST_HAPPY, 90);
    if (p->onGround) anim_set_state(p, ST_JUMP, 70);
    pet_repaint();
}

/* ============================================================
 *  拖拽
 * ============================================================ */
void interact_on_drag_start(Pet *p, int x, int y)
{
    p->dragging = 1;
    p->dragOffset.x = x;
    p->dragOffset.y = y;
    p->dragStartX = p->x;
    p->dragStartY = p->y;
    p->vx = 0;
    anim_set_state(p, ST_DRAG, 0);
    p->mood = MO_SURPRISED;
    interact_say(p, L"哇啊——放我下来！", 120);
}

void interact_on_drag_move(Pet *p, int x, int y)
{
    (void)x; (void)y;
    if (!p->dragging) return;

    POINT cur;
    GetCursorPos(&cur);
    int nx = cur.x - p->dragOffset.x;
    int ny = cur.y - p->dragOffset.y;

    p->x = nx;
    p->y = ny;
    SetWindowPos(p->hwnd, NULL, p->x, p->y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    pet_repaint();
}

void interact_on_drag_end(Pet *p)
{
    if (!p->dragging) return;
    p->dragging = 0;

    /* 松手后落到工作区底部附近（软着陆） */
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);

    int groundY = wa.bottom - PET_H;
    if (p->y > groundY - 120) {
        /* 靠近地面才吸附 */
        p->y = groundY;
        SetWindowPos(p->hwnd, NULL, p->x, p->y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    anim_set_state(p, ST_JUMP, 60);
    interact_say(p, L"呼…安全着陆！", 120);
    config_save();
    pet_repaint();
}

/* ============================================================
 *  右键菜单
 * ============================================================ */
void interact_on_right_click(Pet *p, int x, int y)
{
    (void)x; (void)y;
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, IDM_JUMP,  L"跳一下");
    AppendMenuW(hMenu, MF_STRING, IDM_SLEEP, L"睡觉");
    AppendMenuW(hMenu, MF_STRING, IDM_WAKE,  L"叫醒它");
    AppendMenuW(hMenu, MF_STRING, IDM_TALK,  L"说句话");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (g_alwaysOnTop ? MF_CHECKED : 0),
                IDM_TOPMOST,  L"总在最前");
    AppendMenuW(hMenu, MF_STRING | (g_autoMove ? MF_CHECKED : 0),
                IDM_AUTOMOVE, L"自动活动");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_CENTER, L"回到屏幕中央");
    AppendMenuW(hMenu, MF_STRING, IDM_RESET,  L"重置位置");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT,   L"退出");

    /* 需要 SetForegroundWindow 才能正确关闭菜单 */
    SetForegroundWindow(p->hwnd);
    int cmd = (int)TrackPopupMenu(hMenu,
                                  TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                  pt.x, pt.y, 0, p->hwnd, NULL);
    DestroyMenu(hMenu);

    switch (cmd) {
    case IDM_JUMP:
        anim_set_state(p, ST_JUMP, 70);
        break;
    case IDM_SLEEP:
        anim_set_state(p, ST_SLEEP, 600);
        interact_say(p, L"晚安…zzZ", 200);
        break;
    case IDM_WAKE:
        anim_set_state(p, ST_HAPPY, 90);
        interact_say(p, L"我醒啦！", 140);
        break;
    case IDM_TALK:
    {
        static const wchar_t *words[] = {
            L"我是一只用 C 语言写出来的桌宠~",
            L"我运行在 Win32 API 上哦！",
            L"GDI+ 把矢量图形画得漂漂亮亮！",
            L"右键点我可以打开菜单~",
            L"拖着我到处走走吧！",
            L"别关我，我会想你的…"
        };
        interact_say(p, words[rand_range(0, 5)], 220);
        anim_set_state(p, ST_TALK, 220);
        break;
    }
    case IDM_TOPMOST:
        g_alwaysOnTop = !g_alwaysOnTop;
        SetWindowPos(p->hwnd, g_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        config_save();
        break;
    case IDM_AUTOMOVE:
        g_autoMove = !g_autoMove;
        config_save();
        interact_say(p, g_autoMove ? L"好的，我去散步啦~" : L"那我安静待着~", 120);
        break;
    case IDM_CENTER:
    {
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        p->x = (wa.left + wa.right) / 2 - PET_W / 2;
        p->y = wa.bottom - PET_H;
        SetWindowPos(p->hwnd, NULL, p->x, p->y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        config_save();
        break;
    }
    case IDM_RESET:
    {
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        p->x = wa.right - PET_W - 60;
        p->y = wa.bottom - PET_H;
        SetWindowPos(p->hwnd, NULL, p->x, p->y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        config_save();
        break;
    }
    case IDM_EXIT:
        PostMessageW(p->hwnd, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
    pet_repaint();
}

/* ============================================================
 *  自主 AI 定时触发
 * ============================================================ */
void interact_ai_tick(Pet *p)
{
    if (p->dragging) return;
    if (!g_autoMove) {
        /* 不自动活动时只做轻微待机 */
        if (p->state != ST_SLEEP && p->state != ST_TALK)
            anim_set_state(p, ST_IDLE, 300);
        return;
    }
    /* 正在说话/跳跃中不打断 */
    if (p->state == ST_TALK && p->bubbleFrames > 40) return;
    anim_ai_decide(p);
}
