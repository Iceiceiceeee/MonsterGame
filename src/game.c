#include "raylib.h"
#include "game.h"

typedef enum
{
    GAME_TITLE,
    GAME_WORLD,
    GAME_BATTLE
} GameState;

static GameState currentState;

void InitGame(void)
{
    currentState = GAME_TITLE;
}

void UpdateGame(void)
{
    switch (currentState)
    {
        case GAME_TITLE:
        {
            if (IsKeyPressed(KEY_ENTER))
            {
                currentState = GAME_WORLD;
            }

        } break;

        case GAME_WORLD:
        {
            if (IsKeyPressed(KEY_B))
            {
                currentState = GAME_BATTLE;
            }

        } break;

        case GAME_BATTLE:
        {
            if (IsKeyPressed(KEY_ESCAPE))
            {
                currentState = GAME_WORLD;
            }

        } break;
    }
}

void DrawGame(void)
{
    ClearBackground(BLACK);

    switch (currentState)
    {
        case GAME_TITLE:
        {
            DrawText("POKEMON FIRE RED", 350, 250, 50, RED);

            DrawText("PRESS ENTER", 470, 350, 30, WHITE);

        } break;

        case GAME_WORLD:
        {
            DrawText("WORLD MAP", 470, 300, 40, GREEN);

            DrawText("PRESS B FOR BATTLE", 390, 360, 30, WHITE);

        } break;

        case GAME_BATTLE:
        {
            DrawText("BATTLE!", 500, 300, 50, ORANGE);

            DrawText("PRESS ESC TO EXIT", 400, 360, 30, WHITE);

        } break;
    }
}

void CloseGame(void)
{

}