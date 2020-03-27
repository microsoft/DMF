/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfIncludes_USER_MODE.h

Abstract:

    Includes files specifically for DMF_USER_MODE.

Environment:

    User-mode Driver Framework

--*/

#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Compiler warning filters.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Allow DMF Modules to disable false positive SAL warnings easily when 
// compiled as C++ file.
//
// NOTE: Use with caution. First, verify there are no legitimate warnings. Then, use this option.
//
#if (defined(DMF_SAL_CPP_FILTER) && defined(__cplusplus)) || defined(DMF_USER_MODE)
    // Disable some SAL warnings because they are false positives.
    // NOTE: The pointers are checked via ASSERTs.
    //

    // 'Dereferencing NULL pointer'.
    //
    #pragma warning(disable:6011)
    // '<argument> may be NULL'.
    //
    #pragma warning(disable:6387)
#endif // (defined(DMF_SAL_CPP_FILTER) && defined(__cplusplus)) || defined(DMF_USER_MODE)

// 'warning C4201: nonstandard extension used: nameless struct/union'
//
#pragma warning(disable:4201)

// 'Avoid 'goto''.
//
#pragma warning(disable:26438)

// 'Consider using gsl::finally if final action is intended (gsl.util)'.
//
#pragma warning(disable:26448)

// 'Use 'nullptr' rather than 0 or NULL (es.47)'.
//
#pragma warning(disable:26477)

// 'Don't pass a pointer that may be invalid to a function.'
//
#pragma warning(disable:26486)

// 'Don't dereference a pointer that may be invalid.'
//
#pragma warning(disable:26489)

// 'Variable 'returnValue' is uninitialized. Always initialize an object (type.5).'
//
#pragma warning(disable:26494)

// Check that the Windows version is Win10 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_OR_LATER (NTDDI_WIN10_RS3 && (NTDDI_VERSION >= NTDDI_WIN10))

// Check that the Windows version is RS3 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_RS3_OR_LATER (NTDDI_WIN10_RS3 && (NTDDI_VERSION >= NTDDI_WIN10_RS3))

// Check that the Windows version is RS4 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_RS4_OR_LATER (NTDDI_WIN10_RS4 && (NTDDI_VERSION >= NTDDI_WIN10_RS4))

// Check that the Windows version is RS5 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_RS5_OR_LATER (NTDDI_WIN10_RS5 && (NTDDI_VERSION >= NTDDI_WIN10_RS5))

// Check that the Windows version is 19H1 or earlier. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_19H1_OR_EARLIER (!(NTDDI_WIN10_19H1 && (NTDDI_VERSION > NTDDI_WIN10_19H1)))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// All include files needed by all Modules and the Framework.
// This ensures that all Modules always compile together so that any Module can always be used with any other
// Module without having to deal with include file dependencies.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Some environments use DBG instead of DEBUG. DMF uses DEBUG so, define DEBUG in that case.
//
#if DBG
    #if !defined(DEBUG)
        #define DEBUG
    #endif
#endif

// All DMF Modules need these.
//
#include <stdlib.h>
#include <sal.h>
#include <windows.h>
#include <stdio.h>
#include <wdf.h>
#include <Objbase.h>
// NOTE: This file includes poclass.h. Do not include that again
//       otherwise, redefinition errors will occur.
//
#include <batclass.h>
#include <hidclass.h>
#include <powrprof.h>
#include <usbiodef.h>
#include <cguid.h>
#include <guiddef.h>
#include <wdmguid.h>
#include <cfgmgr32.h>
#include <ndisguid.h>
#include <strsafe.h>
#include <ndisguid.h>

// Turn this on to debug asserts in UMDF.
// Normal assert() causes a crash in UMDF which causes UMDF to just disable the driver
// without showing what assert failed.
//
#if defined(DEBUG)
    #if defined(USE_ASSERT_BREAK)
        // It means, check a condition...If it is false, break into debugger.
        //
        #pragma warning(disable:4127)
        #if defined(ASSERT)
            #undef ASSERT
        #endif // defined(ASSERT)
        #define ASSERT(X)   ((! (X)) ? DebugBreak() : TRUE)
    #else
        #if !defined(ASSERT)
            // It means, use native assert().
            //
            #include <assert.h>
            #define ASSERT(X)   assert(X)
        #endif // !defined(ASSERT)
    #endif // defined(USE_ASSERT_BREAK)
#else
    #if !defined(ASSERT)
        // It means, do not assert at all.
        //
        #define ASSERT(X) TRUE
    #endif // !defined(ASSERT)
#endif // defined(DEBUG)

// DMF Asserts definitions 
//
#if DBG
    #if defined(NO_USE_ASSERT_BREAK)
        #include <assert.h>
        #define DmfAssertMessage(Message, Expression) (!(Expression) ? assert(Expression), FALSE : TRUE)
    #else
        #define DmfAssertMessage(Message, Expression) (!(Expression) ? DbgBreakPoint(), OutputDebugStringA(Message), FALSE : TRUE)
    #endif
#else
    #define DmfAssertMessage(Message, Expression) TRUE        
#endif
#define DmfVerifierAssert(Message, Expression)                          \
    if ((WdfDriverGlobals->DriverFlags & WdfVerifyOn) && !(Expression)) \
    {                                                                   \
        OutputDebugStringA(Message);                                    \
        DbgBreakPoint();                                                \
    }

#define DmfAssert(Expression) DmfAssertMessage(#Expression, Expression)

// eof: DmfIncludes_USER_MODE.h
//
