/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModules.Template.Public.h

Abstract:

    Public definitions specific for the "Template" DMF Library.

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

// Include all Public definitions in "Library" DMF Library.
//
#include "..\Modules.Library\DmfModules.Library.Public.h"

#include "Dmf_ToasterBus_Public.h"
#include "Dmf_OsrFx2_Public.h"
#include "Dmf_NonPnp_Public.h"

// NOTE: The definitions in this file must be surrounded by this annotation to ensure
//       that both C and C++ Clients can easily compile and link with Modules in this Library.
//
#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfModules.Template.Public.h
//
