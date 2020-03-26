/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModules.Core.Public.h

Abstract:

    Public definitions specific for the "Core" DMF Library.

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

#include "Dmf_BranchTrack_Public.h"
#include "Dmf_LiveKernelDump_Public.h"

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfModules.Core.Public.h
//
