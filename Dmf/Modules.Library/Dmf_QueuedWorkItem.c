/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_QueuedWorkItem.c

Abstract:

    This Module provides support for queued workitems. Queued workitems are guaranteed to execute each
    and every time they are enqueued and each workitem execution can contain an execution specific
    parameter.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_QueuedWorkItem.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    DMF_PORTABLE_EVENT* Event;
    NTSTATUS* NtStatus;
} QUEUEDWORKITEM_WAIT_BLOCK;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // ScheduledTask Module ensures every enqueued workitem executes.
    //
    DMFMODULE DmfModuleScheduledTask;
    // BufferQueue contains parameters for every enqueued workitem.
    //
    DMFMODULE DmfModuleBufferQueue;
} DMF_CONTEXT_QueuedWorkItem;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(QueuedWorkItem)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(QueuedWorkItem)

// Memory Pool Tag.
//
#define MemoryTag 'oMWQ'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

__forceinline
QUEUEDWORKITEM_WAIT_BLOCK*
QueuedWorkItem_WaitBlockFromClientBufferWithMetadata(
    _In_ VOID* ClientBufferWithMetadata
    )
/*++

Routine Description:

    Given a Client buffer with meta data, retrieve the corresponding wait block
    which contains the event and address of NTSTATUS buffer.

Arguments:

    ClientBufferWithMetadata - The given Client buffer with meta data.

Return Value:

    Wait block corresponding to the given Client buffer with meta data.

--*/
{
    QUEUEDWORKITEM_WAIT_BLOCK* queuedWorkItemWaitBlock;

    queuedWorkItemWaitBlock = (QUEUEDWORKITEM_WAIT_BLOCK*)ClientBufferWithMetadata;

    return queuedWorkItemWaitBlock;
}

__forceinline
UCHAR*
QueuedWorkItem_ClientBufferFromClientBufferWithMetadata(
    _In_ VOID* ClientBufferWithMetadata
    )
/*++

Routine Description:

    Given a Client buffer with meta data, retrieve the corresponding Client buffer

Arguments:

    ClientBufferWithMetadata - The given Client buffer with meta data.

Return Value:

    Client buffer corresponding to the given Client buffer with meta data.

--*/
{
    UCHAR* clientBuffer;

    clientBuffer = (((UCHAR*)ClientBufferWithMetadata) + sizeof(QUEUEDWORKITEM_WAIT_BLOCK));

    return clientBuffer;
}

__forceinline
QUEUEDWORKITEM_WAIT_BLOCK*
QueuedWorkItem_WaitBlockFromClientBuffer(
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Given a Client buffer, retrieve the corresponding wait block which contains
    the event and address of NTSTATUS buffer.

Arguments:

    ClientBuffer - The given Client buffer.

Return Value:

    Wait block corresponding to the given Client buffer.

--*/
{
    QUEUEDWORKITEM_WAIT_BLOCK* queuedWorkItemWaitBlock;

    queuedWorkItemWaitBlock = (QUEUEDWORKITEM_WAIT_BLOCK*)(((UCHAR*)ClientBuffer) - sizeof(QUEUEDWORKITEM_WAIT_BLOCK));

    return queuedWorkItemWaitBlock;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
ScheduledTask_Result_Type
QueuedWorkItem_CallbackScheduledTask(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* CallbackContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Executes the next workitem in the work queue.

Arguments:

    DmfModule - This Module's handle.
    CallbackContext - Caller's call specific context that is passed to the caller's callback.
    PreviousState - Unused. 

Return Value:

    ScheduledTask_WorkResult_Fail or
    ScheduledTask_WorkResult_Success

--*/
{
    DMFMODULE dmfModuleQueuedWorkItem;
    DMF_CONTEXT_QueuedWorkItem* moduleContext;
    VOID* clientBufferWithMetadata;
    UCHAR* clientBuffer;
    NTSTATUS ntStatus;
    VOID* clientBufferContext;
    DMF_CONFIG_QueuedWorkItem* queuedWorkItemConfig;
    ScheduledTask_Result_Type scheduledTaskWorkResult;

    UNREFERENCED_PARAMETER(PreviousState);
    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    scheduledTaskWorkResult = ScheduledTask_WorkResult_Fail;
    dmfModuleQueuedWorkItem = (DMFMODULE)CallbackContext;
    moduleContext = DMF_CONTEXT_GET(dmfModuleQueuedWorkItem);

    queuedWorkItemConfig = DMF_CONFIG_GET(dmfModuleQueuedWorkItem);

    // Get the client's buffer that is agnostic to this Module. This buffer has the 
    // parameters for the deferred call.
    //
    ntStatus = DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueue,
                                       (VOID**)&clientBufferWithMetadata,
                                       &clientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    clientBuffer = QueuedWorkItem_ClientBufferFromClientBufferWithMetadata(clientBufferWithMetadata);

    // Call the client's deferred routine.
    //
    scheduledTaskWorkResult = (*queuedWorkItemConfig->EvtQueuedWorkitemFunction)(dmfModuleQueuedWorkItem,
                                                                                 clientBuffer,
                                                                                 clientBufferContext);

    QUEUEDWORKITEM_WAIT_BLOCK* queuedWorkItemWaitBlock = QueuedWorkItem_WaitBlockFromClientBufferWithMetadata(clientBufferWithMetadata);
    if (queuedWorkItemWaitBlock->Event)
    {
        DMF_Portable_EventSet(queuedWorkItemWaitBlock->Event);
    }

    // Add the used client buffer back to empty buffer list.
    //
    DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueue,
                          clientBufferWithMetadata);

Exit:

    FuncExitVoid(DMF_TRACE);

    return scheduledTaskWorkResult;;
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
DMF_QueuedWorkItem_ChildModulesAdd(
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
    DMF_CONFIG_QueuedWorkItem* moduleConfig;
    DMF_CONTEXT_QueuedWorkItem* moduleContext;
    DMF_CONFIG_ScheduledTask scheduledTaskConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // ScheduledTask
    // -------------
    //
    DMF_CONFIG_ScheduledTask_AND_ATTRIBUTES_INIT(&scheduledTaskConfig,
                                                 &moduleAttributes);
    scheduledTaskConfig.EvtScheduledTaskCallback = QueuedWorkItem_CallbackScheduledTask;
    scheduledTaskConfig.CallbackContext = DmfModule;
    scheduledTaskConfig.ExecuteWhen = ScheduledTask_ExecuteWhen_Other;
    scheduledTaskConfig.ExecutionMode = ScheduledTask_ExecutionMode_Deferred;
    scheduledTaskConfig.PersistenceType = ScheduledTask_Persistence_NotPersistentAcrossReboots;
    scheduledTaskConfig.TimerPeriodMsOnFail = 0;
    scheduledTaskConfig.TimerPeriodMsOnSuccess = 0;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleScheduledTask);

    // BufferQueue
    // -----------
    //
    DMF_BufferQueue_ATTRIBUTES_INIT(&moduleAttributes);
    moduleAttributes.ModuleConfigPointer = &moduleConfig->BufferQueueConfig;
    moduleAttributes.SizeOfModuleSpecificConfig = sizeof(moduleConfig->BufferQueueConfig);
    moduleConfig->BufferQueueConfig.SourceSettings.BufferSize += sizeof(QUEUEDWORKITEM_WAIT_BLOCK);
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferQueue);

    FuncExitVoid(DMF_TRACE);
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
DMF_QueuedWorkItem_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type QueuedWorkItem.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_QueuedWorkItem;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_QueuedWorkItem;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_QueuedWorkItem);
    dmfCallbacksDmf_QueuedWorkItem.ChildModulesAdd = DMF_QueuedWorkItem_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_QueuedWorkItem,
                                            QueuedWorkItem,
                                            DMF_CONTEXT_QueuedWorkItem,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_QueuedWorkItem.CallbacksDmf = &dmfCallbacksDmf_QueuedWorkItem;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_QueuedWorkItem,
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
_Must_inspect_result_
NTSTATUS
DMF_QueuedWorkItem_Enqueue(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(ContextBufferSize) VOID* ContextBuffer,
    _In_ ULONG ContextBufferSize
    )
/*++

Routine Description:

    Enqueues a deferred call that will execute in a different thread soon.

Arguments:

    DmfModule - This Module's handle.
    ContextBuffer - Contains the parameters the caller wants to send to the deferred
                       call that does work.
    ContextBufferSize - Size of ContextBuffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_QueuedWorkItem* moduleContext;
    DMF_CONFIG_QueuedWorkItem* moduleConfig;
    UCHAR* clientBufferWithMetadata;
    UCHAR* clientBuffer;
    VOID* clientBufferContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 QueuedWorkItem);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Get an empty buffer to place parameters for this call.
    //
    ntStatus = DMF_BufferQueue_Fetch(moduleContext->DmfModuleBufferQueue,
                                     (VOID**)&clientBufferWithMetadata,
                                     &clientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Get the location where Client buffer will be copied.
    //
    clientBuffer = QueuedWorkItem_ClientBufferFromClientBufferWithMetadata(clientBufferWithMetadata);

    // This call is asynchronous. Clear the event.
    //
    QUEUEDWORKITEM_WAIT_BLOCK* queuedWorkItemWaitBlock = QueuedWorkItem_WaitBlockFromClientBufferWithMetadata(clientBufferWithMetadata);
    RtlZeroMemory(queuedWorkItemWaitBlock,
                  sizeof(QUEUEDWORKITEM_WAIT_BLOCK));

    // Copy over the parameters.
    //
    ASSERT(ContextBufferSize <= moduleConfig->BufferQueueConfig.SourceSettings.BufferSize - sizeof(QUEUEDWORKITEM_WAIT_BLOCK));
    RtlCopyMemory(clientBuffer,
                  ContextBuffer,
                  ContextBufferSize);

    // Add to pending work list.
    //
    DMF_BufferQueue_Enqueue(moduleContext->DmfModuleBufferQueue,
                            clientBufferWithMetadata);

    // Execute deferred call.
    //
    DMF_ScheduledTask_ExecuteNowDeferred(moduleContext->DmfModuleScheduledTask,
                                         DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_QueuedWorkItem_EnqueueAndWait(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(ContextBufferSize) VOID* ContextBuffer,
    _In_ ULONG ContextBufferSize
    )
/*++

Routine Description:

    Enqueues a deferred call that will execute in a different thread soon.

Arguments:

    DmfModule - This Module's handle.
    ContextBuffer - Contains the parameters the caller wants to send to the deferred
                       call that does work.
    ContextBufferSize - Size of ContextBuffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_QueuedWorkItem* moduleContext;
    DMF_CONFIG_QueuedWorkItem* moduleConfig;
    UCHAR* clientBufferWithMetadata;
    UCHAR* clientBuffer;
    VOID* clientBufferContext;
    DMF_PORTABLE_EVENT event;
    NTSTATUS ntStatusCall;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 QueuedWorkItem);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Get an empty buffer to place parameters for this call.
    //
    ntStatus = DMF_BufferQueue_Fetch(moduleContext->DmfModuleBufferQueue,
                                     (VOID**)&clientBufferWithMetadata,
                                     &clientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    clientBuffer = clientBufferWithMetadata + sizeof(QUEUEDWORKITEM_WAIT_BLOCK);

    // Copy over the parameters.
    //
    ASSERT(ContextBufferSize <= moduleConfig->BufferQueueConfig.SourceSettings.BufferSize - sizeof(QUEUEDWORKITEM_WAIT_BLOCK));
    RtlCopyMemory(clientBuffer,
                  ContextBuffer,
                  ContextBufferSize);

    QUEUEDWORKITEM_WAIT_BLOCK* queuedWorkItemWaitBlock = QueuedWorkItem_WaitBlockFromClientBufferWithMetadata(clientBufferWithMetadata);

    DMF_Portable_EventCreate(&event,
                             NotificationEvent,
                             FALSE);

    queuedWorkItemWaitBlock->Event = &event;
    queuedWorkItemWaitBlock->NtStatus = &ntStatusCall;

    // Add to pending work list.
    //
    DMF_BufferQueue_Enqueue(moduleContext->DmfModuleBufferQueue,
                            clientBufferWithMetadata);

    // Execute deferred call.
    //
    DMF_ScheduledTask_ExecuteNowDeferred(moduleContext->DmfModuleScheduledTask,
                                         DmfModule);

     // Wait for the work to execute.
     //
    DMF_Portable_EventWaitForSingleObject(&event,
                                          FALSE,
                                          NULL);

    // Copy over the NTSTATUS that will be returned.
    //
    ntStatus = ntStatusCall;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_QueuedWorkItem.c
//
