/**
 * @file ui.c
 * @brief 通用 UI 绘制函数实现
 *
 * 提供面板、进度条、带阴影的面板、居中文字等基础 UI 组件，
 * 内部基于 raylib 的 DrawRectangle / DrawTextEx 封装。
 */

#include "ui.h"
#include <string.h>

/**
 * @brief 绘制矩形面板：先填色，再描边
 * 常用于信息框、菜单背景等
 */
void DrawPanel(Rectangle rect, Color fill, Color border, float borderThick) {
    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, borderThick, border);
}

/**
 * @brief 绘制带阴影的面板
 * 先画阴影（黑色半透明矩形向右下偏移），再覆盖面板本体
 * 体现出 GBA 时代的立体感 UI 风格
 */
void DrawPanelShadow(Rectangle rect, Color fill, Color border, float borderThick, int shadowOffset) {
    Rectangle shadow = { rect.x + shadowOffset, rect.y + shadowOffset, rect.width, rect.height };
    DrawRectangleRec(shadow, (Color){ 0, 0, 0, 80 });
    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, borderThick, border);
}

/**
 * @brief 绘制水平进度条（HP/EXP 条）
 * 先画背景底板 → 再画按比例填充的前景 → 最后描细边
 * 左右各留 1px 内边距让填充区域不贴边
 */
void DrawBar(Rectangle rect, float fraction, Color fill, Color bg) {
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    DrawRectangleRec(rect, bg);
    Rectangle fillRect = { rect.x + 1, rect.y + 1, (rect.width - 2) * fraction, rect.height - 2 };
    DrawRectangleRec(fillRect, fill);
    DrawRectangleLinesEx(rect, 1, (Color){ 64, 64, 64, 255 });
}

/**
 * @brief 在矩形区域内居中绘制文字
 * 先用 MeasureTextEx 计算出文字实际宽度，再算居中的 x/y 坐标
 */
void DrawTextCenteredEx(Font font, const char *text, Rectangle rect, float fontSize, float spacing, Color color) {
    Vector2 textSize = MeasureTextEx(font, text, fontSize, spacing);
    float x = rect.x + (rect.width - textSize.x) / 2.0f;
    float y = rect.y + (rect.height - textSize.y) / 2.0f;
    DrawTextEx(font, text, (Vector2){ x, y }, fontSize, spacing, color);
}
