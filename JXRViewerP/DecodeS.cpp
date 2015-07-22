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

bool DecodeS(const TCHAR *fileName, RasterImage &image, unsigned int scale)
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

    nsJPEGXRDecoder decoder(image, false);// true
    decoder.InitInternal();

    if (decoder.HasError())
    {
        CloseHandle(hFile);
        return false;
    }

    decoder.SetScale(scale);

    unsigned char *buf = new unsigned char[fileSize];
    DWORD bytesRead;
    BOOL res = ReadFile(hFile, buf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!res)
    {
        delete [] buf;
        return false;
    }

    unsigned int processed = 0;

    // Buffer for the simulation of downloading
    const unsigned int maxChunkSize = 
        //0x100; // 256 bytes
        //0x200; // 512 bytes
        //0x400; // 1K
        //0x800; // 2K
        //0x1000; // 4K
        //0x2000; // 8K
        0x4000; // 16K;
        //0x8000; // 32K
        //0x10000; // 64K
        //0x20000; // 128K 
        //39137; // 

    const char *p = (char *)buf;

    for (unsigned int remaining = fileSize; remaining > 0; )
    {
        unsigned int chunkSize = //0 == processed ? 5 : (processed < 2048 ? 50 : 
            Min(maxChunkSize, remaining)
            //)
            ;
        if (remaining <= maxChunkSize)
        {
            int i = 0; // just to set a breakpoint for the last chunk
        }

        decoder.WriteInternal(p, chunkSize);

        if (decoder.HasError())
            break;

        remaining -= chunkSize;
        processed += chunkSize;
        p += chunkSize;
    }

    // Last call with 0 byte count
    if (!decoder.HasError())
    {
        //decoder.WriteInternal(NULL, 0);
        decoder.FinishInternal();
    }

    delete [] buf;

    return true;
}
