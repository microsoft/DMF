/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ThreadedBufferQueue.h

Abstract:

    Companion file to Dmf_ThreadedBufferQueue.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Enum to specify what the Client wants to do with the retrieved work buffer.
//
typedef enum
{
    ThreadedBufferQueue_BufferDisposition_Invalid,
    // Client no longer owns buffer and it is returned to Producer list.
    //
    ThreadedBufferQueue_BufferDisposition_WorkComplete,
    // Client retains ownership of the buffer and will complete it later.
    //
    ThreadedBufferQueue_BufferDisposition_WorkPending,
    ThreadedBufferQueue_BufferDisposition_Maximum
} ThreadedBufferQueue_BufferDisposition;

// Client Driver callback function.
//
typedef
_Function_class_(EVT_DMF_ThreadedBufferQueue_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ThreadedBufferQueue_BufferDisposition
EVT_DMF_ThreadedBufferQueue_Callback(_In_ DMFMODULE DmfModule,
                                     _In_ UCHAR* ClientWorkBuffer,
                                     _In_ ULONG ClientWorkBufferSize,
                                     _In_ VOID* ClientWorkBufferContext,
                                     _Out_ NTSTATUS* NtStatus);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Contains empty buffers in Producer and work buffers in the Consumer.
    //
    DMF_CONFIG_BufferQueue BufferQueueConfig;
    // Optional callback that does work before looping.
    //
    EVT_DMF_Thread_Function* EvtThreadedBufferQueuePre;
    // Mandatory callback that does work when work is ready.
    //
    EVT_DMF_ThreadedBufferQueue_Callback* EvtThreadedBufferQueueWork;
    // Optional callback that does work after looping but before thread ends.
    //
    EVT_DMF_Thread_Function* EvtThreadedBufferQueuePost;
} DMF_CONFIG_ThreadedBufferQueue;

// This macro declares the following functions:
// DMF_ThreadedBufferQueue_ATTRIBUTES_INIT()
// DMF_CONFIG_ThreadedBufferQueue_AND_ATTRIBUTES_INIT()
// DMF_ThreadedBufferQueue_Create()
//
DECLARE_DMF_MODULE(ThreadedBufferQueue)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DMF_ThreadedBufferQueue_Count(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_Enqueue(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ThreadedBufferQueue_EnqueueAndWait(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ThreadedBufferQueue_Fetch(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_Flush(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_Reuse(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ThreadedBufferQueue_Start(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ThreadedBufferQueue_Stop(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_WorkCompleted(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ NTSTATUS NtStatus
    );

// eof: Dmf_ThreadedBufferQueue.h
//
