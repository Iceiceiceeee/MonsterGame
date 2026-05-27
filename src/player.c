#include "player.h"
#include <math.h>
#include <string.h>

#define FRAME_DURATION    0.2f
#define PLAYER_VISUAL_W   56.0f
#define PLAYER_VISUAL_H   72.0f
#define PLAYER_COLLIDE_W  24.0f
#define PLAYER_COLLIDE_H  16.0f

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

/* ---- helpers ---- */

static Rectangle playerCollideRect(const Player *p)
{
    return (Rectangle){
        p->pos.x + (p->size.x - PLAYER_COLLIDE_W) / 2,
        p->pos.y + p->size.y - PLAYER_COLLIDE_H,
        PLAYER_COLLIDE_W,
        PLAYER_COLLIDE_H
    };
}

static bool rectsOverlap(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width &&
           a.x + a.width > b.x &&
           a.y < b.y + b.height &&
           a.y + a.height > b.y;
}

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

/* ---- update ---- */

void UpdatePlayer(Player *p, Map *map, float dt)
{
    /* --- input: direction --- */
    Vector2 input = {0};
    if (IsKeyDown(KEY_W)     || IsKeyDown(KEY_UP))    input.y -= 1;
    if (IsKeyDown(KEY_S)     || IsKeyDown(KEY_DOWN))  input.y += 1;
    if (IsKeyDown(KEY_A)     || IsKeyDown(KEY_LEFT))  input.x -= 1;
    if (IsKeyDown(KEY_D)     || IsKeyDown(KEY_RIGHT)) input.x += 1;

    /* normalize diagonal movement */
    if (input.x != 0 && input.y != 0) {
        float inv = 1.0f / sqrtf(2.0f);
        input.x *= inv;
        input.y *= inv;
    }

    p->running = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    p->moving  = (input.x != 0 || input.y != 0);

    /* --- direction from input --- */
    if (p->moving) {
        if (fabsf(input.y) >= fabsf(input.x)) {
            p->dir = (input.y > 0) ? DIR_DOWN : DIR_UP;
        } else {
            p->dir = (input.x < 0) ? DIR_LEFT : DIR_RIGHT;
        }
    }

    /* --- animation frame --- */
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

    if (p->moving) {
        p->frameTimer += dt;
        if (p->frameTimer >= FRAME_DURATION) {
            p->frameTimer -= FRAME_DURATION;
            p->frame = (p->frame + 1) % p->frameCount;
        }
    } else {
        p->frame = 0;
        p->frameTimer = 0;
    }

    /* --- movement --- */
    float speed = p->running ? p->runSpeed : p->walkSpeed;
    float dx = input.x * speed * dt;
    float dy = input.y * speed * dt;

    /* --- collision detection --- */
    Rectangle solidRects[MAX_SOLID_RECTS];
    int solidCount = GetSolidRects(map, solidRects, MAX_SOLID_RECTS);

    /* move X, resolve */
    Rectangle collide = playerCollideRect(p);
    collide.x += dx;
    for (int i = 0; i < solidCount; i++) {
        resolveCollisionX(&collide, solidRects[i]);
    }
    p->pos.x += (collide.x - playerCollideRect(p).x);

    /* move Y, resolve */
    collide = playerCollideRect(p);
    collide.y += dy;
    for (int i = 0; i < solidCount; i++) {
        resolveCollisionY(&collide, solidRects[i]);
    }
    p->pos.y += (collide.y - playerCollideRect(p).y);

    /* clamp to map bounds */
    float mapW = (float)(map->width  * map->tileWidth);
    float mapH = (float)(map->height * map->tileHeight);
    if (p->pos.x < 0) p->pos.x = 0;
    if (p->pos.y < 0) p->pos.y = 0;
    if (p->pos.x > mapW - p->size.x) p->pos.x = mapW - p->size.x;
    if (p->pos.y > mapH - p->size.y) p->pos.y = mapH - p->size.y;

    /* --- stairs check --- */
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

    /* --- door check --- */
    p->onDoor = false;
    p->doorTargetMap[0] = '\0';
    for (int i = 0; i < map->objectCount; i++) {
        if (strcmp(map->objects[i].type, "door") == 0) {
            if (rectsOverlap(footRect, map->objects[i].rect)) {
                p->onDoor = true;
                strncpy(p->doorTargetMap, map->objects[i].targetMap,
                        sizeof(p->doorTargetMap) - 1);
                p->doorTargetX = map->objects[i].targetX;
                p->doorTargetY = map->objects[i].targetY;
                TraceLog(LOG_INFO, "DOOR: targetMap=%s target=(%.0f, %.0f)",
                         p->doorTargetMap, p->doorTargetX, p->doorTargetY);
                break;
            }
        }
    }
}

/* ---- draw ---- */

void DrawPlayer(Player *p, Map *map)
{
    /* player sheet is the last tileset */
    if (map->tilesetCount == 0) return;
    Tileset *ps = &map->tilesets[map->tilesetCount - 1];

    int tileId = p->animFrames[p->frame];
    int col    = tileId % ps->columns;
    float tw   = (float)ps->tileWidth;
    float th   = (float)ps->tileHeight;

    Rectangle src = { col * tw, 0, tw, th };
    if (p->dir == DIR_RIGHT) {
        src.width = -tw;
    }

    Rectangle dst = {
        p->pos.x,
        p->pos.y,
        p->size.x,
        p->size.y
    };

    DrawTexturePro(ps->texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}
