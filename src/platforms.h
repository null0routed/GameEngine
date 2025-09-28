/*
Copyright (C) 2025 "GameEngine" null0routed

Licensed under the MIT license
*/

#pragma once
#include <stdio.h>
#include "types.h"
#include <math.h>

#if !defined(GAMEENGINE_H)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#if DEBUG
#define Assert(Expression) if (!(Expression)) { *(int *) 0 = 0; }
#else
#define Assert(Expression)
#endif

// TODO @null0routed: Services that the game provides to the platform layer

// Services that the game provides to the platform layer
struct RAGE_GameOffscreenBuffer {
    // BITMAPINFO BitmapInfo = {};
    void *Memory;
    i32 Width;
    i32 Height;
    i32 Pitch;
};

struct RAGE_GameSoundBuffer {
    i32 *Samples;
    i32 SampleCount;
    i32 SamplesPerSec;
};

struct RAGE_GameButtonState {
    i32 HalfTransitionCount;
    bool EndedDown;
};

struct RAGE_GameControllerInput {
    r32 StartX;
    r32 StartY;
    r32 MinX;
    r32 MinY;
    r32 MaxX;
    r32 MaxY;
    r32 EndX;
    r32 EndY;
    bool IsAnalog;
    
    union {
        RAGE_GameButtonState Buttons[24];
        struct {
            RAGE_GameButtonState AButton;
            RAGE_GameButtonState BButton;
            RAGE_GameButtonState XButton;
            RAGE_GameButtonState YButton;
            RAGE_GameButtonState LeftShoulder;
            RAGE_GameButtonState RightShoulder;
            RAGE_GameButtonState LeftTrigger;
            RAGE_GameButtonState RightTrigger;
            RAGE_GameButtonState Start;
            RAGE_GameButtonState Back;
            RAGE_GameButtonState LeftThumb;
            RAGE_GameButtonState RightThumb;
        };
    };
};

struct RAGE_GameInput {
    // TODO @null0routed: Input clocks
    RAGE_GameControllerInput Controllers[4];
};

struct RAGE_GameState {
    i32 BlueOffset = 0;
    i32 GreenOffset = 0;
    i32 Frequency = 440;
};

struct RAGE_GameMemory {
    bool IsInitialized;
    u64 PermanentStorageSize;
    void *PermanentStorage;
    u64 TransientStorageSize;
    void *TransientStorage;
};

static void RAGE_GameUpdateAndRender(RAGE_GameMemory *, RAGE_GameSoundBuffer *, RAGE_GameInput *);
static void RAGE_RenderGradient(RAGE_GameOffscreenBuffer *Buffer, i32 XOffset, i32 YOffset); 
static void RAGE_OutputSound(RAGE_GameSoundBuffer *); 

#define GAMEENGINE_H
#endif