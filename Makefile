SHELL := /bin/bash

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

INC_DIR  := include
SRC_DIR  := src
BUILD_DIR:= build

SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)
TARGET   := $(BUILD_DIR)/monster_game

.PHONY: all clean run help

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	TMPDIR=/tmp TEMP=/tmp TMP=/tmp $(CC) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	TMPDIR=/tmp TEMP=/tmp TMP=/tmp $(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

-include $(DEPS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "用法："
	@echo "  make         编译项目"
	@echo "  make run     编译并运行"
	@echo "  make clean   删除 build 目录"
	@echo "  make help    显示此帮助"
