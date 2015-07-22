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

struct nsIntRect
{
    uint32_t left, top, width, height;

    nsIntRect(uint32_t left_, uint32_t top_, uint32_t width_, uint32_t height_) : 
        left(left_), top(top_), width(width_), height(height_)
    {
    }
};

typedef int nsresult;

#define NS_ERROR_SUCCESS 0
#define NS_ERROR_FAILURE 1

class nsDecoder
{
    bool m_hasSize;
    bool m_hasError;

protected:

    RasterImage *m_image;

public:

    nsDecoder(RasterImage &aImage) : 
        m_image(&aImage), m_hasSize(false), m_hasError(false)
    {
    }

    virtual void WriteInternal(const char *aBuffer, uint32_t aCount)//, DecodeStrategy aStrategy);
    {
        UNREFERENCED_PARAMETER(aBuffer);
        UNREFERENCED_PARAMETER(aCount);
    }

    virtual void FinishInternal()
    {
    }

    bool HasError()
    {
        return m_hasError;
    }

    bool HasSize()
    {
        return m_hasSize;
    }

protected:

    bool IsSizeDecode()
    {
        return false;
    }

    void PostDecoderError(nsresult aFailCode)
    {
        UNREFERENCED_PARAMETER(aFailCode);
        m_hasError = true;
    }

    void PostDataError()
    {
        m_hasError = true;
    }

    void PostInvalidation(const nsIntRect &r)
    {
        m_image->InvalidateRect(r.left, r.top, r.width, r.height, true);
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
};
