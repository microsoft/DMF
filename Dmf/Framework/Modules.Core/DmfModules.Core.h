/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModules.Core.h

Abstract:

    Definitions specific for the "Core" DMF Library.

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

// Include DMF Framework.
//
#include "..\DmfModule.h"
#include "DmfModules.Core.Public.h"

// All the Modules in this Library.
//
#include "Dmf_BufferPool.h"
#include "Dmf_HashTable.h"
#include "Dmf_RingBuffer.h"
#include "Dmf_BranchTrack.h"
#include "Dmf_Bridge.h"
#include "Dmf_LiveKernelDump.h"
#include "Dmf_BufferQueue.h"
#include "Dmf_IoctlHandler.h"

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfModules.Core.h
//
