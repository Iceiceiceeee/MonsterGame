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
 * resolveFullPath - 拼接地图目录与相对路径，得到完整图片路径
 * @dst:          输出缓冲区
 * @dstSize:      缓冲区大小
 * @relativePath: 相对路径（仅提取文件名部分）
 *
 * 例如 g_mapDir = "assets/maps"，relativePath = "../素材/foo.png"，
 * 结果为 "assets/maps/foo.png"。
 */
static void resolveFullPath(char *dst, size_t dstSize, const char *relativePath)
{
    const char *fname = filenameFromPath(relativePath);
    snprintf(dst, dstSize, "%s/%s", g_mapDir, fname);
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
        /* 物品/碰撞对象层（objectgroup "item"） */
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

                /* 从 properties 中提取对象类型（如 solid/stairs） */
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
        /* 玩家出生点层（objectgroup "player"） */
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

    /* --- 加载 tileset 贴图 --- */
    {
        char path[1024];

        /* tileset1: QQ_1779796916957.png, 16x16 瓦片, 35 列 */
        resolveFullPath(path, sizeof(path), "QQ_1779796916957.png");
        Image img = LoadImage(path);
        if (img.data) {
            map.tileset1    = LoadTextureFromImage(img);
            map.ts1Cols     = img.width / 16;
            map.ts1FirstGid = 1;
            UnloadImage(img);
        }

        /* tileset2: 6abad8d14de667fabaecfea4a7242f82.png, 16x16 瓦片, 35 列 */
        resolveFullPath(path, sizeof(path), "6abad8d14de667fabaecfea4a7242f82.png");
        img = LoadImage(path);
        if (img.data) {
            map.tileset2    = LoadTextureFromImage(img);
            map.ts2Cols     = 35;
            map.ts2FirstGid = 946;
            UnloadImage(img);
        }

        /* 玩家精灵表: player_sheet.png, 16x20, 14 列 */
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
    if (map->tileset1.id > 0)    UnloadTexture(map->tileset1);
    if (map->tileset2.id > 0)    UnloadTexture(map->tileset2);
    if (map->playerSheet.id > 0) UnloadTexture(map->playerSheet);
    if (map->hasBackImage)       UnloadTexture(map->backImage);
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

                Rectangle src = {0};
                Texture2D tex = {0};

                /* 根据 GID 判断所属 tileset */
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
        if (strcmp(map->objects[i].type, "solid") == 0) {
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
