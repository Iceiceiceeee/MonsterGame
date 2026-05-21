#include "raylib.h"

int main(void)
{
    InitWindow(1280, 720, "Monster Game");

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(BLACK);

        DrawText("HELLO TRAINER", 400, 300, 40, WHITE);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}