/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AlertableSleep.h

Abstract:

    Companion file to Dmf_AlertableSleep.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#define ALERTABLE_SLEEP_MAXIMUM_TIMERS  32

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Several events are created and each works independently. The code protects
    // against a caller sleeping using the same event twice.
    //
    ULONG EventCount;
} DMF_CONFIG_AlertableSleep;

// This macro declares the following functions:
// DMF_AlertableSleep_ATTRIBUTES_INIT()
// DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT()
// DMF_AlertableSleep_Create()
//
DECLARE_DMF_MODULE(AlertableSleep)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_AlertableSleep_Abort(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_AlertableSleep_ResetForReuse(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AlertableSleep_Sleep(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex,
    _In_ ULONG Milliseconds
    );

// eof: Dmf_AlertableSleep.h
//
