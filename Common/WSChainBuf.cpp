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


// A jxrlib-compliant stream based ona chain buffer

#include "stdafx.h"

EXTERN_C ERR WMPAlloc(void **ppv, size_t cb); // declared in strcodec.h
EXTERN_C ERR WMPFree(void **ppv);  // declared in strcodec.h

class CChaninBufStream
{
    CChainBuffer_16K *m_pChainBuf;
    size_t m_pos;

public:

    CChaninBufStream(CChainBuffer_16K *pChainBuf) : m_pos(0), m_pChainBuf(pChainBuf)
    {
    }

    unsigned int GetPos() const
    {
        return m_pos;
    }

    unsigned int GetTotalSize() const
    {
        return m_pChainBuf->GetTotalSize();
    }

    bool SetPos(unsigned int pos)
    {
        if (pos > m_pChainBuf->GetTotalSize())
            return false; // m_pos == TotalSize means EOS

        m_pos = pos;
        return true;
    }

    bool Read(unsigned char *buf, unsigned int numBytesToRead, unsigned int &bytesRead)
    {
        if (GetPos() + numBytesToRead >= m_pChainBuf->GetTotalSize())
        {
            bytesRead = 0;
            return false;
        }

        bool res = m_pChainBuf->Read(GetPos(), buf, numBytesToRead, bytesRead);

        if (res)
            m_pos += bytesRead;
        else
            bytesRead = 0;

        return res;
    }

    bool Read1(unsigned char *buf, unsigned int numBytesToRead, unsigned int &bytesRead)
    {
        bool res = m_pChainBuf->Read(GetPos(), buf, numBytesToRead, bytesRead);

        if (res)
            m_pos += bytesRead;
        else
            bytesRead = 0;

        return res;
    }

    bool Write(const unsigned char *buf, unsigned int numBytesToWrite)
    {
        bool res = m_pChainBuf->Write(GetPos(), buf, numBytesToWrite);

        if (res)
            m_pos += numBytesToWrite;

        return res;
    }
};

ERR CloseWS_ChainBuf(struct WMPStream **ppWS)
{
    ERR err = WMP_errSuccess;

    WMPStream *pWS = *ppWS;

    if (nullptr != pWS && NULL != pWS->state.pvObj)
    {
        CChaninBufStream *pChainBufStream = (CChaninBufStream *)pWS->state.pvObj;
        delete pChainBufStream;
    }

    Call(WMPFree((void **)ppWS));

Cleanup:
    return err;
}

static Bool EOSWS_ChainBuf(struct WMPStream *me)
{
    if (nullptr == me->state.pvObj)
        return true;//WMP_errNotInitialized;

    CChaninBufStream *pChainBufStream = (CChaninBufStream *)me->state.pvObj;
    return pChainBufStream->GetPos() >= pChainBufStream->GetTotalSize();
}

static ERR ReadWS_ChainBuf(struct WMPStream *me, void *pv, size_t cb)
{
    if (nullptr == me->state.pvObj)
        return WMP_errNotInitialized;

    CChaninBufStream *pChainBufStream = (CChaninBufStream *)me->state.pvObj;
    unsigned int bytesRead;
    bool res = pChainBufStream->Read((unsigned char *)pv, cb, bytesRead);

    if (!res || cb != bytesRead)
        return WMP_errFail; // !!!TMP!!!

    return WMP_errSuccess;
}

static ERR ReadWS_ChainBuf1(struct WMPStream *me, void *pv, size_t cbToRead, size_t *pcbRead)
{
    if (nullptr == me->state.pvObj)
        return WMP_errNotInitialized;

    CChaninBufStream *pChainBufStream = (CChaninBufStream *)me->state.pvObj;
    unsigned int bytesRead;
    bool res = pChainBufStream->Read1((unsigned char *)pv, cbToRead, bytesRead);

    if (!res)
    {
        *pcbRead = 0;
        return WMP_errFail; // !!!TMP!!!
    }

    *pcbRead = bytesRead;
    return WMP_errSuccess;
}

static ERR WriteWS_ChainBuf(struct WMPStream *me, const void *pv, size_t cb)
{
    if (nullptr == me->state.pvObj)
        return WMP_errNotInitialized;

    CChaninBufStream *pChainBufStream = (CChaninBufStream *)me->state.pvObj;
    bool res = pChainBufStream->Write((const unsigned char *)pv, cb);

    return res ? WMP_errSuccess : WMP_errFail;
}

static ERR SetPosWS_ChainBuf(WMPStream *me, size_t offPos)
{
    if (nullptr == me->state.pvObj)
        return WMP_errNotInitialized;

    CChaninBufStream *pChainBufStream = (CChaninBufStream *)me->state.pvObj;
    bool res = pChainBufStream->SetPos(offPos);

    return res ? WMP_errSuccess : WMP_errFail;
}

static ERR GetPosWS_ChainBuf(WMPStream *me, size_t *poffPos)
{
    if (nullptr == me->state.pvObj)
        return WMP_errNotInitialized;

    CChaninBufStream *pChainBufStream = (CChaninBufStream *)me->state.pvObj;
    *poffPos = pChainBufStream->GetPos();

    return WMP_errSuccess;
}

ERR GetTotalSizeWS_ChainBuf(struct WMPStream* me, size_t* pTotalSize)
{
    if (nullptr == me->state.pvObj)
        return WMP_errNotInitialized;

    CChaninBufStream *pChainBufStream = (CChaninBufStream *)me->state.pvObj;
    *pTotalSize = pChainBufStream->GetTotalSize();

    return WMP_errSuccess;
}


ERR CreateWS_ChainBuf(struct WMPStream **ppWS, CChainBuffer_16K *pChainBuf)
{
    ERR err = WMP_errSuccess;
    struct WMPStream *pWS = NULL;

    Call(WMPAlloc((void **)ppWS, sizeof(**ppWS)));
    pWS = *ppWS;

    memset(&pWS->state, 0, sizeof(pWS->state));

    pWS->state.pvObj = new CChaninBufStream(pChainBuf);

    pWS->Close = CloseWS_ChainBuf;
    pWS->EOS = EOSWS_ChainBuf;

    pWS->Read = ReadWS_ChainBuf;
    pWS->Read1 = ReadWS_ChainBuf1;
    pWS->Write = WriteWS_ChainBuf;

    pWS->SetPos = SetPosWS_ChainBuf;
    pWS->GetPos = GetPosWS_ChainBuf;
    //pWS->GetTotalSize = GetTotalSizeWS_ChainBuf;

Cleanup:
    return err;
}

