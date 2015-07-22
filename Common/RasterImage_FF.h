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

typedef uint32_t * (*POnSetSize)(uint32_t width, uint32_t height, void *param);

typedef void (*POnInvalidateRect)(uint32_t left, uint32_t top, uint32_t width, uint32_t height, bool update, void *param);

class RasterImage
{
    uint32_t *m_imageData;
    size_t m_width, m_height;
    bool m_transparent;
    uint8_t m_bgR, m_bgG, m_bgB;

    POnSetSize m_OnSetSize;
    void *m_OnSetSizeParam;

    POnInvalidateRect m_OnInvalidateRect;
    void *m_OnInvalidateRectParam;

public:

    RasterImage() : m_width(0), m_height(0), 
        m_imageData(nullptr), m_transparent(false), m_bgR(0), m_bgG(0), m_bgB(0), 
        m_OnSetSize(nullptr), m_OnSetSizeParam(nullptr), 
        m_OnInvalidateRect(nullptr), m_OnInvalidateRectParam(nullptr)
    {
    }

    size_t GetWidth() const
    {
        return m_width;
    }

    size_t GetHeight() const
    {
        return m_height;
    }

    bool Transparent() const
    {
        return m_transparent;
    }

    void SetTransparent(bool value)
    {
        m_transparent = value;
    }

    uint8_t BgR() const
    {
        return m_bgR;
    }

    uint8_t BgG() const
    {
        return m_bgG;
    }

    uint8_t BgB() const
    {
        return m_bgB;
    }

    void SetBgColor(uint8_t r, uint8_t g, uint8_t b)
    {
        m_bgR = r;
        m_bgG = g;
        m_bgB = b;
    }

    void SetOnSetSize(POnSetSize pProc, void *param)
    {
        m_OnSetSize = pProc;
        m_OnSetSizeParam = param;
    }

    void SetOnInvalidateRect(POnInvalidateRect pProc, void *param)
    {
        m_OnInvalidateRect = pProc;
        m_OnInvalidateRectParam = param;
    }

    void SetSize(unsigned int width, unsigned int height)
    {
        if (nullptr != m_OnSetSize)
            m_imageData = m_OnSetSize(width, height, m_OnSetSizeParam);

        m_width = width;
        m_height = height;
    }

    void InvalidateRect(uint32_t left, uint32_t top, uint32_t width, uint32_t height, bool update)
    {
        if (nullptr != m_OnInvalidateRect)
            m_OnInvalidateRect(left, top, width, height, update, m_OnInvalidateRectParam);
    }

    static uint32_t GetPixelR(uint32_t pixel)
    {
        // The pixel is in BGR format
        return (pixel & 0x00FF0000) >> 16;
    }

    static uint32_t GetPixelG(uint32_t pixel)
    {
        // The pixel is in BGR format
        return (pixel & 0x0000FF00) >> 8;
    }

    static uint32_t GetPixelB(uint32_t pixel)
    {
        // The pixel is in BGR format
        return (pixel & 0x000000FF);
    }

    uint32_t * GetPixel(uint32_t x, uint32_t y);
    void SetPixel(uint32_t *pDestPixel, uint32_t srcPixel);
    void SetPixelWithAlpha(uint32_t *pDestPixel, uint32_t srcPixel, uint32_t x, uint32_t y); // we need coordinates to calculate bacground color when transparent
};

inline int32_t gfxPackedPixelNoPreMultiply(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
    return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r); // this is for RGBA
    //return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b); // this is for BGRA
}

inline int32_t gfxPackedPixel(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
    return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r); // this is for RGB
    //return (uint32_t(a) << 24) | uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b); // this is for BGR
}


