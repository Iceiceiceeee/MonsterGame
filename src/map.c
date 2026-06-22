#include "map.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *filenameFromPath(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') last = p + 1;
    }
    return last;
}

static char g_mapDir[512] = "";

static void resolveFullPath(char *dst, size_t dstSize, const char *relativePath)
{
    if (strncmp(relativePath, "../", 3) == 0) {
        const char *fname = filenameFromPath(relativePath);
        snprintf(dst, dstSize, "%s/%s", g_mapDir, fname);
    } else {
        snprintf(dst, dstSize, "%s/%s", g_mapDir, relativePath);
    }
}

// Parse .tsx (XML) or .tsj (JSON) tileset file
static bool parseTilesetFile(const char *filepath, char *imagePath, size_t pathSize,
                              int *cols, int *tileW, int *tileH)
{
    char *content = LoadFileText(filepath);
    if (!content) return false;

    bool ok = false;

    // JSON format (.tsj)
    if (strstr(filepath, ".tsj") || content[0] == '{')
    {
        cJSON *root = cJSON_Parse(content);
        if (root)
        {
            cJSON *node;
            if ((node = cJSON_GetObjectItem(root, "image")))
                snprintf(imagePath, pathSize, "%s", node->valuestring);
            if ((node = cJSON_GetObjectItem(root, "columns")))
                *cols = node->valueint;
            if ((node = cJSON_GetObjectItem(root, "tilewidth")))
                *tileW = node->valueint;
            if ((node = cJSON_GetObjectItem(root, "tileheight")))
                *tileH = node->valueint;
            cJSON_Delete(root);
            ok = true;
        }
    }
    // XML format (.tsx)
    else
    {
        char *p;

        if ((p = strstr(content, "columns=\"")))
            *cols = atoi(p + 9);
        if ((p = strstr(content, "tilewidth=\"")))
            *tileW = atoi(p + 11);
        if ((p = strstr(content, "tileheight=\"")))
            *tileH = atoi(p + 12);
        if ((p = strstr(content, "source=\"")))
        {
            p += 8;
            char *end = strchr(p, '"');
            if (end)
            {
                size_t len = end - p;
                if (len >= pathSize) len = pathSize - 1;
                memcpy(imagePath, p, len);
                imagePath[len] = '\0';
            }
        }
        ok = (*cols > 0);
    }

    UnloadFileText(content);
    return ok;
}

Map LoadMap(const char *filepath)
{
    Map map;
    memset(&map, 0, sizeof(map));

    // Extract map directory for resolving relative paths
    strncpy(g_mapDir, filepath, sizeof(g_mapDir) - 1);
    char *lastSep = strrchr(g_mapDir, '/');
    char *lastSep2 = strrchr(g_mapDir, '\\');
    if (lastSep2 > lastSep) lastSep = lastSep2;
    if (lastSep) *lastSep = '\0';

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

    map.width       = cJSON_GetObjectItem(root, "width")->valueint;
    map.height      = cJSON_GetObjectItem(root, "height")->valueint;
    map.tileWidth   = cJSON_GetObjectItem(root, "tilewidth")->valueint;
    map.tileHeight  = cJSON_GetObjectItem(root, "tileheight")->valueint;
    map.playerScale = 1.0f;

    map.dataSize = map.width * map.height;
    map.floorData = calloc(map.dataSize, sizeof(int));

    // Iterate layers
    cJSON *layers = cJSON_GetObjectItem(root, "layers");
    cJSON *layer  = NULL;
    cJSON_ArrayForEach(layer, layers) {
        const char *type = cJSON_GetObjectItem(layer, "type")->valuestring;
        const char *name = cJSON_GetObjectItem(layer, "name")->valuestring;

        // Background image layer
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
        // Tile layers — merge all into floorData
        else if (strcmp(type, "tilelayer") == 0) {
            cJSON *chunksArr = cJSON_GetObjectItem(layer, "chunks");
            if (chunksArr && cJSON_IsArray(chunksArr)) {
                // Infinite map: assemble from chunks
                cJSON *chunk = NULL;
                cJSON_ArrayForEach(chunk, chunksArr) {
                    int cx = cJSON_GetObjectItem(chunk, "x")->valueint;
                    int cy = cJSON_GetObjectItem(chunk, "y")->valueint;
                    int cw = cJSON_GetObjectItem(chunk, "width")->valueint;
                    int ch = cJSON_GetObjectItem(chunk, "height")->valueint;
                    cJSON *cdata = cJSON_GetObjectItem(chunk, "data");
                    if (!cdata || !cJSON_IsArray(cdata)) continue;
                    for (int row = 0; row < ch; row++) {
                        for (int col = 0; col < cw; col++) {
                            int mapX = cx + col;
                            int mapY = cy + row;
                            if (mapX >= 0 && mapX < map.width && mapY >= 0 && mapY < map.height) {
                                int idx = mapY * map.width + mapX;
                                cJSON *elem = cJSON_GetArrayItem(cdata, row * cw + col);
                                int gid = elem ? elem->valueint : 0;
                                if (gid != 0) map.floorData[idx] = gid;
                            }
                        }
                    }
                }
            } else {
                // Finite map: read from data array
                cJSON *dataArr = cJSON_GetObjectItem(layer, "data");
                if (dataArr && cJSON_IsArray(dataArr)) {
                    int layerSize = cJSON_GetArraySize(dataArr);
                    for (int i = 0; i < layerSize && i < map.dataSize; i++) {
                        cJSON *elem = cJSON_GetArrayItem(dataArr, i);
                        int gid = elem ? elem->valueint : 0;
                        if (gid != 0) map.floorData[i] = gid;
                    }
                }
            }
        }
        // Player spawn point
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "player") == 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                if (cJSON_GetObjectItem(obj, "gid")) continue;
                map.playerSpawn.x = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                map.playerSpawn.y = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                break;
            }
        }
        // Teleport layer (chuansong)
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "chuansong") == 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                float ox = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                float oy = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;

                cJSON *props = cJSON_GetObjectItem(obj, "properties");
                if (props && cJSON_IsArray(props)) {
                    cJSON *prop = NULL;
                    cJSON_ArrayForEach(prop, props) {
                        const char *pname = cJSON_GetObjectItem(prop, "name")->valuestring;
                        cJSON *pval = cJSON_GetObjectItem(prop, "value");
                        if (pval && cJSON_IsTrue(pval)) {
                            if (map.teleportSpawnCount < MAX_TELEPORT_SPAWNS) {
                                TeleportSpawn *ts = &map.teleportSpawns[map.teleportSpawnCount];
                                strncpy(ts->name, pname, sizeof(ts->name) - 1);
                                ts->pos.x = ox;
                                ts->pos.y = oy;
                                map.teleportSpawnCount++;
                            }
                            // Also register as trigger zone (48x48)
                            if (map.objectCount < MAX_MAP_OBJECTS) {
                                MapObject *mo = &map.objects[map.objectCount];
                                strncpy(mo->type, "chuansong", sizeof(mo->type) - 1);
                                strncpy(mo->name, pname, sizeof(mo->name) - 1);
                                mo->rect = (Rectangle){ ox - 24, oy - 24, 48, 48 };
                                map.objectCount++;
                            }
                        }
                    }
                }
            }
        }
        // NPC layer
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "npc") == 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                if (map.npcCount >= MAX_NPC_OBJECTS) break;
                MapNpc *npc = &map.npcs[map.npcCount];

                cJSON *gidNode = cJSON_GetObjectItem(obj, "gid");
                if (!gidNode) continue;
                npc->gid = gidNode->valueint;

                npc->rect.x      = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                npc->rect.y      = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                npc->rect.width  = (float)cJSON_GetObjectItem(obj, "width")->valuedouble;
                npc->rect.height = (float)cJSON_GetObjectItem(obj, "height")->valuedouble;

                // Extract NPC type from properties
                npc->type[0] = '\0';
                cJSON *props = cJSON_GetObjectItem(obj, "properties");
                if (props && cJSON_IsArray(props)) {
                    cJSON *prop = NULL;
                    cJSON_ArrayForEach(prop, props) {
                        const char *pname = cJSON_GetObjectItem(prop, "name")->valuestring;
                        cJSON *pval       = cJSON_GetObjectItem(prop, "value");
                        if (pval && cJSON_IsTrue(pval)) {
                            if (strncmp(pname, "npc-", 4) == 0) {
                                strncpy(npc->type, pname, sizeof(npc->type) - 1);
                            }
                        }
                    }
                }
                // NPC without type defaults to solid collision object
                if (npc->type[0] == '\0') {
                    if (map.objectCount < MAX_MAP_OBJECTS) {
                        MapObject *mo = &map.objects[map.objectCount];
                        strncpy(mo->type, "solid", sizeof(mo->type) - 1);
                        mo->rect = npc->rect;
                        map.objectCount++;
                    }
                }
                map.npcCount++;
            }
        }
        // Other object groups
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "player") != 0 && strcmp(name, "chuansong") != 0 && strcmp(name, "npc") != 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                if (map.objectCount >= MAX_MAP_OBJECTS) break;
                MapObject *mo = &map.objects[map.objectCount];

                if (cJSON_GetObjectItem(obj, "gid")) continue;

                mo->rect.x      = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                mo->rect.y      = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                mo->rect.width  = (float)cJSON_GetObjectItem(obj, "width")->valuedouble;
                mo->rect.height = (float)cJSON_GetObjectItem(obj, "height")->valuedouble;

                // Extract object type from properties
                cJSON *props = cJSON_GetObjectItem(obj, "properties");
                if (props && cJSON_IsArray(props)) {
                    cJSON *prop = NULL;
                    cJSON_ArrayForEach(prop, props) {
                        const char *pname  = cJSON_GetObjectItem(prop, "name")->valuestring;
                        cJSON *pval        = cJSON_GetObjectItem(prop, "value");
                        if (pval && cJSON_IsTrue(pval)) {
                            bool isCollision = (strcmp(pname, "solid") == 0 ||
                                               strcmp(pname, "door") == 0 ||
                                               strcmp(pname, "stairs") == 0 ||
                                               strcmp(pname, "road") == 0 ||
                                               strcmp(pname, "water") == 0 ||
                                               strncmp(pname, "sign", 4) == 0);
                            if (isCollision) {
                                strncpy(mo->type, pname, sizeof(mo->type) - 1);
                            } else {
                                // Non-collision property → treat as teleport
                                strncpy(mo->type, "chuansong", sizeof(mo->type) - 1);
                                strncpy(mo->name, pname, sizeof(mo->name) - 1);
                                if (mo->rect.width <= 0) mo->rect.width = 48;
                                if (mo->rect.height <= 0) mo->rect.height = 48;
                                mo->rect.x -= mo->rect.width / 2;
                                mo->rect.y -= mo->rect.height / 2;
                                if (map.teleportSpawnCount < MAX_TELEPORT_SPAWNS) {
                                    TeleportSpawn *ts = &map.teleportSpawns[map.teleportSpawnCount];
                                    strncpy(ts->name, pname, sizeof(ts->name) - 1);
                                    ts->pos.x = mo->rect.x + mo->rect.width / 2;
                                    ts->pos.y = mo->rect.y + mo->rect.height / 2;
                                    map.teleportSpawnCount++;
                                }
                            }
                        }
                    }
                }
                // Default to solid
                if (mo->type[0] == '\0') {
                    strncpy(mo->type, "solid", sizeof(mo->type) - 1);
                }
                map.objectCount++;
            }
        }
    }

    // Load tileset textures
    {
        cJSON *tilesetsArr = cJSON_GetObjectItem(root, "tilesets");
        cJSON *ts = NULL;
        map.tilesetCount = 0;

        cJSON_ArrayForEach(ts, tilesetsArr)
        {
            if (map.tilesetCount >= MAX_TILESETS) break;

            int firstGid = cJSON_GetObjectItem(ts, "firstgid")->valueint;
            const char *source = cJSON_GetObjectItem(ts, "source")->valuestring;

            char tsFilePath[1024];
            snprintf(tsFilePath, sizeof(tsFilePath), "%s/%s", g_mapDir, source);

            char imgRelPath[512] = "";
            int cols = 0, tileW = 16, tileH = 16;
            if (!parseTilesetFile(tsFilePath, imgRelPath, sizeof(imgRelPath),
                                  &cols, &tileW, &tileH))
            {
                // Fallback: hardcoded mappings for legacy assets
                if (firstGid == 1) {
                    strncpy(imgRelPath, "QQ_1779796916957.png", sizeof(imgRelPath));
                    cols = 35; tileW = 16; tileH = 16;
                } else if (firstGid == 946) {
                    strncpy(imgRelPath, "6abad8d14de667fabaecfea4a7242f82.png", sizeof(imgRelPath));
                    cols = 35; tileW = 16; tileH = 16;
                } else if (firstGid == 991) {
                    strncpy(imgRelPath, "lance_副本.png", sizeof(imgRelPath));
                    cols = 3; tileW = 16; tileH = 16;
                } else if (firstGid == 1786) {
                    strncpy(imgRelPath, "player_sheet.png", sizeof(imgRelPath));
                    cols = 14; tileW = 16; tileH = 20;
                } else if (firstGid == 2561) {
                    strncpy(imgRelPath, "sucai/dixing.png", sizeof(imgRelPath));
                    cols = 23; tileW = 16; tileH = 16;
                } else if (firstGid == 3366) {
                    strncpy(imgRelPath, "sucai/tall_grass.png", sizeof(imgRelPath));
                    cols = 1; tileW = 16; tileH = 16;
                } else if (firstGid == 3371) {
                    strncpy(imgRelPath, "sucai/green_surf_run_副本.png", sizeof(imgRelPath));
                    cols = 14; tileW = 16; tileH = 20;
                } else {
                    continue;
                }
            }

            char fullPath[1024];
            resolveFullPath(fullPath, sizeof(fullPath), imgRelPath);

            Image img = LoadImage(fullPath);
            if (!img.data) continue;

            TilesetInfo *info = &map.tilesets[map.tilesetCount];
            info->texture  = LoadTextureFromImage(img);
            info->firstGid = firstGid;
            info->cols     = cols;
            info->tileW    = tileW;
            info->tileH    = tileH;
            map.tilesetCount++;
            UnloadImage(img);
        }

        // Identify player sprite sheet: tileH != tileW (character sprites are e.g. 16x20)
        for (int i = 0; i < map.tilesetCount; i++)
        {
            if (map.tilesets[i].tileH != map.tilesets[i].tileW)
            {
                map.playerSheet = map.tilesets[i].texture;
                map.psCols      = map.tilesets[i].cols;
                map.psFirstGid  = map.tilesets[i].firstGid;
                map.psTileW     = map.tilesets[i].tileW;
                map.psTileH     = map.tilesets[i].tileH;
                break;
            }
        }
    }

    // Auto-detect solid tile GIDs from floorData
    {
        int gidFreq[4096] = {0};
        for (int i = 0; i < map.dataSize; i++) {
            int gid = map.floorData[i];
            if (gid > 0 && gid < 4096) gidFreq[gid]++;
        }
        map.solidGidCount = 0;
        for (int gid = 1; gid < 4096 && map.solidGidCount < MAX_SOLID_TILES; gid++) {
            if (gidFreq[gid] == 0) continue;
            // Check edge density
            int edgeCount = 0, totalForGid = gidFreq[gid];
            for (int x = 0; x < map.width; x++) {
                if (map.floorData[0 * map.width + x] == gid) edgeCount++;
                if (map.floorData[(map.height-1) * map.width + x] == gid) edgeCount++;
            }
            for (int y = 0; y < map.height; y++) {
                if (map.floorData[y * map.width + 0] == gid) edgeCount++;
                if (map.floorData[y * map.width + (map.width-1)] == gid) edgeCount++;
            }
            // >15% edge density → wall tile
            if (totalForGid > 10 && edgeCount > totalForGid * 0.15f) {
                map.solidGids[map.solidGidCount++] = gid;
            }
            // Long horizontal runs (≥5) → wall tile
            for (int y = 0; y < map.height; y++) {
                int run = 0;
                for (int x = 0; x < map.width; x++) {
                    if (map.floorData[y * map.width + x] == gid) {
                        run++;
                        if (run >= 5) {
                            bool already = false;
                            for (int s = 0; s < map.solidGidCount; s++) {
                                if (map.solidGids[s] == gid) { already = true; break; }
                            }
                            if (!already && map.solidGidCount < MAX_SOLID_TILES) {
                                map.solidGids[map.solidGidCount++] = gid;
                            }
                            break;
                        }
                    } else { run = 0; }
                }
                if (run >= 5) { y = map.height; break; }
            }
        }
    }

    // Default spawn: map center
    if (map.playerSpawn.x == 0 && map.playerSpawn.y == 0) {
        map.playerSpawn.x = (float)(map.width * map.tileWidth) / 2;
        map.playerSpawn.y = (float)(map.height * map.tileHeight) / 2;
    }

    // Player animation frame tables (tile IDs in sprite sheet)
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
    for (int i = 0; i < map->tilesetCount; i++) {
        if (map->tilesets[i].texture.id > 0)
            UnloadTexture(map->tilesets[i].texture);
    }
    if (map->hasBackImage) UnloadTexture(map->backImage);
    memset(map, 0, sizeof(*map));
}

// Calculate source rectangle within a tileset for a given GID
static Rectangle tileSourceRect(int gid, int firstGid, int cols, int tileW, int tileH)
{
    int localId = gid - firstGid;
    int col = localId % cols;
    int row = localId / cols;
    return (Rectangle){ (float)(col * tileW), (float)(row * tileH), (float)tileW, (float)tileH };
}

void DrawNpcs(Map *map)
{
    for (int i = 0; i < map->npcCount; i++) {
        MapNpc *npc = &map->npcs[i];
        int gid = npc->gid;
        if (gid == 0) continue;

        // Find the tileset containing this GID
        TilesetInfo *ts = NULL;
        for (int j = 0; j < map->tilesetCount; j++) {
            if (gid >= map->tilesets[j].firstGid) {
                if (!ts || map->tilesets[j].firstGid > ts->firstGid)
                    ts = &map->tilesets[j];
            }
        }

        if (ts && ts->texture.id > 0) {
            Rectangle src = tileSourceRect(gid, ts->firstGid, ts->cols,
                                           ts->tileW, ts->tileH);
            Rectangle dst = npc->rect;
            DrawTexturePro(ts->texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        }
    }
}

int GetNpcRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < map->npcCount && count < maxCount; i++) {
        // Expand interaction range
        Rectangle r = map->npcs[i].rect;
        r.x -= 16; r.y -= 16;
        r.width += 32; r.height += 32;
        out[count++] = r;
    }
    return count;
}

bool GetNpcInfo(Map *map, Rectangle rect, char *type, int size)
{
    for (int i = 0; i < map->npcCount; i++) {
        Rectangle nr = map->npcs[i].rect;
        // Expand detection area
        nr.x -= 24; nr.y -= 24;
        nr.width += 48; nr.height += 48;
        if (rect.x < nr.x + nr.width &&
            rect.x + rect.width > nr.x &&
            rect.y < nr.y + nr.height &&
            rect.y + rect.height > nr.y)
        {
            strncpy(type, map->npcs[i].type, size - 1);
            type[size - 1] = '\0';
            return true;
        }
    }
    return false;
}

bool IsGidSolid(Map *map, int gid)
{
    for (int i = 0; i < map->solidGidCount; i++) {
        if (map->solidGids[i] == gid) return true;
    }
    return false;
}

void DrawMap(Map *map)
{
    int mapPixelW = map->width  * map->tileWidth;
    int mapPixelH = map->height * map->tileHeight;

    if (map->hasBackImage) {
        DrawTexture(map->backImage, 0, 0,
                    Fade(WHITE, map->backOpacity));
    } else {
        DrawRectangle(0, 0, mapPixelW, mapPixelH, BLACK);
    }

    if (map->floorData) {
        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                int gid = map->floorData[y * map->width + x];
                if (gid == 0) continue;

                // Find tileset matching this GID
                TilesetInfo *ts = NULL;
                for (int i = 0; i < map->tilesetCount; i++) {
                    if (gid >= map->tilesets[i].firstGid) {
                        if (!ts || map->tilesets[i].firstGid > ts->firstGid)
                            ts = &map->tilesets[i];
                    }
                }

                if (ts && ts->texture.id > 0) {
                    Rectangle src = tileSourceRect(gid, ts->firstGid, ts->cols,
                                                   ts->tileW, ts->tileH);
                    Rectangle dst = {
                        (float)(x * map->tileWidth),
                        (float)(y * map->tileHeight),
                        (float)map->tileWidth,
                        (float)map->tileHeight
                    };
                    DrawTexturePro(ts->texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
                }
            }
        }
    }
}

int GetSolidRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;

    // Collect solid objects
    for (int i = 0; i < map->objectCount && count < maxCount; i++) {
        if (strncmp(map->objects[i].type, "solid", 5) == 0) {
            out[count++] = map->objects[i].rect;
        }
    }

    // Collect solid tile rectangles from floorData (merge adjacent)
    if (map->solidGidCount > 0 && count < maxCount) {
        bool *visited = (bool*)calloc(map->dataSize, sizeof(bool));
        if (visited) {
            for (int y = 0; y < map->height && count < maxCount; y++) {
                for (int x = 0; x < map->width && count < maxCount; x++) {
                    int idx = y * map->width + x;
                    if (visited[idx]) continue;
                    int gid = map->floorData[idx];
                    if (!IsGidSolid(map, gid)) continue;

                    // Expand horizontally
                    int endX = x;
                    while (endX + 1 < map->width) {
                        int nidx = y * map->width + (endX + 1);
                        int ngid = map->floorData[nidx];
                        if (IsGidSolid(map, ngid) && ngid == gid && !visited[nidx])
                            endX++;
                        else break;
                    }
                    // Expand vertically when full rows align
                    int endY = y;
                    for (int vy = y + 1; vy < map->height; vy++) {
                        bool fullRow = true;
                        for (int vx = x; vx <= endX; vx++) {
                            int vidx = vy * map->width + vx;
                            int vgid = map->floorData[vidx];
                            if (!IsGidSolid(map, vgid) || vgid != gid || visited[vidx]) {
                                fullRow = false;
                                break;
                            }
                        }
                        if (fullRow) endY = vy;
                        else break;
                    }

                    // Mark visited
                    for (int vy = y; vy <= endY; vy++) {
                        for (int vx = x; vx <= endX; vx++) {
                            visited[vy * map->width + vx] = true;
                        }
                    }

                    out[count++] = (Rectangle){
                        (float)(x * map->tileWidth),
                        (float)(y * map->tileHeight),
                        (float)((endX - x + 1) * map->tileWidth),
                        (float)((endY - y + 1) * map->tileHeight)
                    };
                }
            }
            free(visited);
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

int GetDoorRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < map->objectCount && count < maxCount; i++) {
        if (strcmp(map->objects[i].type, "door") == 0) {
            out[count++] = map->objects[i].rect;
        }
    }
    return count;
}

int GetStairFirstRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < map->objectCount && count < maxCount; i++) {
        if (strcmp(map->objects[i].type, "stair-first") == 0) {
            out[count++] = map->objects[i].rect;
        }
    }
    return count;
}

int GetChuansongRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < map->objectCount && count < maxCount; i++) {
        if (strcmp(map->objects[i].type, "chuansong") == 0) {
            out[count++] = map->objects[i].rect;
        }
    }
    return count;
}

int GetSignRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < map->objectCount && count < maxCount; i++) {
        if (strncmp(map->objects[i].type, "sign", 4) == 0) {
            out[count++] = map->objects[i].rect;
        }
    }
    return count;
}

bool GetSignName(Map *map, Rectangle rect, char *name, int size)
{
    for (int i = 0; i < map->objectCount; i++) {
        if (strncmp(map->objects[i].type, "sign", 4) == 0) {
            Rectangle sr = map->objects[i].rect;
            sr.x -= 8; sr.y -= 8;
            sr.width += 16; sr.height += 16;
            if (rect.x < sr.x + sr.width &&
                rect.x + rect.width > sr.x &&
                rect.y < sr.y + sr.height &&
                rect.y + rect.height > sr.y)
            {
                strncpy(name, map->objects[i].type, size - 1);
                name[size - 1] = '\0';
                return true;
            }
        }
    }
    return false;
}

bool GetChuansongName(Map *map, Rectangle rect, char *name, int size)
{
    for (int i = 0; i < map->objectCount; i++) {
        if (strcmp(map->objects[i].type, "chuansong") == 0) {
            Rectangle sr = map->objects[i].rect;
            if (rect.x < sr.x + sr.width &&
                rect.x + rect.width > sr.x &&
                rect.y < sr.y + sr.height &&
                rect.y + rect.height > sr.y)
            {
                strncpy(name, map->objects[i].name, size - 1);
                name[size - 1] = '\0';
                return true;
            }
        }
    }
    return false;
}

bool FindTeleportSpawn(Map *map, const char *name, Vector2 *pos)
{
    for (int i = 0; i < map->teleportSpawnCount; i++) {
        if (strcmp(map->teleportSpawns[i].name, name) == 0) {
            *pos = map->teleportSpawns[i].pos;
            return true;
        }
    }
    return false;
}
