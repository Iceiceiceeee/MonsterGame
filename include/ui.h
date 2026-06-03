/**
 * @file ui.h
 * @brief 通用 UI 绘制函数 —— 面板、进度条、文字居中等辅助渲染
 *
 * 提供战斗界面和其他 UI 画面中复用的基础绘制组件。
 */

#ifndef UI_H
#define UI_H

#include "raylib.h"

/**
 * @brief 绘制矩形面板（填充色 + 紫色边框）
 * @param rect         面板区域
 * @param fill         填充颜色
 * @param border       边框颜色
 * @param borderThick  边框粗细（像素）
 */
void DrawPanel(Rectangle rect, Color fill, Color border, float borderThick);

/**
 * @brief 绘制带阴影的矩形面板
 * @param rect         面板区域
 * @param fill         填充颜色
 * @param border       边框颜色
 * @param borderThick  边框粗细
 * @param shadowOffset 阴影偏移量（右下方向，像素）
 */
void DrawPanelShadow(Rectangle rect, Color fill, Color border, float borderThick, int shadowOffset);

/**
 * @brief 绘制进度条（HP 条 / EXP 条通用）
 * @param rect     进度条区域
 * @param fraction 当前比例 (0.0 ~ 1.0)
 * @param fill     填充颜色
 * @param bg       背景颜色
 */
void DrawBar(Rectangle rect, float fraction, Color fill, Color bg);

/**
 * @brief 在指定矩形区域中居中绘制文字
 * @param font     使用的字体
 * @param text     文字内容
 * @param rect     目标区域
 * @param fontSize 字号
 * @param spacing  字间距
 * @param color    文字颜色
 */
void DrawTextCenteredEx(Font font, const char *text, Rectangle rect, float fontSize, float spacing, Color color);

#endif
