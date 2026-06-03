# ======================== 平台检测与配置 ========================

UNAME_S := $(shell uname -s)

# --- Windows (MSYS2/MinGW) 配置 ---
ifneq ($(findstring MINGW,$(UNAME_S)),)
  PLATFORM := windows
  CC := gcc
  EXE_EXT := .exe
  RM := del /q

# --- macOS 配置 ---
else ifeq ($(UNAME_S),Darwin)
  PLATFORM := macos
  CC := gcc
  EXE_EXT :=
  RM := rm -f

# --- Linux 配置 ---
else
  PLATFORM := linux
  CC := cc
  EXE_EXT :=
  RM := rm -f
endif

# ======================== raylib 检测 ========================

# 优先使用 pkg-config（标准方式）
PKG_CONFIG := $(shell which pkg-config 2>/dev/null)
ifneq ($(PKG_CONFIG),)
  RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
  RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)
endif

# pkg-config 不可用或未安装 raylib 时使用平台特定路径
ifeq ($(RAYLIB_CFLAGS),)
  ifeq ($(PLATFORM),windows)
    # MSYS2/MinGW 默认安装路径
    RAYLIB_DIR  := C:/msys64/mingw64
  else ifeq ($(PLATFORM),macos)
    # 先尝试 Homebrew (Apple Silicon)
    ifeq ($(shell test -d /opt/homebrew/opt/raylib && echo yes),yes)
      RAYLIB_DIR := /opt/homebrew/opt/raylib
    else ifeq ($(shell test -d /opt/homebrew/include && echo yes),yes)
      RAYLIB_DIR := /opt/homebrew
    else
      # 本地预编译包
      RAYLIB_DIR := $(HOME)/raylib_local/raylib-5.5_macos
    endif
  else
    # Linux 常见路径
    RAYLIB_DIR := /usr/local
  endif
  RAYLIB_CFLAGS := -I$(RAYLIB_DIR)/include
  RAYLIB_LIBS   := -L$(RAYLIB_DIR)/lib -lraylib
endif

# macOS 额外依赖框架
ifeq ($(PLATFORM),macos)
  RAYLIB_LIBS += -framework CoreVideo -framework IOKit \
                 -framework Cocoa -framework GLUT -framework OpenGL
  # rpath 确保运行时找到动态库
  ifneq ($(RAYLIB_DIR),)
    LDFLAGS := -Wl,-rpath,$(RAYLIB_DIR)/lib
  endif
endif

# ======================== 编译参数 ========================

CFLAGS  := -Wall -Wextra -std=c17 -g -O2 $(RAYLIB_CFLAGS) -Iinclude
LDLIBS  := $(RAYLIB_LIBS)

# ======================== 文件路径 ========================

INC_DIR  := include
SRC_DIR  := src
BUILD_DIR:= build

SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)
TARGET   := $(BUILD_DIR)/monster_game$(EXE_EXT)

.PHONY: all clean run help install-font

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) -o $@ $^ $(LDLIBS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

-include $(DEPS)

run: all
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

# ======================== 安装中文字体 ========================

install-font:
ifeq ($(PLATFORM),macos)
	@echo "=== macOS 中文字体 ==="
	@echo "已内置支持 LXGW WenKai 和 STHeiti，无需额外安装"
	@echo "如中文字符显示异常，运行："
	@echo "  brew install font-lxgw-wenkai"
else ifeq ($(PLATFORM),windows)
	@echo "=== Windows 中文字体 ==="
	@echo "已内置支持 SimHei (黑体)，无需额外安装"
	@echo "如未找到，请确保系统已安装黑体字体"
else
	@echo "=== Linux 中文字体 ==="
	@echo "安装中文字体："
	@echo "  sudo apt install fonts-wqy-microhei   # Debian/Ubuntu"
	@echo "  sudo dnf install wqy-microhei-fonts   # Fedora"
endif

help:
	@echo "=== Monster Game 构建帮助 ==="
	@echo "平台: $(PLATFORM)"
	@echo ""
	@echo "  make             编译项目"
	@echo "  make run         编译并运行"
	@echo "  make clean       删除 build 目录"
	@echo "  make install-font  显示字体安装说明"
	@echo "  make help        显示此帮助"
	@echo ""
	@echo "=== 依赖 ==="
	@echo "需要 raylib 5.x 图形库"
	@echo "macOS:  brew install raylib"
	@echo "Windows: pacman -S mingw-w64-x86_64-raylib (MSYS2)"
	@echo "Linux:  sudo apt install libraylib-dev"
