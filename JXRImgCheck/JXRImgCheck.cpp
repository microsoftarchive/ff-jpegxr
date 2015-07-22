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

typedef bool (&PFFileProc)(const char *fileName, void *param);

struct EnumParam
{
    unsigned int numFiles;
    unsigned int numFilesFailed;
    unsigned int numErrors;

    EnumParam() : numFiles(0), numFilesFailed(0), numErrors(0)
    {
    }
};

bool CheckImage(const char *fileName, void *param)
{
    EnumParam &ep = *(EnumParam *)param;
    unsigned int numErrors = 0;
    ERR err = WMP_errSuccess;

    // determine the size of the file
    HANDLE hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == hFile)
        return false;

    DWORD fileSize = GetFileSize(hFile, NULL);
    CloseHandle(hFile);

    if (INVALID_FILE_SIZE == fileSize)
        return false;

    WMPStream *pStream = NULL;
    Call(CreateWS_File(&pStream, fileName, _T("rb")));

    PKImageDecode *pDecoder;
    Call(PKImageDecode_Create_WMP(&pDecoder));
    Call(pDecoder->Initialize(pDecoder, pStream));

    size_t byteCount = pDecoder->WMP.wmiDEMisc.uImageByteCount;

    if (fileSize < pDecoder->WMP.wmiDEMisc.uImageOffset + byteCount)
    {
        size_t byteCount1 = fileSize - pDecoder->WMP.wmiDEMisc.uImageOffset;
        printf("\n\tInvalid byte count for main image plane: %u (must be %u)", byteCount, byteCount1);
        ++numErrors;
    }

    byteCount = pDecoder->WMP.wmiI_Alpha.uImageByteCount;

    if (pDecoder->WMP.wmiDEMisc.uAlphaOffset + byteCount > fileSize)
    {
        size_t byteCount1 = fileSize - pDecoder->WMP.wmiDEMisc.uAlphaOffset;
        printf("\n\tInvalid byte count for alpha plane: %u (must be %u)", byteCount, byteCount1);
        ++numErrors;
    }

    if (0 == numErrors)
        printf("\n\t- OK\n");
    else
    {
        ep.numErrors += numErrors;
        printf("\n\t- %u error(s) detected\n", numErrors);
    }

Cleanup:

    if (NULL != pDecoder)
        pDecoder->Release(&pDecoder);

    pStream->Close(&pStream);

    bool res = WMP_errSuccess == err;

    if (res)
        ++ep.numFiles;
    else
        ++ep.numFilesFailed;

    return res;
}

void ProcessFolder(const char *folderName, PFFileProc pProc, void *param)
{
    std::string fn = folderName;
    fn += "\\*.*";
    WIN32_FIND_DATA fd;
    unsigned int &totalErrors = *(unsigned int *)param;

    HANDLE h = FindFirstFile(fn.c_str(), &fd);

    while (h != INVALID_HANDLE_VALUE)
    {
        if (strcmp(".", fd.cFileName) == 0 || strcmp("..", fd.cFileName) == 0)
        {
            // do nothing
        }
        else
        {
            std::string path = folderName;
            path += "\\";
            path += fd.cFileName;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                printf("Processing folder %s...\n", path.c_str());
                ProcessFolder(path.c_str(), pProc, param);
            }
            else
            {
                const char *s = path.c_str();
                const char *p = strrchr(s, '.');

                // Process a JPEG-XR image file
                if (NULL != p && (stricmp(".jxr", p) == 0 || strcmp(".wdp", p) == 0 || strcmp(".hdp", p) == 0))
                {
                    printf("\t%s", path.c_str());
                    bool res = pProc(s, param);

                    if (!res)
                        printf("\n- Error processing file\n");
                }
            }
        }

        if (!FindNextFile(h, &fd))
        {
            FindClose(h);
            break;
        }
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2)
    {
        printf("Folder not specified!\n");
        return 1;
    }

    EnumParam ep;
    ProcessFolder(argv[1], CheckImage, &ep);
    printf("\n-------------------------------------------\n");
    printf("%u errors found in %u files\n", ep.numErrors, ep.numFiles);

    if (0 != ep.numFilesFailed)
        printf("Failed to open %u files\n", ep.numErrors, ep.numFilesFailed);

    return 0;
}

