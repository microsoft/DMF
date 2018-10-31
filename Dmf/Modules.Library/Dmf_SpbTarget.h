/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SpbTarget.h

Abstract:

    Companion file to Dmf_SpbTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// These definitions are so that names match from Client's point of view.
// (This is best practice for chained callbacks in Config structures.)
//
typedef EVT_DMF_InterruptResource_InterruptIsr EVT_DMF_SpbTarget_InterruptIsr;
typedef EVT_DMF_InterruptResource_InterruptDpc EVT_DMF_SpbTarget_InterruptDpc;
typedef EVT_DMF_InterruptResource_InterruptPassive EVT_DMF_SpbTarget_InterruptPassive;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Module will not load if Spb Connection not found.
    //
    BOOLEAN SpbConnectionMandatory;
    // GPIO Connection index for this instance.
    //
    ULONG SpbConnectionIndex;
    // Open in Read or Write mode.
    //
    ULONG OpenMode;
    // Share Access.
    //
    ULONG ShareAccess;
    // Interrupt Resource
    //
    DMF_CONFIG_InterruptResource InterruptResource;
} DMF_CONFIG_SpbTarget;

// This macro declares the following functions:
// DMF_SpbTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_SpbTarget_AND_ATTRIBUTES_INIT()
// DMF_SpbTarget_Create()
//
DECLARE_DMF_MODULE(SpbTarget)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_BufferFullDuplex(
    _In_ DMFMODULE DmfModule,
    _In_reads_(InputBufferLength) UCHAR* InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_(OutputBufferLength) UCHAR* OutputBuffer,
    _In_ ULONG OutputBufferLength
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_BufferWrite(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfBytesToWrite) UCHAR* BufferToWrite,
    _In_ ULONG NumberOfBytesToWrite
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_BufferWriteRead(
    _In_ DMFMODULE DmfModule,
    _In_reads_(InputBufferLength) UCHAR* InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_(OutputBufferLength) UCHAR* OutputBuffer,
    _In_ ULONG OutputBufferLength
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_ConnectionLock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_ConnectionUnlock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_ControllerLock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_ControllerUnlock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SpbTarget_InterruptAcquireLock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SpbTarget_InterruptReleaseLock(
    _In_ DMFMODULE DmfModule
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_SpbTarget_InterruptTryToAcquireLock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_SpbTarget_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* SpbConnectionAssigned,
    _Out_opt_ BOOLEAN* InterruptAssigned
    );

// eof: Dmf_SpbTarget.h
//
