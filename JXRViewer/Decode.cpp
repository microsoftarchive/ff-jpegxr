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

#define USE_MEMORY_STREAM

static PKImageDecode *gs_pDecoder;
static PKFormatConverter *gs_pConverter;

bool InitDecoder()
{
    ERR err = WMP_errSuccess;

    // Create JXR file decoder
    Call(PKImageDecode_Create_WMP(&gs_pDecoder));

    // Create converter
    Call(PKCodecFactory_CreateFormatConverter(&gs_pConverter));

Cleanup:

    if (WMP_errSuccess != err)
    {
        UnInitDecoder();
        return false;
    }

    return true;
}

void UnInitDecoder()
{
    if (NULL != gs_pDecoder)
        gs_pDecoder->Release(&gs_pDecoder);

    if (NULL != gs_pConverter)
        gs_pConverter->Release(&gs_pConverter);
}

static bool GetThumbnailSize(size_t &width, size_t &height)
{
    if (nullptr == gs_pDecoder)
    {
        width = height = 0;
        return false;
    }

    I32 w, h;
    gs_pDecoder->GetSize(gs_pDecoder, &w, &h);

    CWMImageInfo wmii;
    wmii.cThumbnailScale = gs_pDecoder->WMP.wmiI.cThumbnailScale;
    wmii.cWidth = w;
    wmii.cHeight = h;

    CalcThumbnailSize(&wmii);
    width = wmii.cThumbnailWidth;
    height = wmii.cThumbnailHeight;

    return true;
}

// !!!Note: this implementation can be used only with source simple pixel formats like RGB24, BGR24, BGRA32 and RGBA32!!!
bool Decode(const TCHAR *fileName, RasterImage &image, unsigned int scale, SUBBAND subBand)
{
    size_t width, height;
    ERR err = WMP_errSuccess;
    U8 *pb = NULL;

    PKPixelFormatGUID enPFTo = GUID_PKPixelFormatDontCare;
    WMPStream *pStream = NULL;
    PKPixelInfo PI;
    unsigned char *buf = NULL;

#ifdef USE_MEMORY_STREAM
    HANDLE hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == hFile)
        return NULL;

    DWORD fileSize = GetFileSize(hFile, NULL);

    if (INVALID_FILE_SIZE == fileSize)
    {
        CloseHandle(hFile);
        return NULL;
    }

    buf = new unsigned char[fileSize];
    DWORD bytesRead;
    BOOL res = ReadFile(hFile, buf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!res)
    {
        delete [] buf;
        return NULL;
    }

    Call(CreateWS_Memory(&pStream, buf, fileSize));
#else
    Call(CreateWS_File(&pStream, fileName, _T("rb")));
#endif

    // attach stream to decoder
    Call(gs_pDecoder->Initialize(gs_pDecoder, pStream));

#if 0//1
    // Simulate orientation
    gs_pDecoder->WMP.wmiI.oOrientation = O_FLIPV;
    //gs_pDecoder->WMP.wmiI.oOrientation = O_FLIPH;
    //gs_pDecoder->WMP.wmiI.oOrientation = O_FLIPVH;
    //gs_pDecoder->WMP.wmiI.oOrientation = O_RCW_FLIPH;
#endif

    gs_pDecoder->WMP.wmiI.cThumbnailScale = scale;
    CalcThumbnailSize(&gs_pDecoder->WMP.wmiI);

    //Call(pDecoder->GetSize(pDecoder, &rect.Width, &rect.Height));
    GetThumbnailSize(width, height);

    image.SetSize(width, height);

    gs_pDecoder->WMP.wmiSCP.sbSubband = subBand;
    gs_pDecoder->WMP.wmiSCP_Alpha.sbSubband = subBand;

#define ROI_TEST_

#ifdef ROI_TEST
    const unsigned int left = 25, top = 37, right = Min(width - 1, (size_t)120), bottom = Min(height - 1, (size_t)100);
#else
    const unsigned int left = 0, top = 0, right = width - 1, bottom = height - 1;
#endif

    // Setup the pixel format
    PI.pGUIDPixFmt = &gs_pDecoder->guidPixFormat;
    Call(PixelFormatLookup(&PI, LOOKUP_FORWARD));
    Call(PixelFormatLookup(&PI, LOOKUP_BACKWARD_TIF));
    bool hasAlpha = (PI.grBit & PK_pixfmtHasAlpha) != 0; // the pixel format contains alpha

    Call(gs_pConverter->Initialize(gs_pConverter, gs_pDecoder, ".bmp", *PI.pGUIDPixFmt));
    PKPixelInfo pPITo;
    Call(gs_pConverter->GetPixelFormat(gs_pConverter, &enPFTo));
    pPITo.pGUIDPixFmt = &enPFTo;
    PixelFormatLookup(&pPITo, LOOKUP_FORWARD);

    size_t rw = right - left + 1;
    size_t rh = bottom - top + 1;

    U32 cbStrideTo = (BD_1 == pPITo.bdBitDepth ? ((pPITo.cbitUnit * rw + 7) >> 3) : (((pPITo.cbitUnit + 7) >> 3) * rw));

    U32 cbStride = cbStrideTo;
    size_t linesPerMBRow = 16 / scale;

    // reserve up to two additional macroblock rows: one for cropped lines below and one at the bottom
    //Call(PKAllocAligned((void **)&pb, cbStride * (linesPerMBRow + (rh + linesPerMBRow - 1) / linesPerMBRow * linesPerMBRow), 128));
    Call(PKAllocAligned((void **)&pb, cbStride * rh, 128));

    gs_pDecoder->WMP.wmiSCP.uAlphaMode = 2; // use alpha if present

    // Fix the wrong image plane byte count
    if (fileSize < gs_pDecoder->WMP.wmiDEMisc.uImageOffset + gs_pDecoder->WMP.wmiDEMisc.uImageByteCount)
    {
        size_t byteCount = fileSize - gs_pDecoder->WMP.wmiDEMisc.uImageOffset;
        gs_pDecoder->WMP.wmiDEMisc.uImageByteCount = byteCount;
        gs_pDecoder->WMP.wmiI.uImageByteCount = byteCount;
    }

    // Here is the only place where we can circumvent the bug in JXELIB's JPEG-XR encoder
    // that writes wrong alpha plane byte count. This is also a safety feature.
    // Adjust the alpha plane byte count if the value is wrong.
    if (gs_pDecoder->WMP.wmiDEMisc.uAlphaOffset + gs_pDecoder->WMP.wmiI_Alpha.uImageByteCount > fileSize)
        gs_pDecoder->WMP.wmiI_Alpha.uImageByteCount = fileSize - gs_pDecoder->WMP.wmiDEMisc.uAlphaOffset;

    // Initialize the rectangle/region of interest (ROI)
    PKRect rect;
    rect.X = left;
    rect.Y = top;
    rect.Width = (I32)rw;
    rect.Height = (I32)rh;

    Call(gs_pConverter->Copy(gs_pConverter, &rect, pb, cbStride));

    // An image could be "windowed". In this case, ROI top is not equal to gs_pDecoder->WMP.cLinesCropped, 
    // and we have to calculate the displacement
    unsigned int destBPP = pPITo.cbitUnit / 8;
    image.UpdateImageRect(pb, cbStride, destBPP, left, top, rect.Width, rect.Height, hasAlpha);

Cleanup:

    pStream->Close(&pStream);
    delete [] buf;

    return WMP_errSuccess == err;
}

void FreeDecodeBuffer(void *buf)
{
    PKFreeAligned(&buf);
}

