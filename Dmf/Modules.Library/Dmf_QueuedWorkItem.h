/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_QueuedWorkItem.h

Abstract:

    Companion file to Dmf_QueuedWorkItem.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client Driver callback function to execute command when being called.
//
typedef
_Function_class_(EVT_DMF_QueuedWorkItem_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ScheduledTask_Result_Type 
EVT_DMF_QueuedWorkItem_Callback(_In_ DMFMODULE DmfModule,
                                _In_ VOID* ClientBuffer,
                                _In_ VOID* ClientBufferContext);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Deferred call callback function to be implemented by Client.
    //
    EVT_DMF_QueuedWorkItem_Callback* EvtQueuedWorkitemFunction;
    // Producer list holds empty preallocated buffers ready for use. 
    // Consumer list holds buffers that have pending work.
    //
    DMF_CONFIG_BufferQueue BufferQueueConfig;
} DMF_CONFIG_QueuedWorkItem;

// Callback to set default (non-zero) values in DMF_CONFIG_QueuedWorkItem
// referenced by DECLARE_DMF_MODULE_EX().
// NOTE: This callback is called by DMF not by Clients directly.
//
__forceinline
VOID
DMF_CONFIG_QueuedWorkItem_DEFAULT(
    _Inout_ DMF_CONFIG_QueuedWorkItem* ModuleConfig
    )
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;

    // This Module's Config reuses (contains) BufferQueue's Config. Thus
    // it is necessary to make sure it is properly initialized using its
    // necessary default values.
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&ModuleConfig->BufferQueueConfig,
                                               &moduleAttributes);
    // This Module's Config has no specific default non-zero values.
    //
}

// This macro declares the following functions:
// DMF_QueuedWorkItem_ATTRIBUTES_INIT()
// DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT()
// DMF_QueuedWorkItem_Create()
//
// DMF_CONFIG_QueuedWorkItem_DEFAULT() must be declared above.
//
DECLARE_DMF_MODULE_EX(QueuedWorkItem)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_QueuedWorkItem_Enqueue(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(ContextBufferSize) VOID* ContextBuffer,
    _In_ ULONG ContextBufferSize
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_QueuedWorkItem_EnqueueAndWait(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(ContextBufferSize) VOID* ContextBuffer,
    _In_ ULONG ContextBufferSize
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_QueuedWorkItem_Flush(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_QueuedWorkItem_StatusSet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ NTSTATUS NtStatus
    );

// eof: Dmf_QueuedWorkItem.h
//
