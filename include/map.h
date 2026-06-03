/**
 * map.h —— 地图模块头文件
 *
 * 定义地图数据结构（Map）和地图对象结构（MapObject），
 * 声明地图加载、卸载、绘制以及碰撞区域查询的接口。
 */

#ifndef MAP_H
#define MAP_H

#include "raylib.h"

/* ========== 常量定义 ========== */

#define MAX_MAP_OBJECTS      128   /**< 地图中最多支持的对象数量 */
#define MAX_SOLID_RECTS       64   /**< 碰撞检测时 solid 矩形数组的最大容量 */
#define MAX_STAIRS_RECTS      16   /**< 楼梯检测时 stairs 矩形数组的最大容量 */
#define MAX_DOOR_RECTS        16   /**< 门检测时 door 矩形数组的最大容量 */
#define MAX_STAIRFIRST_RECTS  16   /**< stair-first 矩形数组的最大容量 */
#define MAX_TILESETS           8   /**< 地图最多支持的 tileset 数量 */

/* ========== 数据结构 ========== */

/**
 * MapObject - 地图中的一个交互/碰撞对象
 *
 * 由 Tiled 编辑器的 object group 层解析而来，
 * 通过 properties 中的布尔属性标明类型（如 solid、stairs、door 等）。
 * 对于 door 类型，额外记录目标地图及传送坐标。
 */
typedef struct {
    char name[64];        /**< 对象名称（Tiled 中定义） */
    char type[32];        /**< 对象类型（从 properties 中提取，如 "solid"、"stairs"、"door"） */
    Rectangle rect;       /**< 对象的位置和尺寸矩形 */
    /* ---- 门传送相关 ---- */
    char targetMap[64];   /**< 目标地图文件名 */
    float targetX;        /**< 目标出生点 X */
    float targetY;        /**< 目标出生点 Y */
} MapObject;

/**
 * TilesetInfo - 单个 tileset 的信息
 *
 * 每个 tileset 对应 Tiled 中的一张贴图，包含其起始 GID、
 * 列数、瓦片尺寸等元数据。
 */
typedef struct {
    Texture2D texture;   /**< tileset 贴图 */
    int firstGid;        /**< 起始全局 GID */
    int cols;            /**< 列数 */
    int tileW;           /**< 瓦片宽度 */
    int tileH;           /**< 瓦片高度 */
} TilesetInfo;

/**
 * Map - 完整的地图数据
 *
 * 包含地图尺寸、瓦片数据、多张 tileset 贴图、
 * 背景图层、玩家精灵表、对象列表、出生点及动画帧表。
 */
typedef struct {
    /* --- 地图基本尺寸 --- */
    int width;           /**< 地图宽（瓦片数） */
    int height;          /**< 地图高（瓦片数） */
    int tileWidth;       /**< 单个瓦片宽度（像素） */
    int tileHeight;      /**< 单个瓦片高度（像素） */

    /* --- 地板瓦片数据 --- */
    int *floorData;      /**< 地板层 GID 数组，长度 = width * height，0 表示空 */
    int dataSize;        /**< floorData 数组长度 */

    /* --- 动态 tileset 数组 --- */
    TilesetInfo tilesets[MAX_TILESETS]; /**< tileset 数组 */
    int tilesetCount;                    /**< 实际 tileset 数量 */

    /* --- 玩家精灵表 --- */
    Texture2D playerSheet; /**< 玩家精灵贴图 */
    int psCols;            /**< 精灵表列数 */
    int psFirstGid;        /**< 精灵表起始全局 GID */
    int psTileW;           /**< 精灵表中单个瓦片宽度（像素） */
    int psTileH;           /**< 精灵表中单个瓦片高度（像素，整行高度） */

    /* --- 背景图片层 --- */
    Texture2D backImage;   /**< 背景贴图（已缩放到地图大小） */
    float backOpacity;     /**< 背景透明度（0.0~1.0） */
    bool hasBackImage;     /**< 是否存在背景图片 */

    /* --- 对象列表（从所有 objectgroup 层解析） --- */
    MapObject objects[MAX_MAP_OBJECTS]; /**< 地图对象数组 */
    int objectCount;                    /**< 实际对象数量 */

    /* --- 玩家出生点 --- */
    Vector2 playerSpawn;  /**< 玩家初始位置（从 player 层第一个对象读取） */

    /* --- 玩家动画帧（精灵表内的本地 tile ID） --- */
    int animFrontLow[3];   /**< 前走低速帧序列 */
    int animFrontFast[3];  /**< 前走快速帧序列 */
    int animBack[3];       /**< 后退帧序列 */
    int animLeft[3];       /**< 左/右移帧序列 */
} Map;

/* ========== 函数声明 ========== */

/**
 * LoadMap - 从 TMJ 文件加载地图
 * @filepath: TMJ 文件路径
 * @return:   解析完成的 Map 结构体
 */
Map  LoadMap(const char *filepath);

/**
 * UnloadMap - 释放地图占用的 GPU 资源和堆内存
 * @map: 地图指针
 */
void UnloadMap(Map *map);

/**
 * DrawMap - 绘制地图（背景 + 地板瓦片）
 * @map: 地图指针
 */
void DrawMap(Map *map);

/**
 * GetSolidRects - 获取地图中所有 solid 类型碰撞矩形
 * @map:      地图指针
 * @out:      输出缓冲区
 * @maxCount: 缓冲区最大容量
 * @return:   实际获取到的 solid 矩形数量
 */
int  GetSolidRects(Map *map, Rectangle *out, int maxCount);

/**
 * GetStairsRects - 获取地图中所有 stairs 类型楼梯矩形
 * @map:      地图指针
 * @out:      输出缓冲区
 * @maxCount: 缓冲区最大容量
 * @return:   实际获取到的 stairs 矩形数量
 */
int  GetStairsRects(Map *map, Rectangle *out, int maxCount);

/**
 * GetDoorRects - 获取地图中所有 door 类型门的矩形
 * @map:      地图指针
 * @out:      输出缓冲区
 * @maxCount: 缓冲区最大容量
 * @return:   实际获取到的 door 矩形数量
 */
int  GetDoorRects(Map *map, Rectangle *out, int maxCount);

/**
 * GetStairFirstRects - 获取地图中所有 stair-first 类型矩形
 * @map:      地图指针
 * @out:      输出缓冲区
 * @maxCount: 缓冲区最大容量
 * @return:   实际获取到的 stair-first 矩形数量
 */
int  GetStairFirstRects(Map *map, Rectangle *out, int maxCount);

#endif /* MAP_H */
