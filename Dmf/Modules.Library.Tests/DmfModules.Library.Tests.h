/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModules.Library.Tests.h

Abstract:

    Definitions specific for the "Library.Tests" DMF Library.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Include DMF Framework and Public Modules.
//
#include "..\Modules.Library\DmfModules.Library.h"
#include "DmfModules.Library.Tests.Public.h"

// NOTE: The definitions in this file must be surrounded by this annotation to ensure
//       that both C and C++ Clients can easily compile and link with Modules in this Library.
//
#if defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus)

// Other library specific includes.
//

#include "TestsUtility.h"

// All the Modules in this Library.
//

#include "Dmf_Tests_BufferPool.h"
#include "Dmf_Tests_BufferQueue.h"
#include "Dmf_Tests_RingBuffer.h"
#include "Dmf_Tests_Registry.h"
#include "Dmf_Tests_PingPongBuffer.h"
#include "Dmf_Tests_ScheduledTask.h"
#include "Dmf_Tests_HashTable.h"
#include "Dmf_Tests_IoctlHandler.h"
#include "Dmf_Tests_SelfTarget.h"
#include "Dmf_Tests_DeviceInterfaceTarget.h"
#include "Dmf_Tests_DefaultTarget.h"
#include "Dmf_Tests_Pdo.h"
#include "Dmf_Tests_String.h"
#include "Dmf_Tests_AlertableSleep.h"

// NOTE: The definitions in this file must be surrounded by this annotation to ensure
//       that both C and C++ Clients can easily compile and link with Modules in this Library.
//
#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfModules.Library.Tests.h
//
