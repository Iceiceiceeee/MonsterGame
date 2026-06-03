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
    PlayerDir dir;
    bool running;
    bool moving;
    bool onStairs;
    bool onDoor;
    char doorTargetMap[64];
    float doorTargetX;
    float doorTargetY;
    bool onStairFirst;

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
