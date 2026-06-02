/**
 * @file battle_sim.c
 * @brief 宝可梦文字对战模拟器 —— 回合制终端战斗
 *
 * 编译: gcc tools/battle_sim.c src/cJSON.c src/pokemon_db.c -Iinclude -o tools/battle_sim
 * 运行: ./tools/battle_sim
 *
 * 使用图鉴中 #1 阿勃梭鲁 vs #2 化石翼龙 作为示例对战
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "pokemon_db.h"

/* ======================== 对战数据结构 ======================== */

typedef struct {
    char name[32];
    char type1[16], type2[16];
    int level;
    int hp_current, hp_max;
    int attack, defense, sp_attack, sp_defense, speed;
    char moveNames[4][24];
    int  movePower[4], movePP[4], moveMaxPP[4];
    char moveType[16];
    int  moveCount;
} BattlePokemon;

/* ======================== 辅助函数 ======================== */

/** 根据种族值和等级计算实际能力 */
static int calcStat(int base, int level, int isHP) {
    if (isHP) return ((base * 2) * level / 100) + level + 10;
    return ((base * 2) * level / 100) + 5;
}

/** 加载宝可梦对战数据 */
static void loadBattlePokemon(BattlePokemon *bp, int speciesId, int level) {
    memset(bp, 0, sizeof(*bp));
    const SpeciesData *sp = GetSpeciesData(speciesId);
    if (!sp) {
        printf("[ERROR] 未找到宝可梦 #%d\n", speciesId);
        return;
    }
    strcpy(bp->name, sp->name);
    strcpy(bp->type1, TypeToChinese(sp->type1));
    if (sp->type2 != TYPE_NONE) strcpy(bp->type2, TypeToChinese(sp->type2));
    bp->level = level;

    if (sp->hasRealStats) {
        bp->hp_max     = calcStat(sp->baseStats.hp,         level, 1);
        bp->attack     = calcStat(sp->baseStats.attack,     level, 0);
        bp->defense    = calcStat(sp->baseStats.defense,    level, 0);
        bp->sp_attack  = calcStat(sp->baseStats.sp_attack,  level, 0);
        bp->sp_defense = calcStat(sp->baseStats.sp_defense, level, 0);
        bp->speed      = calcStat(sp->baseStats.speed,      level, 0);
    }
    bp->hp_current = bp->hp_max;

    /* 配招 */
    bp->moveCount = 0;
    for (int i = 0; i < sp->moveCount && bp->moveCount < 4; i++) {
        const MoveData *md = GetMoveData(sp->moveNames[i]);
        if (md) {
            strcpy(bp->moveNames[bp->moveCount], sp->moveNames[i]);
            bp->movePower[bp->moveCount] = md->power;
            bp->movePP[bp->moveCount]    = md->maxPP;
            bp->moveMaxPP[bp->moveCount] = md->maxPP;
            bp->moveCount++;
        }
    }
}

/** 显示宝可梦状态 */
static void showStatus(BattlePokemon *bp, int num) {
    printf("\n  ┌──────────────────────────────────┐\n");
    printf("  │  %s%d: %-20s Lv.%2d    │\n",
           num == 1 ? "🔴" : "🔵", num, bp->name, bp->level);
    printf("  │  属性: %s", bp->type1);
    if (bp->type2[0]) printf(" / %s", bp->type2);
    printf("\n");
    printf("  │  HP: %3d / %-3d  ", bp->hp_current, bp->hp_max);

    /* HP 条 */
    float frac = (float)bp->hp_current / bp->hp_max;
    int barLen = (int)(frac * 20);
    printf("[");
    for (int i = 0; i < 20; i++) {
        if (i < barLen) printf("█");
        else printf("░");
    }
    printf("]\n");
    printf("  └──────────────────────────────────┘\n");
}

/** 显示技能列表 */
static void showMoves(BattlePokemon *bp) {
    printf("\n  %s 的技能:\n", bp->name);
    for (int i = 0; i < bp->moveCount; i++) {
        printf("    %d. %-12s  威力:%-3d  PP:%d/%d\n",
               i + 1, bp->moveNames[i],
               bp->movePower[i], bp->movePP[i], bp->moveMaxPP[i]);
    }
}

/** 计算伤害 */
static int calcDamage(BattlePokemon *attacker, BattlePokemon *defender,
                      int moveIdx) {
    int power = attacker->movePower[moveIdx];
    if (power == 0) return 0;

    int atk = attacker->attack;
    int def = defender->defense;

    int base = ((2 * attacker->level / 5 + 2) * power * atk / def) / 50 + 2;
    float random = 0.85f + (float)(rand() % 16) / 100.0f;
    int dmg = (int)((float)base * random);
    if (dmg < 1) dmg = 1;
    return dmg;
}

/** 敌人 AI: 选择威力最高的技能 */
static int enemyChooseMove(BattlePokemon *bp) {
    int best = 0, bestPower = 0;
    for (int i = 0; i < bp->moveCount; i++) {
        if (bp->movePP[i] > 0 && bp->movePower[i] > bestPower) {
            bestPower = bp->movePower[i];
            best = i;
        }
    }
    /* 如果所有技能 PP=0, 选第一个 */
    if (bestPower == 0) {
        for (int i = 0; i < bp->moveCount; i++)
            if (bp->movePP[i] > 0) return i;
    }
    return best;
}

/* ======================== 主循环 ======================== */

int main(void) {
    srand(42); /* 固定种子方便测试, 正式版去掉 */

    LoadMoveDB();
    LoadPokemonDB();

    printf("\n  ╔══════════════════════════════════════╗\n");
    printf("  ║    宝可梦 文字对战模拟器 v1.0      ║\n");
    printf("  ╚══════════════════════════════════════╝\n");

    /* 加载双方宝可梦 */
    BattlePokemon player, enemy;
    loadBattlePokemon(&player, 1, 30);  /* 阿勃梭鲁 Lv30 */
    loadBattlePokemon(&enemy, 2, 28);   /* 化石翼龙 Lv28 */

    printf("\n  >>> 战斗开始! <<<\n");
    printf("  我方: %s Lv.%d  VS  敌方: %s Lv.%d\n",
           player.name, player.level, enemy.name, enemy.level);

    int turn = 0;

    /* 主循环 */
    while (1) {
        turn++;
        printf("\n══════════════ 第 %d 回合 ══════════════\n", turn);

        showStatus(&player, 1);
        showStatus(&enemy, 2);

        /* ---- 玩家回合 ---- */
        printf("\n  --- 你的回合 ---\n");
        showMoves(&player);

        /* 检查是否有可用技能 */
        int usable = 0;
        for (int i = 0; i < player.moveCount; i++)
            if (player.movePP[i] > 0) usable++;
        if (usable == 0) {
            printf("\n  [!] 没有可用技能! 挣扎中...\n");
            /* 挣扎: 固定伤害 */
            enemy.hp_current -= 10;
            if (enemy.hp_current < 0) enemy.hp_current = 0;
            printf("  对 %s 造成了 10 点伤害!\n", enemy.name);
            goto enemy_turn;
        }

        printf("  选择技能 (1-%d): ", player.moveCount);
        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            choice = 1;
        }
        choice--;
        if (choice < 0) choice = 0;
        if (choice >= player.moveCount) choice = player.moveCount - 1;

        if (player.movePP[choice] <= 0) {
            printf("  [!] 该技能 PP 不足! 自动使用技能1\n");
            choice = 0;
            if (player.movePP[0] <= 0) {
                for (int i = 1; i < player.moveCount; i++) {
                    if (player.movePP[i] > 0) { choice = i; break; }
                }
            }
        }

        player.movePP[choice]--;

        /* 判断命中 */
        int dmg = calcDamage(&player, &enemy, choice);
        enemy.hp_current -= dmg;
        if (enemy.hp_current < 0) enemy.hp_current = 0;

        printf("\n  ▸ %s 使用了 [%s]!\n", player.name, player.moveNames[choice]);
        if (dmg > 0)
            printf("    对 %s 造成了 %d 点伤害!\n", enemy.name, dmg);
        else
            printf("    但是没有效果...\n");

        /* 检查敌方是否倒下 */
        if (enemy.hp_current <= 0) {
            printf("\n  >>> %s 倒下了! <<<\n", enemy.name);
            printf("\n  ╔══════════════════════════════════════╗\n");
            printf("  ║          🏆 你赢了! 🏆            ║\n");
            printf("  ╚══════════════════════════════════════╝\n\n");
            break;
        }

enemy_turn:
        /* ---- 敌方回合 ---- */
        printf("\n  --- 敌方回合 ---\n");
        int eChoice = enemyChooseMove(&enemy);
        enemy.movePP[eChoice]--;

        int eDmg = calcDamage(&enemy, &player, eChoice);
        player.hp_current -= eDmg;
        if (player.hp_current < 0) player.hp_current = 0;

        printf("  ▸ %s 使用了 [%s]!\n", enemy.name, enemy.moveNames[eChoice]);
        if (eDmg > 0)
            printf("    对 %s 造成了 %d 点伤害!\n", player.name, eDmg);
        else
            printf("    但是没有效果...\n");

        /* 检查玩家是否倒下 */
        if (player.hp_current <= 0) {
            printf("\n  >>> %s 倒下了! <<<\n", player.name);
            printf("\n  ╔══════════════════════════════════════╗\n");
            printf("  ║          💀 你输了... 💀          ║\n");
            printf("  ╚══════════════════════════════════════╝\n\n");
            break;
        }
    }

    /* 战斗日志总结 */
    printf("  ── 战斗日志 ──\n");
    printf("  总回合数: %d\n", turn);
    printf("  我方 %s 剩余 HP: %d/%d\n", player.name, player.hp_current, player.hp_max);
    printf("  敌方 %s 剩余 HP: %d/%d\n", enemy.name, enemy.hp_current, enemy.hp_max);

    UnloadPokemonDB();
    return 0;
}
