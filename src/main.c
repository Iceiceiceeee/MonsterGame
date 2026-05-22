/**
 * @file main.c
 * @brief Monster Game 程序入口
 *
 * 本文件为游戏的入口点，负责：
 * - 创建游戏窗口
 * - 初始化游戏资源
 * - 运行游戏主循环（更新逻辑 → 绘制画面）
 * - 退出时清理资源并关闭窗口
 */

#include "raylib.h"   /* raylib 图形库 */
#include "game.h"     /* 游戏核心模块 */

/**
 * @brief 程序主入口
 *
 * 游戏运行流程：
 * 1. 创建 1280x720 的窗口
 * 2. 设置 60 FPS 的帧率限制
 * 3. 调用 InitGame() 初始化游戏资源
 * 4. 进入主循环，每帧执行：
 *    - UpdateGame()：处理输入与状态更新
 *    - BeginDrawing() / EndDrawing()：绘制画面
 * 5. 窗口关闭后，调用 CloseGame() 清理资源
 * 6. 调用 CloseWindow() 关闭窗口
 *
 * @return 0 表示正常退出
 */
int main(void)
{
    /* ---------- 初始化阶段 ---------- */
    InitWindow(1280, 720, "Monster Game");  /* 创建窗口：1280x720，标题 "Monster Game" */

    SetTargetFPS(60);                       /* 设定目标帧率为 60 FPS */

    InitGame();                              /* 加载游戏资源与初始状态 */

    /* ---------- 游戏主循环 ---------- */
    while (!WindowShouldClose())            /* 循环检测窗口关闭信号（ESC 或点击关闭按钮） */
    {
        UpdateGame();                        /* 更新游戏逻辑（输入处理、状态切换） */

        BeginDrawing();                      /* 开始绘制 */

        DrawGame();                          /* 绘制当前帧画面 */

        EndDrawing();                        /* 结束绘制，交换前后缓冲 */
    }

    /* ---------- 清理退出阶段 ---------- */
    CloseGame();                             /* 释放游戏资源（纹理、字体等） */

    CloseWindow();                           /* 关闭窗口 */

    return 0;                                /* 程序正常退出 */
}
