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
                /* 将背景图拉伸至全地图大小 */
                ImageResize(&img, map.width * map.tileWidth, map.height * map.tileHeight);
                map.backImage = LoadTextureFromImage(img);
                UnloadImage(img);
                map.hasBackImage = true;
            }
        }
        /* 地板图层（tilelayer "floor"） */
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
        /* 玩家出生点层（objectgroup "player"） */
        if (strcmp(type, "objectgroup") == 0 && strcmp(name, "player") == 0) {
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
        /* 对象层（解析所有 objectgroup 层中的对象，player 层除外） */
        else if (strcmp(type, "objectgroup") == 0 && strcmp(name, "player") != 0) {
            cJSON *objects = cJSON_GetObjectItem(layer, "objects");
            cJSON *obj = NULL;
            cJSON_ArrayForEach(obj, objects) {
                if (map.objectCount >= MAX_MAP_OBJECTS) break;
                MapObject *mo = &map.objects[map.objectCount];

                /* 跳过带有 gid 的瓦片对象（玩家精灵表占位），不当作碰撞对象 */
                if (cJSON_GetObjectItem(obj, "gid")) continue;

                mo->rect.x      = (float)cJSON_GetObjectItem(obj, "x")->valuedouble;
                mo->rect.y      = (float)cJSON_GetObjectItem(obj, "y")->valuedouble;
                mo->rect.width  = (float)cJSON_GetObjectItem(obj, "width")->valuedouble;
                mo->rect.height = (float)cJSON_GetObjectItem(obj, "height")->valuedouble;

                /* 从 properties 中提取对象类型（如 solid/stairs/door） */
                cJSON *props = cJSON_GetObjectItem(obj, "properties");
                if (props && cJSON_IsArray(props)) {
                    cJSON *prop = NULL;
                    cJSON_ArrayForEach(prop, props) {
                        const char *pname  = cJSON_GetObjectItem(prop, "name")->valuestring;
                        cJSON *pval        = cJSON_GetObjectItem(prop, "value");
                        if (pval && cJSON_IsTrue(pval)) {
                            /* 属性名为 true 即表示该对象的类型 */
                            strncpy(mo->type, pname, sizeof(mo->type) - 1);
                        }
                    }
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
                } else if (firstGid == 1786) {
                    strncpy(imgRelPath, "player_sheet.png", sizeof(imgRelPath));
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

    /* --- 默认玩家出生点（若未找到玩家对象） --- */
    if (map.playerSpawn.x == 0 && map.playerSpawn.y == 0) {
        map.playerSpawn.x = (float)(map.width * map.tileWidth) / 2;
        map.playerSpawn.y = (float)(map.height * map.tileHeight) / 2;
    }

    /* --- 玩家动画帧表（tile ID 对应 sprite sheet 中的位置） --- */
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
 *
 * 返回实际找到的 solid 矩形数量。
 */
int GetSolidRects(Map *map, Rectangle *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < map->objectCount && count < maxCount; i++) {
        if (strncmp(map->objects[i].type, "solid", 5) == 0) {
            out[count++] = map->objects[i].rect;
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
