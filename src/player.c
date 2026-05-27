/**
 * player.c —— 玩家角色逻辑
 *
 * 该模块负责玩家的初始化、输入处理、移动、碰撞检测、
 * 动画切换以及绘制渲染。
 */

#include "player.h"
#include <math.h>
#include <string.h>

/* ========== 动画与碰撞体积常量 ========== */

#define FRAME_DURATION    0.2f   /* 每帧动画持续时长（秒） */
#define PLAYER_VISUAL_W   56.0f  /* 玩家贴图绘制宽度（像素） */
#define PLAYER_VISUAL_H   72.0f  /* 玩家贴图绘制高度（像素） */
#define PLAYER_COLLIDE_W  24.0f  /* 碰撞盒宽度（比视觉宽小，留出余量） */
#define PLAYER_COLLIDE_H  16.0f  /* 碰撞盒高度（放在角色底部） */

/* ========== 初始化 ========== */

/**
 * InitPlayer - 初始化玩家对象
 * @p:       玩家指针
 * @spawn:   出生点坐标
 * @map:     所属地图（用于获取初始动画帧）
 *
 * 清零玩家结构体，设置位置、尺寸、方向、速度等默认值，
 * 并将动画帧序列指向地图提供的前走低速帧。
 */
void InitPlayer(Player *p, Vector2 spawn, Map *map)
{
    memset(p, 0, sizeof(*p));
    p->pos  = spawn;
    p->size = (Vector2){ PLAYER_VISUAL_W, PLAYER_VISUAL_H };
    p->dir  = DIR_DOWN;
    p->walkSpeed  = 80.0f;
    p->runSpeed   = 180.0f;
    p->animFrames = map->animFrontLow;
    p->frameCount = 3;
    (void)map;
}

/* ========== 内部辅助函数 ========== */

/**
 * playerCollideRect - 计算玩家的碰撞盒（底部矩形区域）
 * @p: 玩家指针
 *
 * 碰撞盒位于角色底部中央，宽度和高度由常量定义，
 * 用于与地图中的实心/楼梯区域做碰撞检测。
 */
static Rectangle playerCollideRect(const Player *p)
{
    return (Rectangle){
        p->pos.x + (p->size.x - PLAYER_COLLIDE_W) / 2,
        p->pos.y + p->size.y - PLAYER_COLLIDE_H,
        PLAYER_COLLIDE_W,
        PLAYER_COLLIDE_H
    };
}

/**
 * rectsOverlap - 判断两个矩形是否相交（AABB 碰撞检测）
 * @a, @b: 两个矩形
 *
 * 返回 true 表示有重叠，false 表示无重叠。
 */
static bool rectsOverlap(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width &&
           a.x + a.width > b.x &&
           a.y < b.y + b.height &&
           a.y + a.height > b.y;
}

/**
 * resolveCollisionX - 在 X 轴上将 collide 矩形推出 solid 矩形
 * @collide: 玩家碰撞盒（会被修改）
 * @solid:   地图上的障碍物（实心块）
 *
 * 计算最小推开距离并沿 X 轴修正 collide 位置。
 * 返回 true 表示发生了碰撞。
 */
static bool resolveCollisionX(Rectangle *collide, Rectangle solid)
{
    if (!rectsOverlap(*collide, solid)) return false;

    float overlapLeft  = (collide->x + collide->width)  - solid.x;
    float overlapRight = (solid.x + solid.width) - collide->x;

    if (overlapLeft < overlapRight) {
        collide->x -= overlapLeft;
    } else {
        collide->x += overlapRight;
    }
    return true;
}

/**
 * resolveCollisionY - 在 Y 轴上将 collide 矩形推出 solid 矩形
 * @collide: 玩家碰撞盒（会被修改）
 * @solid:   地图上的障碍物（实心块）
 *
 * 计算最小推开距离并沿 Y 轴修正 collide 位置。
 * 返回 true 表示发生了碰撞。
 */
static bool resolveCollisionY(Rectangle *collide, Rectangle solid)
{
    if (!rectsOverlap(*collide, solid)) return false;

    float overlapTop    = (collide->y + collide->height) - solid.y;
    float overlapBottom = (solid.y + solid.height) - collide->y;

    if (overlapTop < overlapBottom) {
        collide->y -= overlapTop;
    } else {
        collide->y += overlapBottom;
    }
    return true;
}

/* ========== 更新逻辑 ========== */

/**
 * UpdatePlayer - 每帧更新玩家状态
 * @p:   玩家指针
 * @map: 当前地图
 * @dt:  上一帧到本帧的时间差（delta time）
 *
 * 主要流程：
 *   1. 读取键盘方向输入，对角线移动归一化
 *   2. 检测奔跑/移动状态，根据输入设置朝向
 *   3. 根据朝向和奔跑状态切换动画帧序列
 *   4. 更新动画计时器
 *   5. 计算位移，进行 X / Y 轴分离的碰撞响应
 *   6. 将玩家位置限制在地图边界内
 *   7. 检测玩家是否站在楼梯上
 */
void UpdatePlayer(Player *p, Map *map, float dt)
{
    /* --- 第一步：读取方向输入 --- */
    Vector2 input = {0};
    if (IsKeyDown(KEY_W)     || IsKeyDown(KEY_UP))    input.y -= 1;
    if (IsKeyDown(KEY_S)     || IsKeyDown(KEY_DOWN))  input.y += 1;
    if (IsKeyDown(KEY_A)     || IsKeyDown(KEY_LEFT))  input.x -= 1;
    if (IsKeyDown(KEY_D)     || IsKeyDown(KEY_RIGHT)) input.x += 1;

    /* 对角线移动归一化，防止斜向速度变快 */
    if (input.x != 0 && input.y != 0) {
        float inv = 1.0f / sqrtf(2.0f);
        input.x *= inv;
        input.y *= inv;
    }

    p->running = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    p->moving  = (input.x != 0 || input.y != 0);

    /* --- 第二步：根据输入确定朝向 --- */
    if (p->moving) {
        if (fabsf(input.y) >= fabsf(input.x)) {
            p->dir = (input.y > 0) ? DIR_DOWN : DIR_UP;
        } else {
            p->dir = (input.x < 0) ? DIR_LEFT : DIR_RIGHT;
        }
    }

    /* --- 第三步：切换动画帧序列 --- */
    switch (p->dir) {
    case DIR_DOWN:
        p->animFrames = p->running ? map->animFrontFast : map->animFrontLow;
        break;
    case DIR_UP:
        p->animFrames = map->animBack;
        break;
    case DIR_LEFT:
    case DIR_RIGHT:
        p->animFrames = map->animLeft;
        break;
    }
    p->frameCount = 3;

    /* --- 第四步：动画计时器更新 --- */
    if (p->moving) {
        p->frameTimer += dt;
        if (p->frameTimer >= FRAME_DURATION) {
            p->frameTimer -= FRAME_DURATION;
            p->frame = (p->frame + 1) % p->frameCount;
        }
    } else {
        p->frame = 0;         /* 静止时回到第 0 帧 */
        p->frameTimer = 0;
    }

    /* --- 第五步：位移与碰撞处理 --- */
    float speed = p->running ? p->runSpeed : p->walkSpeed;
    float dx = input.x * speed * dt;
    float dy = input.y * speed * dt;

    /* 从地图获取所有实心障碍物矩形 */
    Rectangle solidRects[MAX_SOLID_RECTS];
    int solidCount = GetSolidRects(map, solidRects, MAX_SOLID_RECTS);

    /* X 轴移动与碰撞纠正 */
    Rectangle collide = playerCollideRect(p);
    collide.x += dx;
    for (int i = 0; i < solidCount; i++) {
        resolveCollisionX(&collide, solidRects[i]);
    }
    p->pos.x += (collide.x - playerCollideRect(p).x);

    /* Y 轴移动与碰撞纠正 */
    collide = playerCollideRect(p);
    collide.y += dy;
    for (int i = 0; i < solidCount; i++) {
        resolveCollisionY(&collide, solidRects[i]);
    }
    p->pos.y += (collide.y - playerCollideRect(p).y);

    /* --- 第六步：限制在地图边界内 --- */
    float mapW = (float)(map->width  * map->tileWidth);
    float mapH = (float)(map->height * map->tileHeight);
    if (p->pos.x < 0) p->pos.x = 0;
    if (p->pos.y < 0) p->pos.y = 0;
    if (p->pos.x > mapW - p->size.x) p->pos.x = mapW - p->size.x;
    if (p->pos.y > mapH - p->size.y) p->pos.y = mapH - p->size.y;

    /* --- 第七步：楼梯检测 --- */
    Rectangle stairsRects[MAX_STAIRS_RECTS];
    int stairsCount = GetStairsRects(map, stairsRects, MAX_STAIRS_RECTS);
    p->onStairs = false;
    Rectangle footRect = playerCollideRect(p);
    for (int i = 0; i < stairsCount; i++) {
        if (rectsOverlap(footRect, stairsRects[i])) {
            p->onStairs = true;
            break;
        }
    }
}

/* ========== 绘制 ========== */

/**
 * DrawPlayer - 绘制玩家精灵
 * @p:   玩家指针
 * @map: 当前地图（用于获取贴图和瓦片信息）
 *
 * 根据当前动画帧的 tile ID 从 tileset 中截取对应区域，
 * 绘制到玩家位置。朝右时通过将 src.width 设为负值实现水平翻转。
 */
void DrawPlayer(Player *p, Map *map)
{
    int tileId = p->animFrames[p->frame];
    int col    = tileId % map->psCols;
    float tw   = (float)map->psTileW;
    float th   = (float)map->psTileH;

    Rectangle src = { col * tw, 0, tw, th };
    if (p->dir == DIR_RIGHT) {
        src.width = -tw;           /* 水平翻转，复用左边帧 */
    }

    Rectangle dst = {
        p->pos.x,
        p->pos.y,
        p->size.x,
        p->size.y
    };

    DrawTexturePro(map->playerSheet, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}
