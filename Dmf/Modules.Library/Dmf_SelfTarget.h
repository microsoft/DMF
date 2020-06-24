/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SelfTarget.h

Abstract:

    Companion file to Dmf_SelfTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// This macro declares the following functions:
// DMF_SelfTarget_ATTRIBUTES_INIT()
// DMF_SelfTarget_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(SelfTarget)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SelfTarget_Get(
    _In_ DMFMODULE DmfModule,
    _Out_ WDFIOTARGET* IoTarget
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SelfTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SelfTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _Out_opt_ size_t* BytesWritten
    );

// eof: Dmf_SelfTarget.h
//
