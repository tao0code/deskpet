# DeskPet 桌宠 — C 语言 Windows 桌面宠物

一只用 **纯 C / Win32 API / GDI+** 从零手写的桌面宠物。无外部图片资源，全部矢量绘制，56KB 单文件绿色运行。

![预览](preview/pet_preview.png)

---

## 一、快速开始

### 直接运行
双击 `build/DeskPet.exe` 即可。无需安装任何运行库（已静态链接）。

### 自己编译

**Windows（MinGW-w64）**
```bat
cd deskpet_c
make CROSS=
```
或不用 make：
```bat
gcc -O2 -c src\main.c src\pet_anim.c src\pet_interact.c src\pet_window.c -DUNICODE -D_UNICODE -municode
g++ -O2 -std=c++17 -c src\pet_render.cpp -DUNICODE -D_UNICODE -municode
windres -I src src\resource.rc -O coff -o build\resource.o
g++ *.o build\resource.o -o build\DeskPet.exe -mwindows -municode -static -lgdiplus -lgdi32 -luser32 -lcomctl32 -lole32 -luuid
```

**Linux / WSL 交叉编译**
```bash
cd deskpet_c
make            # 默认 CROSS=x86_64-w64-mingw32-
```

---

## 二、交互方式

| 操作 | 效果 |
|---|---|
| **左键单击** | 宠物开心地跳一下并说话 |
| **左键拖拽** | 把它拖到屏幕任意位置，松手会软着陆 |
| **右键** | 打开功能菜单 |
| **鼠标靠近** | 宠物眼珠会跟着你的鼠标转 |

### 右键菜单
- 跳一下 / 睡觉 / 叫醒它 / 说句话
- 总在最前（可切换）
- 自动活动（关掉后它就安静待着）
- 回到屏幕中央 / 重置位置
- 退出

---

## 三、项目结构

```
deskpet_c/
├── Makefile              # 构建脚本（支持交叉编译）
├── README.md
├── src/
│   ├── pet.h             # 全局结构体、常量、函数声明（C/C++ 通用）
│   ├── main.c            # 入口：单实例检查 + 消息循环
│   ├── pet_window.c      # 窗口创建、分层透明、消息处理、配置持久化
│   ├── pet_render.cpp    # GDI+ 矢量绘制（唯一 C++ 编译单元）
│   ├── pet_anim.c        # 动画状态机、跳跃物理、自主行为 AI
│   ├── pet_interact.c    # 拖拽、点击、右键菜单、气泡对话
│   ├── resource.rc       # 图标 + 版本信息
│   └── app.ico           # 应用图标
├── build/
│   └── DeskPet.exe       # 编译产物
└── preview/
    └── pet_preview.png   # 各状态效果预览图
```

---

## 四、核心技术实现

### 1. 真正的逐像素透明（无黑边）

这是桌宠最关键的技术点。常见错误做法是 `SetLayeredWindowAttributes` 配色键透明，会有锯齿黑边。本项目用**分层窗口 + 逐像素 Alpha**：

```c
// 1. 窗口加 WS_EX_LAYERED 扩展样式
// 2. 创建 32 位 DIB 作为离屏缓冲
BITMAPINFO bmi = {0};
bmi.bmiHeader.biBitCount = 32;
bmi.bmiHeader.biHeight   = -PET_H;   // 负数 = 自上而下，与 GDI+ 一致
CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);

// 3. GDI+ 在离屏 DC 上绘制带 Alpha 的图形
// 4. UpdateLayeredWindow 把带 Alpha 的位图贴到屏幕
BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
UpdateLayeredWindow(hwnd, screen, &dst, &size, memDC, &src, 0, &bf, ULW_ALPHA);
```

### 2. 矢量绘制宠物

宠物完全由几何图形组合而成，不依赖任何图片：

| 部位 | 绘制方式 |
|---|---|
| 身体 | `LinearGradientBrush` 垂直渐变椭圆 + 白色高光 |
| 耳朵 | `GraphicsPath` 三角形（外耳 + 内耳） |
| 尾巴 | `AddBezier` 贝塞尔曲线，用 `Pen` 圆头描边 |
| 眼睛 | 白眼球 + 黑瞳孔 + 高光点，瞳孔跟随鼠标偏移 |
| 嘴巴 | `AddArc` 圆弧（微笑）或填充椭圆（说话张嘴） |
| 气泡 | 圆角矩形 `GraphicsPath` + 三角尾巴 + 文字居中 |

### 3. 动画状态机

```
        ┌──────────────────────────────┐
        ↓                              │
     [IDLE] ──→ [WALK] ──→ [JUMP] ──→ [SLEEP]
        ↑          │          │          │
        └──────────┴──────────┴──────────┘
                   ↓
              [TALK] / [HAPPY] / [DRAG]
```

- **定时器驱动**：16ms 动画定时器（~60fps）+ 2500ms AI 决策定时器
- **帧率无关**：用 `QueryPerformanceCounter` 计算真实 `dt`，卡顿后自动钳制防止跳帧
- **跳跃物理**：初速度 -9.0，重力加速度 0.62，最高约 61px，滞空 0.47s

### 4. 点击穿透

窗口是 220×300 的矩形，但视觉上只有中间是宠物。用椭圆方程做命中测试，鼠标在透明区域时加 `WS_EX_TRANSPARENT`，让点击穿透到下层窗口：

```c
double dx = (mx - cx) / halfW;
double dy = (my - cy) / halfH;
return (dx*dx + dy*dy) <= 1.05;   // 椭圆内才算点中
```

### 5. 配置持久化

位置、总在最前、自动活动等设置写入 exe 同目录的 `deskpet.ini`，下次启动自动恢复。

---

## 五、开发过程中修复的 Bug（记录）

| # | 问题 | 原因 | 修复 |
|---|---|---|---|
| 1 | GDI+ 头文件在 C 模式下大量报错 | GDI+ 的 `Graphics`/`SolidBrush` 是 C++ 类，需 C++ 编译 | 渲染模块单独用 `.cpp`，其余保持纯 C，通过 `extern "C"` 互调 |
| 2 | `AddArc`/`AddLine` 重载歧义 | `REAL`(float) 与 `int` 参数混合 | 所有绘图字面量显式加 `.0f` |
| 3 | 跳跃时身体被窗口底边裁切 | 窗口高度未预留跳跃余量 | 窗口高 260→300，`PET_JUMP_MAX=62`，静止脚底上移 |
| 4 | **跳跃方向反了（向下坠）** | `lift = -jumpY` 符号错误，`jumpY` 为负已表示向上 | 改为 `lift = jumpY`，并复算初速度使峰值 ≤ 62px |

---

## 六、已验证项

- ✅ 零警告、零错误编译（`-Wall -Wextra`）
- ✅ PE32+ GUI 子系统，静态链接，无外部依赖
- ✅ 图标资源正确嵌入 `.rsrc` 段
- ✅ 渲染布局数值验证：6 种状态所有像素均在窗口内，无裁切
- ✅ 跳跃方向、最高点、滞空时间物理验证
- ✅ 10 分钟（36000 帧）状态机压力测试：零不变量违规，无状态卡死
- ✅ 状态分布合理：待机 48% / 散步 27% / 睡觉 14% / 说话 9% / 跳跃 3%

---

## 七、可扩展方向

- 用 `PlaySound` 加音效（`g_soundOn` 变量已预留）
- 支持多只宠物（把 `g_pet` 改成数组）
- 加系统托盘图标
- 用 `Gdiplus::Image` 加载 PNG 精灵图替换矢量绘制
- 加喂食 / 好感度系统

---

**License**: MIT
