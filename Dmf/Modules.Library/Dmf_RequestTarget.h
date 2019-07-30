/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_RequestTarget.h

Abstract:

    Companion file to Dmf_RequestTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client Driver callback function to be called from single request completion routine.
//
typedef
_Function_class_(EVT_DMF_RequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_RequestTarget_SendCompletion(_In_ DMFMODULE DmfModule,
                                     _In_ VOID* ClientRequestContext,
                                     _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
                                     _In_ size_t InputBufferBytesWritten,
                                     _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
                                     _In_ size_t OutputBufferBytesRead,
                                     _In_ NTSTATUS CompletionStatus);

// This macro declares the following functions:
// DMF_RequestTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_RequestTarget_AND_ATTRIBUTES_INIT()
// DMF_RequestTarget_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(RequestTarget)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestTarget_IoTargetClear(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestTarget_IoTargetSet(
    _In_ DMFMODULE DmfModule,
    _In_ WDFIOTARGET IoTarget
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_ ContinuousRequestTarget_CompletionOptions CompletionOption,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _Out_opt_ size_t* BytesWritten
    );

// eof: Dmf_RequestTarget.h
//
