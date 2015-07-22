// Copyright © Microsoft Corp.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// • Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// • Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.


#include "stdafx.h"
#include "JXRViewer.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
TCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, 
    _In_ LPTSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
    MSG msg;
    HACCEL hAccelTable;

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_JXRVIEWER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_JXRVIEWER));

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_JXRVIEWER));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;//(HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_JXRVIEWER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;

    hInst = hInstance; // Store instance handle in our global variable

    hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

static HBITMAP gs_hSrcBmp;
static unsigned int gs_width, gs_height;
static HDC gs_hMemDC;
static HRGN gs_bgRgn;
static HBRUSH gs_bgBrush;
static unsigned int gs_scale = 1;
static bool gs_transparency;
static COLORREF gs_bgColor = 0x00000000; // BLACK
static enum SUBBAND gs_subBand;
static RasterImage gs_rasterImage;

static void FreeBitmap()
{
    if (NULL != gs_hSrcBmp)
    {
        DeleteObject(gs_hSrcBmp);
        gs_hSrcBmp = NULL;
    }
}

inline unsigned int BMPLineSize(unsigned int width, int bytesPP)
{
    return ((bytesPP * width + 3) / 4) * 4;
}

HBITMAP CreateColorDIB(unsigned int width, unsigned int height, 
    unsigned int bitDepth, void **pBits)
{
    if (NULL != pBits)
        *pBits = NULL;

    if (!(24 == bitDepth || 32 == bitDepth))
        return NULL;

    BITMAPINFO bmi;
    BITMAPINFOHEADER &bmih = bmi.bmiHeader;
    bmih.biBitCount = bitDepth;
    bmih.biClrImportant = 0;
    bmih.biClrUsed = 0;
    bmih.biCompression = BI_RGB;
    bmih.biPlanes = 1;
    bmih.biSize = sizeof(BITMAPINFOHEADER);
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biWidth = width;
    bmih.biHeight = height;

    void *bits;
    HBITMAP hBmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);

    if (NULL != pBits)
        *pBits = bits;

    return hBmp;
}

static void OnSize(HWND hWnd, unsigned int width, unsigned int height)
{
    gs_width = width;
    gs_height = height;

    SetRectRgn(gs_bgRgn, 0, 0, gs_width, gs_height);

    if (NULL != gs_hSrcBmp)
    {
        BITMAP bm;
        GetObject(gs_hSrcBmp, sizeof(bm), &bm);
        unsigned int srcW = bm.bmWidth;
        unsigned int srcH = bm.bmHeight;

        HRGN imgRgn = CreateRectRgn(0, 0, srcW, srcH);
        CombineRgn(gs_bgRgn, gs_bgRgn, imgRgn, RGN_DIFF);
        DeleteObject(imgRgn);
    }
}

static void OnSetImageSize(uint32_t width, uint32_t height, void *param)
{
    void *destData = NULL;
    gs_hSrcBmp = CreateColorDIB(width, height, 32, &destData); // always create a 32-bit bitmap
    BITMAP bm;
    GetObject(gs_hSrcBmp, sizeof(BITMAP), &bm);

    HWND hWnd = (HWND)param;
    OnSize(hWnd, gs_width, gs_height);
    InvalidateRect(hWnd, NULL, FALSE);
    UpdateWindow(hWnd);
}

inline unsigned char AlphaComposite(unsigned char fg, unsigned char alpha, unsigned char bg)
{
    unsigned short temp = (unsigned short)(fg) * (unsigned short)(alpha) + 
        (unsigned short)(bg) * (unsigned short)(0xFF - (unsigned short)(alpha)) + 
        (unsigned short)0x80;

    unsigned char composite = (unsigned char)((temp + (temp >> 8)) >> 8);
    return composite;
}
/*
static void DisplayDecodedImage(const unsigned char *srcData, 
    unsigned int width, unsigned int height, 
    unsigned int srcRowStride, unsigned int bpp, bool alphaPresent)
{
    unsigned char *destData = NULL;
    gs_hSrcBmp = CreateColorDIB(width, height, bpp, (void **)&destData);
    const unsigned char *sl = srcData;

    BITMAP bm;
    GetObject(gs_hSrcBmp, sizeof(BITMAP), &bm);
    unsigned int destRowStride = bm.bmWidthBytes;
    unsigned char *dl = destData + destRowStride * (height - 1);

    unsigned char bgR = GetRValue(gs_bgColor);
    unsigned char bgG = GetGValue(gs_bgColor);
    unsigned char bgB = GetBValue(gs_bgColor);

    if (alphaPresent)
    {
        struct BGRA
        {
            unsigned char b, g, r, a;
        };

        for (unsigned int i = 0; i < height; ++i)
        {
            BGRA *d = (BGRA *)dl;
            const BGRA *s = (const BGRA *)sl;
            unsigned int vSqIndex = i >> 3; // i / 8;

            for (unsigned int j = 0; j < width; ++j)
            {
                const BGRA &sp = *s;
                BGRA &dp = *d;

                if (gs_transparency)
                {
                    unsigned int hSqIndex = j >> 3; // j / 8;
                    unsigned char sqColor = 0 == (vSqIndex + hSqIndex) % 2 ? 0xC0 : 0xFF;
                    bgR = bgG = bgB = sqColor;
                }

                dp.r = AlphaComposite(sp.r, sp.a, bgR);
                dp.g = AlphaComposite(sp.g, sp.a, bgG);
                dp.b = AlphaComposite(sp.b, sp.a, bgB);
                ++s;
                ++d;
            }

            sl += srcRowStride;
            dl -= destRowStride;
        }
    }
    else
    {
        for (unsigned int i = 0; i < height; ++i)
        {
            CopyMemory(dl, sl, srcRowStride);
            sl += srcRowStride;
            dl -= destRowStride;
        }
    }
}
*/
static void OnInvalidate(uint32_t rl, uint32_t rt, uint32_t rw, uint32_t rh, void *param)
{
    RECT r;
    r.left = rl;
    r.top = rt;
    r.right = rw;
    r.bottom = r.top + rh;
    InvalidateRect((HWND)param, &r, FALSE);
    UpdateWindow((HWND)param);
}

static void OnImageRowsAvailable(const uint8_t *src, uint32_t srcRowStride, uint32_t srcBPP, 
    uint32_t rl, uint32_t rt, uint32_t rw, uint32_t rh, bool alphaPresent, void *param)
{
    BITMAP bm;
    GetObject(gs_hSrcBmp, sizeof(BITMAP), &bm);
    unsigned char *dest = (unsigned char *)bm.bmBits;
    unsigned int destRowStride = bm.bmWidthBytes;
    unsigned int width = bm.bmWidth;
    unsigned int height = bm.bmHeight;
    unsigned int destBPP = bm.bmBitsPixel / 8;
    unsigned char *dl = dest + (height - rt - 1) * destRowStride + rl * destBPP;
    const unsigned char *sl = src;
    unsigned int rowStride = Min(srcRowStride, destRowStride);

    if (NULL == src)
    {
        unsigned char *dl = dest;

        // A special command - we just need to cleanup the output without resizing it
        for (unsigned int i = 0; i < height; ++i)
        {
            memset(dl, 0, width * sizeof(int));
            dl += destRowStride;
        }

        return;
    }

    if (alphaPresent)
    {
        struct BGRA
        {
            unsigned char b, g, r, a;
        };

        for (unsigned int i = 0; i < rh; ++i)
        {
            BGRA *d = (BGRA *)dl;
            const BGRA *s = (const BGRA *)sl;

            for (unsigned int j = 0; j < rw; ++j)
            {
                const BGRA &sp = *s;
                BGRA &dp = *d;
                dp.r = AlphaComposite(sp.r, sp.a, 0xFF);
                dp.g = AlphaComposite(sp.g, sp.a, 0xFF);
                dp.b = AlphaComposite(sp.b, sp.a, 0xFF);
                ++s;
                ++d;
            }

            sl += srcRowStride;
            dl -= destRowStride;
        }
    }
    else
    {
        struct BGRA
        {
            unsigned char b, g, r, a;
        };

        for (unsigned int i = 0; i < rh; ++i)
        {
            BGRA *d = (BGRA *)dl;
            const unsigned char *s = sl;

            for (unsigned int j = 0; j < rw; ++j)
            {
                const BGRA &sp = *(const BGRA *)s;
                BGRA &dp = *d;
                dp.r = sp.r;
                dp.g = sp.g;
                dp.b = sp.b;
                s += srcBPP;
                ++d;
            }

            sl += srcRowStride;
            dl -= destRowStride;
        }
    }

    RECT r;
    r.left = rl;
    r.top = rt;
    r.right = rw;
    r.bottom = r.top + rh;
    InvalidateRect((HWND)param, &r, FALSE);
    UpdateWindow((HWND)param);
}

static bool LoadFile(HWND hWnd, const TCHAR *path)
{
    if (NULL != gs_hSrcBmp)
    {
        DeleteObject(gs_hSrcBmp);
        gs_hSrcBmp = NULL;
    }

    const TCHAR *ext = _tcsrchr(path, _T('.'));
    bool unknown = false;

    if (NULL == ext)
        unknown = true;
    else if (0 == _tcsicmp(ext, _T(".bmp"))) // Windows bitmap file
    {
        gs_hSrcBmp = (HBITMAP)LoadImage(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

        if (NULL == gs_hSrcBmp)
            MessageBox(hWnd, path, _T("Could not load the file"), MB_OK | MB_ICONERROR);
        else
        {
            BITMAP bm;
            GetObject(gs_hSrcBmp, sizeof(BITMAP), &bm);

            if (bm.bmBitsPixel != 24 && bm.bmBitsPixel != 32)
            {
                DeleteObject(gs_hSrcBmp);
                gs_hSrcBmp = NULL;

                MessageBox(hWnd, _T("Only 24-bit and 32-bit Windows bitmaps are supported"), 
                    _T("Sorry..."), MB_OK | MB_ICONERROR);
            }
        }
    }
    else if (0 == _tcsicmp(ext, _T(".jxr")) || 0 == _tcsicmp(ext, _T(".hdp")) || 0 == _tcsicmp(ext, _T(".wdp"))) // JPEG-XR file
    {
        gs_rasterImage.SetOnImageRowsAvailable(OnImageRowsAvailable, hWnd);
        gs_rasterImage.SetOnInvalidate(OnInvalidate, hWnd);
        gs_rasterImage.SetOnSetSize(OnSetImageSize, hWnd);
        bool res = Decode(path, gs_rasterImage, gs_scale, SB_ALL);

        if (!res)
            MessageBox(hWnd, _T("Error decoding the source file"), 
                _T("Error"), MB_OK | MB_ICONERROR);
    }
    else
        unknown = true;

    OnSize(hWnd, gs_width, gs_height);
    InvalidateRect(hWnd, NULL, FALSE);

    if (unknown)
        MessageBox(hWnd, path, _T("Unknown file type"), MB_OK | MB_ICONERROR);

    if (NULL == gs_hSrcBmp)
        SetWindowText(hWnd, szTitle);
    else
    {
        String title = path;
        title += _T(" - ");
        title += szTitle;
        SetWindowText(hWnd, title.c_str());
    }

    return NULL != gs_hSrcBmp;
}

static bool OnDropFiles(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    HDROP hDrop = (HDROP)wParam;
    TCHAR path[1024];

    //UINT numObjects = DragQueryFile(hDrop, (UINT)-1, NULL, 0);

    UINT uiRes = DragQueryFile(hDrop, 0, path, sizeof(path) / sizeof(path[0]));
    DragFinish(hDrop);

    LoadFile(hWnd, path);
    return true;
}

static void OnPaint(HWND hWnd, HDC dc, const RECT &rc)
{
    HRGN rgn = CreateRectRgnIndirect(&rc);
    int res = CombineRgn(rgn, rgn, gs_bgRgn, RGN_AND);
    FillRgn(dc, rgn, gs_bgBrush);
    DeleteObject(rgn);

    HBITMAP hBmp = gs_hSrcBmp;

    if (NULL != hBmp)
    {
        BITMAP bm;
        GetObject(gs_hSrcBmp, sizeof(BITMAP), &bm);
        RECT r;
        r.left = 0;
        r.top = 0;
        r.right = bm.bmWidth;
        r.bottom = bm.bmHeight;
        BOOL is = IntersectRect(&r, &r, &rc);

        if (is)
        {
            SelectObject(gs_hMemDC, hBmp);
            BitBlt(dc, r.left, r.top, r.right - r.left + 1, r.bottom - r.top + 1, gs_hMemDC, r.left, r.top, SRCCOPY);
        }
    }
}

static HBRUSH CreateCheckerBoardBrush()
{
    HDC ddc = GetDC(0);
    HDC dc = CreateCompatibleDC(ddc);
    HBITMAP hBmp = CreateCompatibleBitmap(ddc, 16, 16);
    ReleaseDC(0, ddc);

    HBITMAP hOldBmp = (HBITMAP)SelectObject(dc, hBmp);

    // Paint the checker board
    RECT r;
    r.left = r.top = 0;
    r.bottom = r.right = 16;
    FillRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));

    r.bottom = r.right = 8;
    FillRect(dc, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

    r.left = r.top = 8;
    r.bottom = r.right = 16;
    FillRect(dc, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

    SelectObject(dc, hOldBmp);
    DeleteDC(dc);

    HBRUSH br = CreatePatternBrush(hBmp);
    DeleteObject(hBmp);
    return br;
}

static void SetScale(HWND hWnd, UINT id)
{
    HMENU hMenu = GetMenu(hWnd);
    HMENU hSubMenu = GetSubMenu(hMenu, 2);
    hSubMenu = GetSubMenu(hSubMenu, 1);

    CheckMenuRadioItem(hSubMenu, ID_SET_SCALE_1, ID_SET_SCALE_16, id, MF_BYCOMMAND);

    switch (id)
    {
    case ID_SET_SCALE_1:
        gs_scale = 1;
        break;

    case ID_SET_SCALE_2:
        gs_scale = 2;
        break;

    case ID_SET_SCALE_4:
        gs_scale = 4;
        break;

    case ID_SET_SCALE_8:
        gs_scale = 8;
        break;

    case ID_SET_SCALE_16:
        gs_scale = 16;
        break;
    }
}

static void SetSubBand(HWND hWnd, UINT id)
{
    HMENU hMenu = GetMenu(hWnd);
    HMENU hSubMenu = GetSubMenu(hMenu, 2);
    hSubMenu = GetSubMenu(hSubMenu, 2);

    CheckMenuRadioItem(hSubMenu, ID_SB_ALL, ID_SB_NO_FLEXBITS, id, MF_BYCOMMAND);

    switch (id)
    {
    case ID_SB_ALL:
        gs_subBand = SB_ALL;
        break;

    case ID_SB_DC_ONLY:
        gs_subBand = SB_DC_ONLY;
        break;

    case ID_SB_NO_HIGHPASS:
        gs_subBand = SB_NO_HIGHPASS;
        break;

    case ID_SB_NO_FLEXBITS:
        gs_subBand = SB_NO_FLEXBITS;
        break;
    }
}

static void SetBackground(HWND hWnd, UINT id)
{
    HMENU hMenu = GetMenu(hWnd);
    HMENU hSubMenu = GetSubMenu(hMenu, 1);
    hSubMenu = GetSubMenu(hSubMenu, 0);

    CheckMenuRadioItem(hSubMenu, ID_BG_BLACK, ID_BG_TRANSPARENT, id, MF_BYCOMMAND);

    switch (id)
    {
    case ID_BG_BLACK:
        gs_transparency = false;
        gs_bgColor = 0x00000000;
        break;

    case ID_BG_WHITE:
        gs_transparency = false;
        gs_bgColor = 0xFFFFFFFF;
        break;

    case ID_BG_TRANSPARENT:
        gs_transparency = true;
        break;
    }
}

static void OnCreate(HWND hWnd)
{
    gs_bgBrush = CreateCheckerBoardBrush();
    gs_bgRgn = CreateRectRgn(0, 0, 0, 0);
    DragAcceptFiles(hWnd, TRUE);

    HDC dc = GetDC(0);
    gs_hMemDC = CreateCompatibleDC(dc);
    ReleaseDC(0, dc);

    SetScale(hWnd, ID_SET_SCALE_1);
    SetBackground(hWnd, ID_BG_TRANSPARENT);
    SetSubBand(hWnd, ID_SB_ALL);

    InitDecoder();
}

static void OnDestroy()
{
    if (NULL != gs_hMemDC)
    {
        DeleteDC(gs_hMemDC);
        gs_hMemDC = NULL;
    }

    FreeBitmap();
    UnInitDecoder();
    DeleteObject(gs_bgRgn);
    DeleteObject(gs_bgBrush);
}

static void NotImplemented(HWND hWnd)
{
    MessageBox(hWnd, _T("Function not implemented yet"), _T("Sorry..."), MB_ICONINFORMATION);
}

static bool OnCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);
    int wmEvent = HIWORD(wParam);
    bool needReload = false;

    // Parse the menu selections:
    switch (wmId)
    {
    case ID_FILE_OPEN:
        MessageBox(hWnd, _T("Please, use Drag & Drop to open JPEG-XR image files"), _T("Information"), MB_ICONINFORMATION);
        break;

    case ID_SET_ROI:
        NotImplemented(hWnd);
        break;

    case ID_IMAGE_INFO:
        NotImplemented(hWnd);
        break;

    case IDM_ABOUT:
        DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
        break;

    case IDM_EXIT:
        DestroyWindow(hWnd);
        break;

    case ID_SET_SCALE_1:
    case ID_SET_SCALE_2:
    case ID_SET_SCALE_4:
    case ID_SET_SCALE_8:
    case ID_SET_SCALE_16:
        SetScale(hWnd, wmId);
        needReload = true;
        break;

    case ID_BG_BLACK:
    case ID_BG_WHITE:
    case ID_BG_TRANSPARENT:
        SetBackground(hWnd, wmId);
        needReload = true;
        break;

    case ID_SB_ALL:
    case ID_SB_DC_ONLY:
    case ID_SB_NO_HIGHPASS:
    case ID_SB_NO_FLEXBITS:
        SetSubBand(hWnd, wmId);
        needReload = true;
        break;

    default:
        return false;
    }

    if (needReload)
        MessageBox(hWnd, _T("Please, reload the image for the new settings to take effect"), _T("Information"), MB_ICONINFORMATION);

    return true;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        if (!OnCommand(hWnd, wParam, lParam))
            return DefWindowProc(hWnd, message, wParam, lParam);
        break;

    case WM_DROPFILES:
        OnDropFiles(hWnd, wParam, lParam);
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            OnPaint(hWnd, ps.hdc, ps.rcPaint);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_SIZE:
        {
            unsigned int w = (short)LOWORD(lParam);
            unsigned int h = (short)HIWORD(lParam);
            OnSize(hWnd, w, h);
        }
        break;

    case WM_CREATE:
        OnCreate(hWnd);
        break;

    case WM_DESTROY:
        OnDestroy();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        {
            HWND parent = GetParent(hDlg);

            // Center the dialog in parent window
            if (NULL != parent)
            {
                RECT r;
                GetClientRect(parent, &r);
                RECT r1;
                GetWindowRect(hDlg, &r1);
                OffsetRect(&r1, -r1.left, -r1.top);
                POINT p;
                p.x = (r.right - r1.right) / 2;
                p.y = (r.bottom - r1.bottom) / 2;
                ClientToScreen(parent, &p);
                SetWindowPos(hDlg, NULL, p.x, p.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
            }
        }

        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
