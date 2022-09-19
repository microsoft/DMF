/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Time.h

Abstract:

    Companion file to Dmf_Time.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// This macro declares the following functions:
// DMF_Time_ATTRIBUTES_INIT()
// DMF_Time_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(Time)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Time_ElapsedTimeMillisecondsGet(
    _In_ DMFMODULE DmfModule,
    _In_ LONGLONG StartTime,
    _Out_ LONGLONG* ElapsedTimeInMilliSeconds
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Time_ElapsedTimeNanosecondsGet(
    _In_ DMFMODULE DmfModule,
    _In_ LONGLONG StartTick,
    _Out_ LONGLONG* ElapsedTimeInNanoSeconds
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Time_LocalTimeGet(
    _In_ DMFMODULE DmfModule,
    _Out_ DMF_TIME_FIELDS* DmfTimeFields
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Time_SystemTimeGet(
    _In_ DMFMODULE DmfModule,
    _Out_ LARGE_INTEGER* CurrentSystemTime
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
LONGLONG
DMF_Time_TickCountGet(
    _In_ DMFMODULE DmfModule
    );

// eof: Dmf_Time.h
//
