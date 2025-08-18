#define UNICODE
#define _UNICODE

#include <Windows.h>
#include <Xinput.h>
#include "types.h"

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

// TODO @null0routed: Refactor this later
bool APP_RUNNING;
static W32_OffscreenBuffer GlobalBackBuffer;

LRESULT CALLBACK W32_WindowProc(HWND, UINT, WPARAM, LPARAM); // Application Window Procedure
static void W32_ResizeDIBSection(W32_OffscreenBuffer *, i32, i32); // Initialize or resize Device Independant Bitmap
static void W32_DisplayBufferInWindow(W32_OffscreenBuffer *, HDC, i32, i32);
static void W32_RenderGradient(W32_OffscreenBuffer *, i32, i32);
static W32_WindowDimensions W32_GetWindowDimensions(HWND);

// External definitions for XInput

/*
Probably should change this in the future to:
# define XInputGetState XInputGetState_
typedef DWORD (WINAPI W32_XInputGetState_t *)(DWORD, XINPUT_STATE*)
static DWORD WINAPI W32_XInputGetStateStub(DWORD dwUserIndex, XINPUT_STATE *pStateInfo) {
    return 0;
} 
static W32_XInputGetState_t XInputGetState_ = XInputGetStateStub

*/

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
    
    // Initialization
    SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT); // Ensure safe search paths for LoadLibrary
    W32_LoadXInput(); 
    
    LPCWSTR CLASS_NAME = L"GameEngine";
    WNDCLASSEXW WindowClass = {};
    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.hInstance = Instance;
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpszClassName = CLASS_NAME;
    WindowClass.lpfnWndProc = W32_WindowProc;

    W32_ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

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

    i32 XOffset = 0;
    i32 YOffset = 0;
    APP_RUNNING = true;

    HDC DeviceCtx = GetDC(Window); // Get DC here because we created window class with "own dc"
    MSG Message; // Declared here to reduce allocations during message loop
    
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
                OutputDebugString(L"Could not get controller state\n");
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
        }

        XINPUT_VIBRATION Vibration;
        Vibration.wLeftMotorSpeed = 60000;
        Vibration.wRightMotorSpeed = 60000;
        XInputSetState(0, &Vibration);

        W32_RenderGradient(&GlobalBackBuffer, XOffset, YOffset);

        W32_WindowDimensions Dimensions = W32_GetWindowDimensions(Window);
        W32_DisplayBufferInWindow(&GlobalBackBuffer, DeviceCtx, Dimensions.Width, Dimensions.Height);
        
        ++XOffset;
        ++YOffset;
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

static void W32_RenderGradient(W32_OffscreenBuffer *Buffer, i32 XOffset, i32 YOffset) {
    u8 *Row = (u8 *)Buffer->BitmapMemory;

    for (i32 Y = 0; Y < Buffer->BitmapHeight; ++Y) {
        u32 *Pixel = (u32 *)Row;
        for (i32 X = 0; X < Buffer->BitmapWidth; ++X) {
            u8 Blue = (X + XOffset);
            u8 Green = (Y + YOffset);
            *Pixel++ = (Green << 8) | Blue;
        }

        // Can do less allocation with:
        // Row = (u8 *)Pixel;
        Row += Buffer->Pitch;
    }
}

static W32_WindowDimensions W32_GetWindowDimensions(HWND window) {
    W32_WindowDimensions Dimensions;

    RECT ClientRect;
    GetClientRect(window, &ClientRect);
    Dimensions.Width = ClientRect.right - ClientRect.left;
    Dimensions.Height = ClientRect.bottom - ClientRect.top;
    return Dimensions;
} 