/**
 * @file battle.h
 * @brief 战斗系统头文件 —— 战斗状态机与上下文定义
 *
 * 定义了战斗的 6 个状态阶段（开场/菜单/技能选择/执行/消息/结束），
 * 以及战斗上下文（包含双方宝可梦、UI 贴图、动画状态等）。
 */

#ifndef BATTLE_H
#define BATTLE_H

#include "raylib.h"
#include "pokemon.h"

/**
 * @brief 战斗状态枚举（回合制战斗的状态机阶段）
 * BATTLE_INTRO       - 开场："野生的 xxx 出现了!"
 * BATTLE_MENU        - 主菜单选择：战斗 / 道具 / 精灵 / 逃跑
 * BATTLE_MOVE_SELECT - 技能子菜单：选择要使用的招式
 * BATTLE_EXECUTE     - 执行阶段：播放攻击动画、计算伤害
 * BATTLE_MESSAGE     - 消息暂停：等待玩家按键确认
 * BATTLE_END         - 战斗结束：清理资源、切回世界地图
 */
typedef enum {
    BATTLE_INTRO,
    BATTLE_MENU,
    BATTLE_MOVE_SELECT,
    BATTLE_EXECUTE,
    BATTLE_MESSAGE,
    BATTLE_END
} BattleState;

/**
 * @brief 战斗上下文（包含一次战斗的所有运行时状态）
 * @param state         当前战斗状态机阶段
 * @param returnState   从消息状态返回时的目标状态
 * @param playerPoke    玩家宝可梦
 * @param enemyPoke     对手宝可梦
 * @param bgTexture     战斗背景贴图
 * @param platformTex   椭圆站台贴图
 * @param hpBarTex      HP 条贴图
 * @param cursorTex     光标/箭头贴图
 * @param fontCN        中文字体（由 game.c 传入）
 * @param menuSelection 主菜单当前选中项 (0=战斗,1=道具,2=精灵,3=逃跑)
 * @param moveSelection 技能菜单当前选中项 (0..3)
 * @param messageTimer  消息显示计时器（帧数）
 * @param animTimer     动画阶段计时器（帧数）
 * @param animPhase     动画子阶段 (0=显示文本,1=扣血,2=对手文本,3=对手扣血)
 * @param storedDamage  暂存的伤害值（用于动画中逐步扣除 HP）
 * @param message       对话框文本缓冲区
 * @param needsCleanup  是否需要清理并退出战斗
 */
typedef struct {
    BattleState state;
    BattleState returnState;
    Pokemon playerPoke;
    Pokemon enemyPoke;
    Texture2D bgTexture;
    Texture2D platformTex;
    Texture2D hpBarTex;
    Texture2D cursorTex;
    Font fontCN;
    int menuSelection;
    int moveSelection;
    int messageTimer;
    int animTimer;
    int animPhase;
    int storedDamage;
    char message[256];
    bool needsCleanup;
} BattleContext;

/** 初始化战斗：加载背景/站台/精灵贴图，初始化双方宝可梦
 *  @param isBoss  true = boss战（显示"boss派出了"而非"野生的"） */
void InitBattle(BattleContext *bc, Font fontCN, bool isBoss);

/** 每帧更新战斗逻辑：处理玩家输入、状态切换、动画推进 */
void UpdateBattle(BattleContext *bc);

/** 每帧绘制战斗界面：背景 → 站台 → 精灵 → 信息框 → 菜单 → 对话框 */
void DrawBattle(BattleContext *bc);

/** 释放战斗资源：卸载所有贴图，重置战斗上下文 */
void CloseBattle(BattleContext *bc);

/** 查询战斗是否已结束 (逃跑 / 一方倒下) */
bool IsBattleFinished(void);

#endif
