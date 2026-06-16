/**
 * map.c —— 地图加载、渲染与碰撞查询
 *
 * 该模块负责从 TMJ（Tiled Map JSON）格式文件解析地图数据，
 * 加载 tileset 贴图、背景图、玩家精灵表，
 * 提供地图绘制和碰撞区域查询功能。
 */

#include "map.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 内部辅助函数 ========== */

/**
 * filenameFromPath - 从文件路径中提取文件名
 * @path: 完整路径，例如 "../素材/foo.png"
 *
 * 返回指向最后一个 '/' 或 '\\' 之后字符的指针。
 * 如果路径中没有分隔符，则返回路径本身。
 */
static const char *filenameFromPath(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') last = p + 1;
    }
    return last;
}

/* 全局：TMJ 文件所在目录，用于解析图片相对路径 */
static char g_mapDir[512] = "";

/**
 * resolveFullPath - 拼接地图目录与 tileset 中引用的图片路径
 * @dst:          输出缓冲区
 * @dstSize:      缓冲区大小
 * @relativePath: tileset 中记录的图片路径（可能包含子目录）
 *
 * 例如 g_mapDir = "assets/maps"，relativePath = "sucai/foo.png"，
 * 结果为 "assets/maps/sucai/foo.png"。
 * 若路径以 "../" 开头，只取文件名部分（向上引用视为直接放在地图目录下）。
 */
static void resolveFullPath(char *dst, size_t dstSize, const char *relativePath)
{
    if (strncmp(relativePath, "../", 3) == 0) {
        const char *fname = filenameFromPath(relativePath);
        snprintf(dst, dstSize, "%s/%s", g_mapDir, fname);
    } else {
        snprintf(dst, dstSize, "%s/%s", g_mapDir, relativePath);
    }
}

/* ========== 辅助：解析 tileset 文件（.tsx XML 或 .tsj JSON） ========== */

/**
 * parseTilesetFile - 解析 tileset 文件，提取 image 路径与元数据
 * @filepath:  tileset 文件路径（.tsx 或 .tsj）
 * @imagePath: 输出——image 相对路径
 * @pathSize:  imagePath 缓冲区大小
 * @cols:      输出——列数
 * @tileW:     输出——瓦片宽度
 * @tileH:     输出——瓦片高度
 *
 * 自动识别 XML 或 JSON 格式。
 */
static bool parseTilesetFile(const char *filepath, char *imagePath, size_t pathSize,
                              int *cols, int *tileW, int *tileH)
{
    char *content = LoadFileText(filepath);
    if (!content) return false;

    bool ok = false;

    /* JSON 格式（.tsj） */
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
    /* XML 格式（.tsx） */
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

/* ======================== 地图加载 ======================== */

/**
 * LoadMap - 从 TMJ 文件加载地图
 * @filepath: TMJ 文件的路径
 *
 * 解析流程：
 *   1. 记录地图目录，用于后续图片路径拼合
 *   2. 读取并解析 JSON 文件
 *   3. 读取地图尺寸（宽/高/瓦片宽高）
 *   4. 遍历所有图层：
 *      - imagelayer "back"：加载背景图片并缩放到地图大小
 *      - tilelayer "floor"：读取地板瓦片 GID 数组
 *      - objectgroup "item"：解析地图对象（solid/stairs 等）
 *      - objectgroup "player"：读取玩家出生点
 *   5. 加载三张 tileset 贴图：
 *      QQ_1779796916957.png (tileset1)
 *      6abad8d14de667fabaecfea4a7242f82.png (tileset2)
 *      player_sheet.png (玩家精灵表)
 *   6. 若未找到玩家出生点，自动设置在地图中央
 *   7. 设置动画帧表（前走慢/前走快/后退/左移）
 *
 * 返回包含所有数据的 Map 结构体。
 */
Map LoadMap(const char *filepath)
{
    Map map;
    memset(&map, 0, sizeof(map));

    /* --- 记录地图目录 --- */
    strncpy(g_mapDir, filepath, sizeof(g_mapDir) - 1);
    char *lastSep = strrchr(g_mapDir, '/');
    char *lastSep2 = strrchr(g_mapDir, '\\');
    if (lastSep2 > lastSep) lastSep = lastSep2;
    if (lastSep) *lastSep = '\0';

    /* --- 读取 TMJ 文件内容 --- */
    char *jsonStr = LoadFileText(filepath);
    if (!jsonStr) {
        TraceLog(LOG_ERROR, "Failed to load TMJ: %s", filepath);
        return map;
    }

    /* --- 解析 JSON --- */
    cJSON *root = cJSON_Parse(jsonStr);
    if (!root) {
        TraceLog(LOG_ERROR, "Failed to parse TMJ JSON: %s",
                 cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown error");
        UnloadFileText(jsonStr);
        return map;
    }

    /* --- 地图基本尺寸 --- */
    map.width       = cJSON_GetObjectItem(root, "width")->valueint;
    map.height      = cJSON_GetObjectItem(root, "height")->valueint;
    map.tileWidth   = cJSON_GetObjectItem(root, "tilewidth")->valueint;
    map.tileHeight  = cJSON_GetObjectItem(root, "tileheight")->valueint;
    map.playerScale = 1.0f;   /* 默认正常大小 */

    /* --- 准备全地图瓦片数组 --- */
    map.dataSize = map.width * map.height;
    map.floorData = calloc(map.dataSize, sizeof(int));

    /* --- 遍历图层，解析各层数据 --- */
    cJSON *layers = cJSON_GetObjectItem(root, "layers");
    cJSON *layer  = NULL;
    cJSON_ArrayForEach(layer, layers) {
        const char *type = cJSON_GetObjectItem(layer, "type")->valuestring;
        const char *name = cJSON_GetObjectItem(layer, "name")->valuestring;

        /* 背景图层（imagelayer "back"） */
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
        /* 瓦片图层：合并所有 tilelayer 到 floorData（后层覆盖前层） */
        else if (strcmp(type, "tilelayer") == 0) {
            /* 无限地图格式：从 chunks 组装数据 */
            cJSON *chunksArr = cJSON_GetObjectItem(layer, "chunks");
            if (chunksArr && cJSON_IsArray(chunksArr)) {
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
                /* 有限地图格式：从 data 数组读取 */
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
        /* 玩家出生点层（objectgroup "player"） */
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "player") == 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                /* 跳过瓦片对象（有 gid 的占位标记），取第一个普通对象作出生点 */
                if (cJSON_GetObjectItem(obj, "gid")) continue;
                map.playerSpawn.x = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                map.playerSpawn.y = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                break;
            }
        }
        /* 传送点层（objectgroup "chuansong"） —— 解析命名传送点，同时作为触发区域 */
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "chuansong") == 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                float ox = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                float oy = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;

                /* 登记命名传送点 */
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
                            /* 同时作为触发区域添加到对象列表（48x48 触发区） */
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
        /* NPC 专用对象层（objectgroup "npc"）—— 解析NPC精灵对象 */
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "npc") == 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                if (map.npcCount >= MAX_NPC_OBJECTS) break;
                MapNpc *npc = &map.npcs[map.npcCount];

                cJSON *gidNode = cJSON_GetObjectItem(obj, "gid");
                if (!gidNode) continue;  /* 无GID的不是精灵对象，跳过 */
                npc->gid = gidNode->valueint;

                npc->rect.x      = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                npc->rect.y      = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                npc->rect.width  = (float)cJSON_GetObjectItem(obj, "width")->valuedouble;
                npc->rect.height = (float)cJSON_GetObjectItem(obj, "height")->valuedouble;

                /* 从 properties 提取NPC类型 */
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
                /* 无属性的NPC对象默认设为 solid 交互对象，加入 objects 列表 */
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
        /* 对象层（解析所有 objectgroup 层中的对象，player/chuansong/npc 层除外） */
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "player") != 0 && strcmp(name, "chuansong") != 0 && strcmp(name, "npc") != 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                if (map.objectCount >= MAX_MAP_OBJECTS) break;
                MapObject *mo = &map.objects[map.objectCount];

                /* 跳过带有 gid 的瓦片对象，不当作碰撞对象 */
                if (cJSON_GetObjectItem(obj, "gid")) continue;

                mo->rect.x      = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                mo->rect.y      = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                mo->rect.width  = (float)cJSON_GetObjectItem(obj, "width")->valuedouble;
                mo->rect.height = (float)cJSON_GetObjectItem(obj, "height")->valuedouble;

                /* 从 properties 中提取对象类型（如 solid/stairs/door/传送点名） */
                cJSON *props = cJSON_GetObjectItem(obj, "properties");
                if (props && cJSON_IsArray(props)) {
                    cJSON *prop = NULL;
                    cJSON_ArrayForEach(prop, props) {
                        const char *pname  = cJSON_GetObjectItem(prop, "name")->valuestring;
                        cJSON *pval        = cJSON_GetObjectItem(prop, "value");
                        if (pval && cJSON_IsTrue(pval)) {
                            /* 判断是否为已知碰撞/交互类型 */
                            bool isCollision = (strcmp(pname, "solid") == 0 ||
                                               strcmp(pname, "door") == 0 ||
                                               strcmp(pname, "stairs") == 0 ||
                                               strcmp(pname, "road") == 0 ||
                                               strcmp(pname, "water") == 0 ||
                                               strncmp(pname, "sign", 4) == 0);
                            if (isCollision) {
                                strncpy(mo->type, pname, sizeof(mo->type) - 1);
                            } else {
                                /* 非碰撞属性 → 作为 chuansong 传送点处理 */
                                strncpy(mo->type, "chuansong", sizeof(mo->type) - 1);
                                strncpy(mo->name, pname, sizeof(mo->name) - 1);
                                /* 点对象（width/height 为 0）自动扩展为 48x48 触发区 */
                                if (mo->rect.width <= 0) mo->rect.width = 48;
                                if (mo->rect.height <= 0) mo->rect.height = 48;
                                mo->rect.x -= mo->rect.width / 2;
                                mo->rect.y -= mo->rect.height / 2;
                                /* 同时注册 TeleportSpawn 供 player.c 按名匹配 */
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
                /* 没有明确类型的对象默认视为 solid 障碍物 */
                if (mo->type[0] == '\0') {
                    strncpy(mo->type, "solid", sizeof(mo->type) - 1);
                }
                map.objectCount++;
            }
        }
    }

    /* --- 动态加载 tileset 贴图 --- */
    {
        cJSON *tilesetsArr = cJSON_GetObjectItem(root, "tilesets");
        cJSON *ts = NULL;
        map.tilesetCount = 0;

        cJSON_ArrayForEach(ts, tilesetsArr)
        {
            if (map.tilesetCount >= MAX_TILESETS) break;

            int firstGid = cJSON_GetObjectItem(ts, "firstgid")->valueint;
            const char *source = cJSON_GetObjectItem(ts, "source")->valuestring;

            /* 构建 tileset 文件的完整路径 */
            char tsFilePath[1024];
            snprintf(tsFilePath, sizeof(tsFilePath), "%s/%s", g_mapDir, source);

            /* 解析 tileset 文件获取 image 路径与元数据 */
            char imgRelPath[512] = "";
            int cols = 0, tileW = 16, tileH = 16;
            if (!parseTilesetFile(tsFilePath, imgRelPath, sizeof(imgRelPath),
                                  &cols, &tileW, &tileH))
            {
                /* 回退：.tsx/.tsj 文件不存在时使用硬编码映射（兼容旧版 assets 结构） */
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

            /* 解析 image 的全路径 */
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

        /* 玩家精灵表：从 tileset 中查找独立的 player sheet
           特征：tileH != tileW（人物精灵通常是 16x20 而非 16x16） */
        for (int i = 0; i < map.tilesetCount; i++)
        {
            if (map.tilesets[i].tileH != map.tilesets[i].tileW)
            {
                map.playerSheet = map.tilesets[i].texture;
                map.psCols      = map.tilesets[i].cols;
                map.psFirstGid  = map.tilesets[i].firstGid;
                map.psTileW     = map.tilesets[i].tileW;
                map.psTileH     = map.tilesets[i].tileH;
                /* 保留在 tilesets 数组中（纹理由 UnloadMap 统一管理），
                   DrawMap 不会使用它的 GID 范围绘制地板 */
                break;
            }
        }
    }

    /* --- 自动检测 solid 瓦片GID --- */
    /* 在 floorData 中出现频次最高的外层GID通常为墙壁/不可通行瓦片 */
    /* 方法：统计GID出现次数，将地图边缘高频GID标记为solid */
    {
        int gidFreq[4096] = {0};  /* 简单直方图，GID不超过4096 */
        for (int i = 0; i < map.dataSize; i++) {
            int gid = map.floorData[i];
            if (gid > 0 && gid < 4096) gidFreq[gid]++;
        }
        /* 检测地图四边高频GID用作solid */
        map.solidGidCount = 0;
        for (int gid = 1; gid < 4096 && map.solidGidCount < MAX_SOLID_TILES; gid++) {
            if (gidFreq[gid] == 0) continue;
            /* 统计该GID在边缘的密度 */
            int edgeCount = 0, totalForGid = gidFreq[gid];
            for (int x = 0; x < map.width; x++) {
                if (map.floorData[0 * map.width + x] == gid) edgeCount++;
                if (map.floorData[(map.height-1) * map.width + x] == gid) edgeCount++;
            }
            for (int y = 0; y < map.height; y++) {
                if (map.floorData[y * map.width + 0] == gid) edgeCount++;
                if (map.floorData[y * map.width + (map.width-1)] == gid) edgeCount++;
            }
            /* 如果该GID在边缘出现超过总出现次数的15%，认为是墙壁 */
            if (totalForGid > 10 && edgeCount > totalForGid * 0.15f) {
                map.solidGids[map.solidGidCount++] = gid;
            }
            /* 同时：如果该GID形成长连续水平/垂直条（>5个连续），也标记为solid */
            for (int y = 0; y < map.height; y++) {
                int run = 0;
                for (int x = 0; x < map.width; x++) {
                    if (map.floorData[y * map.width + x] == gid) {
                        run++;
                        if (run >= 5) {
                            /* 找到长条状GID，添加为solid */
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
                if (run >= 5) { y = map.height; break; } /* 已找到，跳出 */
            }
        }
    }

    /* --- 默认玩家出生点（若未找到玩家对象） --- */
    if (map.playerSpawn.x == 0 && map.playerSpawn.y == 0) {
        map.playerSpawn.x = (float)(map.width * map.tileWidth) / 2;
        map.playerSpawn.y = (float)(map.height * map.tileHeight) / 2;
    }

	/* --- 玩家动画帧表（tile ID 对应 sprite sheet 中的位置） --- */
    /* 默认值：标准宝可梦精灵表布局 (14列, 每个方向3帧连续排列) */
    map.animFrontLow[0]  = 0;  map.animFrontLow[1]  = 12; map.animFrontLow[2]  = 13;
    map.animFrontFast[0] = 3;  map.animFrontFast[1] = 4;  map.animFrontFast[2] = 5;
    map.animBack[0]      = 6;  map.animBack[1]      = 7;  map.animBack[2]      = 8;
    map.animLeft[0]      = 9;  map.animLeft[1]      = 10; map.animLeft[2]      = 11;

    cJSON_Delete(root);
    UnloadFileText(jsonStr);
    return map;
}

/**
 * UnloadMap - 释放地图占用的资源
 * @map: 地图指针
 *
 * 释放 floorData 堆内存，卸载所有纹理贴图，最后清零结构体。
 */
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

/* ======================== NPC 绘制 ======================== */

/**
 * DrawNpcs - 绘制地图上的所有NPC精灵对象
 * @map: 地图指针
 *
 * 遍历NPC列表，根据GID从tileset中找到对应贴图并绘制。
 */
static Rectangle tileSourceRect(int gid, int firstGid, int cols, int tileW, int tileH);

void DrawNpcs(Map *map)
{
    for (int i = 0; i < map->npcCount; i++) {
        MapNpc *npc = &map->npcs[i];
        int gid = npc->gid;
        if (gid == 0) continue;

        /* 查找GID所属的tileset */
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

/* ======================== NPC 交互查询 ======================== */

/**
 * GetNpcRects - 获取所有NPC的交互矩形
 */
int GetNpcRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < map->npcCount && count < maxCount; i++) {
        /* 扩大交互检测范围 */
        Rectangle r = map->npcs[i].rect;
        r.x -= 16; r.y -= 16;
        r.width += 32; r.height += 32;
        out[count++] = r;
    }
    return count;
}

/**
 * GetNpcInfo - 查找与给定矩形重叠的NPC信息
 */
bool GetNpcInfo(Map *map, Rectangle rect, char *type, int size)
{
    for (int i = 0; i < map->npcCount; i++) {
        Rectangle nr = map->npcs[i].rect;
        /* 扩大检测区域 */
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

/* ======================== Solid GID 查询 ======================== */

/**
 * IsGidSolid - 检查给定的GID是否为solid（不可通行）瓦片
 */
bool IsGidSolid(Map *map, int gid)
{
    for (int i = 0; i < map->solidGidCount; i++) {
        if (map->solidGids[i] == gid) return true;
    }
    return false;
}

/* ======================== 绘制 ======================== */

/**
 * tileSourceRect - 根据全局 GID 计算 tileset 中的源矩形
 * @gid:       全局瓦片 ID
 * @firstGid:  当前 tileset 的起始 GID
 * @cols:      tileset 列数
 * @tileW:     瓦片宽度
 * @tileH:     瓦片高度
 *
 * 先将全局 GID 转换为 tileset 本地索引，
 * 再按行列计算出对应的 UV 矩形区域。
 */
static Rectangle tileSourceRect(int gid, int firstGid, int cols, int tileW, int tileH)
{
    int localId = gid - firstGid;
    int col = localId % cols;
    int row = localId / cols;
    return (Rectangle){ (float)(col * tileW), (float)(row * tileH), (float)tileW, (float)tileH };
}

/**
 * DrawMap - 绘制整个地图
 * @map: 地图指针
 *
 * 绘制顺序：
 *   1. 背景图（若有，按透明度绘制；否则填充黑色）
 *   2. 地板瓦片（遍历每个格子，从对应 tileset 截取贴图绘制）
 */
void DrawMap(Map *map)
{
    int mapPixelW = map->width  * map->tileWidth;
    int mapPixelH = map->height * map->tileHeight;

    /* --- 绘制背景 --- */
    if (map->hasBackImage) {
        DrawTexture(map->backImage, 0, 0,
                    Fade(WHITE, map->backOpacity));
    } else {
        DrawRectangle(0, 0, mapPixelW, mapPixelH, BLACK);
    }

    /* --- 逐瓦片绘制地板层 --- */
    if (map->floorData) {
        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                int gid = map->floorData[y * map->width + x];
                if (gid == 0) continue;   /* 空白瓦片跳过 */

                /* 在 tileset 数组中查找 GID 所属的 tileset */
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

/* ======================== 碰撞区域查询 ======================== */

/**
 * GetSolidRects - 获取地图中所有 "solid" 类型的碰撞矩形
 * @map:      地图指针
 * @out:      输出缓冲区（存放碰撞矩形数组）
 * @maxCount: 缓冲区最大容量
 *
 * 遍历地图对象，筛选出 type == "solid" 的对象，
 * 将其矩形区域填入 out 数组。
 * 同时从 floorData 中检测 solidGids 对应的瓦片矩形。
 *
 * 返回实际找到的 solid 矩形数量。
 */
int GetSolidRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;

    /* 1. 从 MapObject 中收集 solid 矩形 */
    for (int i = 0; i < map->objectCount && count < maxCount; i++) {
        if (strncmp(map->objects[i].type, "solid", 5) == 0) {
            out[count++] = map->objects[i].rect;
        }
    }

    /* 2. 从 floorData 中收集 solidGid 对应的瓦片矩形（合并相邻） */
    if (map->solidGidCount > 0 && count < maxCount) {
        bool *visited = (bool*)calloc(map->dataSize, sizeof(bool));
        if (visited) {
            for (int y = 0; y < map->height && count < maxCount; y++) {
                for (int x = 0; x < map->width && count < maxCount; x++) {
                    int idx = y * map->width + x;
                    if (visited[idx]) continue;
                    int gid = map->floorData[idx];
                    if (!IsGidSolid(map, gid)) continue;

                    /* 扩展水平连续solid块 */
                    int endX = x;
                    while (endX + 1 < map->width) {
                        int nidx = y * map->width + (endX + 1);
                        int ngid = map->floorData[nidx];
                        if (IsGidSolid(map, ngid) && ngid == gid && !visited[nidx])
                            endX++;
                        else break;
                    }
                    /* 尝试扩展垂直方向（仅当整行对齐时） */
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

                    /* 标记已访问 */
                    for (int vy = y; vy <= endY; vy++) {
                        for (int vx = x; vx <= endX; vx++) {
                            visited[vy * map->width + vx] = true;
                        }
                    }

                    /* 添加合并后的矩形 */
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

/**
 * GetStairsRects - 获取地图中所有 "stairs" 类型的楼梯矩形
 * @map:      地图指针
 * @out:      输出缓冲区（存放楼梯矩形数组）
 * @maxCount: 缓冲区最大容量
 *
 * 遍历地图对象，筛选出 type == "stairs" 的对象，
 * 将其矩形区域填入 out 数组。
 *
 * 返回实际找到的楼梯矩形数量。
 */
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
            /* 扩大一点检测区域，方便玩家触发 */
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
