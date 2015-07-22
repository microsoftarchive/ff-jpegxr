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

typedef void (*POnInvalidate)(uint32_t left, uint32_t top, uint32_t width, uint32_t height, void *param);

typedef void (*POnImageRowsAvailable)(const uint8_t *src, uint32_t srcRowStride,
    uint32_t left, uint32_t top, uint32_t width, uint32_t height, bool alphaPresent, void *param);

class RasterImage
{
    unsigned char *m_imageData;
    unsigned int m_width, m_height;
    //unsigned int m_rowStride;

    POnSetSize m_OnSetSize;
    void *m_OnSetSizeParam;

    POnInvalidate m_OnInvalidate;
    void *m_OnInvalidateParam;

    POnImageRowsAvailable m_OnImageRowsAvailable;
    void *m_OnImageRowsAvailableParam;

public:

    RasterImage() : m_width(0), m_height(0), m_imageData(nullptr), 
        m_OnSetSize(nullptr), m_OnSetSizeParam(nullptr), 
        m_OnInvalidate(nullptr), m_OnInvalidateParam(nullptr), 
        m_OnImageRowsAvailable(nullptr), m_OnImageRowsAvailableParam(nullptr)
    {
    }

    void SetOnSetSize(POnSetSize pProc, void *param)
    {
        m_OnSetSize = pProc;
        m_OnSetSizeParam = param;
    }

    void SetOnInvalidate(POnInvalidate pProc, void *param)
    {
        m_OnInvalidate = pProc;
        m_OnInvalidateParam = param;
    }

    void SetOnImageRowsAvailable(POnImageRowsAvailable pProc, void *param)
    {
        m_OnImageRowsAvailable = pProc;
        m_OnImageRowsAvailableParam = param;
    }

    void SetSize(unsigned int width, unsigned int height)
    {
        if (nullptr != m_OnSetSize)
            m_OnSetSize(width, height, m_OnSetSizeParam);

        m_width = width;
        m_height = height;
    }

    void Invalidate(uint32_t left, uint32_t top, uint32_t width, uint32_t height)
    {
        if (nullptr != m_OnInvalidate)
            m_OnInvalidate(left, top, width, height, m_OnInvalidateParam);
    }

    void UpdateImageRect(const uint8_t *src, uint32_t srcRowStride, 
        uint32_t left, uint32_t top, uint32_t width, uint32_t height, bool alphaPresent)
    {
        if (nullptr != m_OnImageRowsAvailable)
            m_OnImageRowsAvailable(src, srcRowStride, left, top, width, height, alphaPresent, m_OnImageRowsAvailableParam);
    }
};