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

struct tagPKImageDecode;
struct tagPKFormatConverter;

//======================================================================
// Decoder for JPEG-XR-Files
//======================================================================

struct nsIntRect
{
    uint32_t left, top, width, height;

    nsIntRect(uint32_t left_, uint32_t top_, uint32_t width_, uint32_t height_) : 
        left(left_), top(top_), width(width_), height(height_)
    {
    }

};

class nsJPEGXRDecoderN
{
    RasterImage *m_image;
    tagPKImageDecode *m_pDecoder;
    tagPKFormatConverter *m_pConverter;
    bool m_hasSize;
    WMPStream *m_pStream;
    size_t m_maxSize;

public:

    // We need maxSize to adjust the wrong image byte count found in some images 
    // created with a buggy version of JXRLib
    nsJPEGXRDecoderN(RasterImage &aImage, size_t maxSize);
    ~nsJPEGXRDecoderN();

    virtual void InitInternal();
    virtual void WriteInternal(const char *aBuffer, uint32_t aCount);//, DecodeStrategy aStrategy);
    virtual void FinishInternal();

    bool IsSizeDecode()
    {
        return false;
    }

    bool HasSize()
    {
        return m_hasSize;
    }

    void PostDataError()
    {
    }

    void PostInvalidation(const nsIntRect &r)
    {
        if (nullptr != m_image)
            m_image->Invalidate(r.left, r.top, r.width, r.height);
    }

    void PostDecodeDone()
    {
    }

    void PostSize(I32 width, I32 height)
    {
        if (nullptr != m_image)
            m_image->SetSize(width, height);

        m_hasSize = true;
    }

    bool HasError()
    {
        return false;
    }

private:
 
    bool m_decoderInitialized;
    uint32_t m_pos;

    CChainBuffer_16K m_chainBuf;
    uint32_t m_totalReceived;

    bool CreateJXRStuff();
    void DestroyJXRStuff();

    bool InitializeDecoder();
    bool DecodeJXRFrame();

    bool DecoderInitialized() const
    {
        return m_decoderInitialized;
    }

    uint32_t GetTotalNumBytesReceived() const
    {
        return m_totalReceived;
    }

    bool GetSize(size_t &width, size_t &height)
    {
        if (nullptr == m_pDecoder)
        {
            width = height = 0;
            return false;
        }

        I32 w, h;
        m_pDecoder->GetSize(m_pDecoder, &w, &h);
        width = w;
        height = h;
        return true;
    }

    bool WriteToStream(const uint8_t *buf, uint32_t count)
    {
        WMPStream *pWS = m_pStream;
        pWS->SetPos(pWS, GetTotalNumBytesReceived());
        pWS->Write(pWS, buf, count);

        return true;
    }
};

#ifdef PR_LOGGING
static PRLogModuleInfo *
GetJPEGXRLog()
{
  static PRLogModuleInfo *sJPEGXRLog;
  if (!sJPEGXRLog)
    sJPEGXRLog = PR_NewLogModule("JPEGXRDecoder");
  return sJPEGXRLog;
}
#endif

inline void * moz_malloc(size_t size)
{
    return new unsigned char [size];
}

inline void moz_free(void *ptr)
{
    unsigned char *p = (unsigned char *)ptr;
    delete [] ptr;
}

static void * MyAlloc(size_t cBytes)
{
    return moz_malloc(cBytes);
}

static void MyFree(void *ptr)
{
    moz_free(ptr);
}

nsJPEGXRDecoderN::nsJPEGXRDecoderN(RasterImage &aImage, size_t maxSize) : //Decoder(aImage), 
    m_image(&aImage), 
    m_hasSize(false), m_pos(0), 
    m_pDecoder(nullptr), m_pConverter(nullptr), m_pStream(nullptr), 
    m_decoderInitialized(false), m_totalReceived(0), 
    m_maxSize(maxSize)
{
    m_chainBuf.SetMemAllocProc(MyAlloc, MyFree);
}

nsJPEGXRDecoderN::~nsJPEGXRDecoderN()
{
    DestroyJXRStuff();
}

//EXTERN_C ERR CreateWS_List(struct WMPStream** ppWS);

bool nsJPEGXRDecoderN::CreateJXRStuff()
{
    ERR err = WMP_errSuccess;

    // Create a JPEG-XR file decoder
    Call(PKImageDecode_Create_WMP(&m_pDecoder));

    // Create a pixel format converter
    Call(PKCodecFactory_CreateFormatConverter(&m_pConverter));

    // Create a stream
    Call(CreateWS_ChainBuf(&m_pStream, &m_chainBuf));
    //Call(CreateWS_List(&m_pStream));

Cleanup:

    if (WMP_errSuccess != err)
        DestroyJXRStuff();

    return true;
}

void nsJPEGXRDecoderN::DestroyJXRStuff()
{
    if (nullptr != m_pDecoder)
        m_pDecoder->Release(&m_pDecoder);

    if (nullptr != m_pConverter)
        m_pConverter->Release(&m_pConverter);

    if (nullptr != m_pStream)
        m_pStream->Close(&m_pStream);
}

#define ROI_TEST

bool nsJPEGXRDecoderN::DecodeJXRFrame()
{
    ERR err = WMP_errSuccess;

    PKPixelInfo PI;
    PI.pGUIDPixFmt = &m_pDecoder->guidPixFormat;
    PixelFormatLookup(&PI, LOOKUP_FORWARD);
    //PixelFormatLookup(&newPI, LOOKUP_BACKWARD_TIF);

    PKPixelFormatGUID enPFTo = GUID_PKPixelFormatDontCare;
    U8 *pb = nullptr;

    size_t width, height;
    GetSize(width, height);

    const uint32_t MBR_HEIGHT = 16;

#ifdef ROI_TEST_
    const unsigned int left = 25, top = 37, right = Min(width - 1, (size_t)400), bottom = Min(height - 1, (size_t)300);
#else
    const unsigned int left = 0, top = 0, right = width - 1, bottom = height - 1;
#endif

    m_pDecoder->WMP.wmiSCP.uAlphaMode = 2; // use alpha if present
    bool hasAlpha = m_pDecoder->WMP.bHasAlpha || (PI.grBit & PK_pixfmtHasAlpha) != 0; // has planar or interleaved alpha

    Call(m_pConverter->Initialize(m_pConverter, m_pDecoder, NULL, hasAlpha ? GUID_PKPixelFormat32bppBGRA : GUID_PKPixelFormat32bppBGR));

    PKPixelInfo pPITo;
    Call(m_pConverter->GetPixelFormat(m_pConverter, &enPFTo));
    pPITo.pGUIDPixFmt = &enPFTo;
    PixelFormatLookup(&pPITo, LOOKUP_FORWARD);

    U32 cbStrideTo = (BD_1 == pPITo.bdBitDepth ? 
        ((pPITo.cbitUnit * width + 7) >> 3) : (((pPITo.cbitUnit + 7) >> 3) * width));

    U32 cbStride = cbStrideTo;

    Call(PKAllocAligned((void **)&pb, cbStride * MBR_HEIGHT, 128));

    PKRect br;
    br.X = left;
    br.Y = top;
    br.Height = bottom - top + 1;
    br.Width = right - left + 1;

#if 0
    // !!!We can do it only in FREQUENCY mode!!!
    SUBBAND sb = SB_NO_FLEXBITS;//SB_NO_HIGHPASS;//SB_DC_ONLY;//
    m_pDecoder->WMP.wmiSCP.sbSubband = sb;
    m_pDecoder->WMP.wmiSCP_Alpha.sbSubband = sb;
#endif

#ifdef WEB_CLIENT_SUPPORT
    Call(JXR_BeginDecodingMBRows(m_pDecoder, &br, pb, cbStride, FALSE, FALSE)); // decoding into a single MB row buffer, NOT fail-safe
#else
    Call(JXR_BeginDecodingMBRows(m_pDecoder, &br, pb, cbStride, FALSE)); // decoding into a single MB row buffer
#endif

    Bool hasPlanarAlpha = m_pDecoder->WMP.bHasAlpha != 0;

    if (hasPlanarAlpha)
#ifdef WEB_CLIENT_SUPPORT
        Call(JXR_BeginDecodingMBRows_Alpha(m_pDecoder, &br, pb, cbStride, FALSE, FALSE)); // decoding into a single MB row buffer, not fail-safe
#else
        Call(JXR_BeginDecodingMBRows_Alpha(m_pDecoder, &br, pb, cbStride, FALSE)); // decoding into a single MB row buffer
#endif

    // !!! This code requires JXRLIB compiled with REENTRANT_MODE defined (see windowsmediaphoto.h)!!!
    for (I32 currLine = br.Y; currLine <= br.Y + br.Height;)
    {
        size_t numLinesDecoded;
        Bool finished;

        Call(JXR_DecodeNextMBRow(m_pDecoder, pb, cbStride, &numLinesDecoded, &finished));

        if (hasPlanarAlpha)
            Call(JXR_DecodeNextMBRow_Alpha(m_pDecoder, pb, cbStride, &numLinesDecoded, &finished));

        if (0 == numLinesDecoded)
        {
            err = WMP_errFail; // should not happen
            break;
        }

        PKRect cr;
        cr.X = left;
        cr.Y = 0;
        cr.Height = numLinesDecoded;
        cr.Width = right - left + 1;
        m_pConverter->Convert(m_pConverter, &cr, pb, cbStride);

        m_image->UpdateImageRect(pb, cbStride, left, currLine, cr.Width, numLinesDecoded, hasAlpha);

        currLine += numLinesDecoded;

        if (finished)
            break;
    }

Cleanup:

    JXR_EndDecodingMBRows(m_pDecoder);

    if (hasPlanarAlpha)
        JXR_EndDecodingMBRows_Alpha(m_pDecoder);

    if (nullptr != pb)
        PKFreeAligned((void **)&pb);

    return WMP_errSuccess == err;
}

void nsJPEGXRDecoderN::InitInternal()
{
    if (!CreateJXRStuff())
    {
        PostDataError();
        return;
    }
}

void nsJPEGXRDecoderN::WriteInternal(const char *aBuffer, uint32_t aCount)//, DecodeStrategy)
{
    //@NS_ABORT_IF_FALSE(!HasError(), "Shouldn't call WriteInternal after error!");

    // aCount = 0 means EOF, mCurLine=0 means we're past end of image
    if (0 == aCount)//@ || !mCurLine)
        return;

    WriteToStream((const unsigned char *)aBuffer, aCount);
    m_totalReceived += aCount;

    if (!DecoderInitialized())
    {
        m_pStream->SetPos(m_pStream, 0);
        bool res = m_pDecoder->Initialize(m_pDecoder, m_pStream) == WMP_errSuccess;

        if (!res)
            return;

        m_decoderInitialized = true;

        if (m_maxSize < m_pDecoder->WMP.wmiDEMisc.uImageOffset + m_pDecoder->WMP.wmiDEMisc.uImageByteCount)
        {
            size_t byteCount = m_maxSize - m_pDecoder->WMP.wmiDEMisc.uImageOffset;
            m_pDecoder->WMP.wmiDEMisc.uImageByteCount = byteCount;
            m_pDecoder->WMP.wmiI.uImageByteCount = byteCount;
        }

        size_t width, height;
        GetSize(width, height);
        PostSize(width, height);

        if (HasError())
        {
            // Setting the size led to an error.
            return;
        }

        // We have the size. If we're doing a size decode, we got what
        // we came for.
        if (IsSizeDecode())
            return;
    }
}

void
nsJPEGXRDecoderN::FinishInternal()
{
    // We shouldn't be called in error cases
    //@NS_ABORT_IF_FALSE(!HasError(), "Can't call FinishInternal on error!");

    // We should never make multiple frames
    //NS_ABORT_IF_FALSE(GetFrameCount() <= 1, "Multiple BMP frames?");

    // Send notifications if appropriate
    if (!IsSizeDecode() && HasSize())
    {
        // Here is the only place where we can circumvent the bug in JXELIB's JPEG-XR encoder
        // that writes wrong alpha plane byte count. This is also a safety feature.
        // Adjust the alpha plane byte count if the value is wrong.
        if (m_pDecoder->WMP.wmiDEMisc.uAlphaOffset + m_pDecoder->WMP.wmiI_Alpha.uImageByteCount > GetTotalNumBytesReceived())
        {
            size_t byteCount = GetTotalNumBytesReceived() - m_pDecoder->WMP.wmiDEMisc.uAlphaOffset;
            m_pDecoder->WMP.wmiI_Alpha.uImageByteCount = byteCount;
        }

        DecodeJXRFrame();

        DestroyJXRStuff();

        // Invalidate
        //size_t width, height;
        //GetSize(width, height);
        //nsIntRect r(0, 0, width, height);
        //PostInvalidation(r);

        //if (mUseAlphaData)
        //{
        //    PostFrameStop(FrameBlender::kFrameHasAlpha);
        //}
        //else
        //{
        //    PostFrameStop(FrameBlender::kFrameOpaque);
        //}

        PostDecodeDone();
    }
}

///////////////////////////////////

bool DecodeN(const TCHAR *fileName, RasterImage &image)
{
    HANDLE hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == hFile)
        return false;

    DWORD fileSize = GetFileSize(hFile, NULL);

    if (INVALID_FILE_SIZE == fileSize)
    {
        CloseHandle(hFile);
        return false;
    }

    unsigned char *buf = new unsigned char[fileSize];
    DWORD bytesRead;
    BOOL res = ReadFile(hFile, buf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!res)
    {
        delete [] buf;
        return false;
    }

    nsJPEGXRDecoderN decoder(image, fileSize);

    decoder.InitInternal();

    unsigned int processed = 0;
    const unsigned int maxChunkSize = 32768;
    const char *p = (char *)buf;

    for (unsigned int remaining = fileSize; remaining > 0; )
    {
#if 0
        unsigned int chunkSize = 0 == processed ? 5 : 
            (processed < 2048 ? 50: Min(maxChunkSize, remaining));
#else
        unsigned int chunkSize = Min(maxChunkSize, remaining);
#endif
        decoder.WriteInternal(p, chunkSize);
        remaining -= chunkSize;
        processed += chunkSize;
        p += chunkSize;
    }

    delete [] buf;

    decoder.FinishInternal();

    return true;
}

