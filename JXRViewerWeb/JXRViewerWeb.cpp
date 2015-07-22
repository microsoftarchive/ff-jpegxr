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
#include "JXRViewerWeb.h"

#define MAX_LOADSTRING 100
#define MAX_URL_LENGTH 512
#define UM_FINISHED_DECODING WM_APP + 111

// Global Variables:
HINSTANCE hInst;
TCHAR szTitle[MAX_LOADSTRING];
TCHAR szWindowClass[MAX_LOADSTRING];

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK OpenRemoteImageDlg(HWND, UINT, WPARAM, LPARAM);

HINTERNET gs_hSession;

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
    MSG msg;
    HACCEL hAccelTable;

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_JXRVIEWERWEB, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_JXRVIEWERWEB));

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (NULL != gs_hSession)
        WinHttpCloseHandle(gs_hSession);

    return (int) msg.wParam;
}

//  FUNCTION: MyRegisterClass()
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
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_JXRVIEWERWEB));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_JXRVIEWERWEB);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

//   FUNCTION: InitInstance(HINSTANCE, int)
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;
    hInst = hInstance; // Store instance handle in our global variable

    hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!hWnd)
        return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Use WinHttpOpen to obtain a session handle.
    gs_hSession = WinHttpOpen(
        L"Web JPEG-XR viewer/1.0", 
        WINHTTP_ACCESS_TYPE_NO_PROXY, //WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
        WINHTTP_NO_PROXY_NAME, 
        WINHTTP_NO_PROXY_BYPASS, 
        0);//WINHTTP_FLAG_ASYNC); // it's easier to create a thread than to use async requests

    if (NULL == gs_hSession)
        return FALSE;

    return TRUE;
}

static HBITMAP gs_hSrcBmp;
static unsigned int gs_width, gs_height;
static HDC gs_hMemDC;
static HRGN gs_bgRgn;
static HBRUSH gs_bgBrush;
static TCHAR gs_imageURL[MAX_URL_LENGTH];
static HANDLE gs_DecodeThread;
static HANDLE gs_hStopEvent;
static unsigned int gs_scale = 1;

RasterImage gs_rasterImage;

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

static uint32_t * OnSetImageSize(uint32_t width, uint32_t height, void *param)
{
    void *destData = NULL;
    gs_hSrcBmp = CreateColorDIB(width, height, 32, &destData); // always create a 32-bit bitmap
    BITMAP bm;
    GetObject(gs_hSrcBmp, sizeof(BITMAP), &bm);

    HWND hWnd = reinterpret_cast<HWND>(param);
    OnSize(hWnd, gs_width, gs_height);

    RECT r;
    r.left = 0;
    r.top = 0;
    r.right = width;
    r.bottom = height;
    InvalidateRect(hWnd, &r, FALSE);
    //UpdateWindow(hWnd);

    return (uint32_t *)destData;
}

static void OnInvalidateImageRect(uint32_t rl, uint32_t rt, uint32_t rw, uint32_t rh, bool update, void *param)
{
    RECT r;
    r.left = rl;
    r.top = rt;
    r.right = rw;
    r.bottom = r.top + rh;
    HWND hWnd = reinterpret_cast<HWND>(param);
    InvalidateRect(hWnd, &r, FALSE);

    if (update)
        UpdateWindow(hWnd);
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

static void SetBackground(HWND hWnd, UINT id)
{
    HMENU hMenu = GetMenu(hWnd);
    HMENU hSubMenu = GetSubMenu(hMenu, 1);
    hSubMenu = GetSubMenu(hSubMenu, 0);

    CheckMenuRadioItem(hSubMenu, ID_BG_BLACK, ID_BG_TRANSPARENT, id, MF_BYCOMMAND);

    switch (id)
    {
    case ID_BG_BLACK:
        gs_rasterImage.SetTransparent(false);
        gs_rasterImage.SetBgColor(0, 0, 0);
        break;

    case ID_BG_WHITE:
        gs_rasterImage.SetTransparent(false);
        gs_rasterImage.SetBgColor(0xFF, 0xFF, 0xFF);
        break;

    case ID_BG_TRANSPARENT:
        gs_rasterImage.SetTransparent(true);
        break;
    }
}

static void OnCreate(HWND hWnd)
{
    gs_bgBrush = CreateCheckerBoardBrush();
    gs_bgRgn = CreateRectRgn(0, 0, 0, 0);

    HDC dc = GetDC(0);
    gs_hMemDC = CreateCompatibleDC(dc);
    ReleaseDC(0, dc);

    gs_hStopEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

    SetScale(hWnd, ID_SET_SCALE_1);
    SetBackground(hWnd, ID_BG_TRANSPARENT);
}

static void OnDestroy()
{
    if (NULL != gs_hMemDC)
    {
        DeleteDC(gs_hMemDC);
        gs_hMemDC = NULL;
    }

    FreeBitmap();
    DeleteObject(gs_bgRgn);
    DeleteObject(gs_bgBrush);

    CloseHandle(gs_hStopEvent); // not necessary
}

struct RequestParam
{
    HINTERNET hConnection;
    const TCHAR *requestStr;
    RasterImage *pImage;
    unsigned int scale;
    HWND hWnd;
};

// Image downloading and decoding thread
static DWORD WINAPI DecodeThreadProc(LPVOID lpParameter)
{
    RequestParam &param = *(RequestParam *)lpParameter;
    DWORD dwError = ERROR_SUCCESS;
    nsJPEGXRDecoder decoder(*param.pImage, false);
    decoder.InitInternal();
    decoder.SetScale(param.scale);
    int httpStatus = 0;

    // Create an HTTP request handle.
    HINTERNET hRequest = WinHttpOpenRequest(
            param.hConnection, 
            _T("GET"), 
            param.requestStr, 
            NULL, 
            WINHTTP_NO_REFERER, 
            WINHTTP_DEFAULT_ACCEPT_TYPES, 
            0
            );

    if (NULL == hRequest)
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Send the request.
    BOOL bResult = WinHttpSendRequest(
            hRequest, 
            WINHTTP_NO_ADDITIONAL_HEADERS, 
            0, 
            WINHTTP_NO_REQUEST_DATA, 
            0,
            0,
            NULL
            );

    // End the request.
    if (!bResult)
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    bResult = WinHttpReceiveResponse(hRequest, NULL);

    if (!bResult)
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Keep checking for data until there is nothing left.
    const DWORD readBufSize = 0x1000; // 4K
    unsigned char readBuffer[readBufSize];

    for (; ;)
    {
        // Check for available data.
        //
        // Important: Do not use the return value of WinHttpQueryDataAvailable 
        //            to determine whether the end of a response has been reached, 
        //            because not all servers terminate responses properly, 
        //            and an improperly terminated response causes WinHttpQueryDataAvailable 
        //            to anticipate more data.

        DWORD dwSize = 0;
        bResult = WinHttpQueryDataAvailable(hRequest, &dwSize);

        if (!bResult)
        {
            dwError = GetLastError();
            goto Cleanup;
        }

        DWORD waitRes = WaitForSingleObject(gs_hStopEvent, 0);

        if (WAIT_OBJECT_0 == waitRes)
            break;

        // Read the data.
        // Use the return value of WinHttpReadData to determine when a response 
        // has been completely read.
        DWORD bytesRead;
        DWORD bytesToRead = readBufSize;
        BOOL bRes = WinHttpReadData(hRequest, (LPVOID)readBuffer, bytesToRead, &bytesRead);

        if (!bRes)
        {
            dwError = GetLastError();
        }
        else
        {
            if (0 == bytesRead)
                break; // We're finished

            decoder.WriteInternal((const char *)readBuffer, bytesRead);
        }
    }

Cleanup:

    decoder.FinishInternal();

    if (NULL != hRequest)
        WinHttpCloseHandle(hRequest);

    gs_DecodeThread = NULL;

    PostMessage(param.hWnd, UM_FINISHED_DECODING, 0, dwError);

    return dwError;
}

static bool Decoding()
{
    return NULL != gs_DecodeThread;
}

static void StopDecoding()
{
    SetEvent(gs_hStopEvent);
    WaitForSingleObject(gs_DecodeThread, INFINITE);
}

static bool ConfirmExit(HWND hWnd)
{
    return IDOK == MessageBox(hWnd, _T("Image downloading is in progress. Are you sure you want to quit the application?"), 
        _T("Please, confirm"), MB_OKCANCEL | MB_ICONQUESTION);
}

static void ModifyMenuItems(HWND hWnd, bool started)
{
    HMENU hMenu = GetMenu(hWnd);
    HMENU hSubMenu = GetSubMenu(hMenu, 0);
    EnableMenuItem(hSubMenu, ID_FILE_OPEN, MF_BYCOMMAND | started ?  MF_GRAYED : MF_ENABLED);
    EnableMenuItem(hSubMenu, ID_STOP, MF_BYCOMMAND | started ? MF_ENABLED : MF_GRAYED);
}

static void OnOpenImage(HWND hWnd)
{
    INT_PTR res = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_OPEN_IMAGE), hWnd, OpenRemoteImageDlg, 
        reinterpret_cast<LPARAM>(gs_imageURL));

    if (IDOK == res)
    {
        if (NULL != gs_hSrcBmp)
        {
            DeleteObject(gs_hSrcBmp);
            gs_hSrcBmp = NULL;
            SetWindowText(hWnd, szTitle);
            OnSize(hWnd, gs_width, gs_height);
            InvalidateRect(hWnd, NULL, TRUE);
            //UpdateWindow(hWnd);
        }

        TCHAR hostName[MAX_URL_LENGTH];
        const TCHAR *pResource = _tcschr(gs_imageURL, _T('/'));

        if (NULL == pResource)
        {
            MessageBox(hWnd, gs_imageURL, _T("Malformed URL!"), MB_ICONERROR | MB_OK);
            return;
        }

        unsigned int len = pResource - gs_imageURL;
        ++pResource;
        _tcsncpy_s<MAX_URL_LENGTH>(hostName, gs_imageURL, len);

        HINTERNET hConnect = WinHttpConnect(gs_hSession, hostName, INTERNET_DEFAULT_HTTP_PORT, 0);

        if (NULL == hConnect)
        {
            MessageBox(hWnd, hostName, _T("Error connecting to host"), MB_ICONERROR | MB_OK);
            return;
        }

        gs_rasterImage.SetOnInvalidateRect(OnInvalidateImageRect, reinterpret_cast<void *>(hWnd));
        gs_rasterImage.SetOnSetSize(OnSetImageSize, reinterpret_cast<void *>(hWnd));

        static RequestParam param;
        param.hConnection = hConnect;
        param.pImage = &gs_rasterImage;
        param.requestStr = pResource; 
        param.scale = gs_scale;
        param.hWnd = hWnd;

        ResetEvent(gs_hStopEvent);

        DWORD threadID;
        gs_DecodeThread = CreateThread(NULL, 0, DecodeThreadProc, &param, 0, &threadID);

        String title = gs_imageURL;
        title += _T(" - ");
        title += szTitle;
        SetWindowText(hWnd, title.c_str());

        ModifyMenuItems(hWnd, true);
    }
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
        OnOpenImage(hWnd);
        break;

    case ID_STOP:
        StopDecoding();
        break;

    case IDM_EXIT:
        if (!Decoding() || ConfirmExit(hWnd))
            DestroyWindow(hWnd);
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
//  PURPOSE:  Processes messages for the main window.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        if (!OnCommand(hWnd, wParam, lParam))
            return DefWindowProc(hWnd, message, wParam, lParam);
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

    case UM_FINISHED_DECODING:
        ModifyMenuItems(hWnd, false);

        if (lParam != ERROR_SUCCESS)
        {
            TCHAR s[256];
            wsprintf(s, _T("Error %u occurred when downloading image."), lParam);
            MessageBox(hWnd, s, _T("Error"), MB_OK | MB_ICONERROR);
        }

        break;

    case WM_CLOSE:
        if (Decoding() && !ConfirmExit(hWnd))
            return 0;

        StopDecoding();
        return DefWindowProc(hWnd, message, wParam, lParam);

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
INT_PTR CALLBACK OpenRemoteImageDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    static TCHAR *s_pURL;

    switch (message)
    {
    case WM_INITDIALOG:
        s_pURL = reinterpret_cast<TCHAR *>(lParam);
        SendDlgItemMessage(hDlg, IDC_EDIT_URL, EM_SETLIMITTEXT, MAX_URL_LENGTH - 1, 0);
        SetDlgItemText(hDlg, IDC_EDIT_URL, s_pURL);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            if (LOWORD(wParam) == IDOK)
                GetDlgItemText(hDlg, IDC_EDIT_URL, s_pURL, MAX_URL_LENGTH);

            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
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
