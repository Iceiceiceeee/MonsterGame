#ifndef MAP_H
#define MAP_H

#include "raylib.h"

#define MAX_MAP_OBJECTS   128
#define MAX_SOLID_RECTS    64
#define MAX_STAIRS_RECTS   16
#define MAX_TILESETS        8

typedef struct {
    Texture2D texture;
    int firstGid;
    int columns;
    int tileWidth;
    int tileHeight;
} Tileset;

typedef struct {
    char name[64];
    char type[32];
    char targetMap[64];
    float targetX;
    float targetY;
    Rectangle rect;
} MapObject;

typedef struct {
    /* map dimensions */
    int width, height;
    int tileWidth, tileHeight;

    /* floor tile gid array (size = width * height), 0 = empty */
    int *floorData;
    int dataSize;

    /* tilesets parsed from TMJ */
    Tileset tilesets[MAX_TILESETS];
    int tilesetCount;

    /* background image layer */
    Texture2D backImage;
    float backOpacity;
    bool hasBackImage;

    /* objects parsed from object layers */
    MapObject objects[MAX_MAP_OBJECTS];
    int objectCount;

    /* player spawn position (from first player object) */
    Vector2 playerSpawn;

    /* animation frame tile ids (local to player sheet) */
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

#endif
