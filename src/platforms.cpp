/*
Copyright (C) 2025 "GameEngine" null0routed

Licensed under the MIT license
*/

#pragma once

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

static void RAGE_GameUpdateAndRender(RAGE_GameOffscreenBuffer *Buffer,
    RAGE_GameSoundBuffer *SoundBuffer, RAGE_GameInput *Input) {
    static i32 BlueOffset = 0;
    static i32 GreenOffset = 0;
    static i32 Frequency = 440;

    RAGE_GameControllerInput *Input0 = &Input->Controllers[0];
    if (Input0->IsAnalog) {
        // Analog tuning
    } else {
        // Digital tuning
    } 
    
    // How do we handle input capture here?
    // Push all events vs. normalizing
    if (Input0->AButton.EndedDown) {
        GreenOffset += 1;
    }

    /*
    Frequency = 256 + (int)(128.0f * (Input0->EndX));
    BlueOffset += (int)(4.0f * (Input0->EndY));
    */

    RAGE_OutputSound(SoundBuffer);
    
    RAGE_RenderGradient(Buffer, BlueOffset, GreenOffset);
}

static void RAGE_RenderGradient(RAGE_GameOffscreenBuffer *Buffer, i32 XOffset, i32 YOffset) {
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

static void RAGE_OutputSound(RAGE_GameSoundBuffer *SoundBuffer) { 
    // TODO @null0routed: Assert region 1 / region 2 sizes are valid
    static r32 t;
    i16 Amplitude = 3000;
    i32 Frequency = 440;
    i32 WavePeriod = SoundBuffer->SamplesPerSec / Frequency;
    i32 *SampleOut = SoundBuffer->Samples; 

    for (i32 idx = 0; idx < SoundBuffer->SampleCount; ++idx) {
        r32 SineValue = sinf(t);
        i16 SampleValue = (i16)(SineValue * Amplitude);
        *SampleOut++ = (SampleValue << 16) | SampleValue;
        t += 2.0f * M_PI * 1.0f / (r32)WavePeriod;
    }
}