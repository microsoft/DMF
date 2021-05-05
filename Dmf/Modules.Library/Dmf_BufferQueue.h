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

// This macro declares the following functions:
// DMF_BufferQueue_ATTRIBUTES_INIT()
// DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT()
// DMF_BufferQueue_Create()
//
DECLARE_DMF_MODULE(BufferQueue)

// Module Methods
//

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
