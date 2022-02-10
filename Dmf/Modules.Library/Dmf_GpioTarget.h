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

// These definitions are so that names match from Client's point of view.
// (This is best practice for chained callbacks in Config structures.)
//
typedef EVT_DMF_InterruptResource_InterruptIsr EVT_DMF_GpioTarget_InterruptIsr;
typedef EVT_DMF_InterruptResource_InterruptDpc EVT_DMF_GpioTarget_InterruptDpc;
typedef EVT_DMF_InterruptResource_InterruptPassive EVT_DMF_GpioTarget_InterruptPassive;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Module will not load if Gpio Connection not found.
    //
    BOOLEAN GpioConnectionMandatory;
    // GPIO Connection index for this instance.
    //
    ULONG GpioConnectionIndex;
    // Open in Read or Write mode.
    //
    ACCESS_MASK OpenMode;
    // Share Access.
    //
    ULONG ShareAccess;
    // Interrupt Resource.
    //
    DMF_CONFIG_InterruptResource InterruptResource;
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

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
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
_Must_inspect_result_
NTSTATUS
DMF_GpioTarget_Read(
    _In_ DMFMODULE DmfModule,
    _Out_ BOOLEAN* PinValue
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_GpioTarget_Write(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Value
    );

// eof: Dmf_GpioTarget.h
//
