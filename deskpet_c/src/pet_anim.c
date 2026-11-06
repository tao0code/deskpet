/* ============================================================
 *  pet_anim.c - 动画状态机
 *  负责：状态切换、物理（重力/跳跃）、眨眼、自主行为决策
 * ============================================================ */
#include "pet.h"

/* 随机数辅助 */
static int rand_range(int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + rand() % (hi - lo + 1);
}

static double frand(void)
{
    return (double)rand() / (double)RAND_MAX;
}

void anim_init(Pet *p)
{
    p->state      = ST_IDLE;
    p->mood       = MO_NORMAL;
    p->stateTimer = 120;
    p->animTime   = 0.0;
    p->blinkTimer = 3.0;
    p->blinking   = 0;
    p->facing     = 1;
    p->vx = p->vy  = 0;
    p->jumpY = 0.0;
    p->jumpV = 0.0;
    p->onGround = 1;
    p->mouthOpen = 0.0;
    p->bubbleFrames = 0;
    p->bubble[0] = 0;
}

const wchar_t *anim_state_name(PetState s)
{
    switch (s) {
    case ST_IDLE:  return L"待机";
    case ST_WALK:  return L"散步";
    case ST_JUMP:  return L"跳跃";
    case ST_SLEEP: return L"睡觉";
    case ST_DRAG:  return L"被拖拽";
    case ST_TALK:  return L"说话";
    case ST_HAPPY: return L"开心";
    default:       return L"?";
    }
}

/* 切换状态，frames<=0 表示无限持续 */
void anim_set_state(Pet *p, PetState s, int frames)
{
    if (p->state == s && frames > 0 && p->stateTimer > 0)
        return;
    p->state      = s;
    p->stateTimer = frames;

    switch (s) {
    case ST_IDLE:
        p->mood = MO_NORMAL;
        p->vx = 0;
        break;
    case ST_WALK:
        p->mood = MO_NORMAL;
        break;
    case ST_JUMP:
        p->mood = MO_HAPPY;
        if (p->onGround) {
            p->jumpV = -9.0;      /* 起跳初速度（最高约 61px，不超 PET_JUMP_MAX） */
            p->onGround = 0;
        }
        break;
    case ST_SLEEP:
        p->mood = MO_SLEEPY;
        p->vx = 0;
        break;
    case ST_HAPPY:
        p->mood = MO_HAPPY;
        break;
    case ST_TALK:
        p->mood = MO_NORMAL;
        break;
    default:
        break;
    }
}

/* 自主行为：随机挑选下一个动作 */
void anim_ai_decide(Pet *p)
{
    int r = rand_range(0, 99);

    if (p->state == ST_DRAG) return;

    /* 长时间无操作后容易睡觉 */
    if (r < 12) {
        anim_set_state(p, ST_SLEEP, rand_range(240, 600));
    } else if (r < 45) {
        /* 走路：随机方向 */
        p->facing = (r % 2) ? 1 : -1;
        p->vx = p->facing * (1.2 + frand() * 1.3);
        anim_set_state(p, ST_WALK, rand_range(90, 240));
    } else if (r < 60) {
        anim_set_state(p, ST_JUMP, rand_range(60, 90));
    } else if (r < 72) {
        static const wchar_t *idle_words[] = {
            L"嘿嘿~", L"摸摸我嘛", L"今天也要加油哦！",
            L"有点无聊…", L"你在忙什么呢？", L"我在这儿陪着你~",
            L"要不要休息一下？", L"咕噜咕噜…"
        };
        interact_say(p, idle_words[rand_range(0, 7)], 160);
        anim_set_state(p, ST_TALK, 160);
    } else {
        anim_set_state(p, ST_IDLE, rand_range(120, 300));
    }
}

/* 每帧更新 */
void anim_update(Pet *p, double dt)
{
    p->animTime += dt;

    /* ---- 眨眼 ---- */
    p->blinkTimer -= dt;
    if (p->blinkTimer <= 0.0) {
        p->blinking = 1;
        if (p->blinkTimer < -0.12) {
            p->blinking = 0;
            p->blinkTimer = 2.0 + frand() * 4.0;
        }
    }

    /* ---- 气泡倒计时 ---- */
    if (p->bubbleFrames > 0) {
        p->bubbleFrames--;
        if (p->bubbleFrames == 0) p->bubble[0] = 0;
    }

    /* ---- 跳跃物理 ---- */
    if (!p->onGround) {
        p->jumpV += 0.62;          /* 重力 */
        p->jumpY += p->jumpV;
        if (p->jumpY >= 0.0) {
            p->jumpY = 0.0;
            p->jumpV = 0.0;
            p->onGround = 1;
            if (p->state == ST_JUMP)
                anim_set_state(p, ST_IDLE, rand_range(60, 150));
        }
    }

    /* ---- 说话口型 ---- */
    if (p->state == ST_TALK || p->bubbleFrames > 0) {
        p->mouthOpen = 0.5 + 0.5 * sin(p->animTime * 16.0);
    } else {
        p->mouthOpen *= 0.85;
    }

    /* ---- 状态机 ---- */
    switch (p->state) {
    case ST_WALK:
        p->x += (int)(p->vx * dt * 60.0);
        /* 撞到屏幕边缘则掉头 */
        {
            RECT wa;
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
            if (p->x < wa.left) {
                p->x = wa.left;
                p->vx = -p->vx;
                p->facing = 1;
            }
            if (p->x + PET_W > wa.right) {
                p->x = wa.right - PET_W;
                p->vx = -p->vx;
                p->facing = -1;
            }
        }
        break;

    case ST_SLEEP:
        p->mood = MO_SLEEPY;
        break;

    case ST_HAPPY:
        p->mood = MO_HAPPY;
        break;

    default:
        break;
    }

    /* 状态计时结束 */
    if (p->stateTimer > 0) {
        p->stateTimer--;
        if (p->stateTimer == 0 && p->state != ST_DRAG) {
            p->vx = 0;
            anim_set_state(p, ST_IDLE, rand_range(90, 180));
        }
    }
}
