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

// This macro declares the following functions:
// DMF_QueuedWorkItem_ATTRIBUTES_INIT()
// DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT()
// DMF_QueuedWorkItem_Create()
//
DECLARE_DMF_MODULE(QueuedWorkItem)

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

// eof: Dmf_QueuedWorkItem.h
//
