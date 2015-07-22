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


#pragma once

typedef void (*POnSetSize)(uint32_t width, uint32_t height, void *param);

typedef void (*POnImageDataAvailable)(const uint8_t *src, uint32_t srcRowStride, uint32_t srcBPP, 
    uint32_t left, uint32_t top, uint32_t width, uint32_t height, bool hasAlpha, void *param);

typedef void (*POnImageDataAvailable_Alpha)(const uint8_t *src, uint32_t srcRowStride,
    uint32_t left, uint32_t top, uint32_t width, uint32_t height, void *param);

typedef void (*POnInvalidateRect)(uint32_t left, uint32_t top, uint32_t width, uint32_t height, bool update, void *param);

class RasterImage
{
    unsigned char *m_imageData;
    unsigned int m_width, m_height;
    //unsigned int m_rowStride;

    POnSetSize m_OnSetSize;
    void *m_OnSetSizeParam;

    POnImageDataAvailable m_OnImageDataAvailable;
    POnImageDataAvailable_Alpha m_OnImageDataAvailable_Alpha;
    void *m_OnImageDataAvailableParam;

    POnInvalidateRect m_OnInvalidateRect;
    void *m_OnInvalidateRectParam;

public:

    RasterImage() : m_width(0), m_height(0), m_imageData(nullptr), 
        m_OnSetSize(nullptr), m_OnSetSizeParam(nullptr), 
        m_OnImageDataAvailable(nullptr), m_OnImageDataAvailableParam(nullptr), 
        m_OnInvalidateRect(nullptr), m_OnInvalidateRectParam(nullptr)
    {
    }

    void SetOnSetSize(POnSetSize pProc, void *param)
    {
        m_OnSetSize = pProc;
        m_OnSetSizeParam = param;
    }

    void SetOnImageDataAvailable(POnImageDataAvailable pProc, POnImageDataAvailable_Alpha pProc_Alpha, void *param)
    {
        m_OnImageDataAvailable = pProc;
        m_OnImageDataAvailable_Alpha = pProc_Alpha;
        m_OnImageDataAvailableParam = param;
    }

    void SetOnInvalidateRect(POnInvalidateRect pProc, void *param)
    {
        m_OnInvalidateRect = pProc;
        m_OnInvalidateRectParam = param;
    }

    void SetSize(unsigned int width, unsigned int height)
    {
        if (nullptr != m_OnSetSize)
            m_OnSetSize(width, height, m_OnSetSizeParam);

        m_width = width;
        m_height = height;
    }

    void ImageDataAvailable(const uint8_t *src, uint32_t srcRowStride, uint32_t srcBPP, 
        uint32_t left, uint32_t top, uint32_t width, uint32_t height, bool hasAlpha)
    {
        if (nullptr != m_OnImageDataAvailable)
            m_OnImageDataAvailable(src, srcRowStride, srcBPP, left, top, width, height, hasAlpha, m_OnImageDataAvailableParam);
    }

    void ImageDataAvailable_Alpha(const uint8_t *src, uint32_t srcRowStride, 
        uint32_t left, uint32_t top, uint32_t width, uint32_t height)
    {
        if (nullptr != m_OnImageDataAvailable_Alpha)
            m_OnImageDataAvailable_Alpha(src, srcRowStride, left, top, width, height, m_OnImageDataAvailableParam);
    }

    void InvalidateRect(uint32_t left, uint32_t top, uint32_t width, uint32_t height, bool update)
    {
        if (nullptr != m_OnInvalidateRect)
            m_OnInvalidateRect(left, top, width, height, update, m_OnInvalidateRectParam);
    }
};