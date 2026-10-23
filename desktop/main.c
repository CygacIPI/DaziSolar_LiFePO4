#include "raylib.h"

int main(void)
{
    const int screen_width = 960;
    const int screen_height = 540;

    InitWindow(screen_width, screen_height, "App LiFePO4 - Sprint 0");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){ 19, 27, 38, 255 });

        DrawText("App LiFePO4", 40, 40, 36, RAYWHITE);
        DrawText("Entorno de escritorio C + raylib preparado.", 40, 100, 20, LIGHTGRAY);
        DrawText("Sprint 0: ventana de prueba; aun no conecta con ninguna bateria.", 40, 140, 18, LIGHTGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
