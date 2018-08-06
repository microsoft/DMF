/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_GpioTarget.h

Abstract:

    Companion file to Dmf_GpioTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef enum
{
    // Sentinel for validity checking.
    //
    GpioTarget_QueuedWorkItem_Invalid,
    // ISR/DPC has no additional work to do.
    //
    GpioTarget_QueuedWorkItem_Nothing,
    // ISR has more work to do at DISPATCH_LEVEL.
    //
    GpioTarget_QueuedWorkItem_Dpc,
    // DPC has more work to do at PASSIVE_LEVEL.
    //
    GpioTarget_QueuedWorkItem_WorkItem
} GpioTarget_QueuedWorkItem_Type;

// Client Driver DIRQL_LEVEL Callback.
//
typedef
_Function_class_(EVT_DMF_GpioTarget_InterruptIsr)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_GpioTarget_InterruptIsr(_In_ DMFMODULE DmfModule,
                                _In_ ULONG MessageId,
                                _Out_ GpioTarget_QueuedWorkItem_Type* QueuedWorkItem);

// Client Driver DPC_LEVEL Callback.
//
typedef
_Function_class_(EVT_DMF_GpioTarget_InterruptDpc)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_GpioTarget_InterruptDpc(_In_ DMFMODULE DmfModule,
                                _Out_ GpioTarget_QueuedWorkItem_Type* QueuedWorkItem);

// Client Driver PASSIVE_LEVEL Callback.
//
typedef
_Function_class_(EVT_DMF_GpioTarget_InterruptPassive)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_GpioTarget_InterruptPassive(_In_ DMFMODULE DmfModule);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Module will not load if Gpio Connection not found.
    //
    BOOLEAN GpioConnectionMandatory;
    // Module will not load if Interrupt not found.
    //
    BOOLEAN InterruptMandatory;
    // GPIO Connection index for this instance.
    //
    ULONG GpioConnectionIndex;
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
    EVT_DMF_GpioTarget_InterruptIsr* EvtGpioTargetInterruptIsr;
    // Optional Callback at DPC_LEVEL Level.
    //
    EVT_DMF_GpioTarget_InterruptDpc* EvtGpioTargetInterruptDpc;
    // Optional Callback at PASSIVE_LEVEL Level.
    //
    EVT_DMF_GpioTarget_InterruptPassive* EvtGpioTargetInterruptPassive;
} DMF_CONFIG_GpioTarget;

// This macro declares the following functions:
// DMF_GpioTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_GpioTarget_AND_ATTRIBUTES_INIT()
// DMF_GpioTarget_Create()
//
DECLARE_DMF_MODULE(GpioTarget)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_GpioTarget_InterruptAcquireLock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_GpioTarget_InterruptReleaseLock(
    _In_ DMFMODULE DmfModule
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_GpioTarget_InterruptTryToAcquireLock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_GpioTarget_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* GpioConnectionAssigned,
    _Out_opt_ BOOLEAN* InterruptAssigned
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_GpioTarget_Read(
    _In_ DMFMODULE DmfModule,
    _Out_ BOOLEAN* PinValue
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_GpioTarget_Write(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Value
    );

// eof: Dmf_GpioTarget.h
//
