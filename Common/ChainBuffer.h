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

typedef void (*PFCopyFromChainBufCallback)(
    const unsigned char *srcData, unsigned int srcDataSize, void *param);

// A quick and dirty chain buffer consisting of "links" - heap-allocated individual 
// buffers of equal size
#if 0
#define CHAIN_LINK_BUF_SIZE 0x4000 // 0x10000 // 64K
#else
template <unsigned int CHAIN_LINK_BUF_SIZE>
#endif
class CChainBuffer
{
public:

    typedef void * (*PAllocProc)(size_t cBytes);
    typedef void (*PFreeProc)(void *ptr);
    typedef std::vector<unsigned char *> BufferChain;

private:

    BufferChain m_bufChain;
    unsigned int m_totalSize;
    PAllocProc m_pAllocProc;
    PFreeProc m_pFreeProc;

    unsigned char * AllocateLink()
    {
        if (NULL == m_pAllocProc)
            return NULL;

        return (unsigned char *)m_pAllocProc(CHAIN_LINK_BUF_SIZE);
    }

public:

    void SetMemAllocProc(PAllocProc pAllocProc, PFreeProc pFreeProc)
    {
        m_pAllocProc = pAllocProc;
        m_pFreeProc = pFreeProc;
    }

protected:

    // newSize is new size to fit in the buffer
    bool SetSize(unsigned int newSize)
    {
        if (GetTotalSize() == newSize)
            return true;

        int linkIndex = 0 == GetTotalSize() ? -1 : (GetTotalSize() - 1) / CHAIN_LINK_BUF_SIZE;
        int newLinkIndex = int((newSize - 1) / CHAIN_LINK_BUF_SIZE);

        if (newLinkIndex > linkIndex)
        {
            for (int i = linkIndex; i < newLinkIndex; ++i)
            {
                unsigned char *buf = AllocateLink();

                if (NULL == buf)
                    return false;

                m_bufChain.push_back(buf);
            }
        }
        else if (newLinkIndex < linkIndex)
        {
            for (int i = linkIndex; i > newLinkIndex; --i)
                m_bufChain.pop_back();
        }

        m_totalSize = newSize;
        return true;
    }

public:

    CChainBuffer() : m_totalSize(0), m_pAllocProc(NULL), m_pFreeProc(NULL)
    {
    }

    ~CChainBuffer()
    {
        Clear();
    }

    static unsigned int GetLinkSize()
    {
        return CHAIN_LINK_BUF_SIZE;
    }

    unsigned int GetTotalSize() const
    {
        return m_totalSize;
    }

    size_t GetLinkCount() const
    {
        return m_bufChain.size();
    }

    unsigned char * HeadLinkBuf()
    {
        return m_bufChain.front();
    }

    unsigned char * LinkBuf(unsigned int index)
    {
        return m_bufChain[index];
    }

    bool Read(unsigned int offset, unsigned char *data, unsigned int numBytes, unsigned int &bytesRead)
    {
        if (0 == GetTotalSize())
        {
            bytesRead = 0;
            return false;
        }

        unsigned int startLinkIndex = 0 == offset ? 0 : (offset - 1) / CHAIN_LINK_BUF_SIZE;
        unsigned int startLinkOffset = offset % CHAIN_LINK_BUF_SIZE;
        unsigned int endOffset = offset + numBytes;

        if (endOffset > GetTotalSize())
            numBytes = GetTotalSize() - offset;

        unsigned char *p = data;
        unsigned int remaining = numBytes;

        if (0 != startLinkOffset)
        {
            unsigned char *buf = m_bufChain[startLinkIndex];
            unsigned int toCopy = Min(numBytes, CHAIN_LINK_BUF_SIZE - startLinkOffset);
            memcpy(p, buf + startLinkOffset, toCopy);
            remaining -= toCopy;
            p += toCopy;
            ++startLinkIndex;
        }

        for (unsigned int i = startLinkIndex; remaining > 0; ++i)
        {
            unsigned char *buf = m_bufChain[i];
            unsigned int toCopy = Min(remaining, CHAIN_LINK_BUF_SIZE);
            memcpy(p, buf, toCopy);
            remaining -= toCopy;
            p += toCopy;
        }

        bytesRead = numBytes;
        return true;
    }

    bool WriteAtEnd(const unsigned char *data, unsigned int numBytes)
    {
        return Write(GetTotalSize(), data, numBytes);
    }

    bool Write(unsigned int offset, const unsigned char *data, unsigned int numBytes)
    {
        if (0 == numBytes)
            return false;

        unsigned int startLinkIndex = offset / CHAIN_LINK_BUF_SIZE;
        unsigned int startLinkOffset = offset % CHAIN_LINK_BUF_SIZE;
        unsigned int newOffset = offset + numBytes;

        if (newOffset > GetTotalSize() && !SetSize(newOffset))
            return false;

        const unsigned char *p = data;
        unsigned int remaining = numBytes;

        if (0 != startLinkOffset)
        {
            unsigned char *buf = m_bufChain[startLinkIndex];
            unsigned int toCopy = Min(numBytes, CHAIN_LINK_BUF_SIZE - startLinkOffset);
            memcpy(buf + startLinkOffset, p, toCopy);
            remaining -= toCopy;
            p += toCopy;
            ++startLinkIndex;
        }

        for (unsigned int i = startLinkIndex; remaining > 0; ++i)
        {
            unsigned char *buf = m_bufChain[i];
            unsigned int toCopy = Min(remaining, CHAIN_LINK_BUF_SIZE);
            memcpy(buf, p, toCopy);
            remaining -= toCopy;
            p += toCopy;
        }

        return true;
    }

    void Clear()
    {
        for (BufferChain::const_iterator i = m_bufChain.begin(); 
            i != m_bufChain.end(); ++i)
        {
            unsigned char *pBuf = *i;
            m_pFreeProc(pBuf);
        }

        m_bufChain.clear();
        m_totalSize = 0;
    }

    // Copy the contents into a contiguous destination buffer
    // numBytes == 0 means "all remaining bytes"
    unsigned int CopyFrom(PFCopyFromChainBufCallback copyCB, void *param, 
        unsigned int offset = 0, unsigned int numBytes = 0)
    {
        if (offset > GetTotalSize())
            return 0;

        if (0 != numBytes && numBytes > TotalSize() - offset)
            numBytes = 0;  // copy all remaining bytes

        // Calculate the index of the starting link
        const unsigned int first = offset / CHAIN_LINK_BUF_SIZE;
        const unsigned int last = 0 == numBytes ? GetLinkCount() - 1 : 
            (offset + numBytes - 1) / CHAIN_LINK_BUF_SIZE;

        unsigned int startOffset = offset % CHAIN_LINK_BUF_SIZE;
        unsigned int totalCopied = 0;

        for (unsigned int i = first; i < last; ++i)
        {
            const unsigned char *pSrc = m_bufChain[i];
            unsigned int numBytesToCopy = CHAIN_LINK_BUF_SIZE;

            if (first == i)
            {
                pSrc += startOffset;
                numBytesToCopy -= startOffset;
            }

            copyCB(pSrc, numBytesToCopy, param);
            totalCopied += numBytesToCopy;
        }

        // Copy the last chunk
        unsigned int remaining = 
            (0 == numBytes ? GetTotalSize() : offset + numBytes) % CHAIN_LINK_BUF_SIZE;

        const unsigned char *pSrc = m_bufChain[last];

        if (last == first)
        {
            pSrc += startOffset;
            remaining -= startOffset;
        }

        copyCB(pSrc, remaining, param);
        totalCopied += remaining;

        return totalCopied;
    }
};
