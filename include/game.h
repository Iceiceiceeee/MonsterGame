/**
 * @file game.h
 * @brief Monster Game 游戏主模块头文件
 * 
 * 声明游戏生命周期相关的四个核心函数：
 * InitGame  - 初始化游戏资源与状态
 * UpdateGame - 更新游戏逻辑（输入处理、状态切换）
 * DrawGame  - 渲染绘制每一帧的画面
 * CloseGame - 释放资源，清理退出
 */

#ifndef GAME_H
#define GAME_H

/**
 * @brief 初始化游戏
 * 
 * 负责加载字体、纹理等资源，并将游戏状态设置为标题界面。
 * 应在创建窗口后、进入游戏主循环前调用一次。
 */
void InitGame(void);

/**
 * @brief 更新游戏逻辑
 * 
 * 处理用户输入、状态切换等逻辑更新。
 * 应在每一帧主循环中调用一次。
 */
void UpdateGame(void);

/**
 * @brief 绘制游戏画面
 * 
 * 根据当前游戏状态绘制标题、故事、对话或制作人员画面。
 * 应在每一帧的 BeginDrawing() / EndDrawing() 之间调用。
 */
void DrawGame(void);

/**
 * @brief 清理游戏资源
 * 
 * 卸载字体、纹理等动态加载的资源。
 * 应在退出主循环后、关闭窗口前调用一次。
 */
void CloseGame(void);

#endif /* GAME_H */
