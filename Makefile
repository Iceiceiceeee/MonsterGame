# ============================================================
# Makefile - Monster Game（宝可梦风格游戏）
#
# 支持平台：
#   - Windows (MSYS2/MinGW)
#   - macOS
#   - Linux
#
# 依赖：raylib 图形库
# ============================================================

# ======================== 平台检测与配置 ========================

UNAME_S := $(shell uname -s)

# --- Windows (MSYS2/MinGW) 配置 ---
ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
  # 确保 MSYS2 MinGW 工具链在 PATH 中
  MSYS2_MINGW := C:/msys64/mingw64
  export PATH := $(MSYS2_MINGW)/bin:$(PATH)
  CC := gcc

# --- macOS 配置 ---
else ifeq ($(UNAME_S),Darwin)
  CC := gcc

# --- Linux 配置 ---
else
  CC := cc
endif

# ======================== 编译参数 ========================

# 获取 raylib 编译参数（头文件路径等）
RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
# 获取 raylib 链接参数（库文件路径等）
RAYLIB_LIBS := $(shell pkg-config --libs raylib)

# macOS 额外依赖框架
ifeq ($(UNAME_S),Darwin)
  RAYLIB_LIBS += -framework CoreVideo -framework IOKit \
                 -framework Cocoa -framework GLUT -framework OpenGL
endif

# 编译选项：开启所有警告、C17 标准、调试信息、二级优化
CFLAGS := -Wall -Wextra -std=c17 -g -O2 $(RAYLIB_CFLAGS) -Iinclude
# 链接选项
LDLIBS := $(RAYLIB_LIBS)

# ======================== 文件路径 ========================

BUILD_DIR := build                     # 编译输出目录
TARGET    := monster_game
SRCS      := src/game.c src/main.c

# ======================== 伪目标声明 ========================

.PHONY: all clean run makerun help

# ======================== 编译规则 ========================

all: $(TARGET)                          # 默认目标：编译整个项目

# 编译所有源文件并链接生成可执行文件
# 使用 -o $(TARGET) 直接生成到项目根目录，避免 Make 4.4.1 对路径中斜杠的兼容性问题
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o "$@" $^ $(LDLIBS)

# 编译并运行游戏
run: $(TARGET)
	"./$(TARGET)"

# makerun 作为 run 的别名
makerun: run

# 清理编译产物（可执行文件 + build 目录）
clean:
	rm -f $(TARGET)
	rm -f $(TARGET).exe
	rm -rf $(BUILD_DIR)

# 显示帮助信息
help:
	@echo "=========================================="
	@echo "  Monster Game - 编译命令说明"
	@echo "=========================================="
	@echo "  make         编译项目"
	@echo "  make run     编译并运行游戏"
	@echo "  make makerun 等同于 make run"
	@echo "  make clean   删除编译产物"
	@echo "  make help    显示此帮助信息"
	@echo "=========================================="
