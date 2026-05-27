/**
 * @file game.c
 * @brief Monster Game 游戏核心逻辑实现
 *
 * 本文件实现了游戏的主要功能，包括：
 * - 四种游戏状态（标题、故事、对话、制作人员）的管理与切换
 * - 像素风格渐变背景的绘制
 * - 中文字体加载与渲染
 * - 人物纹理的加载与显示
 * - 按钮交互与鼠标检测
 */

#include "raylib.h"   /* raylib 图形库 */
#include "game.h"     /* 游戏模块头文件 */
#include "map.h"      /* 地图模块 */
#include "player.h"   /* 玩家模块 */
#include <stdio.h>    /* 用于 snprintf */
#include <string.h>   /* 用于 memcpy */
#include <stdlib.h>   /* 用于 malloc/free */

/* ======================== 类型定义 ======================== */

/**
 * @enum GameState
 * @brief 游戏状态枚举
 *
 * 定义游戏的四种不同界面状态：
 * - GAME_TITLE:    标题画面（显示游戏标题、开始按钮、制作人员按钮）
 * - GAME_STORY:    故事画面（逐行显示剧情文本）
 * - GAME_DIALOGUE: 对话画面（显示角色立绘与对话框）
 * - GAME_CREDITS:  制作人员名单
 */
typedef enum
{
    GAME_TITLE,     /**< 标题界面 */
    GAME_STORY,     /**< 故事剧情 */
    GAME_DIALOGUE,  /**< 角色对话 */
    GAME_CREDITS,   /**< 制作人员 */
    GAME_WORLD      /**< 世界地图游玩 */
} GameState;

/* ======================== 全局状态 ======================== */

static GameState currentState;   /**< 当前游戏状态 */

/* ------------------------- UI 元素 ------------------------- */

static Rectangle btnStartGame;   /**< "开始游戏" 按钮区域（位置与大小） */
static Rectangle btnCredits;     /**< "制作人员" 按钮区域（位置与大小） */
static Vector2   mousePoint;     /**< 当前鼠标位置坐标 */

/* ------------------------- 资源 ------------------------- */

static Font      fontCN;         /**< 中文字体（黑体） */
static Texture2D masterTex;      /**< 人物纹理精灵图 */

/* ------------------------- 状态数据 ------------------------- */

static int storyStep;            /**< 故事进度步数（0~3，对应 4 句剧情文本） */

/* ------------------------- 世界地图 ------------------------- */

static Map      worldMap;        /**< 世界地图 */
static Player   worldPlayer;     /**< 玩家对象 */
static Camera2D worldCamera;     /**< 2D 摄像机 */
static bool     stairsTriggered; /**< 楼梯触发标记 */

/* ======================== 初始化 ======================== */

/**
 * @brief 初始化游戏
 *
 * 设置初始游戏状态为标题界面，初始化按钮位置，
 * 加载中文字体（黑体）和人物纹理图片。
 *
 * @note 字体加载从系统路径 "C:/Windows/Fonts/simhei.ttf" 读取，
 *       人物图片从 "D:/pokemon/master1.png" 加载。
 */
void InitGame(void)
{
    /* ---------- 状态初始化 ---------- */
    currentState = GAME_TITLE;
    storyStep = 0;

    /* ---------- UI 布局 ---------- */
    btnStartGame = (Rectangle){ 330, 300, 300, 60 };   /* "开始游戏" 按钮：居中偏下 */
    btnCredits   = (Rectangle){ 330, 380, 300, 60 };   /* "制作人员" 按钮：在开始按钮下方 */

    /* ---------- 加载中文字体 ---------- */
    /* 将游戏中所有需要用到的中文文本合并，提取所有 codepoint 以生成完整字体 */
    const char *allText = "开始游戏制作人员点击任意位置或按空格继续这个世界有着神奇的生物它们被称之为精灵与我们相伴共生并与一起战斗开启这段旅程吧欢迎来到这个神奇的世界";
    int codepointCount = 0;
    int *codepoints = LoadCodepoints(allText, &codepointCount);  /* 提取所有 Unicode 码点 */

#if defined(__APPLE__)
    /* macOS 系统：使用 Noto Sans SC (开源免费中文字体) */
    /* 如果 ~/Library/Fonts/ 下没有，尝试系统自带华文黑体 */
    const char *fontPath = "/Users/han/Library/Fonts/NotoSansSC[wght].ttf";
    if (FileExists(fontPath))
    {
        fontCN = LoadFontEx(fontPath, 48, codepoints, codepointCount);
    }
    else
    {
        /* 降级方案：使用系统自带华文黑体 (STHeiti) */
        fontCN = LoadFontEx("/System/Library/Fonts/STHeiti Medium.ttc", 48, codepoints, codepointCount);
    }
#else
    /* Windows 系统：使用黑体 (SimHei) */
    fontCN = LoadFontEx("C:/Windows/Fonts/simhei.ttf", 48, codepoints, codepointCount);
#endif
    SetTextureFilter(fontCN.texture, TEXTURE_FILTER_POINT);      /* 像素风：点采样过滤 */

    UnloadCodepoints(codepoints);  /* 释放临时码点数组 */

    /* ---------- 加载人物图片 (绿幕抠图) ---------- */
    Image img = LoadImage("assets/professor.jpg");
    ImageResize(&img, 400, 500);                                 /* 缩放到 400x500 */
    /* 转为 RGBA 格式以便操作 Alpha 通道 */
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    /* 绿幕抠图 (Chroma Key)：
       遍历所有像素，将绿色背景区域设为透明。
       判断标准：绿色通道值明显高于红、蓝色通道，且绿色值足够高 */
    Color *pixels = LoadImageColors(img);
    for (int i = 0; i < img.width * img.height; i++)
    {
        /* 绿幕判断条件：
           - 绿色通道值 > 红色通道值 + 60  (绿色明显偏多)
           - 绿色通道值 > 蓝色通道值 + 60  (绿色明显偏多)
           - 绿色通道值 > 80               (排除暗色区域误判) */
        if (pixels[i].g > pixels[i].r + 60 &&
            pixels[i].g > pixels[i].b + 60 &&
            pixels[i].g > 80)
        {
            pixels[i].a = 0;     /* 绿色背景设为完全透明 */
        }
    }
    /* 用修改后的像素数据更新图片 */
    Image newImg = {
        .data = pixels,
        .width = img.width,
        .height = img.height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };
    masterTex = LoadTextureFromImage(newImg);
    /* 释放像素数据和原始图片 */
    RL_FREE(pixels);
    UnloadImage(img);
    SetTextureFilter(masterTex, TEXTURE_FILTER_POINT);            /* 像素风：点采样过滤 */
}

/* ======================== 逻辑更新 ======================== */

/**
 * @brief 更新游戏逻辑
 *
 * 每帧更新鼠标位置，并根据当前游戏状态处理不同的输入：
 * - GAME_TITLE:    检测鼠标点击 "开始游戏" 或 "制作人员" 按钮
 * - GAME_STORY:    检测点击或空格键推进故事，播完后进入对话
 * - GAME_DIALOGUE: 检测点击、空格或 ESC 返回标题
 * - GAME_CREDITS:  检测点击或 ESC 返回标题
 */
void UpdateGame(void)
{
    mousePoint = GetMousePosition();  /* 更新当前鼠标位置 */

    switch (currentState)
    {
        /* ========== 标题界面 ========== */
        case GAME_TITLE:
        {
            /* 点击 "开始游戏" 按钮 → 进入故事界面 */
            if (CheckCollisionPointRec(mousePoint, btnStartGame) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                currentState = GAME_STORY;
                storyStep = 0;
            }

            /* 点击 "制作人员" 按钮 → 进入制作人员名单 */
            if (CheckCollisionPointRec(mousePoint, btnCredits) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                currentState = GAME_CREDITS;
            }

        } break;

        /* ========== 故事界面 ========== */
        case GAME_STORY:
        {
            /* 点击鼠标左键或按空格键 → 推进故事 */
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE))
            {
                storyStep++;
                /* 故事共 4 句（步数 0~3），播完后自动进入对话界面 */
                if (storyStep >= 4)
                {
                    currentState = GAME_DIALOGUE;
                }
            }

        } break;

        /* ========== 对话界面 ========== */
        case GAME_DIALOGUE:
        {
            /* 点击鼠标、空格或 ESC → 进入世界地图 */
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE))
            {
                worldMap = LoadMap("assets/maps/tootooo.tmj");
                InitPlayer(&worldPlayer, worldMap.playerSpawn, &worldMap);
                worldCamera.target = worldPlayer.pos;
                worldCamera.offset = (Vector2){ 480, 320 };
                worldCamera.rotation = 0.0f;
                worldCamera.zoom = 1.5f;
                stairsTriggered = false;
                currentState = GAME_WORLD;
            }

        } break;

        /* ========== 制作人员名单 ========== */
        case GAME_CREDITS:
        {
            /* 按 ESC 或点击鼠标 → 返回标题界面 */
            if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                currentState = GAME_TITLE;
            }

        } break;

        /* ========== 世界地图游玩 ========== */
        case GAME_WORLD:
        {
            float dt = GetFrameTime();

            /* 按 ESC → 返回标题界面 */
            if (IsKeyPressed(KEY_ESCAPE))
            {
                UnloadMap(&worldMap);
                currentState = GAME_TITLE;
                break;
            }

            UpdatePlayer(&worldPlayer, &worldMap, dt);

            /* 门传送：触碰门自动切换地图 */
            if (worldPlayer.onDoor &&
                worldPlayer.doorTargetMap[0] != '\0')
            {
                char mapPath[512];
                snprintf(mapPath, sizeof(mapPath), "assets/maps/%s",
                         worldPlayer.doorTargetMap);
                TraceLog(LOG_INFO, "TELEPORT: loading %s -> (%.0f, %.0f)",
                         mapPath, worldPlayer.doorTargetX, worldPlayer.doorTargetY);
                Map newMap = LoadMap(mapPath);
                if (newMap.floorData) {
                    UnloadMap(&worldMap);
                    worldMap = newMap;
                    worldPlayer.pos.x = worldPlayer.doorTargetX;
                    worldPlayer.pos.y = worldPlayer.doorTargetY;
                    worldCamera.target = worldPlayer.pos;
                    TraceLog(LOG_INFO, "TELEPORT: success!");
                } else {
                    TraceLog(LOG_WARNING, "TELEPORT: failed to load %s", mapPath);
                }
            }

            stairsTriggered = worldPlayer.onStairs;

            /* 摄像机平滑跟随玩家 */
            Vector2 target = {
                worldPlayer.pos.x + worldPlayer.size.x / 2,
                worldPlayer.pos.y + worldPlayer.size.y / 2
            };
            worldCamera.target.x += (target.x - worldCamera.target.x) * 8.0f * dt;
            worldCamera.target.y += (target.y - worldCamera.target.y) * 8.0f * dt;

        } break;
    }
}

/* ======================== 绘制辅助函数 ======================== */

/**
 * @brief 绘制像素风格渐变背景
 *
 * 使用 32x32 像素块模拟渐变效果，从上到下由深蓝逐渐过渡到浅蓝。
 * 底部绘制由深绿到浅绿的像素块作为地面。
 *
 * 渐变颜色表定义在函数内部，共 20 种蓝色和 2 种绿色。
 */
void DrawPixelGradientBackground(void)
{
    const int blockSize = 32;       /* 像素块大小 */
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();

    /* ---------- 天空渐变：从深蓝到浅蓝共 20 种颜色 ---------- */
    Color gradient[] = {
        (Color){ 10, 20, 50, 255 },    /* 最顶部 —— 深蓝色夜空 */
        (Color){ 15, 30, 65, 255 },
        (Color){ 20, 40, 80, 255 },
        (Color){ 25, 55, 100, 255 },
        (Color){ 30, 70, 120, 255 },
        (Color){ 35, 85, 140, 255 },
        (Color){ 45, 105, 160, 255 },
        (Color){ 55, 125, 180, 255 },
        (Color){ 70, 145, 195, 255 },
        (Color){ 85, 160, 210, 255 },
        (Color){ 100, 175, 220, 255 },
        (Color){ 115, 190, 230, 255 },
        (Color){ 130, 205, 240, 255 },
        (Color){ 145, 215, 245, 255 },
        (Color){ 160, 225, 250, 255 },
        (Color){ 175, 235, 255, 255 }, /* 中下部 —— 浅蓝天际 */
        (Color){ 190, 240, 255, 255 },
        (Color){ 200, 245, 255, 255 },
        (Color){ 210, 248, 255, 255 },
        (Color){ 220, 250, 255, 255 }, /* 最底部 —— 接近白色淡蓝 */
    };

    int rowCount = sizeof(gradient) / sizeof(gradient[0]);  /* 颜色行数 */

    /* 逐行绘制像素块 */
    for (int y = 0; y < screenH; y += blockSize)
    {
        int rowIdx = (y * rowCount) / screenH;  /* 根据垂直位置映射到颜色索引 */
        if (rowIdx >= rowCount) rowIdx = rowCount - 1;

        for (int x = 0; x < screenW; x += blockSize)
        {
            DrawRectangle(x, y, blockSize, blockSize, gradient[rowIdx]);
        }
    }

    /* ---------- 地面：底部约 5 行绿色像素块 ---------- */
    for (int y = 580; y < screenH; y += blockSize)
    {
        /* y < 620 为浅绿色草地，y >= 620 为深绿色草地，营造层次感 */
        Color groundColor = (y < 620) ? (Color){ 50, 130, 50, 255 } : (Color){ 40, 100, 40, 255 };
        for (int x = 0; x < screenW; x += blockSize)
        {
            DrawRectangle(x, y, blockSize, blockSize, groundColor);
        }
    }
}

/* ======================== 绘制 ======================== */

/**
 * @brief 绘制游戏画面
 *
 * 根据当前游戏状态绘制对应的界面元素：
 * - GAME_TITLE:    标题文字、两个交互按钮（带悬停高亮效果）
 * - GAME_STORY:    居中显示故事文本 + 底部操作提示
 * - GAME_DIALOGUE: 像素渐变背景、角色立绘、对话框
 * - GAME_CREDITS:  制作人员名单
 */
void DrawGame(void)
{
    ClearBackground(BLACK);  /* 清除画布为黑色 */

    switch (currentState)
    {
        /* ========== 标题界面 ========== */
        case GAME_TITLE:
        {
            /* 游戏主标题 */
            DrawText("POKEMON FIRE RED", 280, 180, 50, RED);

            /* ---------- "开始游戏" 按钮 ---------- */
            Color btnStartColor = LIME;
            if (CheckCollisionPointRec(mousePoint, btnStartGame))
                btnStartColor = DARKGREEN;   /* 鼠标悬停时变为深绿色 */

            DrawRectangleRec(btnStartGame, btnStartColor);       /* 按钮填充 */
            DrawRectangleLinesEx(btnStartGame, 3, WHITE);        /* 白色边框 */

            const char *startText = "开始游戏";
            int startFontSize = 36;
            Vector2 startTextSize = MeasureTextEx(fontCN, startText, startFontSize, 1);
            /* 文字居中显示在按钮内 */
            DrawTextEx(fontCN, startText,
                       (Vector2){ btnStartGame.x + (btnStartGame.width - startTextSize.x) / 2,
                                  btnStartGame.y + (btnStartGame.height - startTextSize.y) / 2 },
                       startFontSize, 1, WHITE);

            /* ---------- "制作人员" 按钮 ---------- */
            Color btnCreditsColor = SKYBLUE;
            if (CheckCollisionPointRec(mousePoint, btnCredits))
                btnCreditsColor = BLUE;      /* 鼠标悬停时变为蓝色 */

            DrawRectangleRec(btnCredits, btnCreditsColor);       /* 按钮填充 */
            DrawRectangleLinesEx(btnCredits, 3, WHITE);          /* 白色边框 */

            const char *creditsText = "制作人员";
            int creditsFontSize = 36;
            Vector2 creditsTextSize = MeasureTextEx(fontCN, creditsText, creditsFontSize, 1);
            /* 文字居中显示在按钮内 */
            DrawTextEx(fontCN, creditsText,
                       (Vector2){ btnCredits.x + (btnCredits.width - creditsTextSize.x) / 2,
                                  btnCredits.y + (btnCredits.height - creditsTextSize.y) / 2 },
                       creditsFontSize, 1, WHITE);

        } break;

        /* ========== 故事界面 ========== */
        case GAME_STORY:
        {
            /* 4 句故事文本，按 storyStep 逐句显示 */
            const char *storyLines[] = {
                "这个世界有着神奇的生物",
                "它们被称之为精灵",
                "与我们相伴共生并与我们一起战斗",
                "开启这段旅程吧"
            };

            const char *text = storyLines[storyStep];
            int fontSize = 36;
            Vector2 textSize = MeasureTextEx(fontCN, text, fontSize, 1);
            /* 居中显示当前故事台词 */
            DrawTextEx(fontCN, text,
                       (Vector2){ (GetScreenWidth() -textSize.x) / 2, (GetScreenHeight() -textSize.y) / 2 },
                       fontSize, 1, WHITE);

            /* 底部操作提示：根据是否到达最后一句话展示不同文本 */
            const char *hintText = (storyStep < 3) ? "点击任意位置或按下空格继续" : "点击任意位置或按下空格开始";
            Vector2 hintSize = MeasureTextEx(fontCN, hintText, 24, 1);
            DrawTextEx(fontCN, hintText,
                       (Vector2){ (GetScreenWidth() -hintSize.x) / 2, 600 },
                       24, 1, GRAY);

        } break;

        /* ========== 对话界面 ========== */
        case GAME_DIALOGUE:
        {
            /* 绘制像素风天空与地面渐变背景 */
            DrawPixelGradientBackground();

            /* 显示人物立绘（居中偏上，缩放到适合新窗口） */
            float charW = 300.0f;
            float charH = 375.0f;
            Rectangle charSrc = { 0, 0, (float)masterTex.width, (float)masterTex.height };
            Rectangle charDst = { (GetScreenWidth() - charW) / 2, 70, charW, charH };
            DrawTexturePro(masterTex, charSrc, charDst, (Vector2){0, 0}, 0.0f, WHITE);

            /* ---------- 底部对话框 ---------- */
            Rectangle dialogBox = { 60, 460, GetScreenWidth() - 120.0f, 140 };
            DrawRectangleRec(dialogBox, (Color){ 0, 0, 0, 210 });
            DrawRectangleLinesEx(dialogBox, 4, (Color){ 255, 255, 255, 220 });

            /* 对话框内容文字 */
            const char *dialogText = "欢迎来到这个神奇的世界";
            int dialogFontSize = 34;
            Vector2 dialogTextSize = MeasureTextEx(fontCN, dialogText, dialogFontSize, 1);
            DrawTextEx(fontCN, dialogText,
                       (Vector2){ dialogBox.x + (dialogBox.width - dialogTextSize.x) / 2,
                                  dialogBox.y + (dialogBox.height - dialogTextSize.y) / 2 },
                       dialogFontSize, 1, WHITE);

            /* 右下角操作提示 */
            const char *hint = "点击任意位置继续";
            Vector2 hintSize = MeasureTextEx(fontCN, hint, 18, 1);
            DrawTextEx(fontCN, hint,
                       (Vector2){ GetScreenWidth() - hintSize.x - 20, dialogBox.y + dialogBox.height - 26 },
                       18, 1, LIGHTGRAY);

        } break;

        /* ========== 制作人员名单 ========== */
        case GAME_CREDITS:
        {
            /* 标题 */
            const char *titleText = "制作人员";
            Vector2 titleSize = MeasureTextEx(fontCN, titleText, 48, 1);
            DrawTextEx(fontCN, titleText,
                       (Vector2){ (GetScreenWidth() -titleSize.x) / 2, 150 },
                       48, 1, YELLOW);

            /* 制作人员列表 */
            const char *credits[] = { "geuuge", "kobe", "tootooo", "Pogi" };
            int creditCount = sizeof(credits) / sizeof(credits[0]);

            for (int i = 0; i < creditCount; i++)
            {
                int fontSize = 36;
                int textWidth = MeasureText(credits[i], fontSize);
                DrawText(credits[i],
                         (GetScreenWidth() -textWidth) / 2,
                         280 + i * 60,
                         fontSize, WHITE);
            }

            /* 底部操作提示 */
            const char *hintText = "点击任意位置或按 ESC 返回";
            Vector2 hintSize = MeasureTextEx(fontCN, hintText, 24, 1);
            DrawTextEx(fontCN, hintText,
                       (Vector2){ (GetScreenWidth() -hintSize.x) / 2, 550 },
                       24, 1, GRAY);

        } break;

        /* ========== 世界地图游玩 ========== */
        case GAME_WORLD:
        {
            BeginMode2D(worldCamera);

            DrawMap(&worldMap);
            DrawPlayer(&worldPlayer, &worldMap);

            EndMode2D();

            /* 右下角操作提示 */
            const char *ctrl = "WASD 移动 | Shift 跑步 | ESC 返回";
            DrawText(ctrl, 10, 695, 16, LIGHTGRAY);

        } break;
    }
}

/* ======================== 清理 ======================== */

/**
 * @brief 清理游戏资源
 *
 * 卸载人物纹理和中文字体，释放 GPU / 内存资源。
 */
void CloseGame(void)
{
    if (currentState == GAME_WORLD) {
        UnloadMap(&worldMap);
    }
    UnloadTexture(masterTex);  /* 卸载人物纹理 */
    UnloadFont(fontCN);        /* 卸载中文字体 */
}
