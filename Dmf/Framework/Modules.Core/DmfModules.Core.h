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
#include "..\DmfDefinitions.h"
#include "DmfModules.Core.Public.h"

// Interfaces in this Library.
// (None at this time.)
//

// These are Modules used by the Core itself. However, they reside with all other non-Feature
// Modules in Modules.Library.
//
#include "..\Modules.Library\Dmf_BufferPool.h"
#include "..\Modules.Library\Dmf_HashTable.h"
#include "..\Modules.Library\Dmf_RingBuffer.h"
#include "..\Modules.Library\Dmf_BufferQueue.h"
#include "..\Modules.Library\Dmf_IoctlHandler.h"
#include "..\Modules.Library\Dmf_String.h"
#include "..\Modules.Library\Dmf_CrashDump.h"

// Feature Modules.
//
#include "Dmf_BranchTrack.h"
#include "Dmf_Bridge.h"
#if defined(DMF_WDF_DRIVER)
#include "Dmf_LiveKernelDump.h"
#endif

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfModules.Core.h
//
