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


// TestHTTPServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

//
// Macros.
//
#define INITIALIZE_HTTP_RESPONSE( resp, status, reason )    \
    do                                                      \
    {                                                       \
        RtlZeroMemory( (resp), sizeof(*(resp)) );           \
        (resp)->StatusCode = (status);                      \
        (resp)->pReason = (reason);                         \
        (resp)->ReasonLength = (USHORT) strlen(reason);     \
    } while (FALSE)

#define ADD_KNOWN_HEADER(Response, HeaderId, RawValue)               \
    do                                                               \
    {                                                                \
        (Response).Headers.KnownHeaders[(HeaderId)].pRawValue =      \
                                                          (RawValue);\
        (Response).Headers.KnownHeaders[(HeaderId)].RawValueLength = \
            (USHORT) strlen(RawValue);                               \
    } while(FALSE)

#define ALLOC_MEM(cb) HeapAlloc(GetProcessHeap(), 0, (cb))
#define FREE_MEM(ptr) HeapFree(GetProcessHeap(), 0, (ptr))

//
// Prototypes.
//
DWORD
DoReceiveRequests(HANDLE hReqQueue);

DWORD
SendHttpResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest,
    IN USHORT        StatusCode,
    IN PSTR          pReason,
    IN PSTR          pEntity
    );

/*******************************************************************++

Routine Description:
    main routine

Arguments:
    argc - # of command line arguments.
    argv - Arguments.

Return Value:
    Success/Failure

--*******************************************************************/

HANDLE gs_exitEvent;

static BOOL WINAPI MyHandlerRoutine(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        SetEvent(gs_exitEvent);
        return TRUE;
    }

    return FALSE;
}

// We have only one virtual directory abd one corresponding physical directory
static wchar_t *gs_virtDir; // virtual directory
static wchar_t *gs_physDir; // physical directory
static long gs_minChunkSize, gs_maxChunkSize; // minimum and maximum data chunk size
static long gs_chunkPause; // pause, in milliseconds, between data chuns

static void Usage()
{
    wprintf(L"Usage:\n\tTestHTTPServer.exe <virt_dir> <phys_dir> [min_chunk_size] [max_chunk_size] [pause_millisec]\n"
        L"where min_chunk_size <= max_chunk_size");
}

int __cdecl wmain(int argc, wchar_t * argv[])
{
    //HTTP_TIMEOUT_LIMIT_INFO testInfo;

    ULONG retCode;
    HANDLE hReqQueue = NULL;
    HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_1;

    if (argc < 3)
    {
        Usage();
        return -1;
    }

    gs_virtDir = argv[1];
    gs_physDir = argv[2];

    if (argc > 3)
        gs_minChunkSize = _wtol(argv[3]);

    if (gs_minChunkSize <= 0)
        gs_minChunkSize = 2048;

    if (argc > 4)
        gs_maxChunkSize = _wtol(argv[4]);

    if (gs_maxChunkSize <= 0)
        gs_maxChunkSize = gs_minChunkSize;

    if (argc > 5)
        gs_chunkPause = _wtol(argv[5]);

    if (gs_chunkPause <= 0)
        gs_chunkPause = 100;

    if (gs_minChunkSize > gs_maxChunkSize)
    {
        Usage();
        return -1;
    }

    std::wstring URI = L"http://+:80/";

    //
    // Initialize HTTP Server APIs
    //
    retCode = HttpInitialize( 
            HttpApiVersion,
            HTTP_INITIALIZE_SERVER,    // Flags
            NULL                       // Reserved
            );

    if (retCode != NO_ERROR)
    {
        wprintf(L"HttpInitialize failed with %lu \n", retCode);
        return retCode;
    }

    gs_exitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    //
    // Create a Request Queue Handle
    //
    retCode = HttpCreateHttpHandle(
            &hReqQueue,        // Req Queue
            0                  // Reserved
            );

    if (retCode != NO_ERROR)
    {
        wprintf(L"HttpCreateHttpHandle failed with %lu \n", retCode);
        goto CleanUp;
    }

    // Register the URIs to Listen On 
    // The URI is a fully qualified URI and must include the
    // terminating (/) character.
    //
    URI += gs_virtDir;
    URI += L"/";

    wprintf(L"listening for requests on the following url: %s\n", URI.c_str());

    retCode = HttpAddUrl(
            hReqQueue,    // Req Queue
            URI.c_str(),  // Fully qualified URL
            NULL          // Reserved
            );

    if (retCode != NO_ERROR)
    {
        if (ERROR_INVALID_PARAMETER == retCode)
        {
            int k = 0;
        }

        wprintf(L"HttpAddUrl failed with %lu \n", retCode);
        goto CleanUp;
    }

    // Call the Routine to Receive a Request

    DoReceiveRequests(hReqQueue);

    // Cleanup the HTTP Server API

CleanUp:

    //
    // Call HttpRemoveUrl for all added URLs.
    //
    HttpRemoveUrl(hReqQueue, URI.c_str());

    //
    // Close the Request Queue handle.
    //
    if (NULL != hReqQueue)
    {
        CloseHandle(hReqQueue);
    }

    // 
    // Call HttpTerminate.
    //
    HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);

    if (NULL != gs_exitEvent)
        CloseHandle(gs_exitEvent); // not necessary

    return retCode;
}

inline int getrandom(int min, int max)
{
    return rand() % (max + 1 - min) + min;
}

static DWORD SendFile(HANDLE hReqQueue, PHTTP_REQUEST pRequest, 
    const WCHAR *fileName, const char *MIMEStr, bool headerOnly)
{
    HANDLE hFile = CreateFile(fileName, 
        GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ,        // Share for reading
        NULL,                   // No security descriptor
        OPEN_EXISTING,          // Overrwrite existing
        FILE_ATTRIBUTE_NORMAL,  // Normal file
        NULL
        );

    if (INVALID_HANDLE_VALUE == hFile)
    {
        DWORD result = GetLastError();
        return result;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);

#define MAX_ULONG_STR ((ULONG) sizeof("4294967295"))

    // To illustrate entity sends via 
    // HttpSendResponseEntityBody, the response will 
    // be sent over multiple calls. To do this,
    // pass the HTTP_SEND_RESPONSE_FLAG_MORE_DATA
    // flag.
    //
    // Because the response is sent over multiple
    // API calls, add a content-length.
    //
    // Alternatively, the response could have been
    // sent using chunked transfer encoding, by 
    // passing "Transfer-Encoding: Chunked".

    // NOTE: Because the TotalBytesread in a ULONG
    //       are accumulated, this will not work
    //       for entity bodies larger than 4 GB. 
    //       For support of large entity bodies,
    //       use a ULONGLONG.

    // Initialize the HTTP response structure.
    HTTP_RESPONSE response;
    INITIALIZE_HTTP_RESPONSE(&response, 200, "OK");

    CHAR szContentLength[MAX_ULONG_STR];
    sprintf_s(szContentLength, MAX_ULONG_STR, "%lu", fileSize);

    ADD_KNOWN_HEADER(response, HttpHeaderContentType, MIMEStr);

    //if (!headerOnly)
    {
        ADD_KNOWN_HEADER(response, HttpHeaderContentLength, szContentLength);
    }

    DWORD bytesSent;

    DWORD result = HttpSendHttpResponse(
           hReqQueue,           // ReqQueueHandle
           pRequest->RequestId, // Request ID
           headerOnly ? 0 : HTTP_SEND_RESPONSE_FLAG_MORE_DATA, 
           &response,           // HTTP response
           NULL,                // pReserved1
           &bytesSent,          // bytes sent-optional
           NULL,                // pReserved2
           0,                   // Reserved3
           NULL,                // LPOVERLAPPED
           NULL                 // pReserved4
           );

    if (NO_ERROR != result)
    {
        CloseHandle(hFile);

        wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
        return result;
    }

    if (headerOnly)
    {
        CloseHandle(hFile);
        return result;
    }

    // Send entity body from a file handle.
    HTTP_DATA_CHUNK dataChunk;
    dataChunk.DataChunkType = HttpDataChunkFromFileHandle;
    dataChunk.FromFileHandle.FileHandle = hFile;
    DWORD sent = 0;
    DWORD remaining = fileSize;
    srand(GetTickCount());

    for (; remaining > 0;)
    {
        DWORD chunkSize = gs_minChunkSize == gs_maxChunkSize ? gs_minChunkSize : getrandom(gs_minChunkSize, gs_maxChunkSize);
        DWORD toSend = Min(remaining, chunkSize);

        dataChunk.FromFileHandle.ByteRange.StartingOffset.QuadPart = sent;
        dataChunk.FromFileHandle.ByteRange.Length.QuadPart = toSend;
        remaining -= toSend;

        result = HttpSendResponseEntityBody(
                hReqQueue, 
                pRequest->RequestId, 
                0 == remaining ? 0 : HTTP_SEND_RESPONSE_FLAG_MORE_DATA,        // 0 if this is the last send.
                1,           // Entity Chunk Count.
                &dataChunk, 
                NULL, 
                NULL, 
                0, 
                NULL, 
                NULL
                );

        if (NO_ERROR != result)
        {
           wprintf(L"HttpSendResponseEntityBody failed %lu\n", result);
           break;
        }

        sent += toSend;
        wprintf(L"A %lu-byte chunk has been sent, total %lu bytes sent out of %lu\n", toSend, sent, fileSize);
        Sleep(gs_chunkPause);
    }

    if (NO_ERROR == result)
        wprintf(L"Done!\n");

    CloseHandle(hFile);

    return result;
}

static const WCHAR * GetFileExt(const WCHAR *fileName)
{
    if (NULL == fileName)
        return L"";

    const WCHAR *ext = wcsrchr(fileName, L'.');

    if (NULL == ext)
        return L"";

    return ext + 1;
}

static char * GetFileMIMEString(const WCHAR *ext)
{
    if (_wcsicmp(L"jpg", ext) == 0 || _wcsicmp(L"jpeg", ext) == 0)
        return "image/jpeg";

    if (_wcsicmp(L"png", ext) == 0)
        return "image/png";

    if (_wcsicmp(L"tif", ext) == 0 || _wcsicmp(L"tiff", ext) == 0)
        return "image/tiff";

    if (_wcsicmp(L"gif", ext) == 0)
        return "image/gif";

    if (_wcsicmp(L"bmp", ext) == 0)
        return "image/bmp";

    if (_wcsicmp(L"txt", ext) == 0)
        return "text/plain";

    if (_wcsicmp(L"htm", ext) == 0 || _wcsicmp(L"html", ext) == 0)
        return "text/html";

    if (_wcsicmp(L"jxr", ext) == 0 || _wcsicmp(L"hdp", ext) == 0 || _wcsicmp(L"wdp", ext) == 0)
        return "image/vnd.ms-photo";

    return "application/octet-stream";
}

static DWORD HandleHttpHeadGetRequest(HANDLE hReqQueue, PHTTP_REQUEST pRequest, bool isGet)
{
    DWORD result;
    const WCHAR *path = pRequest->CookedUrl.pAbsPath;

    if (NULL == path)
    {
        result = SendHttpResponse(
                hReqQueue, 
                pRequest, 
                204, 
                "No content", 
                NULL
                );

        return result;
    }

    ++path;

    // Try to find the end of the directory name
    const WCHAR *dirEnd = wcschr(path, L'/');

    if (NULL == dirEnd)
    {
        // We do not allow listing of the directory content
        result = SendHttpResponse(
                hReqQueue, 
                pRequest, 
                403, 
                "Forbidden", 
                NULL
                );

        return result;
    }

    // Download a file from a virtual directory.
    // Compose a full folder path first.
    std::wstring fullPath = gs_physDir;
    ++dirEnd;
    fullPath += L"\\";
    fullPath += dirEnd;

    const WCHAR *ext = GetFileExt(fullPath.c_str());
    const char *imgMIMEStr = GetFileMIMEString(ext);

    result = SendFile(hReqQueue, pRequest, fullPath.c_str(), imgMIMEStr, !isGet);

    if (ERROR_FILE_NOT_FOUND == result || 
        ERROR_INVALID_NAME == result)
    {
        result = SendHttpResponse(
                hReqQueue, 
                pRequest, 
                404, 
                "Not found", 
                NULL
                );
    }
    else if (ERROR_NETNAME_DELETED == result) // request has been cancelled
        return result;
    else if (NO_ERROR != result)
    {
        result = SendHttpResponse(
                hReqQueue, 
                pRequest, 
                500, 
                "Internal server error", 
                NULL
                );
    }

    return result;
}

static DWORD
SendHttpResponseW(
    HANDLE        hReqQueue, 
    PHTTP_REQUEST pRequest, 
    USHORT        StatusCode, 
    PCSTR         pReason, 
    PCWSTR        pEntityStringW
    )
{
    HTTP_RESPONSE   response;
    HTTP_DATA_CHUNK dataChunk;
    DWORD           result;
    DWORD           bytesSent;

    // Initialize the HTTP response structure.
    INITIALIZE_HTTP_RESPONSE(&response, StatusCode, pReason);

    // Add a known header.
    ADD_KNOWN_HEADER(response, HttpHeaderContentType, "text/html; charset=utf-16");

    if (NULL != pEntityStringW)
    {
        // Add an entity chunk.
        dataChunk.DataChunkType           = HttpDataChunkFromMemory;
        dataChunk.FromMemory.pBuffer      = (PVOID)pEntityStringW;
        dataChunk.FromMemory.BufferLength = (ULONG)wcslen(pEntityStringW) * sizeof(WCHAR);

        response.EntityChunkCount         = 1;
        response.pEntityChunks            = &dataChunk;
    }

    // Because the entity body is sent in one call, it is not
    // required to specify the Content-Length.
    result = HttpSendHttpResponse(
            hReqQueue,           // ReqQueueHandle
            pRequest->RequestId, // Request ID
            0,                   // Flags
            &response,           // HTTP response
            NULL,                // pReserved1
            &bytesSent,          // bytes sent  (OPTIONAL)
            NULL,                // pReserved2  (must be NULL)
            0,                   // Reserved3   (must be 0)
            NULL,                // LPOVERLAPPED(OPTIONAL)
            NULL                 // pReserved4  (must be NULL)
            );

    if (result != NO_ERROR)
    {
        wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
    }

    return result;
}

// Handle received requests
static DWORD HandleRequest(HANDLE hReqQueue, PHTTP_REQUEST pRequest)
{
    DWORD result;

    switch (pRequest->Verb)
    {
        case HttpVerbGET:
            wprintf(L"Got a GET request for %ws \n", 
                pRequest->CookedUrl.pFullUrl);

            result = HandleHttpHeadGetRequest(hReqQueue, pRequest, true);
            break;

        case HttpVerbHEAD:
            wprintf(L"Got a HEAD request for %ws \n", 
                pRequest->CookedUrl.pFullUrl);

            result = HandleHttpHeadGetRequest(hReqQueue, pRequest, false);
            break;

        default:
            wprintf(L"Got a unknown request %ws for %ws \n", 
                pRequest->pUnknownVerb, pRequest->CookedUrl.pFullUrl);

            result = SendHttpResponse(
                    hReqQueue, 
                    pRequest, 
                    503, 
                    "Not Implemented", 
                    NULL
                    );
            break;
    }

    return result;
}

/*******************************************************************++

Routine Description:
    The function to receive a request. This function calls the  
    corresponding function to handle the response.

Arguments:
    hReqQueue - Handle to the request queue

Return Value:
    Success/Failure.

--*******************************************************************/
DWORD
DoReceiveRequests(
    HANDLE hReqQueue
    )
{
    ULONG              result;
    HTTP_REQUEST_ID    requestId;
    DWORD              bytesRead;

    //
    // Allocate a 2 KB buffer. This size should work for most 
    // requests. The buffer size can be increased if required. Space
    // is also required for an HTTP_REQUEST structure.
    //
    ULONG RequestBufferLength = sizeof(HTTP_REQUEST) + 2048;
    PCHAR pRequestBuffer = (PCHAR)ALLOC_MEM(RequestBufferLength);

    if (NULL == pRequestBuffer)
        return ERROR_NOT_ENOUGH_MEMORY;

    PHTTP_REQUEST pRequest = (PHTTP_REQUEST)pRequestBuffer;

    //
    // Wait for a new request. This is indicated by a NULL 
    // request ID.
    //

    HTTP_SET_NULL_ID(&requestId);

    // Receive a Request
    for (; ;)
    {
        RtlZeroMemory(pRequest, RequestBufferLength);

        result = HttpReceiveHttpRequest(
                    hReqQueue,              // Req Queue
                    requestId,              // Req ID
                    0,                      // Flags
                    pRequest,               // HTTP request buffer
                    RequestBufferLength,    // req buffer length
                    &bytesRead,             // bytes received
                    NULL                    // LPOVERLAPPED
                    );

        // Handle the HTTP Request

        if (NO_ERROR == result)
        {
            result = HandleRequest(hReqQueue, pRequest);

            if (NO_ERROR != result && ERROR_NETNAME_DELETED != result)
                break;

            // Reset the Request ID to handle the next request.
            HTTP_SET_NULL_ID(&requestId);
        }
        else if (result == ERROR_MORE_DATA)
        {
            //
            // The input buffer was too small to hold the request
            // headers. Increase the buffer size and call the 
            // API again. 
            //
            // When calling the API again, handle the request
            // that failed by passing a RequestID.
            //
            // This RequestID is read from the old buffer.
            //
            requestId = pRequest->RequestId;

            //
            // Free the old buffer and allocate a new buffer.
            //
            RequestBufferLength = bytesRead;
            FREE_MEM(pRequestBuffer);
            pRequestBuffer = (PCHAR)ALLOC_MEM( RequestBufferLength );

            if (pRequestBuffer == NULL)
            {
                result = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            pRequest = (PHTTP_REQUEST)pRequestBuffer;

        }
        else if (ERROR_CONNECTION_INVALID == result && 
            !HTTP_IS_NULL_ID(&requestId))
        {
            // The TCP connection was corrupted by the peer when
            // attempting to handle a request with more buffer. 
            // Continue to the next request.
            
            HTTP_SET_NULL_ID(&requestId);
        }
        else
        {
            break;
        }
    } // for(;;)

    if (NULL != pRequestBuffer)
    {
        FREE_MEM(pRequestBuffer);
    }

    return result;
}

/*******************************************************************++

Routine Description:
    The routine sends a HTTP response

Arguments:
    hReqQueue     - Handle to the request queue
    pRequest      - The parsed HTTP request
    StatusCode    - Response Status Code
    pReason       - Response reason phrase
    pEntityString - Response entity body

Return Value:
    Success/Failure.
**********************************************/

// Send an HTTP Response

DWORD
SendHttpResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest,
    IN USHORT        StatusCode,
    IN PSTR          pReason,
    IN PSTR          pEntityString
    )
{
    HTTP_RESPONSE   response;
    HTTP_DATA_CHUNK dataChunk;
    DWORD           result;
    DWORD           bytesSent;

    //
    // Initialize the HTTP response structure.
    //
    INITIALIZE_HTTP_RESPONSE(&response, StatusCode, pReason);

    //
    // Add a known header.
    //
    ADD_KNOWN_HEADER(response, HttpHeaderContentType, "text/html");
   
    if (pEntityString)
    {
        //
        // Add an entity chunk.
        //
        dataChunk.DataChunkType           = HttpDataChunkFromMemory;
        dataChunk.FromMemory.pBuffer      = pEntityString;
        dataChunk.FromMemory.BufferLength = (ULONG) strlen(pEntityString);

        response.EntityChunkCount         = 1;
        response.pEntityChunks            = &dataChunk;
    }

    // 
    // Because the entity body is sent in one call, it is not
    // required to specify the Content-Length.
    //
    
    result = HttpSendHttpResponse(
            hReqQueue,           // ReqQueueHandle
            pRequest->RequestId, // Request ID
            0,                   // Flags
            &response,           // HTTP response
            NULL,                // pReserved1
            &bytesSent,          // bytes sent  (OPTIONAL)
            NULL,                // pReserved2  (must be NULL)
            0,                   // Reserved3   (must be 0)
            NULL,                // LPOVERLAPPED(OPTIONAL)
            NULL                 // pReserved4  (must be NULL)
            ); 

    if (result != NO_ERROR)
    {
        wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
    }

    return result;
}
