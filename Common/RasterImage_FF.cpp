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


#include "StdAfx.h"

uint32_t * RasterImage::GetPixel(uint32_t x, uint32_t y)
{
    return m_imageData + GetWidth() * (GetHeight() - y - 1) + x;
}

inline unsigned char AlphaComposite(unsigned char fg, unsigned char alpha, unsigned char bg)
{
    unsigned short temp = (unsigned short)(fg) * (unsigned short)(alpha) + 
        (unsigned short)(bg) * (unsigned short)(0xFF - (unsigned short)(alpha)) + 
        (unsigned short)0x80;

    unsigned char composite = (unsigned char)((temp + (temp >> 8)) >> 8);
    return composite;
}

void RasterImage::SetPixel(uint32_t *pDestPixel, uint32_t srcPixel)
{
    struct BGRA
    {
        unsigned char b, g, r, a;
    };

    BGRA &dp = *(BGRA *)pDestPixel;

    dp.r = (srcPixel & 0x000000FF);
    dp.g = (srcPixel & 0x0000FF00) >> 8;
    dp.b = (srcPixel & 0x00FF0000) >> 16;
    dp.a = 0;
}

void RasterImage::SetPixelWithAlpha(uint32_t *pDestPixel, uint32_t srcPixel, uint32_t x, uint32_t y)
{
    struct BGRA
    {
        unsigned char b, g, r, a;
    };

    BGRA &dp = *(BGRA *)pDestPixel;
    uint8_t bgR, bgG, bgB;

    if (Transparent())
    {
        unsigned int vSqIndex = y >> 3; // y / 8;
        unsigned int hSqIndex = x >> 3; // x / 8;
        unsigned char sqColor = 0 == (vSqIndex + hSqIndex) % 2 ? 0xC0 : 0xFF;
        bgR = bgG = bgB = sqColor;
    }
    else
    {
        bgR = BgR();
        bgG = BgG();
        bgB = BgB();
    }

    uint8_t a = (srcPixel & 0xFF000000) >> 24;
    dp.r = AlphaComposite((srcPixel & 0x000000FF), a, bgR);
    dp.g = AlphaComposite((srcPixel & 0x0000FF00) >> 8, a, bgG);
    dp.b = AlphaComposite((srcPixel & 0x00FF0000) >> 16, a, bgB);
}
