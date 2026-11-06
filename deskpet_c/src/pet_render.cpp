/* ============================================================
 *  pet_render.cpp - GDI+ 矢量绘制（C++ 编译单元）
 *
 *  为什么用 C++：GDI+ 的 C++ 封装（Graphics/SolidBrush/GraphicsPath）
 *  比 flat API 简洁得多，且能自动管理资源。整个项目其余部分仍是纯 C，
 *  通过 pet.h 中的 extern "C" 声明互相调用。
 *
 *  绘制流程：清空 -> 阴影 -> 尾巴 -> 脚 -> 耳朵 -> 身体
 *            -> 眼睛 -> 嘴巴 -> 腮红 -> 气泡
 * ============================================================ */
#include "pet.h"

#include <objidl.h>
#include <gdiplus.h>

using namespace Gdiplus;

/* ---------- GDI+ 初始化令牌 ---------- */
static ULONG_PTR g_gdiplusToken = 0;

/* ---------- 颜色简写 ---------- */
static inline Color C(BYTE a, BYTE r, BYTE g, BYTE b)
{
    return Color(a, r, g, b);
}

/* ---------- 填充椭圆 ---------- */
static inline void fill_ellipse(Graphics &g, Brush *br,
                                REAL x, REAL y, REAL w, REAL h)
{
    g.FillEllipse(br, x, y, w, h);
}

/* ---------- 圆头粗线 ---------- */
static inline void stroke_line(Graphics &g, Pen *pen,
                               REAL x1, REAL y1, REAL x2, REAL y2)
{
    pen->SetStartCap(LineCapRound);
    pen->SetEndCap(LineCapRound);
    g.DrawLine(pen, x1, y1, x2, y2);
}

/* ============================================================
 *  初始化 / 销毁
 * ============================================================ */
extern "C" void render_init(void)
{
    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdiplusToken, &gsi, NULL);
}

extern "C" void render_shutdown(void)
{
    if (g_gdiplusToken) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

/* ============================================================
 *  气泡对话框
 * ============================================================ */
static void draw_bubble(Graphics &g, Pet *p, REAL cx, REAL topY)
{
    if (p->bubbleFrames <= 0 || p->bubble[0] == 0) return;

    /* 淡出效果 */
    BYTE a = 255;
    if (p->bubbleFrames < 20)
        a = (BYTE)(255 * p->bubbleFrames / 20);

    /* 字体：优先微软雅黑，失败退到宋体 */
    FontFamily ff(L"Microsoft YaHei");
    Font *font = new Font(&ff, 13.0f, FontStyleRegular, UnitPixel);
    if (font->GetLastStatus() != Ok) {
        delete font;
        FontFamily ff2(L"SimSun");
        font = new Font(&ff2, 13.0f, FontStyleRegular, UnitPixel);
    }

    RectF bound;
    g.MeasureString(p->bubble, -1.0f, font, PointF(0.0f, 0.0f), &bound);

    const REAL padX = 10, padY = 6;
    REAL bw = bound.Width + padX * 2;
    REAL bh = bound.Height + padY * 2;
    if (bw > PET_W - 10) bw = (REAL)(PET_W - 10);
    if (bw < 64) bw = 64;

    REAL bx = cx - bw / 2;
    if (bx < 4) bx = 4;
    if (bx + bw > PET_W - 4) bx = PET_W - 4 - bw;
    REAL by = topY - bh - 12;
    if (by < 2) by = 2;

    SolidBrush bg(C(a, 255.0f, 255.0f, 255.0f));
    SolidBrush border(C(a, 130.0f, 170.0f, 225.0f));
    Pen bp(&border, 1.5f);

    /* 圆角矩形 */
    GraphicsPath path;
    const REAL r = 10;
    path.AddArc(bx, by, r * 2.0f, r * 2.0f, 180.0f, 90.0f);
    path.AddArc(bx + bw - r * 2.0f, by, r * 2.0f, r * 2.0f, 270.0f, 90.0f);
    path.AddArc(bx + bw - r * 2.0f, by + bh - r * 2.0f, r * 2.0f, r * 2.0f, 0.0f, 90.0f);
    path.AddArc(bx, by + bh - r * 2.0f, r * 2.0f, r * 2.0f, 90.0f, 90.0f);
    path.CloseFigure();

    /* 指向宠物的小尾巴 */
    REAL tipX = cx;
    if (tipX < bx + 14) tipX = bx + 14;
    if (tipX > bx + bw - 14) tipX = bx + bw - 14;
    GraphicsPath tail;
    tail.AddLine(tipX - 7.0f, by + bh - 1.5f, tipX + 7.0f, by + bh - 1.5f);
    tail.AddLine(tipX + 7.0f, by + bh - 1.5f, tipX, by + bh + 9.0f);
    tail.CloseFigure();

    g.FillPath(&bg, &path);
    g.FillPath(&bg, &tail);
    g.DrawPath(&bp, &path);

    SolidBrush fg(C(a, 40.0f, 50.0f, 70.0f));
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(p->bubble, -1.0f, font, RectF(bx, by, bw, bh), &sf, &fg);

    delete font;
}

/* ============================================================
 *  主绘制
 * ============================================================ */
extern "C" void render_frame(Pet *p)
{
    if (!p->memDC) return;

    Graphics g(p->memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    /* 全透明清屏（分层窗口的关键） */
    g.Clear(Color(0.0f, 0.0f, 0.0f, 0.0f));

    /* ---------- 布局 ---------- */
    const REAL bodyW = 96.0f;
    const REAL bodyH = 88.0f;
    const REAL cx    = PET_W / 2.0f;
    /* 静止时脚底位置：窗口底部留出跳跃余量，避免跳跃时被裁切 */
    const REAL baseY = PET_H - PET_JUMP_MAX - 8.0f;

    /* 呼吸 + 跳跃位移 */
    double breathe = sin(p->animTime * 2.2) * 2.0;
    /* jumpY 为负表示向上（屏幕坐标 Y 向下为正），故直接相加 */
    REAL   lift    = (REAL)(p->jumpY) + (REAL)breathe * 0.5f;

    /* 走路颠簸 */
    if (p->state == ST_WALK)
        lift += (REAL)(fabs(sin(p->animTime * 9.0)) * 4.0);

    /* 睡觉压扁 */
    REAL squash = (p->state == ST_SLEEP) ? 0.86f : 1.0f;
    REAL bodyH2 = bodyH * squash;
    REAL bodyCY = baseY - bodyH2 / 2 + lift;
    REAL bodyCX = cx;

    /* ---------- 1. 地面阴影 ---------- */
    {
        REAL k = 1.0f - (REAL)(-p->jumpY) / 120.0f;   /* jumpY<0 => k 变小 */
        if (k < 0.35f) k = 0.35f;
        REAL shW = bodyW * 0.85f * k;
        REAL shH = 12.0f * k;
        SolidBrush sh(C((BYTE)(70.0f * k), 30.0f, 40.0f, 60.0f));
        fill_ellipse(g, &sh, bodyCX - shW / 2, baseY - 4 - shH / 2, shW, shH);
    }

    /* ---------- 2. 尾巴 ---------- */
    {
        Pen tailPen(C(255.0f, 255.0f, 176.0f, 92.0f), 9.0f);
        REAL tw = (REAL)(sin(p->animTime * 3.0) * 8.0);
        REAL tx = bodyCX - p->facing * (bodyW / 2 - 6);
        REAL ty = bodyCY + bodyH2 * 0.15f;
        GraphicsPath tp;
        tp.AddBezier(tx, ty,
                     tx - p->facing * 26, ty + 4 + tw,
                     tx - p->facing * 34, ty - 22 + tw,
                     tx - p->facing * 20, ty - 34 + tw);
        g.DrawPath(&tailPen, &tp);

        SolidBrush tipBr(C(255.0f, 255.0f, 220.0f, 150.0f));
        fill_ellipse(g, &tipBr,
                     tx - p->facing * 20 - 6, ty - 34 + tw - 6, 12, 12);
    }

    /* ---------- 3. 脚 ---------- */
    {
        SolidBrush footBr(C(255.0f, 240.0f, 158.0f, 74.0f));
        REAL fy = baseY - 7;
        REAL off = (p->state == ST_WALK)
                   ? (REAL)(sin(p->animTime * 9.0) * 7.0) : 0.0f;
        fill_ellipse(g, &footBr, bodyCX - 30 + off, fy, 26, 14);
        fill_ellipse(g, &footBr, bodyCX + 4 - off, fy, 26, 14);
    }

    /* ---------- 4. 耳朵 ---------- */
    {
        SolidBrush earBr(C(255.0f, 255.0f, 176.0f, 92.0f));
        SolidBrush earIn(C(255.0f, 255.0f, 205.0f, 150.0f));
        REAL earY = bodyCY - bodyH2 / 2 - 6;
        REAL ex1 = bodyCX - 26, ex2 = bodyCX + 26;

        GraphicsPath e1;
        e1.AddLine(ex1 - 12.0f, earY + 20.0f, ex1 - 2.0f, earY - 30.0f);
        e1.AddLine(ex1 - 2.0f, earY - 30.0f, ex1 + 16.0f, earY + 14.0f);
        e1.CloseFigure();
        g.FillPath(&earBr, &e1);

        GraphicsPath e1i;
        e1i.AddLine(ex1 - 4.0f, earY + 14.0f, ex1 + 1.0f, earY - 18.0f);
        e1i.AddLine(ex1 + 1.0f, earY - 18.0f, ex1 + 10.0f, earY + 10.0f);
        e1i.CloseFigure();
        g.FillPath(&earIn, &e1i);

        GraphicsPath e2;
        e2.AddLine(ex2 + 12.0f, earY + 20.0f, ex2 + 2.0f, earY - 30.0f);
        e2.AddLine(ex2 + 2.0f, earY - 30.0f, ex2 - 16.0f, earY + 14.0f);
        e2.CloseFigure();
        g.FillPath(&earBr, &e2);

        GraphicsPath e2i;
        e2i.AddLine(ex2 + 4.0f, earY + 14.0f, ex2 - 1.0f, earY - 18.0f);
        e2i.AddLine(ex2 - 1.0f, earY - 18.0f, ex2 - 10.0f, earY + 10.0f);
        e2i.CloseFigure();
        g.FillPath(&earIn, &e2i);
    }

    /* ---------- 5. 身体 ---------- */
    {
        RectF bodyRect(bodyCX - bodyW / 2, bodyCY - bodyH2 / 2, bodyW, bodyH2);

        Color cTop = C(255.0f, 255.0f, 208.0f, 128.0f);
        Color cBot = C(255.0f, 240.0f, 150.0f, 60.0f);
        if (p->mood == MO_ANGRY) {
            cTop = C(255.0f, 255.0f, 150.0f, 130.0f);
            cBot = C(255.0f, 220.0f, 90.0f, 70.0f);
        } else if (p->mood == MO_SLEEPY) {
            cTop = C(255.0f, 235.0f, 200.0f, 150.0f);
            cBot = C(255.0f, 220.0f, 160.0f, 100.0f);
        }
        LinearGradientBrush lg(bodyRect, cTop, cBot,
                               LinearGradientModeVertical);
        g.FillEllipse(&lg, bodyRect);

        /* 高光 */
        SolidBrush hl(C(90.0f, 255.0f, 255.0f, 255.0f));
        fill_ellipse(g, &hl,
                     bodyCX - bodyW * 0.30f, bodyCY - bodyH2 * 0.34f,
                     bodyW * 0.34f, bodyH2 * 0.26f);
    }

    /* ---------- 6. 眼睛 ---------- */
    {
        REAL eyeY  = bodyCY - bodyH2 * 0.10f;
        REAL eyeDX = bodyW * 0.19f;
        REAL eyeR  = 9.0f;

        if (p->state == ST_SLEEP || p->mood == MO_SLEEPY) {
            /* 闭眼弧线 + Zzz */
            Pen ep(C(255.0f, 70.0f, 60.0f, 50.0f), 2.6f);
            ep.SetStartCap(LineCapRound);
            ep.SetEndCap(LineCapRound);

            GraphicsPath a1, a2;
            a1.AddArc(bodyCX - eyeDX - eyeR, eyeY - 4.0f,
                      eyeR * 2, eyeR * 1.6f, 20, 140);
            a2.AddArc(bodyCX + eyeDX - eyeR, eyeY - 4.0f,
                      eyeR * 2, eyeR * 1.6f, 20, 140);
            g.DrawPath(&ep, &a1);
            g.DrawPath(&ep, &a2);

            FontFamily zf(L"Arial");
            Font zfont(&zf, 15.0f, FontStyleBold, UnitPixel);
            REAL zt = (REAL)fmod(p->animTime * 0.6, 1.0);
            SolidBrush zb(C(200.0f, 90.0f, 110.0f, 160.0f));
            g.DrawString(L"Z", -1.0f, &zfont,
                         PointF(bodyCX + 34.0f, eyeY - 26.0f - zt * 18.0f), &zb);
            SolidBrush zb2(C(140.0f, 90.0f, 110.0f, 160.0f));
            g.DrawString(L"z", -1.0f, &zfont,
                         PointF(bodyCX + 46.0f, eyeY - 6.0f - zt * 22.0f), &zb2);
        }
        else if (p->blinking) {
            /* 眨眼 */
            Pen ep(C(255.0f, 60.0f, 50.0f, 45.0f), 3.0f);
            stroke_line(g, &ep, bodyCX - eyeDX - eyeR + 2, eyeY,
                        bodyCX - eyeDX + eyeR - 2, eyeY);
            stroke_line(g, &ep, bodyCX + eyeDX - eyeR + 2, eyeY,
                        bodyCX + eyeDX + eyeR - 2, eyeY);
        }
        else {
            /* 睁眼 */
            REAL er = eyeR;
            if (p->mood == MO_SURPRISED) er = eyeR * 1.25f;
            if (p->mood == MO_HAPPY)     er = eyeR * 1.10f;

            SolidBrush white(C(255.0f, 255.0f, 255.0f, 255.0f));
            SolidBrush pupil(C(255.0f, 45.0f, 38.0f, 32.0f));
            SolidBrush shine(C(255.0f, 255.0f, 255.0f, 255.0f));

            /* 瞳孔跟随鼠标 */
            REAL lookX = 0, lookY = 0;
            POINT cur;
            GetCursorPos(&cur);
            RECT wr;
            GetWindowRect(p->hwnd, &wr);
            REAL mdx = (REAL)(cur.x - (wr.left + PET_W / 2));
            REAL mdy = (REAL)(cur.y - (wr.top + (int)bodyCY));
            REAL dist = sqrtf(mdx * mdx + mdy * mdy);
            if (dist > 1.0f) {
                lookX = mdx / dist * 2.4f;
                lookY = mdy / dist * 2.4f;
            }

            for (int i = 0; i < 2; i++) {
                REAL ex = bodyCX + (i == 0 ? -eyeDX : eyeDX);
                fill_ellipse(g, &white, ex - er, eyeY - er, er * 2, er * 2);
                fill_ellipse(g, &pupil,
                             ex - er * 0.58f + lookX,
                             eyeY - er * 0.58f + lookY,
                             er * 1.16f, er * 1.16f);
                fill_ellipse(g, &shine,
                             ex - er * 0.42f + lookX,
                             eyeY - er * 0.62f + lookY,
                             er * 0.42f, er * 0.42f);
            }

            /* 生气眉毛 */
            if (p->mood == MO_ANGRY) {
                Pen bp(C(255.0f, 90.0f, 60.0f, 50.0f), 3.0f);
                stroke_line(g, &bp, bodyCX - eyeDX - 10, eyeY - 16,
                            bodyCX - eyeDX + 8, eyeY - 10);
                stroke_line(g, &bp, bodyCX + eyeDX + 10, eyeY - 16,
                            bodyCX + eyeDX - 8, eyeY - 10);
            }
        }
    }

    /* ---------- 7. 嘴巴 ---------- */
    {
        REAL my = bodyCY + bodyH2 * 0.20f;
        Pen mp(C(255.0f, 120.0f, 80.0f, 70.0f), 2.4f);
        mp.SetStartCap(LineCapRound);
        mp.SetEndCap(LineCapRound);

        if (p->state == ST_TALK || p->mouthOpen > 0.15) {
            /* 张嘴说话 */
            REAL mo = (REAL)(2.0 + p->mouthOpen * 7.0);
            SolidBrush mb(C(255.0f, 150.0f, 70.0f, 75.0f));
            fill_ellipse(g, &mb, bodyCX - 7, my - mo / 2, 14, mo);
            SolidBrush tongue(C(255.0f, 235.0f, 130.0f, 140.0f));
            fill_ellipse(g, &tongue, bodyCX - 4, my + mo / 2 - 4, 8, 4);
        } else if (p->mood == MO_HAPPY) {
            GraphicsPath smile;
            smile.AddArc(bodyCX - 14.0f, my - 12.0f, 28.0f, 22.0f, 10.0f, 160.0f);
            g.DrawPath(&mp, &smile);
        } else if (p->mood == MO_SLEEPY) {
            SolidBrush mb(C(255.0f, 150.0f, 90.0f, 90.0f));
            fill_ellipse(g, &mb, bodyCX - 4, my - 2, 8, 6);
        } else {
            GraphicsPath smile;
            smile.AddArc(bodyCX - 11.0f, my - 8.0f, 22.0f, 16.0f, 20.0f, 140.0f);
            g.DrawPath(&mp, &smile);
        }
    }

    /* ---------- 8. 腮红 ---------- */
    {
        BYTE ba = (p->mood == MO_HAPPY) ? 130 : 80;
        SolidBrush blush(C(ba, 255.0f, 140.0f, 140.0f));
        REAL by2 = bodyCY + bodyH2 * 0.10f;
        fill_ellipse(g, &blush, bodyCX - bodyW * 0.40f, by2, 18, 11);
        fill_ellipse(g, &blush, bodyCX + bodyW * 0.40f - 18, by2, 18, 11);
    }

    /* ---------- 9. 气泡 ---------- */
    draw_bubble(g, p, bodyCX, bodyCY - bodyH2 / 2 - 40);
}
