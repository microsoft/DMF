/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_BufferQueue.h

Abstract:

    Companion file to Dmf_BufferQueue.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Callback called by DMF_BufferQueue_Reuse so Client
// can finalize buffers before being sent back to Producer.
//
typedef
_Function_class_(EVT_DMF_BufferQueue_ReuseCleanup)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_BufferQueue_ReuseCleanup(_In_ DMFMODULE DmfModule,
                                 _In_ VOID* ClientBuffer,
                                 _In_ VOID* ClientBufferContext);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // BufferQueue has a source and sink list.
    // Source is configured by Client using these settings. 
    // Sink is configured internally.
    //
    BufferPool_SourceSettings SourceSettings;
    // Optional callback for Client to finalize buffer before reuse.
    //
    EVT_DMF_BufferQueue_ReuseCleanup* EvtBufferQueueReuseCleanup;
} DMF_CONFIG_BufferQueue;

// Callback to set default (non-zero) values in DMF_CONFIG_BufferQueue
// referenced by DECLARE_DMF_MODULE_EX().
// NOTE: This callback is called by DMF not by Clients directly.
//
__forceinline
VOID
DMF_CONFIG_BufferQueue_DEFAULT(
    _Inout_ DMF_CONFIG_BufferQueue* ModuleConfig
    )
{
    DMF_CONFIG_BufferPool moduleConfigBufferPool;
    DMF_MODULE_ATTRIBUTES moduleAttributes;

    // This Module's Config reuses (contains) BufferPool's Config. Thus
    // it is necessary to make sure it is properly initialized using its
    // necessary default values.
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);

    // After initializing BufferPool's Config copy Settings from that 
    // Config to this Module's Config.
    //
    ModuleConfig->SourceSettings = moduleConfigBufferPool.Mode.SourceSettings;
}

// This macro declares the following functions:
// DMF_BufferQueue_ATTRIBUTES_INIT()
// DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT()
// DMF_BufferQueue_Create()
//
// DMF_CONFIG_BufferQueue_DEFAULT() must be declared above.
//
DECLARE_DMF_MODULE_EX(BufferQueue)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_ContextGet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _Out_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DMF_BufferQueue_Count(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_Dequeue(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_DequeueWithMemoryDescriptor(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ PWDF_MEMORY_DESCRIPTOR MemoryDescriptor,
    _Out_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Enqueue(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_EnqueueAtHead(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_BufferQueue_EnqueueWithTimer(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ ULONGLONG TimerExpirationMilliseconds,
    _In_ EVT_DMF_BufferPool_TimerCallback* TimerExpirationCallback,
    _In_opt_ VOID* TimerExpirationCallbackContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_BufferPool_Enumeration EntryEnumerationCallback,
    _In_opt_ VOID* ClientDriverCallbackContext,
    _Out_opt_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_Fetch(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Flush(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Reuse(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

// eof: Dmf_BufferQueue.h
//
