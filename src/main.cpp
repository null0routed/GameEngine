#include <Windows.h>
#include "types.h"

struct OffscreenBuffer {
    BITMAPINFO BitmapInfo = {};
    void *BitmapMemory;
    i32 BitmapWidth;
    i32 BitmapHeight;
    i32 BytesPerPx;
    i32 Pitch;
};

// TODO @null0routed: Refactor this later
bool APP_RUNNING;
static OffscreenBuffer GlobalBackBuffer;

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM); // Application Window Procedure
static void ResizeDIBSection(OffscreenBuffer *Buffer, i32, i32); // Initialize or resize Device Independant Bitmap
static void DisplayBufferInWindow(HDC, RECT, OffscreenBuffer, i32, i32, i32, i32);
static void RenderGradient(OffscreenBuffer *Buffer, i32, i32);

int APIENTRY WinMain(HINSTANCE Instance, HINSTANCE Parent, PSTR CommandLine, int ShowCode) {
    
    LPCWSTR CLASS_NAME = L"GameEngine";
    WNDCLASSEXW WindowClass = {};
    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.hInstance = Instance;
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpszClassName = CLASS_NAME;
    WindowClass.lpfnWndProc = WindowProc;

    if (!RegisterClassExW(&WindowClass)) {
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
        OutputDebugStringW(L"Window Failed\n");
        return -1;
    }

    ShowWindow(Window, SW_SHOW);

    i32 XOffset = 0;
    i32 YOffset = 0;
    APP_RUNNING = true;
    MSG Message; // Declared here to reduce allocations during message loop
    
    while(APP_RUNNING) {
        while (PeekMessage(&Message, Window, 0, 0, PM_REMOVE)) {
            if (Message.message == WM_QUIT) {
                APP_RUNNING = false;
            }
            
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        RenderGradient(&GlobalBackBuffer, XOffset, YOffset);

        HDC DeviceCtx = GetDC(Window);
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        i32 WindowWidth = ClientRect.right - ClientRect.left;
        i32 WindowHeight = ClientRect.bottom - ClientRect.top;
        DisplayBufferInWindow(DeviceCtx, ClientRect, GlobalBackBuffer, 0, 0, WindowWidth, WindowHeight);
        ReleaseDC(Window, DeviceCtx);
        
        ++XOffset;
        ++YOffset;
    }  

    return 0;
}

LRESULT CALLBACK WindowProc(HWND WindowHandle, UINT Message, WPARAM wParam, LPARAM lParam) {
    LRESULT Result = 0;
    
    switch (Message) {
        case WM_SIZE: {
            OutputDebugStringW(L"WM_SIZE\n");
            
            RECT ClientRect;
            GetClientRect(WindowHandle, &ClientRect);
            i32 width = ClientRect.right - ClientRect.left;
            i32 height = ClientRect.bottom - ClientRect.top;
            ResizeDIBSection(&GlobalBackBuffer, width, height);
            break;
        } 
        case WM_DESTROY: {
            OutputDebugStringW(L"WM_DESTROY\n");
            // This is a good place to post the quit message to exit the application.
            // PostQuitMessage(0);
            // TODO @null0routed: Handle this as an error - recreate window?
            APP_RUNNING = false;
            break;
        }
        case WM_CLOSE: {
            OutputDebugStringW(L"WM_CLOSE\n");
            // We should destroy the window, which will trigger WM_DESTROY.
            // DestroyWindow(WindowHandle);
            // TODO @null0routed: Handle this as a message to the user
            APP_RUNNING = false;
            break;
        }
        case WM_ACTIVATEAPP: {
            OutputDebugStringW(L"WM_ACTIVATEAPP\n");
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT Paint;
            HDC DeviceCtx = BeginPaint(WindowHandle, &Paint);
            RECT ClientRect;
            GetClientRect(WindowHandle, &ClientRect);

            i32 width = Paint.rcPaint.right - Paint.rcPaint.left;
            i32 height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            PatBlt(DeviceCtx, Paint.rcPaint.left, Paint.rcPaint.top, width, height, WHITENESS);
            DisplayBufferInWindow(DeviceCtx, ClientRect, GlobalBackBuffer, Paint.rcPaint.left, Paint.rcPaint.top, width, height);
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

static void ResizeDIBSection(OffscreenBuffer *Buffer, i32 width, i32 height) {
    if(Buffer->BitmapMemory) {
        VirtualFree(Buffer->BitmapMemory, 0, MEM_RELEASE);
    }

    Buffer->BitmapWidth = width;
    Buffer->BitmapHeight = height;
   
    Buffer->BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    Buffer->BitmapInfo.bmiHeader.biWidth = Buffer->BitmapWidth;
    Buffer->BitmapInfo.bmiHeader.biHeight = -Buffer->BitmapHeight;
    Buffer->BitmapInfo.bmiHeader.biPlanes = 1;
    Buffer->BitmapInfo.bmiHeader.biBitCount = 32;
    Buffer->BitmapInfo.bmiHeader.biCompression = BI_RGB; // No compression so we can write as fast as possible

    // TODO @null0routed: Should we not free first? Should we free after and fallback to free first
    // if that fails?

    Buffer->BytesPerPx = 4;
    i32 BitmapMemorySize = (Buffer->BitmapWidth * Buffer->BitmapHeight * Buffer->BytesPerPx);

    Buffer->BitmapMemory = VirtualAlloc(
        NULL, // Let Win decide where to alloc
        BitmapMemorySize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!Buffer->BitmapMemory) {
        OutputDebugStringW(L"VirtualAlloc for the Bitmap memory failed\n");
        APP_RUNNING = false;
    }

    Buffer->Pitch = Buffer->BitmapWidth * Buffer->BytesPerPx; 
}

static void DisplayBufferInWindow(HDC DeviceCtx, RECT WindowRect, OffscreenBuffer Buffer, i32 x, i32 y, i32 width, i32 height) {
    
    if (!Buffer.BitmapMemory) { 
       OutputDebugStringW(L"No BitmapMemory\n");
       APP_RUNNING = false;
       return;
    }

    i32 WindowWidth = WindowRect.right - WindowRect.left;
    i32 WindowHeight = WindowRect.bottom - WindowRect.top;

    StretchDIBits(
        DeviceCtx,
        0, 0, Buffer.BitmapWidth, Buffer.BitmapHeight,
        0, 0, WindowWidth, WindowHeight,
        Buffer.BitmapMemory,
        &Buffer.BitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

static void RenderGradient(OffscreenBuffer *Buffer, i32 XOffset, i32 YOffset) {
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