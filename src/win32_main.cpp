/*
Copyright (C) 2025 "GameEngine" null0routed

Licensed under the MIT license
*/

#define UNICODE
#define _UNICODE

#include <stdio.h>
#include <Windows.h>
#include <Xinput.h>
#include <dsound.h>
#include <combaseapi.h>
#include <xaudio2.h>
#include "types.h"
#include "platforms.h"

// For testing
#include <math.h>

// Include platform code
#include "platforms.cpp"

struct W32_OffscreenBuffer {
    BITMAPINFO BitmapInfo = {};
    void *BitmapMemory;
    i32 BitmapWidth;
    i32 BitmapHeight;
    i32 BytesPerPx;
    i32 Pitch;
};

struct W32_WindowDimensions {
    i32 Width;
    i32 Height;
};

struct W32_AudioEngineInfo {
    u32 TargetFrameRate;
    u32 FrameSkip;
    u32 BufferSafetyFactor;
};

struct W32_SoundOutput {
    // i32 SquareWavePeriod = 48000/256;
    i32 SamplesPerSec;
    i32 BytesPerSample;
    i32 BufferSize;
    u32 RunningSampleIdx;
    i32 Frequency;
    i32 WavePeriod;
    i16 Amplitude;
    i16 Latency;
};


struct W32_AudioEngine {
    IXAudio2 *XAudio2;
    IXAudio2MasteringVoice *MasteringVoice;
    IXAudio2SourceVoice *SourceVoice;
    WAVEFORMATEX WaveFormat;
    BYTE *SourceBuffer;
    XAUDIO2_BUFFER AudioBuffer;
    W32_AudioEngineInfo AudioEngineInfo;
};

// TODO @null0routed: Refactor this later
// Application globals
bool APP_RUNNING;
static W32_OffscreenBuffer GlobalBackBuffer;    // Graphics render buffer 
static LPDIRECTSOUNDBUFFER GlobalSoundBuffer;   // DirectSound Buffer
static W32_AudioEngine GlobalAudioEngine;       // XAudio2 Audio Engine

// Application function definitions
LRESULT CALLBACK W32_WindowProc(HWND, UINT, WPARAM, LPARAM);                                // Application Window Procedure
static void W32_ResizeDIBSection(W32_OffscreenBuffer *, i32, i32);                          // Initialize or resize Device Independant Bitmap
static void W32_DisplayBufferInWindow(W32_OffscreenBuffer *, HDC, i32, i32);
static W32_WindowDimensions W32_GetWindowDimensions(HWND);
static void W32_InitDSound(HWND, i32, i32);                                                 // Initialize our DirectSound buffers
static void W32_DSoundGenerateSineWave(W32_SoundOutput *, DWORD, DWORD);                    // Generate a sine wave
static int W32_InitXAudio2(W32_AudioEngine *AudioEngine, u32 NumChannels, u32 SamplesPerSec, u32 TargetFrameRate, u32 FrameSkip, u32 BufferSafetyFactor); 
static int W32_XAudioGenerateSquareWave(W32_AudioEngine *, float, i16);                     // Generate a square wave 
static int W32_XAudioGenerateSineWave(W32_AudioEngine *, float, i16);                       // Generate a sine wave 
static int W32_XAudioSubmitSourceBuffer(IXAudio2SourceVoice *, XAUDIO2_BUFFER *);

/* NOTE @null0routed
Probably should change this in the future to:
# define XInputGetState _XInputGetState
typedef DWORD (WINAPI W32_XInputGetState_t *)(DWORD, XINPUT_STATE*)
static DWORD WINAPI W32_XInputGetStateStub(DWORD dwUserIndex, XINPUT_STATE *pStateInfo) {
    return 0;
} 
static W32_XInputGetState_t _XInputGetState = XInputGetStateStub
*/

// External definitions for XInput
// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD, XINPUT_STATE*)
typedef X_INPUT_GET_STATE(W32_XInputGetState_t);
X_INPUT_GET_STATE(XInputGetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
static W32_XInputGetState_t *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputGetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD, XINPUT_VIBRATION*)
typedef X_INPUT_SET_STATE(W32_XInputSetState_t);
X_INPUT_SET_STATE(XInputSetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
static W32_XInputSetState_t *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// External definitions for DirectSound
typedef HRESULT WINAPI DirectSoundCreateFn(LPCGUID, LPDIRECTSOUND *, LPUNKNOWN);

// External definitions for XAudio2
typedef HRESULT XAudio2CreateFn(IXAudio2 **, u32, XAUDIO2_PROCESSOR);

static void W32_LoadXInput() {
    HMODULE XInputLib = LoadLibrary(L"xinput1_4.dll");
    if (!XInputLib) {
        OutputDebugString(L"Could not load XInput library.\n");
        return;
    }

    // TODO @null0routed: Could fallback to a previous XInput library here

    XInputGetState = (W32_XInputGetState_t *)GetProcAddress(XInputLib, "XInputGetState");
    if (!XInputGetState) { XInputGetState = XInputGetStateStub; } 
    
    XInputSetState = (W32_XInputSetState_t *)GetProcAddress(XInputLib, "XInputSetState");
    if (!XInputSetState) { XInputSetState = XInputSetStateStub; }
}

int APIENTRY WinMain(HINSTANCE Instance, HINSTANCE Parent, PSTR CommandLine, int ShowCode) {
    
    // Initialization setup
    SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT); // Ensure safe search paths for LoadLibrary
    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        OutputDebugString(L"Could not initialize COM\n");
        CoUninitialize();
        return -1;
    }

    // Init vars
    u32 PeriodInSamples = 48000 / 440;
    static bool XAudioOnce = false;

    // Load initial libraries 
    W32_LoadXInput();
    W32_InitXAudio2(&GlobalAudioEngine, 2, 48000, 120, PeriodInSamples, 2); // Channels = 2, Samples = 48khz, Tgt FPS = 120, Skip frames = freq, Safety Factor = 2
    W32_XAudioGenerateSineWave(&GlobalAudioEngine, 440.0f, 7000);
    // W32_XAudioSubmitSourceBuffer(GlobalAudioEngine.SourceVoice, &GlobalAudioEngine.AudioBuffer);
    
    // Generate main window and class
    LPCWSTR CLASS_NAME = L"GameEngine";
    WNDCLASSEXW WindowClass = {};
    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.hInstance = Instance;
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpszClassName = CLASS_NAME;
    WindowClass.lpfnWndProc = W32_WindowProc;

    W32_ResizeDIBSection(&GlobalBackBuffer, 1920, 1080);

    if (!RegisterClassEx(&WindowClass)) {
        return -1;
    }

    HWND Window = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Game Engine",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        0,
        0,
        Instance,
        0
    );

    if (!Window) {
        OutputDebugString(L"Window Failed\n");
        return -1;
    }

    ShowWindow(Window, SW_SHOW);

    // Graphics vars
    i32 XOffset = 0;
    i32 YOffset = 0;
    
    // Application vars
    APP_RUNNING = true;
    LARGE_INTEGER perf_counter_freq;
    QueryPerformanceFrequency(&perf_counter_freq);
    i64 PerfCounterFreq = perf_counter_freq.QuadPart;

    // Initialize DirectSound
    W32_SoundOutput SoundOutput = {};
    SoundOutput.SamplesPerSec = 48000;
    SoundOutput.BytesPerSample = sizeof(i16) * 2;
    SoundOutput.BufferSize = SoundOutput.SamplesPerSec * SoundOutput.BytesPerSample;
    SoundOutput.RunningSampleIdx = 0;
    SoundOutput.Frequency = 440;
    SoundOutput.Amplitude = 8000;
    SoundOutput.WavePeriod = SoundOutput.SamplesPerSec / SoundOutput.Frequency;
    SoundOutput.Latency = SoundOutput.SamplesPerSec / 120;

    W32_InitDSound(Window, SoundOutput.BufferSize, SoundOutput.SamplesPerSec);
    W32_DSoundGenerateSineWave(&SoundOutput, 0, SoundOutput.Latency * SoundOutput.BytesPerSample);
    GlobalSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

    HDC DeviceCtx = GetDC(Window); // Get DC here because we created window class with "own dc"
    MSG Message; // Declared here to reduce allocations during message loop
    
    LARGE_INTEGER PerfCounter;
    QueryPerformanceCounter(&PerfCounter);

    while(APP_RUNNING) {
        
        while (PeekMessage(&Message, Window, 0, 0, PM_REMOVE)) {
            if (Message.message == WM_QUIT) {
                APP_RUNNING = false;
            }
            
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        // TODO @null0routed: Should we poll this more frequently?
        for (DWORD CtrlrIndex = 0; CtrlrIndex < XUSER_MAX_COUNT; ++CtrlrIndex) {
            XINPUT_STATE ControllerState;
            if (XInputGetState(CtrlrIndex, &ControllerState) != ERROR_SUCCESS) {
                // OutputDebugString(L"Could not get controller state\n");
                continue;
            }
       
            bool Up = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP); 
            bool Down = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN); 
            bool Left = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT); 
            bool Right = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT); 
            bool Start = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_START); 
            bool Back = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK); 
            bool LeftShoulder = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER); 
            bool RightShoulder = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER); 
            bool AButton = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_A); 
            bool BButton = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_B); 
            bool XButton = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_X); 
            bool YButton = (ControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_Y); 

            i16 StickX = ControllerState.Gamepad.sThumbLX;
            i16 StickY = ControllerState.Gamepad.sThumbLY;

            // TODO @null0routed: Need to develop proper deadzone handling
            XOffset += StickX / 4096;
            YOffset += StickY / 4096;
        }

        XINPUT_VIBRATION Vibration;
        Vibration.wLeftMotorSpeed = 60000;
        Vibration.wRightMotorSpeed = 60000;
        XInputSetState(0, &Vibration);

        // XAudio2 Sound Test
        // static u32 AudioLoopCounter = 0;
        // do { 
        //    AudioLoopCounter = 0;
            // W32_XAudioGenerateSquareWave(&GlobalAudioEngine, 110.0f, 3000);
        
        // } while (++AudioLoopCounter >= (PeriodInSamples));
        
        GameOffscreenBuffer Buffer = {};
        Buffer.Memory = GlobalBackBuffer.BitmapMemory;
        Buffer.Width = GlobalBackBuffer.BitmapWidth;
        Buffer.Height = GlobalBackBuffer.BitmapHeight;
        Buffer.Pitch = GlobalBackBuffer.Pitch;
        RAGE_GameUpdateAndRender(&Buffer, XOffset, YOffset);

        // Sound output test
        DWORD PlayCursorPos;
        DWORD WriteCursorPos;
        if (SUCCEEDED(GlobalSoundBuffer->GetCurrentPosition(&PlayCursorPos, &WriteCursorPos))) {

            DWORD TargetCursor = (PlayCursorPos + (SoundOutput.Latency * SoundOutput.BytesPerSample)) % SoundOutput.BufferSize;
            DWORD BytesToWrite = 0;
            DWORD ByteToLock = (SoundOutput.RunningSampleIdx * SoundOutput.BytesPerSample) % SoundOutput.BufferSize;
            if (ByteToLock > TargetCursor) {
                BytesToWrite = SoundOutput.BufferSize - ByteToLock;
                BytesToWrite += TargetCursor;
            } else {
                BytesToWrite = TargetCursor - ByteToLock;
            }
            
            // Do more stuff here
            if (BytesToWrite > 0) {
                W32_DSoundGenerateSineWave(&SoundOutput, ByteToLock, BytesToWrite);
            }
        }    
        
        W32_WindowDimensions Dimensions = W32_GetWindowDimensions(Window);
        W32_DisplayBufferInWindow(&GlobalBackBuffer, DeviceCtx, Dimensions.Width, Dimensions.Height);
        
        ++XOffset;
        ++YOffset;

        LARGE_INTEGER PerfCounterEnd;
        QueryPerformanceCounter(&PerfCounterEnd);

        // TODO @null0routed: Display counter info
        u64 CounterElapsed = PerfCounterEnd.QuadPart - PerfCounter.QuadPart;
        u32 MSPerFrame = (i32)(1000 * CounterElapsed) / PerfCounterFreq;
        u32 FPS = PerfCounterFreq / (i32)CounterElapsed;
        
        wchar_t strBuffer[256];
        wsprintf(strBuffer, L"ms per frame: %d FPS: %d\n", MSPerFrame);
        OutputDebugString(strBuffer);

        PerfCounter = PerfCounterEnd;
    }  

    ReleaseDC(Window, DeviceCtx);

    return 0;
}

LRESULT CALLBACK W32_WindowProc(HWND WindowHandle, UINT Message, WPARAM wParam, LPARAM lParam) {
    LRESULT Result = 0;
    
    switch (Message) {
        case WM_SIZE: {
            OutputDebugString(L"WM_SIZE\n");
            
            break;
        } 
        
        case WM_DESTROY: {
            OutputDebugString(L"WM_DESTROY\n");
            // This is a good place to post the quit message to exit the application.
            // PostQuitMessage(0);
            // TODO @null0routed: Handle this as an error - recreate window?
            APP_RUNNING = false;
            break;
        }
        
        case WM_CLOSE: {
            OutputDebugString(L"WM_CLOSE\n");
            // We should destroy the window, which will trigger WM_DESTROY.
            // DestroyWindow(WindowHandle);
            // TODO @null0routed: Handle this as a message to the user
            APP_RUNNING = false;
            break;
        }
        
        case WM_ACTIVATEAPP: {
            OutputDebugString(L"WM_ACTIVATEAPP\n");
            break;
        }

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: 
        case WM_KEYDOWN:
        case WM_KEYUP: {
            bool WasDown = ((lParam & (1 << 30)) != 0);
            bool IsDown = ((lParam & (1 << 31)) == 0);

            switch (wParam) {
                case VK_UP: {
                    OutputDebugString(L"VK_UP\n");
                    break;
                }
                case VK_DOWN: {
                    OutputDebugString(L"VK_DOWN\n");
                    break;
                }
                case VK_LEFT: {
                    OutputDebugString(L"VK_LEFT\n");
                    break;
                }
                case VK_RIGHT: {
                    OutputDebugString(L"VK_RIGHT\n");
                    break;
                }
                case VK_ESCAPE: {
                    OutputDebugString(L"VK_ESCAPE\n");
                    APP_RUNNING = false;
                    break;
                }
                case VK_SPACE: {
                    OutputDebugString(L"VK_SPACE\n");
                    break;
                }
                case VK_F4: {
                    // Check if the ALT key is pressed
                    if ((lParam & (1 << 29)) != 0) {
                        OutputDebugString(L"ALT-F4\n");
                        APP_RUNNING = false;
                    }
                    break;
                } 
                case 'W': {
                    OutputDebugString(L"W\n");
                    break;
                }
                case 'A': {
                    OutputDebugString(L"A\n");
                    break;
                }
                case 'S': {
                    OutputDebugString(L"S\n");
                    break;
                }
                case 'D': {
                    OutputDebugString(L"D\n");
                    break;
                }
                default:
                    break;
            }
            break;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT Paint;
            HDC DeviceCtx = BeginPaint(WindowHandle, &Paint);
            
            W32_WindowDimensions Dimensions = W32_GetWindowDimensions(WindowHandle);
            W32_DisplayBufferInWindow(&GlobalBackBuffer, DeviceCtx, Dimensions.Width, Dimensions.Height);
            
            EndPaint(WindowHandle, &Paint);
            break;
        }
        
        default: {
            // Let the default window procedure handle any messages we don't care about.
            Result = DefWindowProc(WindowHandle, Message, wParam, lParam);
            break;
        }
    }

    return Result;
}

static void W32_ResizeDIBSection(W32_OffscreenBuffer *Buffer, i32 width, i32 height) {
    if(Buffer->BitmapMemory) {
        VirtualFree(Buffer->BitmapMemory, 0, MEM_RELEASE);
    }

    Buffer->BitmapWidth = width;
    Buffer->BitmapHeight = height;
    Buffer->BytesPerPx = 4;
   
    Buffer->BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    Buffer->BitmapInfo.bmiHeader.biWidth = Buffer->BitmapWidth;
    Buffer->BitmapInfo.bmiHeader.biHeight = -Buffer->BitmapHeight; // Treats the bitmap as top-down, not bottom up. 0,0 = top left
    Buffer->BitmapInfo.bmiHeader.biPlanes = 1;
    Buffer->BitmapInfo.bmiHeader.biBitCount = 32;
    Buffer->BitmapInfo.bmiHeader.biCompression = BI_RGB; // No compression so we can write as fast as possible

    // TODO @null0routed: Should we not free first? Should we free after and fallback to free first
    // if that fails?

    i32 BitmapMemorySize = (Buffer->BitmapWidth * Buffer->BitmapHeight * Buffer->BytesPerPx);

    Buffer->BitmapMemory = VirtualAlloc(
        NULL, // Let Win decide where to alloc
        BitmapMemorySize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!Buffer->BitmapMemory) {
        OutputDebugString(L"VirtualAlloc for the Bitmap memory failed\n");
        APP_RUNNING = false;
    }

    Buffer->Pitch = Buffer->BitmapWidth * Buffer->BytesPerPx; 
}

static void W32_DisplayBufferInWindow(W32_OffscreenBuffer *Buffer, HDC DeviceCtx, i32 windowWidth, i32 windowHeight) {
    
    if (!Buffer->BitmapMemory) { 
       OutputDebugString(L"No BitmapMemory\n");
       APP_RUNNING = false;
       return;
    }

    // TODO @null0routed: Correct the aspect ratio here

    StretchDIBits(
        DeviceCtx,
        0, 0, windowWidth, windowHeight,
        0, 0, Buffer->BitmapWidth, Buffer->BitmapHeight,
        Buffer->BitmapMemory,
        &Buffer->BitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}


static W32_WindowDimensions W32_GetWindowDimensions(HWND window) {
    W32_WindowDimensions Dimensions;

    RECT ClientRect;
    GetClientRect(window, &ClientRect);
    Dimensions.Width = ClientRect.right - ClientRect.left;
    Dimensions.Height = ClientRect.bottom - ClientRect.top;
    return Dimensions;
}

static void W32_InitDSound(HWND Window, i32 BufferSize, i32 SamplesPerSec) {
    // Load the library
    HMODULE DSoundLib = LoadLibrary(L"dsound.dll");
    if (!DSoundLib) {
        OutputDebugString(L"Could not load DSound library.\n");
        return;
    }

    // Get a DirectSound object + cooperative mode
   DirectSoundCreateFn *DirectSoundCreate = (DirectSoundCreateFn *)GetProcAddress(DSoundLib, "DirectSoundCreate");
    if (!DirectSoundCreate) {
        OutputDebugString(L"Could not get DirectSoundCreate.\n");
        return;
    }
    
    LPDIRECTSOUND DirectSound;
    if (FAILED(DirectSoundCreate(0, &DirectSound, 0))) {
        OutputDebugString(L"Could not create DirectSound.\n");
        return;
    }

    if (FAILED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
        OutputDebugString(L"Could not set cooperative level.\n");
        return;
    }
   
    // Create Wave Format for Buffers
    WAVEFORMATEX WaveFormat = {};
    WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    WaveFormat.nChannels = 2;
    WaveFormat.nSamplesPerSec = SamplesPerSec;
    WaveFormat.wBitsPerSample = 16;
    WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
    WaveFormat.cbSize = 0;
    WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
    
    // Create a primary buffer
    LPDIRECTSOUNDBUFFER PrimaryBuffer;
    DSBUFFERDESC BufferDescription = {};
    BufferDescription.dwSize = sizeof(DSBUFFERDESC);
    BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
    BufferDescription.dwBufferBytes = 0;
    if (FAILED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))) {
        OutputDebugString(L"Could not create primary buffer.\n");
        return;
    }
    
    if (FAILED(PrimaryBuffer->SetFormat(&WaveFormat))) {
        OutputDebugString(L"Could not set primary buffer format.\n");
        return;
    }
     
    // Create a secondary buffer
    DSBUFFERDESC SecBufferDescription = {};
    SecBufferDescription.dwSize = sizeof(DSBUFFERDESC);
    SecBufferDescription.dwFlags = 0;
    SecBufferDescription.dwBufferBytes = BufferSize;
    SecBufferDescription.lpwfxFormat = &WaveFormat;

    if (FAILED(DirectSound->CreateSoundBuffer(&SecBufferDescription, &GlobalSoundBuffer, 0))) {
        OutputDebugString(L"Could not create secondary buffer.\n");
        return;
    }

    OutputDebugString(L"Secondary buffer created successfully.\n");
    return;
}

static void W32_DSoundGenerateSineWave(W32_SoundOutput *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite) {

    void *Region1, *Region2;
    DWORD Region1Size, Region2Size;

    if(FAILED(GlobalSoundBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, NULL))) {
        OutputDebugString(L"Could not lock buffer\n");
        printf("Last error: %d\n", GetLastError());
        return;
    }
    
    // TODO @null0routed: Assert region 1 / region 2 sizes are valid
    i32 *SampleOut = (i32 *)Region1;
    DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    for (DWORD idx = 0; idx < Region1SampleCount; ++idx) {
        /*
        i32 SquareWaveOutput = ((RunningSampleIdx++ / (SquareWavePeriod / 2)) % 2) ? 3000 : -3000;
        *SampleOut++ = (SquareWaveOutput << (sizeof(i16)*8)) & SquareWaveOutput;
        */
        r32 t = 2.0f * M_PI * (r32)SoundOutput->RunningSampleIdx / (r32)SoundOutput->WavePeriod;
        r32 SineValue = sinf(t);
        i16 SampleValue = (i16)(SineValue * SoundOutput->Amplitude);
        *SampleOut++ = (SampleValue << 16) & SampleValue;
        ++SoundOutput->RunningSampleIdx;
    }
    
    DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    SampleOut = (i32 *)Region2;
    for (DWORD idx = 0; idx < (Region2SampleCount); ++idx) {
        /*
        i32 SquareWaveOutputR2 = ((RunningSampleIdx++ / (SquareWavePeriod / 2)) % 2) ? 3000 : -3000;
        *SampleOutR2++ = (SquareWaveOutputR2 << (sizeof(i16)*8)) & SquareWaveOutputR2;
        */
        r32 t = 2.0f * M_PI * (r32)SoundOutput->RunningSampleIdx / (r32)SoundOutput->WavePeriod;
        r32 SineValue = sinf(t);
        i16 SampleValue = (i16)(SineValue * SoundOutput->Amplitude);
        *SampleOut++ = (SampleValue << 16) & SampleValue;
        ++SoundOutput->RunningSampleIdx;
    }
    
    GlobalSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

static int W32_InitXAudio2(W32_AudioEngine *AudioEngine, u32 NumChannels, u32 SamplesPerSec, u32 TargetFrameRate, u32 FrameSkip, u32 BufferSafetyFactor) {
    // Load the XAudio2 library
    HMODULE XAudio2Lib = LoadLibrary(L"xaudio2_9.dll");
    if (!XAudio2Lib) {
        OutputDebugString(L"Could not load XAudio2.9 library.\n");

        // Fallback to XAudio2.8
        XAudio2Lib = LoadLibrary(L"xaudio2_8.dll");
        if (!XAudio2Lib) {
            OutputDebugString(L"Could not load XAudio2.8 library.\n");
            CoUninitialize();
            return -1;
        }

        OutputDebugString(L"Using XAudio2.8\n");
    }

    // Get handle to the XAudio2Create function from XAudio2Lib
    XAudio2CreateFn *XAudio2Create = (XAudio2CreateFn *)GetProcAddress(XAudio2Lib, "XAudio2Create");
    if (!XAudio2Create) {
        OutputDebugString(L"Could not get XAudio2Create.\n");
        CoUninitialize();
        return -1;
    }

    // Check AudioEngine for nullptr
    if (!AudioEngine) {
        OutputDebugString(L"Audio engine not initialized.\n");
        CoUninitialize();
        return -1;
    } 
    
    // Create the game XAudio2 object
    // IXAudio2 *XAudio2;
    if (FAILED(XAudio2Create(&AudioEngine->XAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        OutputDebugString(L"Could not create XAudio2.\n");
        CoUninitialize();
        return -1;
    }
    
    // Create mastering voice
    // IXAudio2MasteringVoice *MasteringVoice;
    AUDIO_STREAM_CATEGORY AudioStreamCategory = AudioCategory_GameEffects;
    if (FAILED(AudioEngine->XAudio2->CreateMasteringVoice(&AudioEngine->MasteringVoice, NumChannels, SamplesPerSec, 0, NULL, NULL, AudioStreamCategory))) {
        OutputDebugString(L"Could not create mastering voice.\n");
        CoUninitialize();
        return -1;
    }

    // Populate AudioEngineInfo struct
    AudioEngine->AudioEngineInfo = {};
    AudioEngine->AudioEngineInfo.TargetFrameRate = TargetFrameRate;
    AudioEngine->AudioEngineInfo.FrameSkip = FrameSkip;
    AudioEngine->AudioEngineInfo.BufferSafetyFactor = BufferSafetyFactor;

    // Populate Wave Format
    // WAVEFORMATEX WaveFormat = {};
    AudioEngine->WaveFormat = {};
    AudioEngine->WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    AudioEngine->WaveFormat.nChannels = NumChannels;
    AudioEngine->WaveFormat.nSamplesPerSec = SamplesPerSec;
    AudioEngine->WaveFormat.wBitsPerSample = 16;
    AudioEngine->WaveFormat.nBlockAlign = (AudioEngine->WaveFormat.nChannels * AudioEngine->WaveFormat.wBitsPerSample) / 8;
    AudioEngine->WaveFormat.cbSize = 0;
    AudioEngine->WaveFormat.nAvgBytesPerSec = AudioEngine->WaveFormat.nSamplesPerSec * AudioEngine->WaveFormat.nBlockAlign;

    // Create a source voice
    // IXAudio2SourceVoice *SourceVoice;
    if (FAILED(AudioEngine->XAudio2->CreateSourceVoice(&AudioEngine->SourceVoice, &AudioEngine->WaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL))) {
        OutputDebugString(L"Could not create source voice.\n");
        CoUninitialize();
        return -1;
    }

    // Buffer pre-allocation
    u32 SamplesPerFrame = SamplesPerSec / TargetFrameRate;
    int BufferSize = SamplesPerFrame * FrameSkip * AudioEngine->AudioEngineInfo.BufferSafetyFactor * AudioEngine->WaveFormat.nChannels * (AudioEngine->WaveFormat.wBitsPerSample / 8);
    // int _BufferSize = AudioEngine->WaveFormat.nSamplesPerSec * AudioEngine->WaveFormat.nChannels * (AudioEngine->WaveFormat.wBitsPerSample / 8) * duration;
    AudioEngine->SourceBuffer = (BYTE *)VirtualAlloc(NULL, BufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!AudioEngine->SourceBuffer) {
        OutputDebugString(L"Could not allocate sound buffer.\n");
        return -1;
    }

    AudioEngine->AudioBuffer = {};
    AudioEngine->AudioBuffer.Flags = 0;
    AudioEngine->AudioBuffer.AudioBytes = BufferSize; // Size of the buffer in bytes
    AudioEngine->AudioBuffer.LoopBegin = 0;
    AudioEngine->AudioBuffer.LoopLength = (SamplesPerFrame * FrameSkip); // Loop = # samples we play x # frames to skip loading reducing CPU load
    AudioEngine->AudioBuffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    AudioEngine->AudioBuffer.pAudioData = AudioEngine->SourceBuffer; // Pointer to the audio data
    AudioEngine->AudioBuffer.pContext = NULL;     

    return 0;
}

static void W32_XAudio2CreateSource(IXAudio2SourceVoice **SourceVoice, WAVEFORMATEX *WaveFormat, void *AudioData, u32 AudioDataSize) {
    // Create source voice
    // TODO @null0routed: Should probably move this into a seperate function. Should I track these as part of the global struct?
    if (FAILED(GlobalAudioEngine.XAudio2->CreateSourceVoice(SourceVoice, WaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL))) {
        OutputDebugString(L"Could not create source voice.\n");
        CoUninitialize();
        return;
    }

    XAUDIO2_BUFFER AudioBuffer = {};
    AudioBuffer.Flags = 0;
    AudioBuffer.AudioBytes = AudioDataSize; // Size of the buffer in bytes
    AudioBuffer.LoopBegin = 0;
    AudioBuffer.LoopLength = 0;
    AudioBuffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    AudioBuffer.pAudioData = (BYTE *)AudioData; // Pointer to the audio data
    AudioBuffer.pContext = NULL;
    
    return;
}

static int W32_XAudioGenerateSquareWave(W32_AudioEngine *AudioEng, float freq, i16 amplitude) {

    if (!AudioEng) {
        OutputDebugString(L"Audio engine not initialized.\n");
        CoUninitialize();
        return -1;
    }

    u32 SamplesPerFrame = AudioEng->WaveFormat.nSamplesPerSec / AudioEng->AudioEngineInfo.TargetFrameRate;
    u32 BufferSize = SamplesPerFrame * AudioEng->AudioEngineInfo.FrameSkip * AudioEng->AudioEngineInfo.BufferSafetyFactor * AudioEng->WaveFormat.nChannels * (AudioEng->WaveFormat.wBitsPerSample / 8);

    // Ensure BufferSize doesn't exceed the buffer capacity
    if (BufferSize > AudioEng->AudioBuffer.AudioBytes) {
        OutputDebugString(L"Error: Calculated BufferSize exceeds SourceBufferCapacity.\n");
        return -1; // Or handle this error appropriately
    }

    // Verify SourceBuffer is properly aligned for 16-bit writes.  This is critical!
    if ((uintptr_t)AudioEng->SourceBuffer % sizeof(i16) != 0) {
        OutputDebugString(L"Error: SourceBuffer not properly aligned for i16 writes.\n");
        return -1;
    }

    // Main loop
    // int NumSamples = AudioEng->WaveFormat.nSamplesPerSec * duration;
    int NumSamples = BufferSize / (AudioEng->WaveFormat.nChannels * sizeof(i16));
    int PeriodInSamples = AudioEng->WaveFormat.nSamplesPerSec / (int)freq;
    i16 *CurrentWritePos = (i16 *)AudioEng->SourceBuffer;
    for (int i = 0; i < NumSamples; ++i) {
        // Generate a square wave with period of 1/freq
        i16 ChannelSample = ((i % PeriodInSamples) <= (PeriodInSamples / 2)) ? amplitude : -amplitude;
            *CurrentWritePos++ = ChannelSample;
            *CurrentWritePos++ = ChannelSample;
    }

    return 0;
}

static int W32_XAudioGenerateSineWave(W32_AudioEngine *AudioEng, float freq, i16 amplitude) {

    if (!AudioEng) {
        OutputDebugString(L"Audio engine not initialized.\n");
        CoUninitialize();
        return -1;
    }

    u32 SamplesPerFrame = AudioEng->WaveFormat.nSamplesPerSec / AudioEng->AudioEngineInfo.TargetFrameRate;
    u32 BufferSize = SamplesPerFrame * AudioEng->AudioEngineInfo.FrameSkip * AudioEng->AudioEngineInfo.BufferSafetyFactor * AudioEng->WaveFormat.nChannels * (AudioEng->WaveFormat.wBitsPerSample / 8);
    AudioEng->AudioBuffer.LoopLength = AudioEng->WaveFormat.nSamplesPerSec / (int)freq; // This should be the samples per period of the sine wave

    // Ensure BufferSize doesn't exceed the buffer capacity
    if (BufferSize > AudioEng->AudioBuffer.AudioBytes) {
        OutputDebugString(L"Error: Calculated BufferSize exceeds SourceBufferCapacity.\n");
        return -1; // Or handle this error appropriately
    }

    // Verify SourceBuffer is properly aligned with 16-bit chunks 
    if ((uintptr_t)AudioEng->SourceBuffer % sizeof(i16) != 0) {
        OutputDebugString(L"Error: SourceBuffer not properly aligned for i16 writes.\n");
        return -1;
    }

    // Grab a temp buffer to write the next audio to
    BYTE *tempBuffer = new BYTE[BufferSize];

    if ((uintptr_t)tempBuffer % sizeof(i16) != 0) {
        OutputDebugString(L"Error: tempBuffer not properly aligned for i16 writes.\n");
        return -1;
    }

    // Main loop
    // int NumSamples = AudioEng->WaveFormat.nSamplesPerSec * duration;
    int NumSamples = BufferSize / (AudioEng->WaveFormat.nChannels * sizeof(i16));
    // int PeriodInSamples = AudioEng->WaveFormat.nSamplesPerSec / (int)freq;
    r32 phaseIncrement = 2.0f * M_PI * freq / AudioEng->WaveFormat.nSamplesPerSec;
    // i16 *CurrentWritePos = (i16 *)AudioEng->SourceBuffer;
    i16 *CurrentWritePos = (i16 *)tempBuffer;
    for (int i = 0; i < NumSamples; ++i) {
        // Generate a sine wave with period of 1/freq
        r32 SineValue = sinf(phaseIncrement * i);
        i16 ChannelSample = (i16)(SineValue * amplitude);

        *CurrentWritePos++ = ChannelSample;
        *CurrentWritePos++ = ChannelSample;
    }

    // Copy the tempBuffer to the actual buffer
    memcpy(AudioEng->SourceBuffer, tempBuffer, BufferSize);

    delete[] tempBuffer;

    return 0;
}

static int W32_XAudioSubmitSourceBuffer(IXAudio2SourceVoice *SourceVoice, XAUDIO2_BUFFER *AudioBuffer) {
    // SourceVoice nullptr check
    if (!SourceVoice) {
        OutputDebugString(L"Source voice not initialized.\n");
        CoUninitialize();
        return -1;
    }

    // AudioBuffer nullptr check
    if (!AudioBuffer) {
        OutputDebugString(L"Audio buffer not initialized.\n");
        CoUninitialize();
        return -1;
    }

    // Submit Source Voice
    if (FAILED(SourceVoice->SubmitSourceBuffer(AudioBuffer, NULL))) {
        OutputDebugString(L"Could not submit source buffer.\n");
        CoUninitialize();
        return -1;
    }

    // Play the sound
    if (FAILED(SourceVoice->Start(0))) {
        OutputDebugString(L"Could not start source voice.\n");
        CoUninitialize();
        return -1;
    }

    return 0;
}
