 /*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ThreadedBufferQueue.c

Abstract:

    Implements a System ThreadedBufferQueue and provides support to manipulate the thread.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_ThreadedBufferQueue.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Optional Status.
    //
    NTSTATUS* NtStatus;
    // Optional event if caller wants to wait.
    //
    DMF_PORTABLE_EVENT* Event;
} ThreadedBufferQueue_WorkBufferInternal;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // BufferQueue that holds empty buffers and pending work.
    //
    DMFMODULE DmfModuleBufferQueue;
    // Thread that reads BufferQueue to get work and return buffers.
    //
    DMFMODULE DmfModuleThread;
} DMF_CONTEXT_ThreadedBufferQueue;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(ThreadedBufferQueue)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ThreadedBufferQueue)

// Memory Pool Tag.
//
#define MemoryTag 'MQBT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

__forceinline
ThreadedBufferQueue_WorkBufferInternal*
ThreadedBufferQueueBuffer_ClientToInternal(
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Given a Client buffer, get the corresponding internal buffer.

Arguments:

    ClientBuffer - The given Client buffer.

Return Value:

    The corresponding internal buffer.

--*/
{
    ThreadedBufferQueue_WorkBufferInternal* workBuffer;

    workBuffer = (ThreadedBufferQueue_WorkBufferInternal*)(((UCHAR*)(ClientBuffer)) - sizeof(ThreadedBufferQueue_WorkBufferInternal));

    return workBuffer;
}

__forceinline
VOID*
ThreadedBufferQueueBuffer_InternalToClient(
    _In_ ThreadedBufferQueue_WorkBufferInternal* ThreadedBufferQueueBufferInternal
    )
/*++

Routine Description:

    Given an internal buffer, get the corresponding client buffer.

Arguments:

    ThreadedBufferQueueBufferInternal - The given Internal buffer.

Return Value:

    The corresponding client buffer.

--*/
{
    return ThreadedBufferQueueBufferInternal + 1;
}

VOID
ThreadedBufferQueue_WorkCompleted(
    _In_ DMFMODULE DmfModule,
    _In_ ThreadedBufferQueue_WorkBufferInternal* ThreadedBufferQueueBufferInternal,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    Complete work for a previously pended work buffer.

Arguments:

    DmfModule - This Module's handle.
    ThreadedBufferQueueBufferInternal - Internal buffer that contains the work that was pended.
    NtStatus - Status indicating result of work.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Write back to calling thread before setting calling thread event.
    //
    if (ThreadedBufferQueueBufferInternal->NtStatus != NULL)
    {
        *ThreadedBufferQueueBufferInternal->NtStatus = NtStatus;
    }

    // Wake up calling thread.
    //
    if (ThreadedBufferQueueBufferInternal->Event != NULL)
    {
        DMF_Portable_EventSet(ThreadedBufferQueueBufferInternal->Event);
    }

    // Return the buffer back to pool of available buffers.
    //
    DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueue,
                          ThreadedBufferQueueBufferInternal);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
VOID
ThreadedBufferQueueThreadCallback(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    The underlying Thread calls this function when work is available. It dequeues the work buffer
    from the Consumer List and sends the work buffer to the Client. Then, it returns the work
    buffer to the Producer List.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    ThreadedBufferQueue_WorkBufferInternal* workBuffer;
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;
    DMF_CONFIG_ThreadedBufferQueue* moduleConfig;
    VOID* clientWorkBuffer;
    VOID* clientWorkBufferContext;
    DMFMODULE dmfModuleThreadedBufferQueue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModuleThreadedBufferQueue = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleThreadedBufferQueue);
    moduleConfig = DMF_CONFIG_GET(dmfModuleThreadedBufferQueue);

Start:

    // Get a buffer that contains the work the Client wants to do.
    //
    ntStatus = DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueue,
                                       (VOID**)&workBuffer,
                                       &clientWorkBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        // NOTE: Failure is expected and normal. It means there is no more work to do.
        //       This is how the loop exits.
        //
        goto Exit;
    }

    // The Client just gets the Client's buffer, not the meta data used by this Module.
    //
    clientWorkBuffer = ThreadedBufferQueueBuffer_InternalToClient(workBuffer);

    // Allow the Client to do the work based on work buffer contents.
    //
    ThreadedBufferQueue_BufferDisposition bufferDisposition;

    bufferDisposition = moduleConfig->EvtThreadedBufferQueueWork(dmfModuleThreadedBufferQueue,
                                                                 (UCHAR*)clientWorkBuffer,
                                                                 moduleConfig->BufferQueueConfig.SourceSettings.BufferSize,
                                                                 (VOID*)clientWorkBufferContext,
                                                                 &ntStatus);
    if (ThreadedBufferQueue_BufferDisposition_WorkComplete == bufferDisposition)
    {
        // Client no longer owns buffer.
        //
        ThreadedBufferQueue_WorkCompleted(dmfModuleThreadedBufferQueue,
                                          workBuffer,
                                          ntStatus);
    }
    else if (ThreadedBufferQueue_BufferDisposition_WorkPending == bufferDisposition)
    {
        // Client owns buffer and must return it using DMF_ThreadedBufferQueue_WorkCompleted().
        // Do not retrieve the next buffer. 
        // (If Client wants to retrieve next buffer, Client should set this Module's work ready event.)
        //
        goto Exit;
    }
    else
    {
        ASSERT(FALSE);
    }

    goto Start;

Exit:
    ;

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

VOID
ThreadedBufferQueue_WorkReady(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Sets the work ready event.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Thread_WorkReady(moduleContext->DmfModuleThread);

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ThreadedBufferQueue_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_ThreadedBufferQueue* moduleConfig;
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;
    DMF_CONFIG_Thread moduleConfigThread;
    DMF_CONFIG_BufferQueue moduleBufferQueueConfigList;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // DmfModuleBufferQueue
    // --------------------
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&moduleBufferQueueConfigList,
                                               &moduleAttributes);
    moduleBufferQueueConfigList = moduleConfig->BufferQueueConfig;
    moduleBufferQueueConfigList.SourceSettings.BufferSize = sizeof(ThreadedBufferQueue_WorkBufferInternal) +
                                                            moduleBufferQueueConfigList.SourceSettings.BufferSize;
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferQueue);

    // DmfModuleThread
    // ---------------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadPre = moduleConfig->EvtThreadedBufferQueuePre;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = ThreadedBufferQueueThreadCallback;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadPost = moduleConfig->EvtThreadedBufferQueuePost;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThread);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_ThreadedBufferQueue_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type ThreadedBufferQueue.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // In case, Client has not explicitly stopped the thread, do that now.
    //
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Thread_Stop(moduleContext->DmfModuleThread);

    FuncExitNoReturn(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ThreadedBufferQueue_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type ThreadedBufferQueue.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_ThreadedBufferQueue;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_ThreadedBufferQueue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_ThreadedBufferQueue);
    dmfCallbacksDmf_ThreadedBufferQueue.ChildModulesAdd = DMF_ThreadedBufferQueue_ChildModulesAdd;
    dmfCallbacksDmf_ThreadedBufferQueue.DeviceClose = DMF_ThreadedBufferQueue_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_ThreadedBufferQueue,
                                            ThreadedBufferQueue,
                                            DMF_CONTEXT_ThreadedBufferQueue,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_ThreadedBufferQueue.CallbacksDmf = &dmfCallbacksDmf_ThreadedBufferQueue;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_ThreadedBufferQueue,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DMF_ThreadedBufferQueue_Count(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Return the number of entries currently in the list.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;
    ULONG numberOfEntriesInList;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ThreadedBufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    numberOfEntriesInList = DMF_BufferQueue_Count(moduleContext->DmfModuleBufferQueue);

    FuncExit(DMF_TRACE, "numberOfEntriesInList=%d", numberOfEntriesInList);

    return numberOfEntriesInList;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_Enqueue(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Adds a Client Buffer to the list and sets the work ready event.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;
    ThreadedBufferQueue_WorkBufferInternal* workBuffer;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ThreadedBufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    workBuffer = ThreadedBufferQueueBuffer_ClientToInternal(ClientBuffer);

    workBuffer->Event = NULL;
    workBuffer->NtStatus = NULL;

    DMF_BufferQueue_Enqueue(moduleContext->DmfModuleBufferQueue,
                            workBuffer);

    ThreadedBufferQueue_WorkReady(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ThreadedBufferQueue_EnqueueAndWait(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Adds a Client Buffer to the list and sets the work ready event. Then,
    waits for the work to be completed and returns the NTSTATUS of that 
    deferred work.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;
    ThreadedBufferQueue_WorkBufferInternal* workBuffer;
    NTSTATUS ntStatus;
    DMF_PORTABLE_EVENT event;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ThreadedBufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Portable_EventCreate(&event,
                             NotificationEvent,
                             FALSE);

    workBuffer = ThreadedBufferQueueBuffer_ClientToInternal(ClientBuffer);

    workBuffer->Event = &event;
    workBuffer->NtStatus = &ntStatus;

    DMF_BufferQueue_Enqueue(moduleContext->DmfModuleBufferQueue,
                            workBuffer);

    ThreadedBufferQueue_WorkReady(DmfModule);

    // Wait for the work to execute.
    //
    DMF_Portable_EventWaitForSingleObject(&event,
                                          FALSE,
                                          NULL);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ThreadedBufferQueue_Fetch(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Removes the next buffer in the list (head of the list) if there is a buffer.
    Then, returns the Client Buffer and its associated Client Buffer Context.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    STATUS_SUCCESS if a buffer is removed from the list.
    STATUS_UNSUCCESSFUL if the list is empty.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;
    ThreadedBufferQueue_WorkBufferInternal* workBuffer;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ThreadedBufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    workBuffer = NULL;

    ntStatus = DMF_BufferQueue_Fetch(moduleContext->DmfModuleBufferQueue,
                                     (VOID**)&workBuffer,
                                     ClientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    *ClientBuffer = ThreadedBufferQueueBuffer_InternalToClient(workBuffer);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_Flush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Removes all pending entries in the pending work queue.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;
    NTSTATUS ntStatus;
    VOID* bufferContext;
    ThreadedBufferQueue_WorkBufferInternal* workBuffer;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ThreadedBufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Get pending work buffers from Consumer List, set optional status and events, and return 
    // buffers to the Producer List.
    //
    ntStatus = STATUS_SUCCESS;
    while (NT_SUCCESS(ntStatus))
    {
        ntStatus = DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueue,
                                           (VOID**)&workBuffer,
                                           &bufferContext);
        if (NT_SUCCESS(ntStatus))
        {
            // Return to free queue and tell caller no work was done.
            //
            ThreadedBufferQueue_WorkCompleted(DmfModule,
                                              workBuffer,
                                              STATUS_CANCELLED);
        }
    }

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ThreadedBufferQueue_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts the given Module's thread.

Arguments:

    DmfModule - The given Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ThreadedBufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ThreadedBufferQueue_Stop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops the given Module's thread.

Arguments:

    DmfModule - The given Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ThreadedBufferQueue* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ThreadedBufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Thread_Stop(moduleContext->DmfModuleThread);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_WorkCompleted(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    Allows the Client to complete work for a previously pended work buffer.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - Buffer that contains the work that was pended.
    NtStatus - Status indicating result of work.

Return Value:

    None

--*/
{
    ThreadedBufferQueue_WorkBufferInternal* workBuffer;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ThreadedBufferQueue);

    workBuffer = ThreadedBufferQueueBuffer_ClientToInternal(ClientBuffer);

    ThreadedBufferQueue_WorkCompleted(DmfModule,
                                      workBuffer,
                                      NtStatus);

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_ThreadedBufferQueue.c
//
