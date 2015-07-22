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

static void UnInitDecoder();

static bool InitDecoder()
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

static void UnInitDecoder()
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

bool DecodeR(const TCHAR *fileName, RasterImage &image, unsigned int scale, bool perfTest, unsigned int &ticks)
{
    ERR err = WMP_errSuccess;
    U8 *pb = NULL;

    PKPixelFormatGUID enPFTo = GUID_PKPixelFormatDontCare;
    WMPStream *pStream = NULL;
    PKPixelInfo PI;
    unsigned char *buf = NULL;

#ifdef USE_MEMORY_STREAM
    HANDLE hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == hFile)
        return false;

    DWORD fileSize = GetFileSize(hFile, NULL);

    if (INVALID_FILE_SIZE == fileSize)
    {
        CloseHandle(hFile);
        return false;
    }

    buf = new unsigned char[fileSize];
    DWORD bytesRead;
    BOOL res = ReadFile(hFile, buf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!res)
    {
        delete [] buf;
        return false;
    }

    if (!InitDecoder())
    {
        delete [] buf;
        return false;
    }

    bool result = false;

    Call(CreateWS_Memory(&pStream, buf, fileSize));
#else
    Call(CreateWS_File(&pStream, fileName, _T("rb")));
#endif

    // attach stream to decoder
    Call(gs_pDecoder->Initialize(gs_pDecoder, pStream));
    bool upsideDown = false;
    ORIENTATION &orientation = gs_pDecoder->WMP.wmiI.oOrientation;

    if (orientation >= O_RCW)
        orientation = O_NONE; // jxrlib cannot rotate images
    else if (O_FLIPV == orientation || O_FLIPVH == orientation)
        upsideDown = true;

    gs_pDecoder->WMP.wmiI.cThumbnailScale = scale;
    CalcThumbnailSize(&gs_pDecoder->WMP.wmiI);

    size_t width, height;
    GetThumbnailSize(width, height);
    image.SetSize(width, height);

#if 0
    const SUBBAND subBand = SB_ALL;
    gs_pDecoder->WMP.wmiSCP.sbSubband = subBand;
    gs_pDecoder->WMP.wmiSCP_Alpha.sbSubband = subBand;
#endif

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

    //Call(pDecoder->GetSize(pDecoder, &rect.Width, &rect.Height));

    Call(gs_pConverter->Initialize(gs_pConverter, gs_pDecoder, ".bmp", 
        //GUID_PKPixelFormat32bppRGB));
        *PI.pGUIDPixFmt));
    PKPixelInfo pPITo;

    Call(gs_pConverter->GetPixelFormat(gs_pConverter, &enPFTo));
    pPITo.pGUIDPixFmt = &enPFTo;
    PixelFormatLookup(&pPITo, LOOKUP_FORWARD);

    U32 cbStrideTo = (BD_1 == pPITo.bdBitDepth ? ((pPITo.cbitUnit * width + 7) >> 3) : 
        (((pPITo.cbitUnit + 7) >> 3) * width));

#ifdef ENABLE_OPTIMIZATIONS
    cbStrideTo = (cbStrideTo + 127) / 128 * 128;
#endif

    U32 cbStride = cbStrideTo;
    const size_t MBR_HEIGHT = 16;
    const size_t linesPerMBRow = MBR_HEIGHT / scale;

    Call(PKAllocAligned((void **)&pb, cbStride * linesPerMBRow, 128));

    gs_pDecoder->WMP.wmiSCP.uAlphaMode = 2; // use alpha if present

    // Fix the wrong image plane byte count
    if (fileSize < gs_pDecoder->WMP.wmiDEMisc.uImageOffset + gs_pDecoder->WMP.wmiDEMisc.uImageByteCount)
    {
        size_t byteCount = fileSize - gs_pDecoder->WMP.wmiDEMisc.uImageOffset;
        gs_pDecoder->WMP.wmiDEMisc.uImageByteCount = byteCount;
        gs_pDecoder->WMP.wmiI.uImageByteCount = byteCount;
    }

    // Here is the only place where we can circumvent the bug in JXELIB's JPEG-XR encoder
    // that used to write wrong alpha plane byte count. This is also a safety feature.
    // Adjust the alpha plane byte count if the value is wrong.
    if (gs_pDecoder->WMP.wmiDEMisc.uAlphaOffset + gs_pDecoder->WMP.wmiI_Alpha.uImageByteCount > fileSize)
        gs_pDecoder->WMP.wmiI_Alpha.uImageByteCount = fileSize - gs_pDecoder->WMP.wmiDEMisc.uAlphaOffset;

    bool hasPlanarAlpha = gs_pDecoder->WMP.bHasAlpha;
    gs_pDecoder->WMP.wmiSCP.uAlphaMode = hasPlanarAlpha ? 0 : 2;//hasPlanarAlpha ? 2 : 0; // !!!TMP!!! Currently, do not decode planar alpha!!!

    if (hasPlanarAlpha)
        hasAlpha = false; // !!!TMP!!! Currently, do not decode planar alpha!!!

    PKRect br;
    br.X = left;
    br.Y = top;
    br.Height = bottom - top + 1;
    br.Width = right - left + 1;

    // Decoding into a single-macroblock row buffer
#ifdef WEB_CLIENT_SUPPORT
    err = JXR_BeginDecodingMBRows(gs_pDecoder, &br, pb, cbStride, FALSE, FALSE);
#else
    err = JXR_BeginDecodingMBRows(gs_pDecoder, &br, pb, cbStride, FALSE);
#endif

    if (WMP_errSuccess != err)
        goto Cleanup;

    size_t srcBPP = pPITo.cbitUnit / 8;

    ticks = GetTickCount();

    unsigned int numIter = perfTest ? 100 : 1;

    for (unsigned int i = 0; i < numIter; ++i)
    {
        int stepStride = cbStride;
        int currLine, coeff, adjCoeff;

        if (upsideDown)
        {
            currLine = bottom + 1;
            coeff = -1;
            adjCoeff = 1;
        }
        else
        {
            currLine = 0;
            coeff = 1;
            adjCoeff = 0;
        }

        while (TRUE)
        {
            size_t numLinesDecoded;
            Bool finished;
            err = JXR_DecodeNextMBRow(gs_pDecoder, pb, cbStride, &numLinesDecoded, &finished);

            if (WMP_errSuccess != err)
            {
                //MessageBox();
                break;
            }

            if (0 == numLinesDecoded)
                break; // should not happen

            //!!! We test the performance of decoding only, so there is no output in pefTest mode !!!
            if (!perfTest)
            {
                // All conversion routines ignore the left edge of the rectangle, they only use its width.
                // So set the width of the rectangle correspondingly
                PKRect cr;
                cr.X = 0;//left;
                cr.Y = 0;
                cr.Height = numLinesDecoded;
                cr.Width = right;

                gs_pConverter->Convert(gs_pConverter, &cr, pb, cbStride);
                image.ImageDataAvailable(pb, cbStride, srcBPP, left, currLine - numLinesDecoded * adjCoeff, right - left + 1, numLinesDecoded, hasAlpha);
            }

            if (finished)
                break;

            currLine += numLinesDecoded * coeff;
        }
    }

    ticks = GetTickCount() - ticks;

    JXR_EndDecodingMBRows(gs_pDecoder);

    image.InvalidateRect(0, 0, width, height, true);
    result = true;

Cleanup:

    UnInitDecoder();
    pStream->Close(&pStream);
    delete [] buf;
    return result;
}

void FreeDecodeBuffer(void *buf)
{
    PKFreeAligned(&buf);
}

