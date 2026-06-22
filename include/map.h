#ifndef MAP_H
#define MAP_H

#include "raylib.h"

#define MAX_MAP_OBJECTS     128
#define MAX_SOLID_RECTS      64
#define MAX_STAIRS_RECTS     16
#define MAX_DOOR_RECTS       16
#define MAX_STAIRFIRST_RECTS 16
#define MAX_SIGN_RECTS       16
#define MAX_TILESETS          8
#define MAX_TELEPORT_SPAWNS   8
#define MAX_NPC_OBJECTS      16
#define MAX_SOLID_TILES      64

typedef struct {
    char name[64];
    char type[32];
    Rectangle rect;
} MapObject;

typedef struct {
    Texture2D texture;
    int firstGid;
    int cols;
    int tileW;
    int tileH;
} TilesetInfo;

typedef struct {
    char name[64];
    Vector2 pos;
} TeleportSpawn;

typedef struct {
    int gid;
    Rectangle rect;
    char type[32];
} MapNpc;

typedef struct {
    int width;
    int height;
    int tileWidth;
    int tileHeight;

    int *floorData;
    int dataSize;

    TilesetInfo tilesets[MAX_TILESETS];
    int tilesetCount;

    Texture2D playerSheet;
    int psCols;
    int psFirstGid;
    int psTileW;
    int psTileH;

    Texture2D backImage;
    float backOpacity;
    bool hasBackImage;

    MapObject objects[MAX_MAP_OBJECTS];
    int objectCount;

    Vector2 playerSpawn;

    TeleportSpawn teleportSpawns[MAX_TELEPORT_SPAWNS];
    int teleportSpawnCount;

    MapNpc npcs[MAX_NPC_OBJECTS];
    int npcCount;

    int solidGids[MAX_SOLID_TILES];
    int solidGidCount;

    float playerScale;

    int animFrontLow[3];
    int animFrontFast[3];
    int animBack[3];
    int animLeft[3];
} Map;

Map  LoadMap(const char *filepath);
void UnloadMap(Map *map);
void DrawMap(Map *map);

int  GetSolidRects(Map *map, Rectangle *out, int maxCount);
int  GetStairsRects(Map *map, Rectangle *out, int maxCount);
int  GetDoorRects(Map *map, Rectangle *out, int maxCount);
int  GetStairFirstRects(Map *map, Rectangle *out, int maxCount);
int  GetChuansongRects(Map *map, Rectangle *out, int maxCount);
int  GetSignRects(Map *map, Rectangle *out, int maxCount);
bool GetSignName(Map *map, Rectangle rect, char *name, int size);
bool GetChuansongName(Map *map, Rectangle rect, char *name, int size);
bool FindTeleportSpawn(Map *map, const char *name, Vector2 *pos);
void DrawNpcs(Map *map);
int  GetNpcRects(Map *map, Rectangle *out, int maxCount);
bool GetNpcInfo(Map *map, Rectangle rect, char *type, int size);
bool IsGidSolid(Map *map, int gid);

#endif
