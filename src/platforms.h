#pragma once
#include <stdio.h>
#include "types.h"

#if !defined(GAMEENGINE_H)

// TODO @null0routed: Services that the game provides to the platform layer

// Services that the game provides to the platform layer
struct GameOffscreenBuffer {
    // BITMAPINFO BitmapInfo = {};
    void *Memory;
    i32 Width;
    i32 Height;
    i32 Pitch;
};

static void GameUpdateAndRender(GameOffscreenBuffer *);
static void RAGE_RenderGradient(GameOffscreenBuffer *Buffer, i32 XOffset, i32 YOffset); 

#define GAMEENGINE_H
#endif // !defined(GAMEENGINE_H)