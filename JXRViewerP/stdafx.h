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

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// TODO: reference additional headers your program requires here
#include <ShellApi.h> // File drag & drop stuff

#include <string>

#ifdef _UNICODE
typedef std::wstring String;
#else
typedef std::string String;
#endif

////////////////// JXR stuff ///////////////////
#include "JXRGlue.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;

#include "RasterImage_FF.h"

#include "nsDecoder.h"

//#define USE_CHAIN_BUFFER

#ifdef USE_CHAIN_BUFFER

#include <vector>
#include "ChainBuffer.h"

typedef CChainBuffer<0x4000> CChainBuffer_16K;
//typedef CChainBuffer<0x1000> CChainBuffer_16K;

ERR CreateWS_ChainBuf(struct WMPStream **ppWS, CChainBuffer_16K *pChainBuf);

#endif // USE_CHAIN_BUFFER


#include "nsJPEGXRDecoder.h"

bool DecodeS(const TCHAR *fileName, RasterImage &image, unsigned int scale);

template <typename T>
T Min(T a, T b)
{
    return a < b ? a : b;
}

//#include <algorithm> // std::sort()
#include <set>