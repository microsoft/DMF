/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_InterruptResource.h

Abstract:

    Companion file to Dmf_InterruptResource.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef enum
{
    // Sentinel for validity checking.
    //
    InterruptResource_QueuedWorkItem_Invalid,
    // ISR/DPC has no additional work to do.
    //
    InterruptResource_QueuedWorkItem_Nothing,
    // ISR has more work to do at DISPATCH_LEVEL.
    //
    InterruptResource_QueuedWorkItem_Dpc,
    // DPC has more work to do at PASSIVE_LEVEL.
    //
    InterruptResource_QueuedWorkItem_WorkItem
} InterruptResource_QueuedWorkItem_Type;

// Client Driver DIRQL_LEVEL Callback.
//
typedef
_Function_class_(EVT_DMF_InterruptResource_InterruptIsr)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_InterruptResource_InterruptIsr(_In_ DMFMODULE DmfModule,
                                       _In_ ULONG MessageId,
                                       _Out_ InterruptResource_QueuedWorkItem_Type* QueuedWorkItem);

// Client Driver DPC_LEVEL Callback.
//
typedef
_Function_class_(EVT_DMF_InterruptResource_InterruptDpc)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_InterruptResource_InterruptDpc(_In_ DMFMODULE DmfModule,
                                       _Out_ InterruptResource_QueuedWorkItem_Type* QueuedWorkItem);

// Client Driver PASSIVE_LEVEL Callback.
//
typedef
_Function_class_(EVT_DMF_InterruptResource_InterruptPassive)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_InterruptResource_InterruptPassive(_In_ DMFMODULE DmfModule);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Module will not load if Interrupt not found.
    //
    BOOLEAN InterruptMandatory;
    // Interrupt index for this instance.
    //
    ULONG InterruptIndex;
    // Passive handling.
    //
    BOOLEAN PassiveHandling;
    // Can the interrupt resource wake the device.
    //
    BOOLEAN CanWakeDevice;
    // Optional Callback from ISR (with Interrupt Spin Lock held).
    //
    EVT_DMF_InterruptResource_InterruptIsr* EvtInterruptResourceInterruptIsr;
    // Optional Callback at DPC_LEVEL Level.
    //
    EVT_DMF_InterruptResource_InterruptDpc* EvtInterruptResourceInterruptDpc;
    // Optional Callback at PASSIVE_LEVEL Level.
    //
    EVT_DMF_InterruptResource_InterruptPassive* EvtInterruptResourceInterruptPassive;
} DMF_CONFIG_InterruptResource;

// This macro declares the following functions:
// DMF_InterruptResource_ATTRIBUTES_INIT()
// DMF_CONFIG_InterruptResource_AND_ATTRIBUTES_INIT()
// DMF_InterruptResource_Create()
//
DECLARE_DMF_MODULE(InterruptResource)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_InterruptResource_InterruptAcquireLock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_InterruptResource_InterruptReleaseLock(
    _In_ DMFMODULE DmfModule
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_InterruptResource_InterruptTryToAcquireLock(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_InterruptResource_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* InterruptAssigned
    );

// eof: Dmf_InterruptResource.h
//
