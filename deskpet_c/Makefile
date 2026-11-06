# ============================================================
#  DeskPet - Windows 桌面宠物 (C / Win32 / GDI+)
#
#  交叉编译（Linux/WSL）:  make
#  原生编译（Windows MinGW）: make CROSS=
#  产物: build/DeskPet.exe
# ============================================================

CROSS   ?= x86_64-w64-mingw32-
CC      := $(CROSS)gcc
CXX     := $(CROSS)g++
WINDRES := $(CROSS)windres

TARGET  := build/DeskPet.exe
SRCDIR  := src
BUILDDIR:= build

# C 源文件
CSRCS  := $(wildcard $(SRCDIR)/*.c)
# C++ 源文件（GDI+ 渲染模块）
CXXSRCS:= $(wildcard $(SRCDIR)/*.cpp)

COBJS  := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(CSRCS))
CXXOBJS:= $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(CXXSRCS))
RES    := $(BUILDDIR)/resource.o

# 公共编译选项
COMMON := -O2 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 \
          -finput-charset=UTF-8 -fexec-charset=UTF-8

# -municode : 使用 wWinMain 入口
CFLAGS   := $(COMMON) -std=c99 -municode
CXXFLAGS := $(COMMON) -std=c++17 -municode -fno-exceptions -fno-rtti

# -mwindows : 无控制台窗口
LDFLAGS := -mwindows -municode -static -s
LIBS    := -lgdiplus -lgdi32 -luser32 -lkernel32 -lcomctl32 -lole32 -luuid

.PHONY: all clean run info

all: $(TARGET)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/pet.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/pet.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(RES): $(SRCDIR)/resource.rc $(SRCDIR)/app.ico | $(BUILDDIR)
	$(WINDRES) -I $(SRCDIR) $< -O coff -o $@

$(TARGET): $(COBJS) $(CXXOBJS) $(RES)
	$(CXX) $(COBJS) $(CXXOBJS) $(RES) -o $@ $(LDFLAGS) $(LIBS)
	@echo ""
	@echo "=============================================="
	@echo "  构建成功 -> $(TARGET)"
	@ls -lh $(TARGET) | awk '{print "  文件大小: " $$5}'
	@echo "=============================================="

clean:
	rm -rf $(BUILDDIR)

run: $(TARGET)
	wine $(TARGET)

info:
	@echo "CC     = $(CC)"
	@echo "CXX    = $(CXX)"
	@echo "C src  = $(CSRCS)"
	@echo "C++ src= $(CXXSRCS)"
