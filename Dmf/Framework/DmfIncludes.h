/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfIncludes.h

Abstract:

    This file chooses the correct include files and creates additional
    definitions for the platform that is being compiled.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework
    Non-native WDF platforms

--*/

// Allow Clients to know see current DMF version at compile time.
//
#include "..\DmfVersion.h"

// Automatically define DMF_USER_MODE if in the UMDF environment.
// (Previously, UMDF drivers had to explicitly set this setting.)
//
#if defined(UMDF_VERSION_MAJOR)
    // Maintain backward compatibility for drivers that already define DMF_USER_MODE.
    //
    #if !defined(DMF_USER_MODE)
        #define DMF_USER_MODE
    #endif
#endif

// NOTE: All non-native WDF platforms require DmfPlatform.h.
//
#if defined(DMF_WIN32_MODE) 

    #include "..\Platform\DmfPlatform.h"

#else

    #define DMF_WDF_DRIVER
    #define DMF_INCLUDE_TMH

    #if defined(DMF_USER_MODE)
        // UMDF driver.
        //
        #include "DmfIncludes_USER_MODE.h"
    #else
        // KMDF driver. (Allow Client Driver to define DMF_KERNEL_MODE for consistency
        // although it was always the default selection.)
        //
        #if !defined(DMF_KERNEL_MODE)
            #define DMF_KERNEL_MODE
        #endif
        #include "DmfIncludes_KERNEL_MODE.h"
     #endif

    #include <hidusage.h>
    #include <hidpi.h>
    // NOTE: This is necessary in order to avoid redefinition errors. It is not clear why
    //       this is the case.
    //
    #define DEVPKEY_H_INCLUDED

#endif

// eof: DmfIncludes.h
//
