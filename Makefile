CC       := cc
CFLAGS   := -Wall -Wextra -std=c17 -g -O2
INC_DIR  := include
SRC_DIR  := src
BUILD_DIR:= build

SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)
TARGET   := $(BUILD_DIR)/monster_game

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
RAYLIB_LIBS   := $(shell pkg-config --libs   raylib)
LDLIBS   := $(RAYLIB_LIBS) \
            -framework CoreVideo -framework IOKit \
            -framework Cocoa -framework GLUT -framework OpenGL

CFLAGS   += $(RAYLIB_CFLAGS) -I$(INC_DIR)

.PHONY: all clean run help

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

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
