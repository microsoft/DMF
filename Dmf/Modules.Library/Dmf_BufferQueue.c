/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_BufferQueue.c

Abstract:

    Creates a producer buffer pool and a consumer buffer pool and exposes primitives to
    use those pools.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_BufferQueue.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_BufferQueue
{
    // DMFMODULE to Producer BufferPool.
    //
    DMFMODULE DmfModuleBufferPoolProducer;
    // DMFMODULE to Consumer BufferPool.
    //
    DMFMODULE DmfModuleBufferPoolConsumer;
} DMF_CONTEXT_BufferQueue;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(BufferQueue)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(BufferQueue)

// Memory Pool Tag.
//
#define MemoryTag 'oMQB'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _BufferQueue_BufferContext
{
    // TimerCallback assigned by the client when using DMF_BufferQueue_EnqueueWithTimer.
    //
    EVT_DMF_BufferPool_TimerCallback* ClientTimerExpirationCallback;
} BufferQueue_BufferContextInternal;

VOID
BufferQueue_BufferContextInternalGet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _Out_ BufferQueue_BufferContextInternal** BufferContextInternal
    )
/*++

Routine Description:

    Get the section of the Buffer's context used internally by this Module.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to retrieve the context for.
    BufferContextInternal - The section of the buffer's context used internally by this Module.

Return Value:

    None

--*/
{
    DMF_CONFIG_BufferQueue* moduleConfig;
    DMF_CONTEXT_BufferQueue* moduleContext;
    UINT8* clientBufferContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    clientBufferContext = NULL;

    DMF_BufferPool_ContextGet(moduleContext->DmfModuleBufferPoolProducer,
                              ClientBuffer,
                              (VOID**)&clientBufferContext);

    *BufferContextInternal = (BufferQueue_BufferContextInternal*)(clientBufferContext + moduleConfig->SourceSettings.BufferContextSize);

    FuncExitVoid(DMF_TRACE);
}

_Function_class_(EVT_DMF_BufferPool_TimerCallback)
VOID
_IRQL_requires_max_(DISPATCH_LEVEL)
BufferQueue_TimerCallback(
    _In_ DMFMODULE DmfModuleBufferPoolConsumer,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
/*++

Routine Description:

    This callback function will be called if the buffer in DmfModuleBufferPoolConsumer times out.
    This is used to chain the timer callback to the callback set by the client in DMF_BufferQueue_EnqueueWithTimer.

Arguments:

    DmfModule - The Consumer BufferPool Module.
    ClientBuffer - The buffer in the BufferPool.
    ClientBufferContext - Context associated with the buffer.
    ClientDriverCallContext - Context associated with the timer callback.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_BufferQueue* moduleContext;
    DMF_CONFIG_BufferQueue* moduleConfig;
    BufferQueue_BufferContextInternal* bufferContextInternal;
    EVT_DMF_BufferPool_TimerCallback* clientTimerExpirationCallback;

    FuncEntry(DMF_TRACE);

    dmfModule = DMF_ParentModuleGet(DmfModuleBufferPoolConsumer);
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);
    bufferContextInternal = NULL;
    
    BufferQueue_BufferContextInternalGet(dmfModule,
                                         ClientBuffer,
                                         &bufferContextInternal);

    DmfAssert(bufferContextInternal != NULL);
    DmfAssert(bufferContextInternal->ClientTimerExpirationCallback != NULL);

    clientTimerExpirationCallback = bufferContextInternal->ClientTimerExpirationCallback;
    bufferContextInternal->ClientTimerExpirationCallback = NULL;

    // clientTimerExpirationCallback cannot be NULL. DMF_BufferQueue_EnqueueWithTimer fails
    // if client does not assign a TimerExpirationCallback.
    //
    clientTimerExpirationCallback(dmfModule,
                                  ClientBuffer,
                                  ClientBufferContext,
                                  ClientDriverCallbackContext);

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
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_BufferQueue_ChildModulesAdd(
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
    DMF_CONFIG_BufferPool moduleConfigProducer;
    DMF_CONFIG_BufferPool moduleConfigConsumer;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_BufferQueue* moduleConfig;
    DMF_CONTEXT_BufferQueue* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // BufferPoolProducer
    // ------------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigProducer,
                                              &moduleAttributes);
    moduleConfigProducer.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigProducer.Mode.SourceSettings = moduleConfig->SourceSettings;
    moduleConfigProducer.Mode.SourceSettings.BufferContextSize = moduleConfigProducer.Mode.SourceSettings.BufferContextSize + sizeof(BufferQueue_BufferContextInternal);
    moduleAttributes.ClientModuleInstanceName = "BufferPoolProducer";
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolProducer);

    // BufferPoolConsumer
    // ------------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigConsumer,
                                              &moduleAttributes);
    moduleConfigConsumer.BufferPoolMode = BufferPool_Mode_Sink;
    moduleAttributes.ClientModuleInstanceName = "BufferPoolConsumer";
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolConsumer);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_BufferQueue_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type BufferQueue.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // This causes the Client's clean up callback to be called in case the Client
    // referenced or allocated objects associated with the buffers.
    //
    DMF_BufferQueue_Flush(DmfModule);

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
DMF_BufferQueue_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type BufferQueue.

Arguments:

    Device - Client Driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_BufferQueue;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_BufferQueue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_BufferQueue);
    dmfCallbacksDmf_BufferQueue.ChildModulesAdd = DMF_BufferQueue_ChildModulesAdd;
    dmfCallbacksDmf_BufferQueue.DeviceClose = DMF_BufferQueue_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_BufferQueue,
                                            BufferQueue,
                                            DMF_CONTEXT_BufferQueue,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_BufferQueue.CallbacksDmf = &dmfCallbacksDmf_BufferQueue;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_BufferQueue,
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
VOID
DMF_BufferQueue_ContextGet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _Out_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Get the context associated with the given Client buffer.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The given Client buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferPool_ContextGet(moduleContext->DmfModuleBufferPoolProducer,
                              ClientBuffer,
                              ClientBufferContext);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DMF_BufferQueue_Count(
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
    DMF_CONTEXT_BufferQueue* moduleContext;
    ULONG numberOfEntriesInList;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    numberOfEntriesInList = DMF_BufferPool_Count(moduleContext->DmfModuleBufferPoolConsumer);

    FuncExit(DMF_TRACE, "numberOfEntriesInList=%d", numberOfEntriesInList);

    return numberOfEntriesInList;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_Dequeue(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Removes the next buffer in the consumer list (head of the list) if there is a buffer.
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
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolConsumer,
                                  ClientBuffer,
                                  ClientBufferContext);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_DequeueWithMemoryDescriptor(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ PWDF_MEMORY_DESCRIPTOR MemoryDescriptor,
    _Out_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Removes the next buffer in the list (head of the list) if there is a buffer.
    Then, returns the Client Buffer and its associated Memory Descriptor and ClientBufferContext.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    MemoryDescriptor - WDF Memory Descriptor associated with removed buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    STATUS_SUCCESS if a buffer is removed from the list.
    STATUS_UNSUCCESSFUL if the list is empty.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_BufferPool_GetWithMemoryDescriptor(moduleContext->DmfModuleBufferPoolConsumer,
                                                      ClientBuffer,
                                                      MemoryDescriptor,
                                                      ClientBufferContext);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Enqueue(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Adds a Client Buffer to the end of the consumer list. This list is consumed in FIFO order.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolConsumer,
                       ClientBuffer);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_EnqueueAtHead(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Adds a Client Buffer to the head of the consumer list. This list is consumed in LIFO order.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferPool_PutAtHead(moduleContext->DmfModuleBufferPoolConsumer,
                             ClientBuffer);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_BufferQueue_EnqueueWithTimer(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ ULONGLONG TimerExpirationMilliseconds,
    _In_ EVT_DMF_BufferPool_TimerCallback* TimerExpirationCallback,
    _In_opt_ VOID* TimerExpirationCallbackContext
    )
/*++

Routine Description:

    Adds a Client Buffer to the end of the consumer list and starts a timer. If the buffer is still in the list 
    when the timer expires, buffer will be removed from the list, and the TimerExpirationCallback
    will be called. Client owns the buffer in TimerExpirationCallback.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by this Module.
    TimerExpirationMilliseconds - Set the optional timer to expire after this many milliseconds.
    TimerExpirationCallback - Callback function to call when timer expires.
    TimerExpirationCallbackContext - This Context will be passed to TimerExpirationCallback.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;
    DMF_CONFIG_BufferQueue* moduleConfig;
    BufferQueue_BufferContextInternal* bufferContextInternal;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    bufferContextInternal = NULL;
    ntStatus = STATUS_SUCCESS;

    if (TimerExpirationCallback == NULL)
    {
        TraceError(DMF_TRACE, "TimerExpirationCallback cannot be NULL");
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    BufferQueue_BufferContextInternalGet(DmfModule,
                                         ClientBuffer,
                                         &bufferContextInternal);
    DmfAssert(bufferContextInternal != NULL);

    bufferContextInternal->ClientTimerExpirationCallback = TimerExpirationCallback;

    DMF_BufferPool_PutInSinkWithTimer(moduleContext->DmfModuleBufferPoolConsumer,
                                      ClientBuffer,
                                      TimerExpirationMilliseconds,
                                      BufferQueue_TimerCallback,
                                      TimerExpirationCallbackContext);

    FuncExitVoid(DMF_TRACE);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_BufferPool_Enumeration EntryEnumerationCallback,
    _In_opt_ VOID* ClientDriverCallbackContext,
    _Out_opt_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Enumerate all the buffers in the consumer buffer list, calling a Client Driver's callback function
    for each buffer. If the Client wishes, the buffer can be removed from the list.
    NOTE: Module lock is held during this call.

Arguments:

    DmfModule - This Module's handle.
    EntryEnumerationCallback - Caller's enumeration function called for each buffer in the list.
    ClientDriverCallbackContext - Context passed for this call.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferPool_Enumerate(moduleContext->DmfModuleBufferPoolConsumer,
                             EntryEnumerationCallback,
                             ClientDriverCallbackContext,
                             ClientBuffer,
                             ClientBufferContext);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_Fetch(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Removes the next buffer in the producer list (head of the list) if there is a buffer.
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
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolProducer,
                                  ClientBuffer,
                                  ClientBufferContext);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Flush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Remove all buffers from consumer buffer pool and place them in producer pool.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;
    VOID* buffer;
    VOID* bufferContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(DmfModule,
                                            BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;
    while (NT_SUCCESS(ntStatus))
    {
        ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolConsumer,
                                      &buffer,
                                      &bufferContext);
        if (NT_SUCCESS(ntStatus))
        {
            DMF_BufferQueue_Reuse(DmfModule,
                                  buffer);
        }
    }

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Reuse(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Adds a Client Buffer to the producer list.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    None

--*/
{
    DMF_CONFIG_BufferQueue* moduleConfig;
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(DmfModule,
                                            BufferQueue);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // If Config EvtBufferQueueReuseCleanup callback present, call
    // with buffer before handing back to Producer BufferPool.
    //
    if (moduleConfig->EvtBufferQueueReuseCleanup)
    {
        VOID* clientBufferContext = NULL;

        DMF_BufferPool_ContextGet(moduleContext->DmfModuleBufferPoolConsumer,
                                  ClientBuffer,
                                  &clientBufferContext);

        (moduleConfig->EvtBufferQueueReuseCleanup)(DmfModule,
                                                   ClientBuffer,
                                                   clientBufferContext);
    }

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolProducer,
                       ClientBuffer);

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_BufferQueue.c
//
