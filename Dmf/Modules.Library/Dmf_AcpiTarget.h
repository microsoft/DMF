/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AcpiTarget.h

Abstract:

    Companion file to Dmf_AcpiTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // The DSM revision required by the Client.
    //
    ULONG DsmRevision;
    // The GUID that identifies the DSMs the Client will invoke.
    //
    GUID Guid;
} DMF_CONFIG_AcpiTarget;

// This macro declares the following functions:
// DMF_AcpiTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_AcpiTarget_AND_ATTRIBUTES_INIT()
// DMF_AcpiTarget_Create()
//
DECLARE_DMF_MODULE(AcpiTarget)

// Module Methods
//

_Must_inspect_result_
__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiTarget_EvaluateMethod(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG MethodName,
    _In_opt_ VOID* InputBuffer,
    __deref_opt_out_bcount_opt(*ReturnBufferSize) VOID* *ReturnBuffer,
    _Out_opt_ ULONG* ReturnBufferSize,
    _In_ ULONG Tag
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiTarget_InvokeDsm(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG FunctionIndex,
    _In_ ULONG FunctionCustomArgument,
    _Out_opt_ VOID* ReturnBuffer,
    _Inout_opt_ ULONG* ReturnBufferSize
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiTarget_InvokeDsmRaw(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG FunctionIndex,
    _In_ ULONG FunctionCustomArgument,
    _Out_writes_opt_(*ReturnBufferSize) VOID** ReturnBuffer,
    _Out_opt_ ULONG* ReturnBufferSize,
    _In_ ULONG Tag
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiTarget_InvokeDsmWithCustomBuffer(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG FunctionIndex,
    _In_reads_opt_(FunctionCustomArgumentsBufferSize) VOID* FunctionCustomArgumentsBuffer,
    _In_ ULONG FunctionCustomArgumentsBufferSize
    );

// eof: Dmf_AcpiTarget.h
//
