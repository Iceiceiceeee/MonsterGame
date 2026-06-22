/**
 * @file pokemon_db.h
 * @brief 宝可梦数据库 —— 从 JSON 加载技能库和种族库
 */

#ifndef POKEMON_DB_H
#define POKEMON_DB_H

#include "pokemon.h"
#include "cJSON.h"

/** 加载技能库 (assets/moves.json) */
void LoadMoveDB(void);

/** 加载宝可梦种族库 (assets/pokemon.json) */
void LoadPokemonDB(void);

/** 释放数据库 */
void UnloadPokemonDB(void);

/** 按名字查找技能 */
const MoveData *GetMoveData(const char *name);

/** 按图鉴编号查找种族 */
const SpeciesData *GetSpeciesData(int id);

/** 获取数据库中的宝可梦数量 */
int GetSpeciesCount(void);

/** 按索引获取种族 (0..count-1) */
const SpeciesData *GetSpeciesByIndex(int index);

/** 计算种族值总和 */
int TotalBaseStats(const SpeciesData *sp);

/** 种族数据序列化为 JSON 对象 (调用者负责 cJSON_Delete) */
cJSON *SpeciesToJSON(const SpeciesData *sp);

#endif
