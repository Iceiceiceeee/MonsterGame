/**
 * @file pokemon.c
 * @brief 宝可梦个体操作 —— 初始化、伤害计算、状态管理
 *
 * InitPokemon 从 JSON 数据库查询种族数据,
 * 自动生成属性值（简化公式），加载精灵贴图。
 */

#include "pokemon.h"
#include "pokemon_db.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ---- 属性相克表 ---- */

static float GetTypeEffectiveness(PokemonType moveType, PokemonType defType1, PokemonType defType2) {
    if (moveType == TYPE_FIRE) {
        if (defType1 == TYPE_GRASS || defType1 == TYPE_ICE || defType1 == TYPE_BUG || defType1 == TYPE_STEEL) return 2.0f;
        if (defType1 == TYPE_WATER || defType1 == TYPE_ROCK || defType1 == TYPE_DRAGON || defType1 == TYPE_FIRE) return 0.5f;
        if (defType2 == TYPE_GRASS || defType2 == TYPE_ICE || defType2 == TYPE_BUG || defType2 == TYPE_STEEL) return 2.0f;
        if (defType2 == TYPE_WATER || defType2 == TYPE_ROCK || defType2 == TYPE_DRAGON || defType2 == TYPE_FIRE) return 0.5f;
    }
    if (moveType == TYPE_WATER) {
        if (defType1 == TYPE_FIRE || defType1 == TYPE_GROUND || defType1 == TYPE_ROCK) return 2.0f;
        if (defType1 == TYPE_WATER || defType1 == TYPE_GRASS || defType1 == TYPE_DRAGON) return 0.5f;
        if (defType2 == TYPE_FIRE || defType2 == TYPE_GROUND || defType2 == TYPE_ROCK) return 2.0f;
        if (defType2 == TYPE_WATER || defType2 == TYPE_GRASS || defType2 == TYPE_DRAGON) return 0.5f;
    }
    if (moveType == TYPE_GRASS) {
        if (defType1 == TYPE_WATER || defType1 == TYPE_GROUND || defType1 == TYPE_ROCK) return 2.0f;
        if (defType1 == TYPE_FIRE || defType1 == TYPE_GRASS || defType1 == TYPE_POISON || defType1 == TYPE_FLYING || defType1 == TYPE_BUG || defType1 == TYPE_DRAGON || defType1 == TYPE_STEEL) return 0.5f;
        if (defType2 == TYPE_WATER || defType2 == TYPE_GROUND || defType2 == TYPE_ROCK) return 2.0f;
        if (defType2 == TYPE_FIRE || defType2 == TYPE_GRASS || defType2 == TYPE_POISON || defType2 == TYPE_FLYING || defType2 == TYPE_BUG || defType2 == TYPE_DRAGON || defType2 == TYPE_STEEL) return 0.5f;
    }
    if (moveType == TYPE_ELECTRIC) {
        if (defType1 == TYPE_WATER || defType1 == TYPE_FLYING) return 2.0f;
        if (defType1 == TYPE_ELECTRIC || defType1 == TYPE_GRASS || defType1 == TYPE_DRAGON) return 0.5f;
        if (defType1 == TYPE_GROUND) return 0.0f;
        if (defType2 == TYPE_WATER || defType2 == TYPE_FLYING) return 2.0f;
        if (defType2 == TYPE_ELECTRIC || defType2 == TYPE_GRASS || defType2 == TYPE_DRAGON) return 0.5f;
        if (defType2 == TYPE_GROUND) return 0.0f;
    }
    if (moveType == TYPE_ICE) {
        if (defType1 == TYPE_GRASS || defType1 == TYPE_GROUND || defType1 == TYPE_FLYING || defType1 == TYPE_DRAGON) return 2.0f;
        if (defType1 == TYPE_FIRE || defType1 == TYPE_WATER || defType1 == TYPE_ICE || defType1 == TYPE_STEEL) return 0.5f;
        if (defType2 == TYPE_GRASS || defType2 == TYPE_GROUND || defType2 == TYPE_FLYING || defType2 == TYPE_DRAGON) return 2.0f;
        if (defType2 == TYPE_FIRE || defType2 == TYPE_WATER || defType2 == TYPE_ICE || defType2 == TYPE_STEEL) return 0.5f;
    }
    if (moveType == TYPE_FIGHTING) {
        if (defType1 == TYPE_NORMAL || defType1 == TYPE_ICE || defType1 == TYPE_ROCK || defType1 == TYPE_DARK || defType1 == TYPE_STEEL) return 2.0f;
        if (defType1 == TYPE_POISON || defType1 == TYPE_FLYING || defType1 == TYPE_PSYCHIC || defType1 == TYPE_BUG || defType1 == TYPE_FAIRY) return 0.5f;
        if (defType1 == TYPE_GHOST) return 0.0f;
        if (defType2 == TYPE_NORMAL || defType2 == TYPE_ICE || defType2 == TYPE_ROCK || defType2 == TYPE_DARK || defType2 == TYPE_STEEL) return 2.0f;
        if (defType2 == TYPE_POISON || defType2 == TYPE_FLYING || defType2 == TYPE_PSYCHIC || defType2 == TYPE_BUG || defType2 == TYPE_FAIRY) return 0.5f;
        if (defType2 == TYPE_GHOST) return 0.0f;
    }
    if (moveType == TYPE_POISON) {
        if (defType1 == TYPE_GRASS || defType1 == TYPE_FAIRY) return 2.0f;
        if (defType1 == TYPE_POISON || defType1 == TYPE_GROUND || defType1 == TYPE_ROCK || defType1 == TYPE_GHOST) return 0.5f;
        if (defType1 == TYPE_STEEL) return 0.0f;
        if (defType2 == TYPE_GRASS || defType2 == TYPE_FAIRY) return 2.0f;
        if (defType2 == TYPE_POISON || defType2 == TYPE_GROUND || defType2 == TYPE_ROCK || defType2 == TYPE_GHOST) return 0.5f;
        if (defType2 == TYPE_STEEL) return 0.0f;
    }
    if (moveType == TYPE_GROUND) {
        if (defType1 == TYPE_FIRE || defType1 == TYPE_ELECTRIC || defType1 == TYPE_POISON || defType1 == TYPE_ROCK || defType1 == TYPE_STEEL) return 2.0f;
        if (defType1 == TYPE_GRASS || defType1 == TYPE_BUG) return 0.5f;
        if (defType1 == TYPE_FLYING) return 0.0f;
        if (defType2 == TYPE_FIRE || defType2 == TYPE_ELECTRIC || defType2 == TYPE_POISON || defType2 == TYPE_ROCK || defType2 == TYPE_STEEL) return 2.0f;
        if (defType2 == TYPE_GRASS || defType2 == TYPE_BUG) return 0.5f;
        if (defType2 == TYPE_FLYING) return 0.0f;
    }
    if (moveType == TYPE_FLYING) {
        if (defType1 == TYPE_GRASS || defType1 == TYPE_FIGHTING || defType1 == TYPE_BUG) return 2.0f;
        if (defType1 == TYPE_ROCK || defType1 == TYPE_STEEL || defType1 == TYPE_ELECTRIC) return 0.5f;
        if (defType2 == TYPE_GRASS || defType2 == TYPE_FIGHTING || defType2 == TYPE_BUG) return 2.0f;
        if (defType2 == TYPE_ROCK || defType2 == TYPE_STEEL || defType2 == TYPE_ELECTRIC) return 0.5f;
    }
    if (moveType == TYPE_PSYCHIC) {
        if (defType1 == TYPE_FIGHTING || defType1 == TYPE_POISON) return 2.0f;
        if (defType1 == TYPE_PSYCHIC || defType1 == TYPE_STEEL) return 0.5f;
        if (defType1 == TYPE_DARK) return 0.0f;
        if (defType2 == TYPE_FIGHTING || defType2 == TYPE_POISON) return 2.0f;
        if (defType2 == TYPE_PSYCHIC || defType2 == TYPE_STEEL) return 0.5f;
        if (defType2 == TYPE_DARK) return 0.0f;
    }
    if (moveType == TYPE_BUG) {
        if (defType1 == TYPE_GRASS || defType1 == TYPE_PSYCHIC || defType1 == TYPE_DARK) return 2.0f;
        if (defType1 == TYPE_FIRE || defType1 == TYPE_FIGHTING || defType1 == TYPE_POISON || defType1 == TYPE_FLYING || defType1 == TYPE_GHOST || defType1 == TYPE_STEEL || defType1 == TYPE_FAIRY) return 0.5f;
        if (defType2 == TYPE_GRASS || defType2 == TYPE_PSYCHIC || defType2 == TYPE_DARK) return 2.0f;
        if (defType2 == TYPE_FIRE || defType2 == TYPE_FIGHTING || defType2 == TYPE_POISON || defType2 == TYPE_FLYING || defType2 == TYPE_GHOST || defType2 == TYPE_STEEL || defType2 == TYPE_FAIRY) return 0.5f;
    }
    if (moveType == TYPE_ROCK) {
        if (defType1 == TYPE_FIRE || defType1 == TYPE_ICE || defType1 == TYPE_FLYING || defType1 == TYPE_BUG) return 2.0f;
        if (defType1 == TYPE_FIGHTING || defType1 == TYPE_GROUND || defType1 == TYPE_STEEL) return 0.5f;
        if (defType2 == TYPE_FIRE || defType2 == TYPE_ICE || defType2 == TYPE_FLYING || defType2 == TYPE_BUG) return 2.0f;
        if (defType2 == TYPE_FIGHTING || defType2 == TYPE_GROUND || defType2 == TYPE_STEEL) return 0.5f;
    }
    if (moveType == TYPE_GHOST) {
        if (defType1 == TYPE_PSYCHIC || defType1 == TYPE_GHOST) return 2.0f;
        if (defType1 == TYPE_DARK) return 0.5f;
        if (defType1 == TYPE_NORMAL) return 0.0f;
        if (defType2 == TYPE_PSYCHIC || defType2 == TYPE_GHOST) return 2.0f;
        if (defType2 == TYPE_DARK) return 0.5f;
        if (defType2 == TYPE_NORMAL) return 0.0f;
    }
    if (moveType == TYPE_DRAGON) {
        if (defType1 == TYPE_DRAGON) return 2.0f;
        if (defType1 == TYPE_STEEL) return 0.5f;
        if (defType1 == TYPE_FAIRY) return 0.0f;
        if (defType2 == TYPE_DRAGON) return 2.0f;
        if (defType2 == TYPE_STEEL) return 0.5f;
        if (defType2 == TYPE_FAIRY) return 0.0f;
    }
    if (moveType == TYPE_DARK) {
        if (defType1 == TYPE_PSYCHIC || defType1 == TYPE_GHOST) return 2.0f;
        if (defType1 == TYPE_FIGHTING || defType1 == TYPE_DARK || defType1 == TYPE_FAIRY) return 0.5f;
        if (defType2 == TYPE_PSYCHIC || defType2 == TYPE_GHOST) return 2.0f;
        if (defType2 == TYPE_FIGHTING || defType2 == TYPE_DARK || defType2 == TYPE_FAIRY) return 0.5f;
    }
    if (moveType == TYPE_STEEL) {
        if (defType1 == TYPE_ICE || defType1 == TYPE_ROCK || defType1 == TYPE_FAIRY) return 2.0f;
        if (defType1 == TYPE_FIRE || defType1 == TYPE_WATER || defType1 == TYPE_ELECTRIC || defType1 == TYPE_STEEL) return 0.5f;
        if (defType2 == TYPE_ICE || defType2 == TYPE_ROCK || defType2 == TYPE_FAIRY) return 2.0f;
        if (defType2 == TYPE_FIRE || defType2 == TYPE_WATER || defType2 == TYPE_ELECTRIC || defType2 == TYPE_STEEL) return 0.5f;
    }
    if (moveType == TYPE_FAIRY) {
        if (defType1 == TYPE_FIGHTING || defType1 == TYPE_DRAGON || defType1 == TYPE_DARK) return 2.0f;
        if (defType1 == TYPE_FIRE || defType1 == TYPE_POISON || defType1 == TYPE_STEEL) return 0.5f;
        if (defType2 == TYPE_FIGHTING || defType2 == TYPE_DRAGON || defType2 == TYPE_DARK) return 2.0f;
        if (defType2 == TYPE_FIRE || defType2 == TYPE_POISON || defType2 == TYPE_STEEL) return 0.5f;
    }
    if (moveType == TYPE_NORMAL) {
        if (defType1 == TYPE_GHOST) return 0.0f;
        if (defType1 == TYPE_ROCK || defType1 == TYPE_STEEL) return 0.5f;
        if (defType2 == TYPE_GHOST) return 0.0f;
        if (defType2 == TYPE_ROCK || defType2 == TYPE_STEEL) return 0.5f;
    }
    return 1.0f;
}

/* ---- 从种族值+等级计算实际能力值 (简化公式) ---- */

static int CalcStatFromBase(int base, int level, bool isHP) {
    if (isHP) return ((base * 2) * level / 100) + level + 10;
    return ((base * 2) * level / 100) + 5;
}

/* ---- 初始化宝可梦个体 ---- */

void InitPokemon(Pokemon *p, int speciesId, int level) {
    memset(p, 0, sizeof(*p));

    const SpeciesData *sp = GetSpeciesData(speciesId);

    /* 默认种族值 (未知编号时使用) */
    BaseStats bs = { 50, 50, 50, 50, 50, 50 };
    if (sp && sp->hasRealStats) {
        bs = sp->baseStats;
    } else if (sp) {
        /* 有数据但没 stats: 用自动公式 */
        bs.hp         = 45 + (speciesId % 20) * 3;
        bs.attack     = 40 + (speciesId % 15) * 4;
        bs.defense    = 40 + (speciesId % 12) * 4;
        bs.sp_attack  = 40 + (speciesId % 18) * 3;
        bs.sp_defense = 40 + (speciesId % 14) * 3;
        bs.speed      = 40 + (speciesId % 10) * 5;
    }

    if (!sp) {
        snprintf(p->name, MAX_NAME_LEN, "No.%d", speciesId);
        p->speciesId = speciesId;
        p->type1 = TYPE_NORMAL;
        p->type2 = TYPE_NONE;
    } else {
        snprintf(p->name, MAX_NAME_LEN, "%s", sp->name);
        p->speciesId = sp->id;
        p->type1 = sp->type1;
        p->type2 = sp->type2;
    }

    p->level = level;
    p->maxHP      = CalcStatFromBase(bs.hp,         level, true);
    p->attack     = CalcStatFromBase(bs.attack,     level, false);
    p->defense    = CalcStatFromBase(bs.defense,    level, false);
    p->spAttack   = CalcStatFromBase(bs.sp_attack,  level, false);
    p->spDefense  = CalcStatFromBase(bs.sp_defense, level, false);
    p->speed      = CalcStatFromBase(bs.speed,      level, false);
    p->currentHP  = p->maxHP;
    p->expToNext  = level * level * level;

    /* 配招: 从技能库查找 */
    p->moveCount = 0;
    for (int i = 0; i < sp->moveCount && p->moveCount < MAX_MOVES; i++) {
        const MoveData *md = GetMoveData(sp->moveNames[i]);
        if (md) {
            p->moves[p->moveCount] = *md;
            p->movePP[p->moveCount] = md->maxPP;
            p->moveCount++;
        }
    }
    /* 如果没有配招, 给一个默认撞击 */
    if (p->moveCount == 0) {
        const MoveData *tackle = GetMoveData("撞击");
        if (tackle) {
            p->moves[0] = *tackle;
            p->movePP[0] = tackle->maxPP;
            p->moveCount = 1;
        }
    }

    /* 加载精灵贴图 */
    char path[256];
    snprintf(path, sizeof(path), "assets/images/front/front_%d.png", speciesId);
    if (FileExists(path)) {
        Image img = LoadImage(path);
        if (img.data) {
            ImageResize(&img, img.width * 3, img.height * 3);
            p->frontSprite = LoadTextureFromImage(img);
            SetTextureFilter(p->frontSprite, TEXTURE_FILTER_POINT);
            UnloadImage(img);
        }
    }

    snprintf(path, sizeof(path), "assets/images/back/back_%d.png", speciesId);
    if (FileExists(path)) {
        Image img = LoadImage(path);
        if (img.data) {
            ImageResize(&img, img.width * 3, img.height * 3);
            p->backSprite = LoadTextureFromImage(img);
            SetTextureFilter(p->backSprite, TEXTURE_FILTER_POINT);
            UnloadImage(img);
        }
    }
}

/* ---- 伤害计算 ---- */

int CalculateDamage(Pokemon *attacker, Pokemon *defender, MoveData *move) {
    if (move->power == 0) return 0;

    /* 物理/特殊判定 (前3代: 属性决定) */
    bool isSpecial = (move->type == TYPE_FIRE || move->type == TYPE_WATER ||
                      move->type == TYPE_ELECTRIC || move->type == TYPE_GRASS ||
                      move->type == TYPE_ICE || move->type == TYPE_PSYCHIC ||
                      move->type == TYPE_DRAGON || move->type == TYPE_DARK);

    int atk = isSpecial ? attacker->spAttack : attacker->attack;
    int def = isSpecial ? defender->spDefense : defender->defense;

    int baseDmg = ((2 * attacker->level / 5 + 2) * move->power * atk / def) / 50 + 2;

    float stab = (move->type == attacker->type1 || move->type == attacker->type2) ? 1.5f : 1.0f;
    float eff = GetTypeEffectiveness(move->type, defender->type1, defender->type2);
    float random = 0.85f + (float)(rand() % 16) / 100.0f;

    int final = (int)((float)baseDmg * stab * eff * random);
    if (final < 1) final = 1;
    return final;
}

void PokemonTakeDamage(Pokemon *p, int damage) {
    p->currentHP -= damage;
    if (p->currentHP < 0) p->currentHP = 0;
}

bool PokemonIsFainted(Pokemon *p) {
    return p->currentHP <= 0;
}

void HealPokemon(Pokemon *p) {
    p->currentHP = p->maxHP;
    for (int i = 0; i < p->moveCount; i++) {
        p->movePP[i] = p->moves[i].maxPP;
    }
}
