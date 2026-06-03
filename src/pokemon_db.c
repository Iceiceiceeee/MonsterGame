/**
 * @file pokemon_db.c
 * @brief JSON 数据库加载 —— 解析 moves.json 和 pokemon.json
 *
 * 支持新格式 type: ["草", "毒"] 数组 + stats: {...} 对象
 */

#include "pokemon_db.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ---- 内部存储 ---- */

static MoveData   *moveDB = NULL;
static int         moveCount = 0;
static SpeciesData *speciesDB = NULL;
static int          speciesCount = 0;

/* ---- 类型字符串 ↔ 枚举 ---- */

int ParseTypeString(const char *s) {
    if (!s) return TYPE_NONE;
    /* 英文 */
    if (strcmp(s, "NORMAL")   == 0) return TYPE_NORMAL;
    if (strcmp(s, "FIRE")     == 0) return TYPE_FIRE;
    if (strcmp(s, "WATER")    == 0) return TYPE_WATER;
    if (strcmp(s, "ELECTRIC") == 0) return TYPE_ELECTRIC;
    if (strcmp(s, "GRASS")    == 0) return TYPE_GRASS;
    if (strcmp(s, "ICE")      == 0) return TYPE_ICE;
    if (strcmp(s, "FIGHTING") == 0) return TYPE_FIGHTING;
    if (strcmp(s, "POISON")   == 0) return TYPE_POISON;
    if (strcmp(s, "GROUND")   == 0) return TYPE_GROUND;
    if (strcmp(s, "FLYING")   == 0) return TYPE_FLYING;
    if (strcmp(s, "PSYCHIC")  == 0) return TYPE_PSYCHIC;
    if (strcmp(s, "BUG")      == 0) return TYPE_BUG;
    if (strcmp(s, "ROCK")     == 0) return TYPE_ROCK;
    if (strcmp(s, "GHOST")    == 0) return TYPE_GHOST;
    if (strcmp(s, "DRAGON")   == 0) return TYPE_DRAGON;
    if (strcmp(s, "STEEL")    == 0) return TYPE_STEEL;
    if (strcmp(s, "DARK")     == 0) return TYPE_DARK;
    if (strcmp(s, "FAIRY")    == 0) return TYPE_FAIRY;
    /* 中文 */
    if (strcmp(s, "一般")   == 0) return TYPE_NORMAL;
    if (strcmp(s, "火")     == 0) return TYPE_FIRE;
    if (strcmp(s, "水")     == 0) return TYPE_WATER;
    if (strcmp(s, "电")     == 0) return TYPE_ELECTRIC;
    if (strcmp(s, "草")     == 0) return TYPE_GRASS;
    if (strcmp(s, "冰")     == 0) return TYPE_ICE;
    if (strcmp(s, "格斗")   == 0) return TYPE_FIGHTING;
    if (strcmp(s, "毒")     == 0) return TYPE_POISON;
    if (strcmp(s, "地面")   == 0) return TYPE_GROUND;
    if (strcmp(s, "飞行")   == 0) return TYPE_FLYING;
    if (strcmp(s, "超能力") == 0) return TYPE_PSYCHIC;
    if (strcmp(s, "虫")     == 0) return TYPE_BUG;
    if (strcmp(s, "岩石")   == 0) return TYPE_ROCK;
    if (strcmp(s, "幽灵")   == 0) return TYPE_GHOST;
    if (strcmp(s, "龙")     == 0) return TYPE_DRAGON;
    if (strcmp(s, "钢")     == 0) return TYPE_STEEL;
    if (strcmp(s, "恶")     == 0) return TYPE_DARK;
    if (strcmp(s, "妖精")   == 0) return TYPE_FAIRY;
    return TYPE_NONE;
}

const char *TypeToChinese(PokemonType t) {
    switch (t) {
        case TYPE_NORMAL:   return "一般";
        case TYPE_FIRE:     return "火";
        case TYPE_WATER:    return "水";
        case TYPE_ELECTRIC: return "电";
        case TYPE_GRASS:    return "草";
        case TYPE_ICE:      return "冰";
        case TYPE_FIGHTING: return "格斗";
        case TYPE_POISON:   return "毒";
        case TYPE_GROUND:   return "地面";
        case TYPE_FLYING:   return "飞行";
        case TYPE_PSYCHIC:  return "超能力";
        case TYPE_BUG:      return "虫";
        case TYPE_ROCK:     return "岩石";
        case TYPE_GHOST:    return "幽灵";
        case TYPE_DRAGON:   return "龙";
        case TYPE_STEEL:    return "钢";
        case TYPE_DARK:     return "恶";
        case TYPE_FAIRY:    return "妖精";
        default:            return "???";
    }
}

static const char *EnglishTypeName(PokemonType t) {
    switch (t) {
        case TYPE_NORMAL:   return "NORMAL";
        case TYPE_FIRE:     return "FIRE";
        case TYPE_WATER:    return "WATER";
        case TYPE_ELECTRIC: return "ELECTRIC";
        case TYPE_GRASS:    return "GRASS";
        case TYPE_ICE:      return "ICE";
        case TYPE_FIGHTING: return "FIGHTING";
        case TYPE_POISON:   return "POISON";
        case TYPE_GROUND:   return "GROUND";
        case TYPE_FLYING:   return "FLYING";
        case TYPE_PSYCHIC:  return "PSYCHIC";
        case TYPE_BUG:      return "BUG";
        case TYPE_ROCK:     return "ROCK";
        case TYPE_GHOST:    return "GHOST";
        case TYPE_DRAGON:   return "DRAGON";
        case TYPE_STEEL:    return "STEEL";
        case TYPE_DARK:     return "DARK";
        case TYPE_FAIRY:    return "FAIRY";
        default:            return "NONE";
    }
}

/* ---- 读取文件 ---- */

static char *ReadFileText(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, f);
        buf[size] = '\0';
    }
    fclose(f);
    return buf;
}

/* ---- 加载技能库 ---- */

void LoadMoveDB(void) {
    char *jsonStr = ReadFileText("assets/moves.json");
    if (!jsonStr) { printf("[WARN] 找不到 assets/moves.json\n"); return; }

    cJSON *root = cJSON_Parse(jsonStr);
    free(jsonStr);
    if (!root) return;

    moveCount = 0;
    cJSON *item = root->child;
    while (item) { moveCount++; item = item->next; }

    moveDB = (MoveData *)malloc(sizeof(MoveData) * moveCount);
    int idx = 0;
    item = root->child;

    while (item && idx < moveCount) {
        MoveData *md = &moveDB[idx];
        strncpy(md->name, item->string, MAX_MOVE_NAME_LEN - 1);

        cJSON *t  = cJSON_GetObjectItem(item, "type");
        cJSON *p  = cJSON_GetObjectItem(item, "power");
        cJSON *a  = cJSON_GetObjectItem(item, "accuracy");
        cJSON *pp = cJSON_GetObjectItem(item, "pp");

        md->type     = ParseTypeString(t ? t->valuestring : "NORMAL");
        md->power    = p  ? p->valueint  : 0;
        md->accuracy = a  ? a->valueint  : 100;
        md->maxPP    = pp ? pp->valueint : 10;
        idx++;
        item = item->next;
    }
    cJSON_Delete(root);
    printf("[DB] 加载了 %d 个技能\n", moveCount);
}

const MoveData *GetMoveData(const char *name) {
    for (int i = 0; i < moveCount; i++) {
        if (strcmp(moveDB[i].name, name) == 0) return &moveDB[i];
    }
    return NULL;
}

/* ---- 解析属性数组 ["草", "毒"] ---- */

static void ParseTypeArray(cJSON *typeArr, PokemonType *t1, PokemonType *t2) {
    *t1 = TYPE_NONE;
    *t2 = TYPE_NONE;
    if (!typeArr || !cJSON_IsArray(typeArr)) return;

    int size = cJSON_GetArraySize(typeArr);
    if (size >= 1) {
        cJSON *item0 = cJSON_GetArrayItem(typeArr, 0);
        if (item0 && item0->valuestring) *t1 = ParseTypeString(item0->valuestring);
    }
    if (size >= 2) {
        cJSON *item1 = cJSON_GetArrayItem(typeArr, 1);
        if (item1 && item1->valuestring) *t2 = ParseTypeString(item1->valuestring);
    }
}

/* ---- 解析 stats 对象 ---- */

static void ParseStats(cJSON *statsObj, BaseStats *bs, bool *hasReal) {
    memset(bs, 0, sizeof(*bs));
    *hasReal = false;
    if (!statsObj) return;

    cJSON *hp  = cJSON_GetObjectItem(statsObj, "hp");
    cJSON *atk = cJSON_GetObjectItem(statsObj, "attack");
    cJSON *def = cJSON_GetObjectItem(statsObj, "defense");
    cJSON *spa = cJSON_GetObjectItem(statsObj, "sp_attack");
    cJSON *spd = cJSON_GetObjectItem(statsObj, "sp_defense");
    cJSON *spe = cJSON_GetObjectItem(statsObj, "speed");

    if (hp)  { bs->hp        = hp->valueint;  *hasReal = true; }
    if (atk) { bs->attack    = atk->valueint; *hasReal = true; }
    if (def) { bs->defense   = def->valueint; *hasReal = true; }
    if (spa) { bs->sp_attack = spa->valueint; *hasReal = true; }
    if (spd) { bs->sp_defense= spd->valueint; *hasReal = true; }
    if (spe) { bs->speed     = spe->valueint; *hasReal = true; }
}

/* ---- 加载宝可梦种族库 ---- */

void LoadPokemonDB(void) {
    char *jsonStr = ReadFileText("assets/pokemon.json");
    if (!jsonStr) { printf("[WARN] 找不到 assets/pokemon.json\n"); return; }

    cJSON *root = cJSON_Parse(jsonStr);
    free(jsonStr);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return;
    }

    speciesCount = cJSON_GetArraySize(root);
    speciesDB = (SpeciesData *)malloc(sizeof(SpeciesData) * speciesCount);
    memset(speciesDB, 0, sizeof(SpeciesData) * speciesCount);

    for (int i = 0; i < speciesCount; i++) {
        cJSON *entry = cJSON_GetArrayItem(root, i);
        if (!entry) continue;

        SpeciesData *sp = &speciesDB[i];

        cJSON *id   = cJSON_GetObjectItem(entry, "id");
        cJSON *name = cJSON_GetObjectItem(entry, "name");
        cJSON *type = cJSON_GetObjectItem(entry, "type");
        cJSON *stats= cJSON_GetObjectItem(entry, "stats");
        cJSON *mv   = cJSON_GetObjectItem(entry, "moves");

        sp->id = id ? id->valueint : 0;
        if (name) strncpy(sp->name, name->valuestring, MAX_NAME_LEN - 1);

        /* 优先用 "type": ["草","毒"] 格式，兼容旧 "type1"/"type2" */
        if (type && cJSON_IsArray(type)) {
            ParseTypeArray(type, &sp->type1, &sp->type2);
        } else {
            cJSON *t1 = cJSON_GetObjectItem(entry, "type1");
            cJSON *t2 = cJSON_GetObjectItem(entry, "type2");
            sp->type1 = ParseTypeString(t1 ? t1->valuestring : "NORMAL");
            sp->type2 = ParseTypeString(t2 ? t2->valuestring : "NONE");
        }

        ParseStats(stats, &sp->baseStats, &sp->hasRealStats);

        /* 配招 */
        if (mv && cJSON_IsArray(mv)) {
            sp->moveCount = cJSON_GetArraySize(mv);
            if (sp->moveCount > MAX_LEARNSET) sp->moveCount = MAX_LEARNSET;
            for (int j = 0; j < sp->moveCount; j++) {
                cJSON *mname = cJSON_GetArrayItem(mv, j);
                if (mname && mname->valuestring) {
                    strncpy(sp->moveNames[j], mname->valuestring, MAX_MOVE_NAME_LEN - 1);
                }
            }
        }
    }

    cJSON_Delete(root);
    printf("[DB] 加载了 %d 只宝可梦\n", speciesCount);
}

const SpeciesData *GetSpeciesData(int id) {
    for (int i = 0; i < speciesCount; i++) {
        if (speciesDB[i].id == id) return &speciesDB[i];
    }
    return NULL;
}

int GetSpeciesCount(void) {
    return speciesCount;
}

const SpeciesData *GetSpeciesByIndex(int index) {
    if (index < 0 || index >= speciesCount) return NULL;
    return &speciesDB[index];
}

void UnloadPokemonDB(void) {
    free(moveDB);  moveDB = NULL;
    free(speciesDB); speciesDB = NULL;
    moveCount = 0;
    speciesCount = 0;
}

/* ---- 公开: 种族值总和 ---- */

int TotalBaseStats(const SpeciesData *sp) {
    if (!sp) return 0;
    if (sp->hasRealStats) {
        return sp->baseStats.hp + sp->baseStats.attack + sp->baseStats.defense
             + sp->baseStats.sp_attack + sp->baseStats.sp_defense + sp->baseStats.speed;
    }
    return 0;
}

/* ---- 生成 JSON (供管理器保存用) ---- */

cJSON *SpeciesToJSON(const SpeciesData *sp) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "id", sp->id);
    cJSON_AddStringToObject(obj, "name", sp->name);

    /* type 数组 */
    cJSON *typeArr = cJSON_CreateArray();
    cJSON_AddItemToArray(typeArr, cJSON_CreateString(EnglishTypeName(sp->type1)));
    if (sp->type2 != TYPE_NONE) {
        cJSON_AddItemToArray(typeArr, cJSON_CreateString(EnglishTypeName(sp->type2)));
    }
    cJSON_AddItemToObject(obj, "type", typeArr);

    /* stats 对象 */
    cJSON *statsObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(statsObj, "hp",         sp->baseStats.hp);
    cJSON_AddNumberToObject(statsObj, "attack",     sp->baseStats.attack);
    cJSON_AddNumberToObject(statsObj, "defense",    sp->baseStats.defense);
    cJSON_AddNumberToObject(statsObj, "sp_attack",  sp->baseStats.sp_attack);
    cJSON_AddNumberToObject(statsObj, "sp_defense", sp->baseStats.sp_defense);
    cJSON_AddNumberToObject(statsObj, "speed",      sp->baseStats.speed);
    cJSON_AddItemToObject(obj, "stats", statsObj);

    /* moves 数组 */
    cJSON *moveArr = cJSON_CreateArray();
    for (int i = 0; i < sp->moveCount; i++) {
        cJSON_AddItemToArray(moveArr, cJSON_CreateString(sp->moveNames[i]));
    }
    cJSON_AddItemToObject(obj, "moves", moveArr);

    return obj;
}
