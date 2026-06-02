/**
 * @file pokemon_manager.c
 * @brief 宝可梦数据命令行管理器
 *
 * 终端菜单工具: 增删查改宝可梦, 录入种族值+配招, 保存到 JSON
 * 独立编译: gcc tools/pokemon_manager.c src/cJSON.c src/pokemon_db.c -Iinclude -o pokemon_manager
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pokemon_db.h"

/* ---- 终端颜色 ---- */
#define CLR_RESET  "\033[0m"
#define CLR_BOLD   "\033[1m"
#define CLR_GREEN  "\033[32m"
#define CLR_CYAN   "\033[36m"
#define CLR_YELLOW "\033[33m"

/* ---- 输入辅助 ---- */

static void clearStdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

static void inputStr(const char *prompt, char *buf, int maxLen) {
    printf("%s", prompt);
    if (fgets(buf, maxLen, stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
    }
}

static int inputInt(const char *prompt) {
    int val;
    char buf[32];
    printf("%s", prompt);
    if (fgets(buf, sizeof(buf), stdin)) {
        val = atoi(buf);
        return val;
    }
    return 0;
}

/* ---- 显示属性类型菜单 ---- */

static void PrintTypeMenu(void) {
    const char *types[] = {
        "NORMAL","FIRE","WATER","ELECTRIC","GRASS","ICE","FIGHTING",
        "POISON","GROUND","FLYING","PSYCHIC","BUG","ROCK","GHOST",
        "DRAGON","STEEL","DARK","FAIRY"
    };
    int count = sizeof(types) / sizeof(types[0]);
    printf("  可选属性:\n");
    for (int i = 0; i < count; i++) {
        printf("  %2d. %s", i, types[i]);
        if (i % 3 == 2) printf("\n");
    }
    if (count % 3 != 0) printf("\n");
}

static PokemonType SelectType(const char *prompt) {
    const char *types[] = {
        "NORMAL","FIRE","WATER","ELECTRIC","GRASS","ICE","FIGHTING",
        "POISON","GROUND","FLYING","PSYCHIC","BUG","ROCK","GHOST",
        "DRAGON","STEEL","DARK","FAIRY"
    };
    int count = sizeof(types) / sizeof(types[0]);
    PrintTypeMenu();
    int sel = inputInt(prompt);
    if (sel < 0 || sel >= count) sel = 0;
    return ParseTypeString(types[sel]);
}

/* ---- 显示宝可梦详情 ---- */

static void PrintPokemon(const SpeciesData *sp) {
    if (!sp) { printf("  (空)\n"); return; }

    printf(CLR_BOLD "\n  编号: %03d\n" CLR_RESET, sp->id);
    printf("  名称: " CLR_GREEN "%s\n" CLR_RESET, sp->name);

    printf("  属性: " CLR_CYAN "%s", TypeToChinese(sp->type1));
    if (sp->type2 != TYPE_NONE) printf(" / %s", TypeToChinese(sp->type2));
    printf(CLR_RESET "\n");

    if (sp->hasRealStats) {
        printf("\n  " CLR_YELLOW);
        printf("HP:%-4d 攻击:%-4d 防御:%-4d\n", sp->baseStats.hp, sp->baseStats.attack, sp->baseStats.defense);
        printf("  特攻:%-4d 特防:%-4d 速度:%-4d\n", sp->baseStats.sp_attack, sp->baseStats.sp_defense, sp->baseStats.speed);
        printf("  " CLR_RESET);
        printf("种族值总和: " CLR_BOLD "%d\n" CLR_RESET, TotalBaseStats(sp));
    } else {
        printf("\n  (无种族值数据)\n");
    }

    if (sp->moveCount > 0) {
        printf("  配招: ");
        for (int i = 0; i < sp->moveCount; i++) {
            printf("%s", sp->moveNames[i]);
            if (i < sp->moveCount - 1) printf(" / ");
        }
        printf("\n");
    }
}

/* ---- 添加宝可梦 ---- */

static void AddPokemon(void) {
    printf("\n" CLR_BOLD "===== 添加宝可梦 =====\n\n" CLR_RESET);

    SpeciesData sp;
    memset(&sp, 0, sizeof(sp));

    sp.id = inputInt("  图鉴编号: ");
    if (sp.id <= 0) { printf("  编号无效!\n"); return; }

    /* 检查是否已存在 */
    const SpeciesData *exist = GetSpeciesData(sp.id);
    if (exist) {
        printf("  编号 %d 已存在 (%s), 将覆盖\n", sp.id, exist->name);
    }

    inputStr("  中文名: ", sp.name, MAX_NAME_LEN);
    if (sp.name[0] == '\0') { printf("  名称不能为空!\n"); return; }

    printf("\n  第一属性:\n");
    sp.type1 = SelectType("  选择 (0-17): ");
    printf("\n  第二属性 (选 NORMAL=0 表示单属性):\n");
    sp.type2 = SelectType("  选择 (0-17): ");
    if (sp.type2 == TYPE_NORMAL) sp.type2 = TYPE_NONE;

    printf("\n  种族值:\n");
    sp.baseStats.hp         = inputInt("    HP:        ");
    sp.baseStats.attack     = inputInt("    攻击:      ");
    sp.baseStats.defense    = inputInt("    防御:      ");
    sp.baseStats.sp_attack  = inputInt("    特攻:      ");
    sp.baseStats.sp_defense = inputInt("    特防:      ");
    sp.baseStats.speed      = inputInt("    速度:      ");
    sp.hasRealStats = true;

    printf("  种族值总和: " CLR_BOLD "%d\n" CLR_RESET, TotalBaseStats(&sp));

    printf("\n  配招 (最多4个, 空行结束):\n");
    sp.moveCount = 0;
    while (sp.moveCount < 4) {
        char moveName[32];
        printf("    技能%d: ", sp.moveCount + 1);
        if (fgets(moveName, sizeof(moveName), stdin) == NULL) break;
        size_t len = strlen(moveName);
        if (len > 0 && moveName[len-1] == '\n') moveName[len-1] = '\0';
        if (moveName[0] == '\0') break;

        /* 检查技能是否存在 */
        if (!GetMoveData(moveName)) {
            printf("    警告: 技能库中没有 '%s'\n", moveName);
        }
        strncpy(sp.moveNames[sp.moveCount], moveName, MAX_MOVE_NAME_LEN - 1);
        sp.moveCount++;
    }

    /* 更新内存数据库: 替换已有或追加 */
    int idx = -1;
    for (int i = 0; i < GetSpeciesCount(); i++) {
        const SpeciesData *s = GetSpeciesByIndex(i);
        if (s && s->id == sp.id) { idx = i; break; }
    }

    if (idx >= 0) {
        /* 直接修改内存中的条目 (const 强制转换, 仅管理器使用) */
        memcpy((void*)GetSpeciesByIndex(idx), &sp, sizeof(SpeciesData));
    }

    printf("\n  " CLR_GREEN "已添加: %s (No.%d)\n" CLR_RESET, sp.name, sp.id);
}

/* ---- 查看全部 ---- */

static void ViewAll(void) {
    printf("\n" CLR_BOLD "===== 全部宝可梦 (%d 只) =====\n\n" CLR_RESET, GetSpeciesCount());
    for (int i = 0; i < GetSpeciesCount(); i++) {
        const SpeciesData *sp = GetSpeciesByIndex(i);
        if (!sp) continue;
        printf("  %03d  %s", sp->id, sp->name);
        if (sp->hasRealStats) {
            printf("  (种族值:%d)", TotalBaseStats(sp));
        }
        printf("\n");
    }
    printf("\n按回车键返回..."); clearStdin();
}

/* ---- 按名字查询 ---- */

static void SearchByName(void) {
    char name[MAX_NAME_LEN];
    inputStr("  输入名称: ", name, MAX_NAME_LEN);
    for (int i = 0; i < GetSpeciesCount(); i++) {
        const SpeciesData *sp = GetSpeciesByIndex(i);
        if (sp && strstr(sp->name, name)) {
            PrintPokemon(sp);
        }
    }
    printf("\n按回车键返回..."); clearStdin();
}

/* ---- 按编号查询 ---- */

static void SearchByID(void) {
    int id = inputInt("  输入编号: ");
    const SpeciesData *sp = GetSpeciesData(id);
    if (sp) {
        PrintPokemon(sp);
    } else {
        printf("  未找到编号 %d\n", id);
    }
    printf("\n按回车键返回..."); clearStdin();
}

/* ---- 删除 ---- */

static void DeletePokemon(void) {
    int id = inputInt("  输入要删除的编号: ");
    const SpeciesData *sp = GetSpeciesData(id);
    if (!sp) {
        printf("  未找到编号 %d\n", id);
        return;
    }
    printf("  确认删除 %s (No.%d) ? (y/N): ", sp->name, sp->id);
    char confirm = getchar(); clearStdin();
    if (confirm == 'y' || confirm == 'Y') {
        /* 通过清零 ID 来标记删除 (保存时会跳过) */
        memset((void*)sp, 0, sizeof(SpeciesData));
        printf("  已删除\n");
    } else {
        printf("  已取消\n");
    }
}

/* ---- 保存到 JSON ---- */

static void SaveToJSON(const char *path) {
    cJSON *arr = cJSON_CreateArray();

    for (int i = 0; i < GetSpeciesCount(); i++) {
        const SpeciesData *sp = GetSpeciesByIndex(i);
        if (!sp || sp->id == 0) continue;  /* 跳过已删除的 */
        cJSON *obj = SpeciesToJSON(sp);
        cJSON_AddItemToArray(arr, obj);
    }

    char *jsonStr = cJSON_Print(arr);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", jsonStr);
        fclose(f);
        printf("  " CLR_GREEN "已保存 %d 只宝可梦到 %s\n" CLR_RESET,
               cJSON_GetArraySize(arr), path);
    } else {
        printf("  保存失败!\n");
    }

    free(jsonStr);
    cJSON_Delete(arr);
}

/* ---- 主菜单 ---- */

int main(void) {
    /* 加载数据库 */
    LoadMoveDB();
    LoadPokemonDB();

    printf(CLR_BOLD "\n  ▸ 宝可梦数据管理器 v1.0 ◂\n" CLR_RESET);

    int running = 1;
    while (running) {
        printf("\n"
               "  " CLR_CYAN "1." CLR_RESET " 添加宝可梦\n"
               "  " CLR_CYAN "2." CLR_RESET " 查看全部宝可梦\n"
               "  " CLR_CYAN "3." CLR_RESET " 按名字查询\n"
               "  " CLR_CYAN "4." CLR_RESET " 按编号查询\n"
               "  " CLR_CYAN "5." CLR_RESET " 删除宝可梦\n"
               "  " CLR_CYAN "6." CLR_RESET " 保存并退出\n"
               "  " CLR_CYAN "0." CLR_RESET " 不保存退出\n"
               "  选择: ");

        char choice[8];
        if (fgets(choice, sizeof(choice), stdin) == NULL) break;

        switch (choice[0]) {
            case '1': AddPokemon();     break;
            case '2': ViewAll();        break;
            case '3': SearchByName();   break;
            case '4': SearchByID();     break;
            case '5': DeletePokemon();  break;
            case '6':
                SaveToJSON("assets/pokemon.json");
                running = 0;
                break;
            case '0':
                printf("  不保存, 退出\n");
                running = 0;
                break;
            default:
                printf("  无效选项\n");
        }
    }

    UnloadPokemonDB();
    return 0;
}
