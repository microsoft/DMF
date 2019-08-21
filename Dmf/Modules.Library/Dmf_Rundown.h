/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Rundown.h

Abstract:

    Companion file to Dmf_Rundown.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// This macro declares the following functions:
// DMF_Registry_ATTRIBUTES_INIT()
// DMF_Registry_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(Rundown)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Rundown_Dereference(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Rundown_EndAndWait(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Rundown_Reference(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Rundown_Start(
    _In_ DMFMODULE DmfModule
    );

// eof: Dmf_Rundown.h
//
