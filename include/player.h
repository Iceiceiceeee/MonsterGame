#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "map.h"

typedef enum {
    DIR_DOWN,
    DIR_UP,
    DIR_LEFT,
    DIR_RIGHT
} PlayerDir;

typedef struct {
    Vector2 pos;
    Vector2 size;
    float scale;         /**< 精灵缩放因子 (1.0=默认, 0.333=室外) */
    PlayerDir dir;
    bool running;
    bool moving;
    bool onStairs;
    bool onDoor;
    bool onStairFirst;
    bool onChuansong;
    char chuansongName[64];  /**< 当前踩中的传送点名称 */
    bool onSign;
    char signName[64];       /**< 当前接触的标牌名称 */
    bool onNpc;              /**< 是否接触到NPC */
    char npcType[32];        /**< 当前接触的NPC类型（npc-boss, npc-teacher） */

    /* animation */
    int frame;
    float frameTimer;
    int frameCount;
    int *animFrames;

    float walkSpeed;
    float runSpeed;
} Player;

void InitPlayer(Player *p, Vector2 spawn, Map *map);
void UpdatePlayer(Player *p, Map *map, float dt);
void DrawPlayer(Player *p, Map *map);

#endif
