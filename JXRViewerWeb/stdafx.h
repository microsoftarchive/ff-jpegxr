// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

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
#include <Winhttp.h>

#include <string>

#if defined(UNICODE)
    typedef std::wstring String;
#elif defined(_UNICODE)
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


#ifdef USE_CHAIN_BUFFER
    #include <vector>
#endif

template <typename T>
T Min(T a, T b)
{
    return a < b ? a : b;
}

#ifdef USE_CHAIN_BUFFER
    #include "ChainBuffer.h"
    typedef CChainBuffer<0x4000> CChainBuffer_16K;
#endif

#include "RasterImage_FF.h"
#include "nsDecoder.h"
#include "nsJPEGXRDecoder.h"

bool DecodeS(const TCHAR *fileName, RasterImage &image);

#ifdef USE_CHAIN_BUFFER
ERR CreateWS_ChainBuf(struct WMPStream **ppWS, CChainBuffer_16K *pChainBuf);
#endif

#include <set>
