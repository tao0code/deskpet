/* ============================================================
 *  pet.h - DeskPet 桌面宠物 全局定义
 *  纯 Win32 API + GDI+ 矢量绘制，无外部资源依赖
 * ============================================================ */
#ifndef DESKPET_H
#define DESKPET_H

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------- 常量 ---------- */
#define PET_CLASS_NAME   L"DeskPetWindowClass"
#define PET_APP_NAME     L"DeskPet"
#define PET_W            220          /* 窗口宽（含气泡空间） */
#define PET_H            300          /* 窗口高（含跳跃余量） */
#define PET_JUMP_MAX     62           /* 最大跳跃高度（px） */
#define PET_TIMER_ANIM   16           /* ~60fps 动画定时器 */
#define PET_TIMER_AI     2500         /* AI 决策定时器 */
#define PET_TIMER_BUBBLE 1            /* 气泡倒计时 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- 宠物状态机 ---------- */
typedef enum {
    ST_IDLE = 0,      /* 待机呼吸 */
    ST_WALK,          /* 走路 */
    ST_JUMP,          /* 跳跃 */
    ST_SLEEP,         /* 睡觉 */
    ST_DRAG,          /* 被拖拽 */
    ST_TALK,          /* 说话 */
    ST_HAPPY,         /* 开心（被点击） */
    ST_COUNT
} PetState;

/* ---------- 情绪（影响表情） ---------- */
typedef enum {
    MO_NORMAL = 0,
    MO_HAPPY,
    MO_SLEEPY,
    MO_SURPRISED,
    MO_ANGRY
} PetMood;

/* ---------- 宠物主体 ---------- */
typedef struct {
    /* 窗口 */
    HWND  hwnd;
    HDC   memDC;            /* 离屏 DC */
    HBITMAP memBmp;         /* 离屏位图 */
    HBITMAP oldBmp;
    int   width, height;

    /* 位置（屏幕坐标，窗口左上角） */
    int   x, y;
    int   vx, vy;           /* 速度 */

    /* 状态 */
    PetState state;
    PetMood  mood;
    int   stateTimer;       /* 状态剩余帧数 */
    double animTime;        /* 全局动画时间（秒） */
    double blinkTimer;      /* 眨眼计时 */
    int   blinking;

    /* 朝向：1 右，-1 左 */
    int   facing;

    /* 拖拽 */
    int   dragging;
    POINT dragOffset;
    int   dragStartX, dragStartY;

    /* 气泡 */
    wchar_t bubble[256];
    int     bubbleFrames;   /* 剩余显示帧数 */

    /* 物理 */
    double jumpY;           /* 跳跃偏移 */
    double jumpV;
    int    onGround;

    /* 说话/口型 */
    double mouthOpen;

    /* 鼠标悬停 */
    int   hover;
} Pet;

/* ---------- 全局实例 ---------- */
#ifdef __cplusplus
extern "C" {
#endif

extern Pet g_pet;

/* ---------- 配置持久化 ---------- */
void  config_load(void);
void  config_save(void);
extern int g_alwaysOnTop;
extern int g_autoMove;
extern int g_soundOn;

/* ---------- pet_render.c ---------- */
void  render_init(void);
void  render_shutdown(void);
void  render_frame(Pet *p);

/* ---------- pet_anim.c ---------- */
void  anim_init(Pet *p);
void  anim_update(Pet *p, double dt);
void  anim_set_state(Pet *p, PetState s, int frames);
void  anim_ai_decide(Pet *p);
const wchar_t *anim_state_name(PetState s);

/* ---------- pet_interact.c ---------- */
void  interact_say(Pet *p, const wchar_t *text, int frames);
void  interact_on_click(Pet *p, int x, int y);
void  interact_on_drag_start(Pet *p, int x, int y);
void  interact_on_drag_move(Pet *p, int x, int y);
void  interact_on_drag_end(Pet *p);
void  interact_on_right_click(Pet *p, int x, int y);
void  interact_ai_tick(Pet *p);

/* ---------- pet_window.c ---------- */
BOOL  pet_window_create(HINSTANCE hInst);
void  pet_window_destroy(void);
void  pet_apply_alpha(void);
void  pet_repaint(void);

#ifdef __cplusplus
}
#endif

#endif /* DESKPET_H */
