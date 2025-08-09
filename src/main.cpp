#include <Windows.h>
#include "types.h"

// TODO @null0routed: Refactor this later
static bool APP_RUNNING;
static BITMAPINFO BitmapInfo = {};
static void *BitmapMemory;
static int BitmapWidth;
static int BitmapHeight;

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM); // Application Window Procedure
static void ResizeDIBSection(int, int); // Initialize or resize Device Independant Bitmap
static void EngineUpdateWindow(HDC, RECT *, int, int, int, int);
static void RenderGradient(int, int);

int APIENTRY WinMain(HINSTANCE Instance, HINSTANCE Parent, PSTR CommandLine, int ShowCode) {
    
    LPCWSTR CLASS_NAME = L"GameEngine";
    WNDCLASSEXW WindowClass = {};
    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.hInstance = Instance;
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
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

    int XOffset = 0;
    int YOffset = 0;
    APP_RUNNING = true;
    while(APP_RUNNING) {
        MSG Message;
        while (PeekMessage(&Message, Window, 0, 0, PM_REMOVE)) {
            if (Message.message == WM_QUIT) {
                APP_RUNNING = false;
            }
            
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        RenderGradient(XOffset, YOffset);

        HDC DeviceCtx = GetDC(Window);
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        int WindowWidth = ClientRect.right - ClientRect.left;
        int WindowHeight = ClientRect.bottom - ClientRect.top;
        EngineUpdateWindow(DeviceCtx, &ClientRect, 0, 0, WindowWidth, WindowHeight);
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
            int width = ClientRect.right - ClientRect.left;
            int height = ClientRect.bottom - ClientRect.top;
            ResizeDIBSection(width, height);
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

            int width = Paint.rcPaint.right - Paint.rcPaint.left;
            int height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            PatBlt(DeviceCtx, Paint.rcPaint.left, Paint.rcPaint.top, width, height, WHITENESS);
            EngineUpdateWindow(DeviceCtx, &ClientRect, Paint.rcPaint.left, Paint.rcPaint.top, width, height);
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

static void ResizeDIBSection(int width, int height) {
    if(BitmapMemory) {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }
   
    BitmapWidth = width;
    BitmapHeight = height;
    
    BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    BitmapInfo.bmiHeader.biWidth = width;
    BitmapInfo.bmiHeader.biHeight = -height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB; // No compression so we can write as fast as possible

    // TODO @null0routed: Should we not free first? Should we free after and fallback to free first
    // if that fails?

    int BytesPerPx = 4;
    int BitmapMemorySize = (width * height * BytesPerPx);

    BitmapMemory = VirtualAlloc(
        NULL, // Let Win decide where to alloc
        BitmapMemorySize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!BitmapMemory) {
        OutputDebugStringW(L"VirtualAlloc for the Bitmap memory failed\n");
        APP_RUNNING = false;
    }
}

static void EngineUpdateWindow(HDC DeviceCtx, RECT *WindowRect, int x, int y, int width, int height) {
    
    if (!BitmapMemory) { 
       OutputDebugStringW(L"No BitmapMemory\n");
       APP_RUNNING = false;
       return;
    }

    int WindowWidth = WindowRect->right - WindowRect->left;
    int WindowHeight = WindowRect->bottom - WindowRect->top;

    StretchDIBits(
        DeviceCtx,
        0, 0, BitmapWidth, BitmapHeight,
        0, 0, WindowWidth, WindowHeight,
        BitmapMemory,
        &BitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

static void RenderGradient(int XOffset, int YOffset) {
    int BytesPerPx = 4;
    int Pitch = BitmapWidth * BytesPerPx; 
    u8 *Row = (u8 *)BitmapMemory;

    for (int Y = 0; Y < BitmapHeight; ++Y) {
        u32 *Pixel = (u32 *)Row;
        for (int X = 0; X < BitmapWidth; ++X) {
            u8 Blue = (X + XOffset);
            u8 Green = (Y + YOffset);
            *Pixel++ = (Green << 8) | Blue;
        }

        // Can do less allocation with:
        // Row = (u8 *)Pixel;
        Row += Pitch;
    }
}