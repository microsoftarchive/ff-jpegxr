// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>

// TODO: reference additional headers your program requires here
#include    <http.h>

#include <string>

template <typename T>
T Min(T a, T b)
{
    return a < b ? a : b;
}
