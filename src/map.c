#include "map.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* extract filename from a path, e.g. "../素材/foo.png" -> "foo.png" */
static const char *filenameFromPath(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') last = p + 1;
    }
    return last;
}

/* directory containing the TMJ file */
static char g_mapDir[512] = "";

static void resolveFullPath(char *dst, size_t dstSize, const char *relativePath)
{
    const char *fname = filenameFromPath(relativePath);
    snprintf(dst, dstSize, "%s/%s", g_mapDir, fname);
}

/* ======================== map loading ======================== */

Map LoadMap(const char *filepath)
{
    Map map;
    memset(&map, 0, sizeof(map));

    /* store map directory for image resolution */
    strncpy(g_mapDir, filepath, sizeof(g_mapDir) - 1);
    char *lastSep = strrchr(g_mapDir, '/');
    char *lastSep2 = strrchr(g_mapDir, '\\');
    if (lastSep2 > lastSep) lastSep = lastSep2;
    if (lastSep) *lastSep = '\0';

    /* read TMJ */
    char *jsonStr = LoadFileText(filepath);
    if (!jsonStr) {
        TraceLog(LOG_ERROR, "Failed to load TMJ: %s", filepath);
        return map;
    }

    cJSON *root = cJSON_Parse(jsonStr);
    if (!root) {
        TraceLog(LOG_ERROR, "Failed to parse TMJ JSON: %s",
                 cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown error");
        UnloadFileText(jsonStr);
        return map;
    }

    /* ---- map dimensions ---- */
    map.width       = cJSON_GetObjectItem(root, "width")->valueint;
    map.height      = cJSON_GetObjectItem(root, "height")->valueint;
    map.tileWidth   = cJSON_GetObjectItem(root, "tilewidth")->valueint;
    map.tileHeight  = cJSON_GetObjectItem(root, "tileheight")->valueint;

    /* ---- parse layers ---- */
    cJSON *layers = cJSON_GetObjectItem(root, "layers");
    cJSON *layer  = NULL;
    cJSON_ArrayForEach(layer, layers) {
        const char *type = cJSON_GetObjectItem(layer, "type")->valuestring;
        const char *name = cJSON_GetObjectItem(layer, "name")->valuestring;

        if (strcmp(type, "imagelayer") == 0 && strcmp(name, "back") == 0) {
            const char *imgPath = cJSON_GetObjectItem(layer, "image")->valuestring;
            cJSON *opacityNode  = cJSON_GetObjectItem(layer, "opacity");
            map.backOpacity     = opacityNode ? (float)opacityNode->valuedouble : 1.0f;

            char fullPath[1024];
            resolveFullPath(fullPath, sizeof(fullPath), imgPath);
            Image img = LoadImage(fullPath);
            if (img.data) {
                ImageResize(&img, map.width * map.tileWidth, map.height * map.tileHeight);
                map.backImage = LoadTextureFromImage(img);
                UnloadImage(img);
                map.hasBackImage = true;
            }
        }
        else if (strcmp(type, "tilelayer") == 0 && strcmp(name, "floor") == 0) {
            cJSON *dataArr = cJSON_GetObjectItem(layer, "data");
            if (dataArr && cJSON_IsArray(dataArr)) {
                map.dataSize = cJSON_GetArraySize(dataArr);
                map.floorData = malloc(sizeof(int) * map.dataSize);
                for (int i = 0; i < map.dataSize; i++) {
                    cJSON *elem = cJSON_GetArrayItem(dataArr, i);
                    map.floorData[i] = elem ? elem->valueint : 0;
                }
            }
        }
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "item") == 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                if (map.objectCount >= MAX_MAP_OBJECTS) break;
                MapObject *mo = &map.objects[map.objectCount];

                mo->rect.x      = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                mo->rect.y      = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                mo->rect.width  = (float)cJSON_GetObjectItem(obj, "width")->valuedouble;
                mo->rect.height = (float)cJSON_GetObjectItem(obj, "height")->valuedouble;

                /* extract type from properties */
                cJSON *props = cJSON_GetObjectItem(obj, "properties");
                if (props && cJSON_IsArray(props)) {
                    cJSON *prop = NULL;
                    cJSON_ArrayForEach(prop, props) {
                        const char *pname  = cJSON_GetObjectItem(prop, "name")->valuestring;
                        cJSON *pval        = cJSON_GetObjectItem(prop, "value");
                        if (pval && cJSON_IsTrue(pval)) {
                            strncpy(mo->type, pname, sizeof(mo->type) - 1);
                        }
                    }
                }
                map.objectCount++;
            }
        }
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "player") == 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            bool firstPlayer = true;
            cJSON_ArrayForEach(obj, objects) {
                if (firstPlayer) {
                    map.playerSpawn.x = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                    map.playerSpawn.y = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                    firstPlayer = false;
                }
            }
        }
    }

    /* ---- load tileset textures ---- */
    {
        char path[1024];

        /* tileset 1: QQ_1779796916957.png, 16x16, 35 cols */
        resolveFullPath(path, sizeof(path), "QQ_1779796916957.png");
        Image img = LoadImage(path);
        if (img.data) {
            map.tileset1    = LoadTextureFromImage(img);
            map.ts1Cols     = img.width / 16;
            map.ts1FirstGid = 1;
            UnloadImage(img);
        }

        /* tileset 2: 6abad8d14de667fabaecfea4a7242f82.png, 16x16, 35 cols */
        resolveFullPath(path, sizeof(path), "6abad8d14de667fabaecfea4a7242f82.png");
        img = LoadImage(path);
        if (img.data) {
            map.tileset2    = LoadTextureFromImage(img);
            map.ts2Cols     = 35;
            map.ts2FirstGid = 946;
            UnloadImage(img);
        }

        /* player sheet: player_sheet.png, 16x20, 14 cols */
        resolveFullPath(path, sizeof(path), "player_sheet.png");
        img = LoadImage(path);
        if (img.data) {
            map.playerSheet  = LoadTextureFromImage(img);
            map.psCols       = img.width / 16;
            map.psFirstGid   = 1786;
            map.psTileW      = 16;
            map.psTileH      = img.height;
            UnloadImage(img);
        }
    }

    /* ---- default player spawn if none found ---- */
    if (map.playerSpawn.x == 0 && map.playerSpawn.y == 0) {
        map.playerSpawn.x = (float)(map.width * map.tileWidth) / 2;
        map.playerSpawn.y = (float)(map.height * map.tileHeight) / 2;
    }

    /* ---- animation frame tables ---- */
    map.animFrontLow[0]  = 0;  map.animFrontLow[1]  = 12; map.animFrontLow[2]  = 13;
    map.animFrontFast[0] = 3;  map.animFrontFast[1] = 4;  map.animFrontFast[2] = 5;
    map.animBack[0]      = 6;  map.animBack[1]      = 7;  map.animBack[2]      = 8;
    map.animLeft[0]      = 9;  map.animLeft[1]      = 10; map.animLeft[2]      = 11;

    cJSON_Delete(root);
    UnloadFileText(jsonStr);
    return map;
}

void UnloadMap(Map *map)
{
    free(map->floorData);
    if (map->tileset1.id > 0)    UnloadTexture(map->tileset1);
    if (map->tileset2.id > 0)    UnloadTexture(map->tileset2);
    if (map->playerSheet.id > 0) UnloadTexture(map->playerSheet);
    if (map->hasBackImage)       UnloadTexture(map->backImage);
    memset(map, 0, sizeof(*map));
}

/* ======================== drawing ======================== */

static Rectangle tileSourceRect(int gid, int firstGid, int cols, int tileW, int tileH)
{
    int localId = gid - firstGid;
    int col = localId % cols;
    int row = localId / cols;
    return (Rectangle){ (float)(col * tileW), (float)(row * tileH), (float)tileW, (float)tileH };
}

void DrawMap(Map *map)
{
    int mapPixelW = map->width  * map->tileWidth;
    int mapPixelH = map->height * map->tileHeight;

    /* background image */
    if (map->hasBackImage) {
        DrawTexture(map->backImage, 0, 0,
                    Fade(WHITE, map->backOpacity));
    } else {
        DrawRectangle(0, 0, mapPixelW, mapPixelH, BLACK);
    }

    /* floor tiles */
    if (map->floorData) {
        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                int gid = map->floorData[y * map->width + x];
                if (gid == 0) continue;

                Rectangle src = {0};
                Texture2D tex = {0};

                if (gid >= map->ts2FirstGid && map->tileset2.id > 0) {
                    src = tileSourceRect(gid, map->ts2FirstGid, map->ts2Cols,
                                         map->tileWidth, map->tileHeight);
                    tex = map->tileset2;
                } else if (gid >= map->ts1FirstGid && map->tileset1.id > 0) {
                    src = tileSourceRect(gid, map->ts1FirstGid, map->ts1Cols,
                                         map->tileWidth, map->tileHeight);
                    tex = map->tileset1;
                }

                if (tex.id > 0) {
                    Rectangle dst = {
                        (float)(x * map->tileWidth),
                        (float)(y * map->tileHeight),
                        (float)map->tileWidth,
                        (float)map->tileHeight
                    };
                    DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
                }
            }
        }
    }
}

/* ======================== collision queries ======================== */

int GetSolidRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < map->objectCount && count < maxCount; i++) {
        if (strcmp(map->objects[i].type, "solid") == 0) {
            out[count++] = map->objects[i].rect;
        }
    }
    return count;
}

int GetStairsRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < map->objectCount && count < maxCount; i++) {
        if (strcmp(map->objects[i].type, "stairs") == 0) {
            out[count++] = map->objects[i].rect;
        }
    }
    return count;
}
