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

#define MAX_MAP_OBJECTS    128   /**< 地图中最多支持的对象数量 */
#define MAX_SOLID_RECTS     64   /**< 碰撞检测时 solid 矩形数组的最大容量 */
#define MAX_STAIRS_RECTS    16   /**< 楼梯检测时 stairs 矩形数组的最大容量 */
#define MAX_DOOR_RECTS      16   /**< 门检测时 door 矩形数组的最大容量 */
#define MAX_STAIRFIRST_RECTS 16  /**< stair-first 矩形数组的最大容量 */
#define MAX_SIGN_RECTS       16  /**< 标牌矩形数组的最大容量 */
#define MAX_TILESETS         8   /**< 地图最多支持的 tileset 数量 */
#define MAX_TELEPORT_SPAWNS  8   /**< 传送点最大数量 */
#define MAX_NPC_OBJECTS      16  /**< 地图中NPC对象的最大数量 */
#define MAX_SOLID_TILES      64  /**< 硬编码solid GID的最大数量 */

/* ========== 数据结构 ========== */

/**
 * MapObject - 地图中的一个交互/碰撞对象
 *
 * 由 Tiled 编辑器的 object group 层解析而来，
 * 通过 properties 中的布尔属性标明类型（如 solid、stairs、door 等）。
 */
typedef struct {
    char name[64];       /**< 对象名称（Tiled 中定义） */
    char type[32];       /**< 对象类型（从 properties 中提取，如 "solid"、"stairs"、"door"） */
    Rectangle rect;      /**< 对象的位置和尺寸矩形 */
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
 * TeleportSpawn - 传送点信息
 *
 * 存储传送目的地的名称与坐标，
 * 由 chuansong 对象层解析而来。
 */
typedef struct {
    char name[64];       /**< 传送点名称 (如 "home", "home2") */
    Vector2 pos;         /**< 传送目标坐标 */
} TeleportSpawn;

/**
 * MapNpc - 地图中的NPC对象
 *
 * 由 npc 对象层解析而来，包含渲染所需的GID、
 * 位置矩形和类型（如 npc-boss, npc-teacher）。
 */
typedef struct {
    int gid;             /**< 全局瓦片ID（用于渲染NPC精灵） */
    Rectangle rect;      /**< NPC位置和尺寸 */
    char type[32];       /**< NPC类型：npc-boss, npc-teacher 等 */
} MapNpc;

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

    /* --- 传送点列表（从 chuansong 层解析） --- */
    TeleportSpawn teleportSpawns[MAX_TELEPORT_SPAWNS];
    int teleportSpawnCount;

    /* --- NPC对象列表（从 npc 层解析） --- */
    MapNpc npcs[MAX_NPC_OBJECTS];
    int npcCount;

    /* --- solid 瓦片GID列表（从图块层解析的不可通行GID） --- */
    int solidGids[MAX_SOLID_TILES];
    int solidGidCount;

    /* --- 玩家精灵缩放（1.0=正常, 0.33=室外缩小3倍） --- */
    float playerScale;     /**< 玩家视觉缩放因子 */

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

int  GetStairFirstRects(Map *map, Rectangle *out, int maxCount);

/**
 * GetChuansongRects - 获取地图中所有 chuansong 类型传送矩形
 * @map:      地图指针
 * @out:      输出缓冲区
 * @maxCount: 缓冲区最大容量
 * @return:   实际获取到的 chuansong 矩形数量
 */
int  GetChuansongRects(Map *map, Rectangle *out, int maxCount);

/**
 * GetSignRects - 获取地图中所有 sign 类型标牌矩形
 * @map:      地图指针
 * @out:      输出缓冲区
 * @maxCount: 缓冲区最大容量
 * @return:   实际获取到的 sign 矩形数量
 */
int  GetSignRects(Map *map, Rectangle *out, int maxCount);

/**
 * GetSignName - 查找与给定矩形重叠的标牌名称
 * @map:  地图指针
 * @rect: 查询矩形
 * @name: 输出——标牌名称缓冲区
 * @size: name 缓冲区大小
 * @return: true 表示找到
 */
bool GetSignName(Map *map, Rectangle rect, char *name, int size);

/**
 * GetChuansongName - 查找与给定矩形重叠的传送点名称
 * @map:  地图指针
 * @rect: 查询矩形（如玩家碰撞盒）
 * @name: 输出——传送点名称缓冲区
 * @size: name 缓冲区大小
 * @return: true 表示找到
 */
bool GetChuansongName(Map *map, Rectangle rect, char *name, int size);

/**
 * FindTeleportSpawn - 根据名称查找传送点坐标
 * @map:  地图指针
 * @name: 传送点名称（如 "home", "home2"）
 * @pos:  输出——传送目标坐标（若未找到则不变）
 * @return: true 表示找到
 */
bool FindTeleportSpawn(Map *map, const char *name, Vector2 *pos);

/**
 * DrawNpcs - 绘制地图中所有NPC精灵
 * @map: 地图指针
 */
void DrawNpcs(Map *map);

/**
 * GetNpcRects - 获取所有NPC的交互矩形
 * @map:      地图指针
 * @out:      输出缓冲区
 * @maxCount: 缓冲区最大容量
 * @return:   实际获取到的NPC矩形数量
 */
int  GetNpcRects(Map *map, Rectangle *out, int maxCount);

/**
 * GetNpcInfo - 查找与给定矩形重叠的NPC信息
 * @map:  地图指针
 * @rect: 查询矩形
 * @type: 输出——NPC类型（如 "npc-boss", "npc-teacher"）
 * @size: type 缓冲区大小
 * @return: true 表示找到（返回第一个匹配的）
 */
bool GetNpcInfo(Map *map, Rectangle rect, char *type, int size);

/**
 * IsGidSolid - 检查给定的GID是否为solid瓦片
 * @map: 地图指针
 * @gid: 瓦片GID
 * @return: true 表示该瓦片不可通行
 */
bool IsGidSolid(Map *map, int gid);

#endif /* MAP_H */
