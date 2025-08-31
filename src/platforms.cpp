#include "platforms.h"

/*
Another method we could use:

---- plaforms.h -----
struct PlatformWindow;
PlatformWindow *PlatformOpenWindow(char *Title, ...);

---- win32_gameengine.cpp -----
Define the PlatformWindow struct
Define the PlaformOpenWindow function
*/

static void RAGE_GameUpdateAndRender(GameOffscreenBuffer *Buffer, i32 BlueOffset, i32 GreenOffset) {
    RAGE_RenderGradient(Buffer, BlueOffset, GreenOffset);
}

static void RAGE_RenderGradient(GameOffscreenBuffer *Buffer, i32 XOffset, i32 YOffset) {
    u8 *Row = (u8 *)Buffer->Memory;

    for (i32 Y = 0; Y < Buffer->Height; ++Y) {
        u32 *Pixel = (u32 *)Row;
        for (i32 X = 0; X < Buffer->Width; ++X) {
            u8 Blue = (X + XOffset);
            u8 Green = (Y + YOffset);
            *Pixel++ = (Green << 8) | Blue;
        }

        // Can do less allocation with:
        // Row = (u8 *)Pixel;
        Row += Buffer->Pitch;
    }
}