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

#define MAX_MAP_OBJECTS   128   /**< 地图中最多支持的对象数量 */
#define MAX_SOLID_RECTS    64   /**< 碰撞检测时 solid 矩形数组的最大容量 */
#define MAX_STAIRS_RECTS   16   /**< 楼梯检测时 stairs 矩形数组的最大容量 */

/* ========== 数据结构 ========== */

/**
 * MapObject - 地图中的一个交互/碰撞对象
 *
 * 由 Tiled 编辑器的 object group 层解析而来，
 * 通过 properties 中的布尔属性标明类型（如 solid、stairs 等）。
 */
typedef struct {
    char name[64];       /**< 对象名称（Tiled 中定义） */
    char type[32];       /**< 对象类型（从 properties 中提取，如 "solid"、"stairs"） */
    Rectangle rect;      /**< 对象的位置和尺寸矩形 */
} MapObject;

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

    /* --- tileset 1（QQ_1779796916957.png） --- */
    Texture2D tileset1;  /**< tileset1 贴图 */
    int ts1Cols;         /**< tileset1 列数 */
    int ts1FirstGid;     /**< tileset1 起始全局 GID（值为 1） */

    /* --- tileset 2（6abad8d14de667fabaecfea4a7242f82.png） --- */
    Texture2D tileset2;  /**< tileset2 贴图 */
    int ts2Cols;         /**< tileset2 列数（35） */
    int ts2FirstGid;     /**< tileset2 起始全局 GID（值为 946） */

    /* --- 玩家精灵表（player_sheet.png） --- */
    Texture2D playerSheet; /**< 玩家精灵贴图 */
    int psCols;            /**< 精灵表列数 */
    int psFirstGid;        /**< 精灵表起始全局 GID（值为 1786） */
    int psTileW;           /**< 精灵表中单个瓦片宽度（像素） */
    int psTileH;           /**< 精灵表中单个瓦片高度（像素，整行高度） */

    /* --- 背景图片层 --- */
    Texture2D backImage;   /**< 背景贴图（已缩放到地图大小） */
    float backOpacity;     /**< 背景透明度（0.0~1.0） */
    bool hasBackImage;     /**< 是否存在背景图片 */

    /* --- 对象列表（从 item 层解析） --- */
    MapObject objects[MAX_MAP_OBJECTS]; /**< 地图对象数组 */
    int objectCount;                    /**< 实际对象数量 */

    /* --- 玩家出生点 --- */
    Vector2 playerSpawn;  /**< 玩家初始位置（从 player 层第一个对象读取） */

    /* --- 玩家动画帧（精灵表内的本地 tile ID） --- */
    int animFrontLow[3];   /**< 前走低速帧序列（0, 12, 13） */
    int animFrontFast[3];  /**< 前走快速帧序列（3, 4, 5） */
    int animBack[3];       /**< 后退帧序列（6, 7, 8） */
    int animLeft[3];       /**< 左/右移帧序列（9, 10, 11） */
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

#endif /* MAP_H */
