/**
 * @file pokemon.h
 * @brief 宝可梦数据结构 —— 属性、技能、种族数据库 (JSON 驱动)
 */

#ifndef POKEMON_H
#define POKEMON_H

#include "raylib.h"

#define MAX_MOVES 4
#define MAX_NAME_LEN 32
#define MAX_MOVE_NAME_LEN 24
#define MAX_LEARNSET 10

/** 19 种属性 */
typedef enum {
    TYPE_NORMAL, TYPE_FIRE, TYPE_WATER, TYPE_ELECTRIC,
    TYPE_GRASS, TYPE_ICE, TYPE_FIGHTING, TYPE_POISON,
    TYPE_GROUND, TYPE_FLYING, TYPE_PSYCHIC, TYPE_BUG,
    TYPE_ROCK, TYPE_GHOST, TYPE_DRAGON, TYPE_STEEL,
    TYPE_DARK, TYPE_FAIRY, TYPE_NONE
} PokemonType;

/** 技能模板（从 moves.json 加载） */
typedef struct {
    char name[MAX_MOVE_NAME_LEN];
    PokemonType type;
    int power;
    int accuracy;
    int maxPP;
} MoveData;

/** 种族值 */
typedef struct {
    int hp;
    int attack;
    int defense;
    int sp_attack;
    int sp_defense;
    int speed;
} BaseStats;

/** 宝可梦种族模板（从 pokemon.json 加载） */
typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    PokemonType type1;
    PokemonType type2;
    BaseStats baseStats;
    bool hasRealStats;        /**< 是否从 JSON 读取了真实种族值 */
    char moveNames[MAX_LEARNSET][MAX_MOVE_NAME_LEN];
    int moveCount;
} SpeciesData;

/** 战斗中的宝可梦个体 */
typedef struct {
    int speciesId;
    char name[MAX_NAME_LEN];
    PokemonType type1;
    PokemonType type2;
    int level;
    int currentHP;
    int maxHP;
    int attack;
    int defense;
    int spAttack;
    int spDefense;
    int speed;
    int currentExp;
    int expToNext;
    MoveData moves[MAX_MOVES];
    int moveCount;
    int movePP[MAX_MOVES];
    Texture2D frontSprite;
    Texture2D backSprite;
} Pokemon;

/** 属性名字符串 → 枚举值 */
int  ParseTypeString(const char *s);

/** 属性枚举 → 中文名 */
const char *TypeToChinese(PokemonType t);

/** 从数据库初始化宝可梦个体 */
void InitPokemon(Pokemon *p, int speciesId, int level);

/** 伤害计算 */
int  CalculateDamage(Pokemon *attacker, Pokemon *defender, MoveData *move);
void PokemonTakeDamage(Pokemon *p, int damage);
bool PokemonIsFainted(Pokemon *p);
void HealPokemon(Pokemon *p);

#endif
