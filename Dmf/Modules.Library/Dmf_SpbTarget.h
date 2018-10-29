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

typedef enum
{
    // Sentinel for validity checking.
    //
    SpbTarget_QueuedWorkItem_Invalid,
    // ISR/DPC has no additional work to do.
    //
    SpbTarget_QueuedWorkItem_Nothing,
    // ISR has more work to do at DISPATCH_LEVEL.
    //
    SpbTarget_QueuedWorkItem_Dpc,
    // DPC has more work to do at PASSIVE_LEVEL.
    //
    SpbTarget_QueuedWorkItem_WorkItem
} SpbTarget_QueuedWorkItem_Type;

// Client Driver DIRQL_LEVEL Callback.
//
typedef
_Function_class_(EVT_DMF_SpbTarget_InterruptIsr)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_SpbTarget_InterruptIsr(_In_ DMFMODULE DmfModule,
                               _In_ ULONG MessageId,
                               _Out_ SpbTarget_QueuedWorkItem_Type* QueuedWorkItem);

// Client Driver DPC_LEVEL Callback.
//
typedef
_Function_class_(EVT_DMF_SpbTarget_InterruptDpc)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_SpbTarget_InterruptDpc(_In_ DMFMODULE DmfModule,
                               _Out_ SpbTarget_QueuedWorkItem_Type* QueuedWorkItem);

// Client Driver PASSIVE_LEVEL Callback.
//
typedef
_Function_class_(EVT_DMF_SpbTarget_InterruptPassive)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_SpbTarget_InterruptPassive(_In_ DMFMODULE DmfModule);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Module will not load if Spb Connection not found.
    //
    BOOLEAN SpbConnectionMandatory;
    // Module will not load if Interrupt not found.
    //
    BOOLEAN InterruptMandatory;
    // GPIO Connection index for this instance.
    //
    ULONG SpbConnectionIndex;
    // Interrupt index for this instance.
    //
    ULONG InterruptIndex;
    // Open in Read or Write mode.
    //
    ULONG OpenMode;
    // Share Access.
    //
    ULONG ShareAccess;
    // Passive handling.
    //
    BOOLEAN PassiveHandling;
    // Can GPIO wake the device.
    //
    BOOLEAN CanWakeDevice;
    // Optional Callback from ISR (with Interrupt Spin Lock held).
    //
    EVT_DMF_SpbTarget_InterruptIsr* EvtSpbTargetInterruptIsr;
    // Optional Callback at DPC_LEVEL Level.
    //
    EVT_DMF_SpbTarget_InterruptDpc* EvtSpbTargetInterruptDpc;
    // Optional Callback at PASSIVE_LEVEL Level.
    //
    EVT_DMF_SpbTarget_InterruptPassive* EvtSpbTargetInterruptPassive;
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
