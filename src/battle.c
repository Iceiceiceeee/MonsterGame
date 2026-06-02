/**
 * @file battle.c
 * @brief 宝可梦火红 —— 对战界面复刻
 *
 * 基于 raylib 实现 960×640 回合制对战界面。
 * 包含:
 *   - 全屏战斗背景 + 双层椭圆站台 (远近透视)
 *   - 对手/己方宝可梦精灵渲染
 *   - 对手信息面板 (左上) + 己方信息面板 (右侧)
 *   - 2×2 指令菜单 (战斗/道具/精灵/逃跑) + 技能选择子菜单
 *   - 底部对话框 (模仿 GBA 深蓝底白字)
 *   - 帧计时器驱动的攻击动画序列
 *
 * 键盘操作:
 *   Z/空格/回车 = 确认
 *   X/Backspace  = 返回
 *   ↑↓←→ / WASD = 导航
 */

#include "battle.h"
#include "pokemon_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ======================== 屏幕与布局常量 ======================== */

#define SCREEN_W         960   /**< 窗口宽度 (GBA 240×4) */
#define SCREEN_H         640   /**< 窗口高度 (GBA 160×4) */

/* ---- 对手信息面板 (左上角) ---- */
#define ENEMY_INFO_X     24    /**< 面板左边界 X */
#define ENEMY_INFO_Y     18    /**< 面板上边界 Y */
#define ENEMY_INFO_W     306   /**< 面板宽度 */
#define ENEMY_INFO_H     78    /**< 面板高度 */

/* ---- 己方信息面板 (右下区域, 紧贴底部 UI) ---- */
#define PLAYER_INFO_X    612   /**< 面板左边界 X */
#define PLAYER_INFO_Y    390   /**< 面板上边界 Y (底部=498, 紧贴 UI@500) */
#define PLAYER_INFO_W    330   /**< 面板宽度 */
#define PLAYER_INFO_H    108   /**< 面板高度 */

/* ---- 对手宝可梦精灵位置 (右上, 较小) ---- */
#define ENEMY_POKEMON_X  600   /**< 精灵锚点 X (左移40) */
#define ENEMY_POKEMON_Y  160  /**< 精灵锚点 Y (再上移10) */
#define ENEMY_POKEMON_W  192   /**< 精灵渲染宽度 (64×3) */
#define ENEMY_POKEMON_H  192   /**< 精灵渲染高度 (64×3) */

/* ---- 己方宝可梦精灵位置 (左下, 较大) ---- */
#define PLAYER_POKEMON_X  60   /**< 精灵锚点 X */
#define PLAYER_POKEMON_Y  310  /**< 精灵锚点 Y (下移60) */
#define PLAYER_POKEMON_W  224  /**< 精灵渲染宽度 (64×3.5) */
#define PLAYER_POKEMON_H  224  /**< 精灵渲染高度 (64×3.5) */

/* ---- 对手站台位置 (远处, 跟随精灵移动) ---- */
/* 对手精灵: X=610 W=192 → 中心X=706, Y=130 H=192 → 底部Y=322 */
#define ENEMY_PLATFORM_X  610  /**< 站台左边界 X (706 - 90) */
#define ENEMY_PLATFORM_Y  290  /**< 站台上边界 Y (上移10) */
#define ENEMY_PLATFORM_W  180  /**< 站台宽度 */
#define ENEMY_PLATFORM_H  36   /**< 站台高度 */

/* ---- 己方站台位置 (近处, 跟随精灵移动) ---- */
/* 己方精灵: X=60 W=224 → 中心X=172, Y=332 H=224 → 底部Y=556 */
#define PLAYER_PLATFORM_X  47   /**< 站台左边界 X (172 - 125) */
#define PLAYER_PLATFORM_Y  536  /**< 站台上边界 Y (556 - 20) */
#define PLAYER_PLATFORM_W  250  /**< 站台宽度 */
#define PLAYER_PLATFORM_H  50   /**< 站台高度 */

/* ---- 底部对话框 ---- */
#define TEXTBOX_X         0    /**< 对话框左边界 X */
#define TEXTBOX_Y         500  /**< 对话框上边界 Y */
#define TEXTBOX_W         600  /**< 对话框宽度 */
#define TEXTBOX_H         140  /**< 对话框高度 */

/* ---- 指令菜单框 (右下) ---- */
#define COMMAND_BOX_X     612  /**< 菜单左边界 X */
#define COMMAND_BOX_Y     500  /**< 菜单上边界 Y */
#define COMMAND_BOX_W     348  /**< 菜单宽度 */
#define COMMAND_BOX_H     140  /**< 菜单高度 */

/* ---- 技能选择框 (同位置) ---- */
#define FIGHT_BOX_X       612
#define FIGHT_BOX_Y       500
#define FIGHT_BOX_W       348
#define FIGHT_BOX_H       140

/* ======================== HP 条颜色 ======================== */

/** HP > 50%: 绿色 */
static const Color HP_GREEN  = { 80,  200, 72, 255 };
/** HP 20%~50%: 黄色 */
static const Color HP_YELLOW = { 248, 176, 32, 255 };
/** HP < 20%: 红色 */
static const Color HP_RED    = { 240, 56,  32, 255 };

/* ======================== UI 配色 (GBA 火红风格) ======================== */

static const Color CLR_BEIGE     = { 248, 240, 208, 255 };  /**< 米色面板填充 */
static const Color CLR_DARKBROWN = { 72,  64,  48,  255 };  /**< 深棕边框 */
static const Color CLR_DARKBLUE  = { 40,  56,  104, 255 };  /**< 对话框深蓝底色 */
static const Color CLR_LIGHTGRAY = { 220, 220, 220, 255 };  /**< 菜单浅灰底色 */
static const Color CLR_SHADOW    = { 0,   0,   0,   80  };  /**< 半透明阴影 */
static const Color CLR_TEXTBLACK = { 40,  40,  40,  255 };  /**< 文字黑色 */
static const Color CLR_HPBG      = { 120, 112, 96,  255 };  /**< HP 条背景灰褐 */
static const Color CLR_EXPBG     = { 180, 176, 160, 255 };  /**< EXP 条背景浅灰 */
static const Color CLR_EXPBLUE   = { 64,  144, 208, 255 };  /**< EXP 条填充蓝 */

/* ======================== 内部数据结构 ======================== */

/**
 * @brief 战斗内部使用的宝可梦数据 (轻量, 不依赖 pokemon.h)
 * 与 pokemon.h 中的 Pokemon 结构体独立, 避免头文件循环依赖
 */
typedef struct {
    char name[32];          /**< 名称 */
    int  level;             /**< 等级 */
    int  hp_current;        /**< 当前 HP */
    int  hp_max;            /**< 最大 HP */
    int  exp_current;       /**< 当前经验值 */
    int  exp_max;           /**< 升级所需经验值 */
    char moves[4][24];      /**< 4 个技能名称 */
    int  move_pp[4];        /**< 技能当前 PP */
    int  move_pp_max[4];    /**< 技能最大 PP */
    int  move_count;        /**< 实际技能数量 */
} LocalPokemon;

/**
 * @brief 战斗子状态 (回合内细分)
 * STATE_INTRO   - 开场 "野生的 XXX 出现了!"
 * STATE_COMMAND - 主菜单选择
 * STATE_FIGHT   - 技能选择
 * STATE_EXECUTE - 动画执行中
 * STATE_MESSAGE - 消息暂停等待按键
 */
typedef enum {
    STATE_INTRO,
    STATE_COMMAND,
    STATE_FIGHT,
    STATE_EXECUTE,
    STATE_MESSAGE,
} BattleSubState;

/* ======================== 全局变量 ======================== */

/* ---- 纹理资源 ---- */
static Texture2D texBackground;      /**< 战斗背景贴图 */
static Texture2D texBattleUI;        /**< 战斗 UI 全屏覆盖层 (battle_ui.png) */
static Texture2D texEnemyPokemon;    /**< 对手宝可梦贴图 (正面) */
static Texture2D texPlayerPokemon;   /**< 己方宝可梦贴图 (背面) */
static Texture2D texEnemyPlatform;   /**< 对手站台贴图 */
static Texture2D texPlayerPlatform;  /**< 己方站台贴图 */
static Texture2D texEnemyInfoBar;    /**< 对手信息栏背景贴图 */
static Texture2D texPlayerInfoBar;   /**< 己方信息栏背景贴图 */

/* ---- 字体 ---- */
static Font      fontBattle;  /**< 中文字体 (由 game.c 传入) */
static bool      hasFont;     /**< 是否有有效字体 */

/* ---- 对战数据 ---- */
static LocalPokemon playerPoke;  /**< 己方宝可梦 */
static LocalPokemon enemyPoke;   /**< 对手宝可梦 */

/* ---- 状态管理 ---- */
static BattleSubState subState = STATE_INTRO;  /**< 当前子状态 */
static int  cursorPos = 0;       /**< 当前光标位置 (0~3) */
static int  animTimer = 0;       /**< 动画计时器 (帧数) */
static int  animPhase = 0;       /**< 动画子阶段 (0~3) */
static int  storedDamage = 0;    /**< 暂存伤害值 */
static char messageText[256] = "";  /**< 对话框文本 */
static bool battleFinished = false; /**< 战斗是否结束 */

/* ======================== 内部函数声明 ======================== */

static Color GetHPColor(int current, int max);
static void DrawPanelShadow(Rectangle r, Color fill, Color border, float t, int off);
static void LoadBattleResources(void);
static void UnloadBattleResources(void);
static void InitBattleData(void);
static void UpdateBattleLogic(void);
static void DrawBackground(void);
static void DrawPlatforms(void);
static void DrawPokemonSprites(void);
static void DrawEnemyInfoPanel(void);
static void DrawPlayerInfoPanel(void);
static void DrawHPBar(int x, int y, int w, int h, int current, int max);
static void DrawEXPBar(int x, int y, int w, int h, int current, int max);
static void DrawCommandBox(void);
static void DrawFightBox(void);
static void DrawMessageBox(void);

/* ======================== 辅助函数 ======================== */

/**
 * @brief 根据 HP 比例返回对应颜色
 * > 50% → 绿, 20%~50% → 黄, < 20% → 红
 */
static Color GetHPColor(int current, int max) {
    float ratio = (float)current / (float)max;
    if (ratio > 0.5f) return HP_GREEN;
    if (ratio > 0.2f) return HP_YELLOW;
    return HP_RED;
}

/**
 * @brief 绘制带右下阴影的矩形面板
 * 先画偏移的黑色半透明阴影, 再覆盖面板本体+边框
 * 整个对战的 HP/EXP/菜单/信息框都使用这个统一风格
 */
static void DrawPanelShadow(Rectangle r, Color fill, Color border, float t, int off) {
    Rectangle s = { r.x + off, r.y + off, r.width, r.height };
    DrawRectangleRec(s, CLR_SHADOW);
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, t, border);
}

/**
 * @brief 安全加载精灵贴图 (支持 4-bit colormap PNG)
 *
 * 通过 LoadImage + ImageResize 方式加载,
 * 解决部分 GBA rip 素材的色板格式兼容问题.
 * 统一放大到 3 倍并设置点采样保持像素风格.
 *
 * @param tex  输出纹理指针
 * @param path 图片文件路径
 * @param scale 放大倍率 (相对于 64×64 原图)
 */
static void LoadSprite(Texture2D *tex, const char *path, int scale) {
    Image img = LoadImage(path);
    if (img.data != NULL) {
        int newW = img.width * scale;
        int newH = img.height * scale;
        ImageResize(&img, newW, newH);
        *tex = LoadTextureFromImage(img);
        SetTextureFilter(*tex, TEXTURE_FILTER_POINT);
        UnloadImage(img);
    }
}

/* ======================== 资源加载与释放 ======================== */

/**
 * @brief 加载所有对战纹理资源
 * 路径指向 assets/images/ 下的 front/ back/ pokefirered_pngs/ 目录
 * 每个贴图加载后设置为点采样 (TEXTURE_FILTER_POINT) 保持像素风格
 */
static void LoadBattleResources(void) {
    /* 战斗背景: battle grass.png (全屏拉伸) */
    if (FileExists("assets/images/battle grass.png")) {
        texBackground = LoadTexture("assets/images/battle grass.png");
        SetTextureFilter(texBackground, TEXTURE_FILTER_POINT);
    }

    /* 精灵: 通过 Image 方式加载以兼容 4-bit colormap PNG */
    /* 对手 — 正面精灵 (3x 放大 = 192×192) */
    if (FileExists("assets/images/front/front_250.png")) {
        LoadSprite(&texEnemyPokemon, "assets/images/front/front_250.png", 3);
    }
    /* 己方 — 背面精灵 (3.5x 放大 = 224×224) */
    if (FileExists("assets/images/back/back_248.png")) {
        LoadSprite(&texPlayerPokemon, "assets/images/back/back_248.png", 3);
    }

    /* 站台贴图 —— 白底抠图, 灰度图转带 Alpha 的阴影 */
    if (FileExists("assets/images/pokefirered_pngs/blit_wide_ellipse.png")) {
        Image platformImg = LoadImage("assets/images/pokefirered_pngs/blit_wide_ellipse.png");
        if (platformImg.data != NULL) {
            ImageFormat(&platformImg, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            Color *pixels = LoadImageColors(platformImg);
            for (int i = 0; i < platformImg.width * platformImg.height; i++) {
                /* 灰度图: R=G=B=亮度值, 越亮(接近白色)越透明 */
                unsigned char brightness = pixels[i].r;
                if (brightness > 200) {
                    pixels[i].a = 0;                   /* 白色/浅色背景 → 全透明 */
                } else {
                    pixels[i].a = 255 - brightness;    /* 暗色区域 → 阴影半透明 */
                    pixels[i].r = 0;
                    pixels[i].g = 0;
                    pixels[i].b = 0;
                }
            }
            Image newImg = {
                .data = pixels, .width = platformImg.width, .height = platformImg.height,
                .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, .mipmaps = 1
            };
            texEnemyPlatform = LoadTextureFromImage(newImg);
            texPlayerPlatform = LoadTextureFromImage(newImg);
            SetTextureFilter(texEnemyPlatform, TEXTURE_FILTER_POINT);
            SetTextureFilter(texPlayerPlatform, TEXTURE_FILTER_POINT);
            RL_FREE(pixels);
            UnloadImage(platformImg);
        }
    }

    /* 战斗 UI 全屏覆盖层 (battle_ui.png, 1536×1024 → 拉伸到 960×640) */
    if (FileExists("assets/images/battle_ui.png")) {
        texBattleUI = LoadTexture("assets/images/battle_ui.png");
        SetTextureFilter(texBattleUI, TEXTURE_FILTER_POINT);
    }

    /* 信息栏背景 (不存在则 fallback 到代码绘制) */
    if (FileExists("res/enemy_info.png")) {
        texEnemyInfoBar = LoadTexture("res/enemy_info.png");
        SetTextureFilter(texEnemyInfoBar, TEXTURE_FILTER_POINT);
    }
    if (FileExists("res/player_info.png")) {
        texPlayerInfoBar = LoadTexture("res/player_info.png");
        SetTextureFilter(texPlayerInfoBar, TEXTURE_FILTER_POINT);
    }

    /* 字体由 game.c 传入, 此处仅标记 */
    hasFont = false;
}

/**
 * @brief 释放所有纹理资源
 * 逐一检查 id > 0 再释放, 避免重复释放或释放空纹理
 */
static void UnloadBattleResources(void) {
    if (texBackground.id > 0)      UnloadTexture(texBackground);
    if (texBattleUI.id > 0)        UnloadTexture(texBattleUI);
    if (texEnemyPokemon.id > 0)    UnloadTexture(texEnemyPokemon);
    if (texPlayerPokemon.id > 0)   UnloadTexture(texPlayerPokemon);
    if (texEnemyPlatform.id > 0)   UnloadTexture(texEnemyPlatform);
    if (texPlayerPlatform.id > 0)  UnloadTexture(texPlayerPlatform);
    if (texEnemyInfoBar.id > 0)    UnloadTexture(texEnemyInfoBar);
    if (texPlayerInfoBar.id > 0)   UnloadTexture(texPlayerInfoBar);
    if (hasFont)                   UnloadFont(fontBattle);
}

/* ======================== 宝可梦数据初始化 ======================== */

/**
 * @brief 从 JSON 数据库初始化双方宝可梦
 *
 * 读取 pokemon.json 真实种族值 → 计算 Lv 对应能力值,
 * 从 moves.json 查技能详情, 自动加载精灵贴图.
 */
static int CalcStat(int base, int level, int isHP) {
    if (isHP) return ((base * 2) * level / 100) + level + 10;
    return ((base * 2) * level / 100) + 5;
}

static void InitPokemonFromDB(LocalPokemon *p, int speciesId, int level) {
    memset(p, 0, sizeof(*p));
    const SpeciesData *sp = GetSpeciesData(speciesId);
    if (!sp) return;

    strcpy(p->name, sp->name);
    p->level = level;
    p->move_count = 0;

    if (sp->hasRealStats) {
        p->hp_max     = CalcStat(sp->baseStats.hp,         level, 1);
        p->hp_current = p->hp_max;
    } else {
        p->hp_max     = 45 + (speciesId % 20) * 3 + level * 2;
        p->hp_current = p->hp_max;
    }
    p->exp_current = 0;
    p->exp_max = level * level * level;

    for (int i = 0; i < sp->moveCount && p->move_count < 4; i++) {
        const MoveData *md = GetMoveData(sp->moveNames[i]);
        if (md) {
            strcpy(p->moves[p->move_count], md->name);
            p->move_pp[p->move_count] = md->maxPP;
            p->move_pp_max[p->move_count] = md->maxPP;
            p->move_count++;
        }
    }
}

static void InitBattleData(void) {
    /* 己方: 喷火龙 (#6) Lv36 */
    InitPokemonFromDB(&playerPoke, 6, 36);

    /* 对手: 杰尼龟 (#7) Lv10 */
    InitPokemonFromDB(&enemyPoke, 7, 10);

    snprintf(messageText, sizeof(messageText), "野生的 %s 出现了!", enemyPoke.name);
    subState = STATE_INTRO;
    cursorPos = 0;
    battleFinished = false;
}

/* ======================== 逻辑更新 ======================== */

/**
 * @brief 战斗状态机主循环 (每帧调用一次)
 *
 * 状态流转:
 *   STATE_INTRO → STATE_COMMAND → STATE_FIGHT → STATE_EXECUTE
 *                     ↑                              |
 *                     |         STATE_MESSAGE ←───────┘
 *                     ↓
 *               (逃跑/战败/胜利)
 *
 * 键盘映射:
 *   Z / Space / Enter = 确认
 *   X / Backspace      = 返回
 *   Arrow keys / WASD  = 导航
 */
static void UpdateBattleLogic(void) {
    switch (subState) {

        /* ===== 开场: 等待玩家按键后进入主菜单 ===== */
        case STATE_INTRO:
            if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                subState = STATE_COMMAND;
                cursorPos = 0;
                snprintf(messageText, sizeof(messageText), "%s 要做什么?", playerPoke.name);
            }
            break;

        /* ===== 主菜单: 2×2 网格导航 ===== */
        case STATE_COMMAND:
            /* 左右切换: 奇偶列变换 */
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                if (cursorPos % 2 == 0) cursorPos += 1;
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                if (cursorPos % 2 == 1) cursorPos -= 1;
            }
            /* 上下切换: 跳 2 格 */
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                if (cursorPos < 2) cursorPos += 2;
            }
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                if (cursorPos >= 2) cursorPos -= 2;
            }

            if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
                switch (cursorPos) {
                    case 0: /* 战斗 → 技能选择 */
                        subState = STATE_FIGHT;
                        cursorPos = 0;
                        break;
                    case 1: /* 背包 → 暂未实现 */
                        subState = STATE_MESSAGE;
                        strcpy(messageText, "背包里空空如也!");
                        break;
                    case 2: /* 宝可梦 → 暂未实现 */
                        subState = STATE_MESSAGE;
                        strcpy(messageText, "没有其他宝可梦了!");
                        break;
                    case 3: /* 逃跑 → 退出战斗 */
                        strcpy(messageText, "逃跑成功了!");
                        battleFinished = true;
                        break;
                }
            }
            break;

        /* ===== 技能选择: 2×2 网格, 类似主菜单 ===== */
        case STATE_FIGHT:
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                if (cursorPos % 2 == 0) cursorPos += 1;
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                if (cursorPos % 2 == 1) cursorPos -= 1;
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                if (cursorPos < 2) cursorPos += 2;
            }
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                if (cursorPos >= 2) cursorPos -= 2;
            }

            if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
                /* 防止越界 */
                if (cursorPos >= playerPoke.move_count) break;

                /* PP 不足 → 提示 */
                if (playerPoke.move_pp[cursorPos] <= 0) {
                    subState = STATE_MESSAGE;
                    strcpy(messageText, "技能点数不足!");
                    break;
                }

                /* 扣减 PP, 记录招式文本和模拟伤害 */
                playerPoke.move_pp[cursorPos]--;
                snprintf(messageText, sizeof(messageText), "%s 使用了 %s!",
                         playerPoke.name, playerPoke.moves[cursorPos]);
                int baseDmg[] = { 30, 25, 40, 30 };
                storedDamage = baseDmg[cursorPos];
                subState = STATE_EXECUTE;
                animPhase = 0;
                animTimer = 0;
            }

            /* 返回主菜单 */
            if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) {
                subState = STATE_COMMAND;
                cursorPos = 0;
            }
            break;

        /* ===== 执行阶段: 帧计时器驱动的攻击动画 ===== */
        case STATE_EXECUTE:
            animTimer++;
            if (animPhase == 0) {
                /* phase 0: 显示招式文本 40 帧后扣血 */
                if (animTimer > 40) {
                    enemyPoke.hp_current -= storedDamage;
                    if (enemyPoke.hp_current < 0) enemyPoke.hp_current = 0;
                    animPhase = 1;
                    animTimer = 0;
                }
            } else if (animPhase == 1) {
                /* phase 1: HP 条减少动画 30 帧, 检查对手是否倒下 */
                if (animTimer > 30) {
                    if (enemyPoke.hp_current <= 0) {
                        snprintf(messageText, sizeof(messageText), "野生的 %s 倒下了!", enemyPoke.name);
                        subState = STATE_MESSAGE;
                        battleFinished = true;
                    } else {
                        /* 对手存活 → 对手反击 */
                        animPhase = 2;
                        animTimer = 0;
                        int moveIdx = 0;
                        for (int i = 0; i < enemyPoke.move_count; i++) {
                            if (enemyPoke.move_pp[i] > 0) { moveIdx = i; break; }
                        }
                        snprintf(messageText, sizeof(messageText), "野生的 %s 使用了 %s!",
                                 enemyPoke.name, enemyPoke.moves[moveIdx]);
                    }
                }
            } else if (animPhase == 2) {
                /* phase 2: 显示对手招式文本 40 帧后扣血 */
                if (animTimer > 40) {
                    int enemyDmg = 35;  /* 对手固定伤害 */
                    playerPoke.hp_current -= enemyDmg;
                    if (playerPoke.hp_current < 0) playerPoke.hp_current = 0;
                    animPhase = 3;
                    animTimer = 0;
                }
            } else if (animPhase == 3) {
                /* phase 3: 对手伤害动画 30 帧, 检查己方是否倒下 */
                if (animTimer > 30) {
                    if (playerPoke.hp_current <= 0) {
                        snprintf(messageText, sizeof(messageText), "%s 倒下了!", playerPoke.name);
                        battleFinished = true;
                        subState = STATE_MESSAGE;
                    } else {
                        /* 双方存活 → 回到主菜单, 开始下一回合 */
                        subState = STATE_COMMAND;
                        cursorPos = 0;
                        snprintf(messageText, sizeof(messageText), "%s 要做什么?", playerPoke.name);
                    }
                }
            }
            break;

        /* ===== 消息暂停: 等待按键后返回 ===== */
        case STATE_MESSAGE:
            if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (battleFinished) break;  /* 战斗已结束, 不再回到菜单 */
                subState = STATE_COMMAND;
                cursorPos = 0;
                snprintf(messageText, sizeof(messageText), "%s 要做什么?", playerPoke.name);
            }
            break;
    }
}

/* ======================== 绘制函数 ======================== */

/**
 * @brief 绘制战斗背景 (全屏拉伸)
 *
 * 有贴图时: 将背景贴图拉伸到 960×640 覆盖整个窗口
 * 无贴图时: 代码绘制上半天空 + 下半草地的简单场景
 */
static void DrawBackground(void) {
    if (texBackground.id > 0) {
        /* 全屏拉伸 (0, 0) → (960, 640) */
        DrawTexturePro(texBackground,
            (Rectangle){ 0, 0, (float)texBackground.width, (float)texBackground.height },
            (Rectangle){ 0, 0, SCREEN_W, SCREEN_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        /* Fallback: 上半天空蓝 + 下半草地绿 */
        int halfH = SCREEN_H / 2;
        DrawRectangle(0, 0,      SCREEN_W, halfH,     (Color){ 136, 200, 248, 255 });
        DrawRectangle(0, halfH,  SCREEN_W, SCREEN_H - halfH, (Color){ 120, 200, 80, 255 });
        DrawRectangle(0, halfH - 3, SCREEN_W, 6,       (Color){ 88, 160, 60, 255 });
    }
}

/**
 * @brief 绘制两个站台 (远近分层, 体现纵深)
 *
 * 对手站台: 离玩家远, 较小, 偏上半屏
 * 己方站台: 离玩家近, 较大, 偏下半屏
 * 半透明渲染让站台与背景融合
 */
static void DrawPlatforms(void) {
    /* 对手站台 — 远处, 较小 (纹理已含 Alpha 通道, WHITE tint 直接透出) */
    if (texEnemyPlatform.id > 0) {
        DrawTexturePro(texEnemyPlatform,
            (Rectangle){ 0, 0, (float)texEnemyPlatform.width, (float)texEnemyPlatform.height },
            (Rectangle){ ENEMY_PLATFORM_X, ENEMY_PLATFORM_Y, ENEMY_PLATFORM_W, ENEMY_PLATFORM_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        DrawEllipse(ENEMY_PLATFORM_X + ENEMY_PLATFORM_W / 2,
                    ENEMY_PLATFORM_Y + ENEMY_PLATFORM_H / 2,
                    ENEMY_PLATFORM_W / 2, ENEMY_PLATFORM_H / 2,
                    (Color){ 96, 176, 72, 255 });
    }

    /* 己方站台 — 近处, 较大 (纹理已含 Alpha 通道) */
    if (texPlayerPlatform.id > 0) {
        DrawTexturePro(texPlayerPlatform,
            (Rectangle){ 0, 0, (float)texPlayerPlatform.width, (float)texPlayerPlatform.height },
            (Rectangle){ PLAYER_PLATFORM_X, PLAYER_PLATFORM_Y, PLAYER_PLATFORM_W, PLAYER_PLATFORM_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        DrawEllipse(PLAYER_PLATFORM_X + PLAYER_PLATFORM_W / 2,
                    PLAYER_PLATFORM_Y + PLAYER_PLATFORM_H / 2,
                    PLAYER_PLATFORM_W / 2, PLAYER_PLATFORM_H / 2,
                    (Color){ 96, 176, 72, 255 });
    }
}

/**
 * @brief 绘制双方宝可梦精灵
 *
 * 对手: front_250.png 正面 (192×192)
 * 己方: back_248.png  背面 (192×192)
 *
 * 精灵贴图在加载时已通过 LoadSprite 放大,
 * 这里用 DrawTexturePro 缩放到布局定义的目标尺寸.
 * 无贴图时显示灰色占位矩形 + 文字提示.
 */
static void DrawPokemonSprites(void) {
    /* 对手 — 正面, 右上方, 较小 */
    if (texEnemyPokemon.id > 0) {
        DrawTexturePro(texEnemyPokemon,
            (Rectangle){ 0, 0, (float)texEnemyPokemon.width, (float)texEnemyPokemon.height },
            (Rectangle){ ENEMY_POKEMON_X, ENEMY_POKEMON_Y, ENEMY_POKEMON_W, ENEMY_POKEMON_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        /* 无贴图时显示灰色占位框 */
        DrawRectangle(ENEMY_POKEMON_X, ENEMY_POKEMON_Y,
                      ENEMY_POKEMON_W, ENEMY_POKEMON_H, (Color){ 200, 200, 200, 180 });
        if (hasFont) {
            DrawTextEx(fontBattle, "对手精灵",
                       (Vector2){ ENEMY_POKEMON_X + 40, ENEMY_POKEMON_Y + 80 }, 20, 1, DARKGRAY);
        }
    }

    /* 己方 — 背面, 左下方, 较大 */
    if (texPlayerPokemon.id > 0) {
        DrawTexturePro(texPlayerPokemon,
            (Rectangle){ 0, 0, (float)texPlayerPokemon.width, (float)texPlayerPokemon.height },
            (Rectangle){ PLAYER_POKEMON_X, PLAYER_POKEMON_Y, PLAYER_POKEMON_W, PLAYER_POKEMON_H },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        DrawRectangle(PLAYER_POKEMON_X, PLAYER_POKEMON_Y,
                      PLAYER_POKEMON_W, PLAYER_POKEMON_H, (Color){ 200, 200, 200, 180 });
        if (hasFont) {
            DrawTextEx(fontBattle, "己方精灵",
                       (Vector2){ PLAYER_POKEMON_X + 50, PLAYER_POKEMON_Y + 95 }, 20, 1, DARKGRAY);
        }
    }
}

/**
 * @brief 绘制 HP 条
 *
 * 灰褐底色 + 按比例填充的前景色 (绿/黄/红) + 深灰细边
 * 左右各留 1px 内边距让填充区域不贴边框
 */
static void DrawHPBar(int x, int y, int w, int h, int current, int max) {
    float frac = (float)current / (float)max;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;

    Rectangle bg   = { x, y, w, h };
    Rectangle fill = { x + 1, y + 1, (w - 2) * frac, h - 2 };

    DrawRectangleRec(bg, CLR_HPBG);
    DrawRectangleRec(fill, GetHPColor(current, max));
    DrawRectangleLinesEx(bg, 1, (Color){ 64, 64, 64, 255 });
}

/**
 * @brief 绘制 EXP 经验条
 *
 * 浅灰底色 + 蓝色填充 + 深灰细边
 * 结构同 HP 条, 但颜色不同
 */
static void DrawEXPBar(int x, int y, int w, int h, int current, int max) {
    float frac = (max > 0) ? (float)current / (float)max : 0.0f;
    if (frac > 1.0f) frac = 1.0f;

    Rectangle bg   = { x, y, w, h };
    Rectangle fill = { x + 1, y + 1, (w - 2) * frac, h - 2 };

    DrawRectangleRec(bg, CLR_EXPBG);
    DrawRectangleRec(fill, CLR_EXPBLUE);
    DrawRectangleLinesEx(bg, 1, (Color){ 64, 64, 64, 255 });
}

/**
 * @brief 绘制对手信息面板 (左上角)
 *
 * 布局:
 *   ┌──────────────────────────┐
 *   │ 凤王  Lv45              │  ← 名字 + 等级
 *   │ HP  [████████████████]  │  ← HP 标签 + HP 条
 *   └──────────────────────────┘
 *
 * 优先使用 texEnemyInfoBar 贴图, 不存在则用代码绘制米色面板
 */
static void DrawEnemyInfoPanel(void) {
    Rectangle panelRect = { ENEMY_INFO_X, ENEMY_INFO_Y, ENEMY_INFO_W, ENEMY_INFO_H };

    if (texEnemyInfoBar.id > 0) {
        DrawTexturePro(texEnemyInfoBar,
            (Rectangle){ 0, 0, (float)texEnemyInfoBar.width, (float)texEnemyInfoBar.height },
            panelRect, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        /* 米色面板 + 深棕边框 + 阴影 */
        DrawPanelShadow(panelRect, CLR_BEIGE, CLR_DARKBROWN, 2.5f, 3);
    }

    /* 名字 + 等级 */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  Lv%d", enemyPoke.name, enemyPoke.level);
    if (hasFont) {
        DrawTextEx(fontBattle, buf,
                   (Vector2){ ENEMY_INFO_X + 12, ENEMY_INFO_Y + 8 }, 22, 1, CLR_TEXTBLACK);
        DrawTextEx(fontBattle, "HP",
                   (Vector2){ ENEMY_INFO_X + 12, ENEMY_INFO_Y + 38 }, 16, 1, CLR_TEXTBLACK);
    } else {
        DrawText(buf, ENEMY_INFO_X + 12, ENEMY_INFO_Y + 8, 22, CLR_TEXTBLACK);
        DrawText("HP", ENEMY_INFO_X + 12, ENEMY_INFO_Y + 38, 16, CLR_TEXTBLACK);
    }

    /* HP 条 */
    DrawHPBar(ENEMY_INFO_X + 44, ENEMY_INFO_Y + 40, 200, 14,
              enemyPoke.hp_current, enemyPoke.hp_max);
}

/**
 * @brief 绘制己方信息面板 (右侧中下区域)
 *
 * 布局:
 *   ┌────────────────────────────────┐
 *   │ 班基拉斯  Lv50    175 / 175   │  ← 名字+等级 (左) + HP数值 (右)
 *   │ HP  [████████████████████████] │  ← HP 条
 *   │ EXP [███████████─────────────] │  ← EXP 条
 *   └────────────────────────────────┘
 *
 * 比对手面板多了 HP 数值和经验条
 */
static void DrawPlayerInfoPanel(void) {
    Rectangle panelRect = { PLAYER_INFO_X, PLAYER_INFO_Y, PLAYER_INFO_W, PLAYER_INFO_H };

    if (texPlayerInfoBar.id > 0) {
        DrawTexturePro(texPlayerInfoBar,
            (Rectangle){ 0, 0, (float)texPlayerInfoBar.width, (float)texPlayerInfoBar.height },
            panelRect, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        DrawPanelShadow(panelRect, CLR_BEIGE, CLR_DARKBROWN, 2.5f, 3);
    }

    if (hasFont) {
        char buf[64];

        /* 第一行左: 名字+等级 */
        snprintf(buf, sizeof(buf), "%s  Lv%d", playerPoke.name, playerPoke.level);
        DrawTextEx(fontBattle, buf,
                   (Vector2){ PLAYER_INFO_X + 12, PLAYER_INFO_Y + 8 }, 22, 1, CLR_TEXTBLACK);

        /* 第一行右: HP 数值 */
        snprintf(buf, sizeof(buf), "%d / %d", playerPoke.hp_current, playerPoke.hp_max);
        DrawTextEx(fontBattle, buf,
                   (Vector2){ PLAYER_INFO_X + 210, PLAYER_INFO_Y + 8 }, 20, 1, CLR_TEXTBLACK);

        /* HP / EXP 标签 */
        DrawTextEx(fontBattle, "HP",
                   (Vector2){ PLAYER_INFO_X + 12, PLAYER_INFO_Y + 58 }, 14, 1, CLR_TEXTBLACK);
        DrawTextEx(fontBattle, "EXP",
                   (Vector2){ PLAYER_INFO_X + 12, PLAYER_INFO_Y + 80 }, 14, 1, CLR_TEXTBLACK);
    } else {
        /* 无字体时用 raylib 默认字体 (不支持中文) */
        char buf[64];
        snprintf(buf, sizeof(buf), "%s Lv%d", playerPoke.name, playerPoke.level);
        DrawText(buf, PLAYER_INFO_X + 12, PLAYER_INFO_Y + 8, 22, CLR_TEXTBLACK);
        snprintf(buf, sizeof(buf), "%d/%d", playerPoke.hp_current, playerPoke.hp_max);
        DrawText(buf, PLAYER_INFO_X + 210, PLAYER_INFO_Y + 8, 20, CLR_TEXTBLACK);
    }

    /* HP 条 + EXP 条 */
    DrawHPBar(PLAYER_INFO_X + 12, PLAYER_INFO_Y + 36, 306, 18,
              playerPoke.hp_current, playerPoke.hp_max);
    DrawEXPBar(PLAYER_INFO_X + 12, PLAYER_INFO_Y + 68, 306, 10,
               playerPoke.exp_current, playerPoke.exp_max);
}

/**
 * @brief 绘制 2×2 指令菜单 (主菜单)
 *
 * 布局:
 *   ┌──────────┬──────────┐
 *   │ > 战斗   │   道具   │  ← 选中项: 红底白字 + > 箭头
 *   ├──────────┼──────────┤
 *   │   精灵   │   逃跑   │  ← 未选中: 黑色文字
 *   └──────────┴──────────┘
 *
 * 十字分隔线划分四个选项格
 */
static void DrawCommandBox(void) {
    Rectangle panelRect = { COMMAND_BOX_X, COMMAND_BOX_Y, COMMAND_BOX_W, COMMAND_BOX_H };
    DrawPanelShadow(panelRect, CLR_LIGHTGRAY, CLR_DARKBROWN, 3.0f, 3);

    const char *labels[] = { "战斗", "道具", "精灵", "逃跑" };
    float cellW = COMMAND_BOX_W / 2.0f;
    float cellH = COMMAND_BOX_H / 2.0f;

    for (int i = 0; i < 4; i++) {
        int row = i / 2;
        int col = i % 2;
        float cx = COMMAND_BOX_X + col * cellW;
        float cy = COMMAND_BOX_Y + row * cellH;

        if (i == cursorPos) {
            /* 选中项: 红色半透明高亮 + 白色文字 */
            DrawRectangle(cx + 4, cy + 4, cellW - 8, cellH - 8,
                          (Color){ 200, 64, 64, 200 });
            if (hasFont) {
                DrawTextEx(fontBattle, ">",
                           (Vector2){ cx + 14, cy + 24 }, 28, 1, WHITE);
                DrawTextEx(fontBattle, labels[i],
                           (Vector2){ cx + 52, cy + 24 }, 28, 1, WHITE);
            } else {
                DrawText(">", cx + 14, cy + 24, 28, WHITE);
                DrawText(labels[i], cx + 52, cy + 24, 28, WHITE);
            }
        } else {
            /* 未选中: 黑色文字 */
            if (hasFont) {
                DrawTextEx(fontBattle, labels[i],
                           (Vector2){ cx + 42, cy + 26 }, 26, 1, CLR_TEXTBLACK);
            } else {
                DrawText(labels[i], cx + 42, cy + 26, 26, CLR_TEXTBLACK);
            }
        }
    }

    /* 十字分隔线 */
    float midX = COMMAND_BOX_X + cellW;
    float midY = COMMAND_BOX_Y + cellH;
    DrawLineEx((Vector2){ midX, COMMAND_BOX_Y + 8 },
               (Vector2){ midX, COMMAND_BOX_Y + COMMAND_BOX_H - 8 },
               1.5f, CLR_DARKBROWN);
    DrawLineEx((Vector2){ COMMAND_BOX_X + 8, midY },
               (Vector2){ COMMAND_BOX_X + COMMAND_BOX_W - 8, midY },
               1.5f, CLR_DARKBROWN);
}

/**
 * @brief 绘制技能选择菜单 (2×2 网格)
 *
 * 布局同指令菜单, 但每格显示技能名 + 属性类型 + PP 值
 * PP 为 0 的技能显示为灰色
 * 当前光标位置高亮
 */
static void DrawFightBox(void) {
    Rectangle panelRect = { FIGHT_BOX_X, FIGHT_BOX_Y, FIGHT_BOX_W, FIGHT_BOX_H };
    DrawPanelShadow(panelRect, CLR_LIGHTGRAY, CLR_DARKBROWN, 3.0f, 3);

    float cellW = FIGHT_BOX_W / 2.0f;
    float cellH = FIGHT_BOX_H / 2.0f;

    for (int i = 0; i < playerPoke.move_count; i++) {
        int row = i / 2;
        int col = i % 2;
        float cx = FIGHT_BOX_X + col * cellW;
        float cy = FIGHT_BOX_Y + row * cellH;

        /* 选中行高亮 */
        if (i == cursorPos && subState == STATE_FIGHT) {
            DrawRectangle(cx + 2, cy + 2, cellW - 4, cellH - 4,
                          (Color){ 200, 64, 64, 200 });
        }

        /* PP 耗尽 → 灰色文字 */
        Color textClr = (playerPoke.move_pp[i] > 0)
                            ? CLR_TEXTBLACK
                            : (Color){ 160, 160, 160, 255 };
        if (i == cursorPos && subState == STATE_FIGHT) textClr = WHITE;

        /* 技能名 + PP */
        if (hasFont) {
            DrawTextEx(fontBattle, playerPoke.moves[i],
                       (Vector2){ cx + 12, cy + 16 }, 22, 1, textClr);
            char ppBuf[24];
            snprintf(ppBuf, sizeof(ppBuf), "PP:%d/%d",
                     playerPoke.move_pp[i], playerPoke.move_pp_max[i]);
            DrawTextEx(fontBattle, ppBuf,
                       (Vector2){ cx + 12, cy + 48 }, 16, 1, textClr);
        } else {
            DrawText(playerPoke.moves[i], cx + 12, cy + 16, 22, textClr);
            char ppBuf[24];
            snprintf(ppBuf, sizeof(ppBuf), "PP:%d/%d",
                     playerPoke.move_pp[i], playerPoke.move_pp_max[i]);
            DrawText(ppBuf, cx + 12, cy + 48, 16, textClr);
        }
    }

    /* 十字分隔线 */
    float midX = FIGHT_BOX_X + cellW;
    float midY = FIGHT_BOX_Y + cellH;
    DrawLineEx((Vector2){ midX, FIGHT_BOX_Y + 8 },
               (Vector2){ midX, FIGHT_BOX_Y + FIGHT_BOX_H - 8 },
               1.5f, CLR_DARKBROWN);
    DrawLineEx((Vector2){ FIGHT_BOX_X + 8, midY },
               (Vector2){ FIGHT_BOX_X + FIGHT_BOX_W - 8, midY },
               1.5f, CLR_DARKBROWN);
}

/**
 * @brief 绘制底部对话框
 *
 * 样式: 米色外框 + 深蓝色内底 + 白色文字
 * 模仿 GBA 原版 "xxx 要做什么?" 提示框
 */
static void DrawMessageBox(void) {
    Rectangle boxRect = { TEXTBOX_X + 4, TEXTBOX_Y + 4, TEXTBOX_W - 8, TEXTBOX_H - 8 };
    /* 代码绘制: 深蓝填充 + 米色边框 */
    DrawRectangleRec(boxRect, CLR_DARKBLUE);
    DrawRectangleLinesEx(boxRect, 2.5f, CLR_BEIGE);

    /* 文本左对齐, 留 20px 内边距 */
    if (hasFont) {
        DrawTextEx(fontBattle, messageText,
                   (Vector2){ TEXTBOX_X + 20, TEXTBOX_Y + 28 }, 26, 1, WHITE);
    } else {
        DrawText(messageText, TEXTBOX_X + 20, TEXTBOX_Y + 28, 26, WHITE);
    }
}

/* ======================== 公共接口 (由 game.c 调用) ======================== */

/**
 * @brief 初始化战斗
 *
 * 1. 保存字体引用
 * 2. 加载所有贴图资源
 * 3. 初始化双方宝可梦数据
 *
 * @param bc   战斗上下文 (当前版本使用内部全局变量, 此参数保留给后续重构)
 * @param font 中文字体 (由 game.c 的 InitGame 加载传入)
 */
void InitBattle(BattleContext *bc, Font font) {
    (void)bc;

    fontBattle = font;
    hasFont = (font.texture.id > 0);

    memset(&playerPoke, 0, sizeof(playerPoke));
    memset(&enemyPoke, 0, sizeof(enemyPoke));

    LoadBattleResources();
    InitBattleData();
}

/**
 * @brief 每帧更新战斗状态机
 * @param bc 战斗上下文 (保留参数)
 */
void UpdateBattle(BattleContext *bc) {
    (void)bc;
    UpdateBattleLogic();
}

/**
 * @brief 每帧绘制战斗界面
 *
 * 绘制顺序 (从远到近):
 *   背景 → 站台 → 精灵 → 信息面板 → 菜单/技能框 → 对话框
 *
 * @param bc 战斗上下文 (保留参数)
 */
void DrawBattle(BattleContext *bc) {
    (void)bc;
    ClearBackground(BLACK);

    /* 第 1 层: 全屏背景 */
    DrawBackground();

    /* 第 2 层: 双层站台 (远处先画 -> 近处后画) */
    DrawPlatforms();

    /* 第 3 层: 宝可梦精灵 */
    DrawPokemonSprites();

    /* 第 4 层: 信息面板 */
    DrawEnemyInfoPanel();
    DrawPlayerInfoPanel();

    /* 第 5 层: 底部 UI (根据子状态切换) */
    switch (subState) {
        case STATE_INTRO:
        case STATE_MESSAGE:
            DrawMessageBox();
            break;
        case STATE_COMMAND:
            DrawMessageBox();
            DrawCommandBox();
            break;
        case STATE_FIGHT:
            DrawFightBox();
            break;
        case STATE_EXECUTE:
            DrawMessageBox();
            break;
    }

    /* 第 6 层: 底部 UI 覆盖层 (紧贴精灵下方) */
    if (texBattleUI.id > 0) {
        DrawTexturePro(texBattleUI,
            (Rectangle){ 0, 0, (float)texBattleUI.width, (float)texBattleUI.height },
            (Rectangle){ 0, 500, SCREEN_W, 140 },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
}

/**
 * @brief 释放所有战斗纹理资源
 *
 * 逐一检查贴图 id, 有效则释放.
 * 重置宝可梦数据和战斗完成标记.
 *
 * @param bc 战斗上下文 (保留参数)
 */
void CloseBattle(BattleContext *bc) {
    (void)bc;
    UnloadBattleResources();
    memset(&playerPoke, 0, sizeof(playerPoke));
    memset(&enemyPoke, 0, sizeof(enemyPoke));
    battleFinished = false;
}

/**
 * @brief 查询战斗是否已结束
 *
 * game.c 在 UpdateGame 的 GAME_BATTLE 分支中轮询此函数,
 * 返回 true 时切回 GAME_WORLD 状态.
 *
 * @return true = 战斗结束 (逃跑/一方倒下)
 */
bool IsBattleFinished(void) {
    return battleFinished;
}
