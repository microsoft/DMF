/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModules.Library.Public.h

Abstract:

    Public definitions specific for the "Library" DMF Library. 

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// NOTE: The definitions in this file must be surrounded by this annotation to ensure
//       that both C and C++ Clients can easily compile and link with Modules in this Library.
//
#if defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus)

// Include all Public definitions in "Core" DMF Library.
//
#include "..\Framework\Modules.Core\DmfModules.Core.Public.h"

#include "Dmf_CrashDump_Public.h"

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfModules.Library.Public.h
//
