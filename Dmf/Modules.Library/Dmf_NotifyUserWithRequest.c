/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_NotifyUserWithRequest.c

Abstract:

    Allows applications to receive asynchronous notifications about driver events.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_NotifyUserWithRequest.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include "Dmf_NotifyUserWithRequest_EventLog.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Queue of pending requests from Client Applications who want to know when
    // processes appear or disappear.
    //
    WDFQUEUE EventRequestQueue;

    // Number of event requests held.
    //
    LONG EventCountHeld;

    // List used to store events.
    //
    DMFMODULE DmfModuleBufferQueue;
} DMF_CONTEXT_NotifyUserWithRequest;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(NotifyUserWithRequest)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(NotifyUserWithRequest)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Common data structure used to store User-mode events.
//
typedef struct
{
    // Client callback function invoked by passing a request and usermode event data which is
    // used to complete the request.
    // 
    EVT_DMF_NotifyUserWithRequeset_Complete* EventCallbackFunction;
    // Event Callback context.
    //
    VOID* EventCallbackContext;
    // Status used to complete the request.
    //
    NTSTATUS NtStatus;
} USEREVENT_ENTRY;

EVT_WDF_IO_QUEUE_IO_CANCELED_ON_QUEUE EvtIoCanceledOnQueue;

VOID
EvtIoCanceledOnQueue(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Cancel a request that is pending in this Module's queue.

Arguments:

    Queue - WDFQUEUE that holds the request.
    Request - WDFREQUST to cancel.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_NotifyUserWithRequest* moduleContext;
    DMF_CONFIG_NotifyUserWithRequest* moduleConfig;

    FuncEntry(DMF_TRACE);

    // The Queue's Module Context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Queue);

    // Now, get the Module's Context.
    //
    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    ASSERT(moduleContext != NULL);

    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);
    ASSERT(moduleConfig != NULL);

    // Cancel the request and decrement our held event count.
    //
    ASSERT(moduleContext->EventCountHeld > 0);
    InterlockedDecrement(&moduleContext->EventCountHeld);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "CANCEL 0x%p PendingEvents=%d", Request, moduleContext->EventCountHeld);

    if (moduleConfig->EvtPendingRequestsCancel != NULL)
    {
        // Caller must eventually complete the request. But this callback allows Client to
        // delay if necessary.
        //
        moduleConfig->EvtPendingRequestsCancel(*dmfModuleAddress,
                                               Request,
                                               (ULONG_PTR)NULL,
                                               STATUS_CANCELLED);
    }
    else
    {
        // Return the request to the caller. Caller will know it did not a process event since an error code will be set.
        //
        WdfRequestComplete(Request,
                           STATUS_CANCELLED);
    }

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
ULONG
NotifyUserWithRequest_EventRequestReturn(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequeset_Complete* EventCallbackFunction,
    _In_opt_ ULONG_PTR EventCallbackContext,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    Dequeue a pending request, populate it and return it to caller.

Arguments:

    DmfModule - This Module's handle.
    EventCallbackFunction - The function that populates and returns the request. If NULL, then the
                            dequeued request is simply returned.
    EventCallbackContext - Client Context
    NtStatus - NTSTATUS to populate in the request.

Return Value:

    0 - No requests were available to dequeue.
    1 - One request was dequeued.

--*/
{
    DMF_CONTEXT_NotifyUserWithRequest* moduleContext;
    NTSTATUS ntStatus;
    ULONG numberOfRequestsCompleted;
    WDFREQUEST request;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Complete all the requests in the queue at this time.
    //
    numberOfRequestsCompleted = 0;

    ntStatus = WdfIoQueueRetrieveNextRequest(moduleContext->EventRequestQueue,
                                             &request);
    if (NT_SUCCESS(ntStatus))
    {
        // NOTE: The decrement must happen before the request returns because
        //       the caller may immediately enqueue another request.
        //
        ASSERT(moduleContext->EventCountHeld > 0);
        InterlockedDecrement(&moduleContext->EventCountHeld);
        numberOfRequestsCompleted++;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DEQUEUE request=0x%p PendingEvents=%d", request, moduleContext->EventCountHeld);
        if (NULL == EventCallbackFunction)
        {
            // Complete the request on behalf of Client Driver.
            // NOTE: NtStatus can be STATUS_CANCELLED or any other NTSTATUS.
            //
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Complete request=0x%p", request);
            WdfRequestComplete(request,
                               NtStatus);
        }
        else
        {
            // Allow Client Driver to complete this request.
            //
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Pass request=0x%p to Client Driver", request);
            EventCallbackFunction(DmfModule,
                                  request,
                                  EventCallbackContext,
                                  NtStatus);
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Cannot find request ntStatus:%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "numberOfRequestsCompleted=%d", numberOfRequestsCompleted);

    return numberOfRequestsCompleted;
}
#pragma code_seg()

#pragma code_seg("PAGE")
NTSTATUS
NotifyUserWithRequest_CompleteRequestWithEventData(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Looks for a request and a User-mode event. If both are found, calls the client callback function
    to complete the request with the User-mode event.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFREQUEST request;
    VOID* clientBuffer;
    VOID* clientBufferContext;
    DMF_CONTEXT_NotifyUserWithRequest* moduleContext;
    DMF_CONFIG_NotifyUserWithRequest* moduleConfig;
    USEREVENT_ENTRY* userEventEntry;
    ULONG numberOfRequestsCompleted;
    BOOLEAN lockAcquired;
    BOOLEAN clientBufferExtracted;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    numberOfRequestsCompleted = 0;
    ntStatus = STATUS_SUCCESS;
    clientBuffer = NULL;
    lockAcquired = FALSE;
    clientBufferExtracted = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);
    lockAcquired = TRUE;

    // Check if request is available.
    //
    ntStatus = WdfIoQueueFindRequest(moduleContext->EventRequestQueue,
                                     NULL,
                                     NULL,
                                     NULL,
                                     &request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "WdfIoQueueFindRequest failed: ntStatus=%!STATUS!", ntStatus);
        // Correct the error status.
        //
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    // Dereferencing the request object because every successful call to WdfIoQueueFindRequest
    // increments the reference count of the request object.
    //
    WdfObjectDereference(request);

    // Get a buffer with event data from the consumer list.
    //
    ntStatus = DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueue,
                                       &clientBuffer,
                                       &clientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        // Failed to get a buffer from the consumer buffer list, so haven't retrieved the request yet, so log the error and exit.
        //
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_BufferQueue_Dequeue fails: ntStatus=%!STATUS!", ntStatus);
        // Correct the error status.
        //
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    userEventEntry = (USEREVENT_ENTRY*)clientBuffer;

    // Now a request and a valid event are available. Complete the request with the event data.
    //
    numberOfRequestsCompleted = NotifyUserWithRequest_EventRequestReturn(DmfModule,
                                                                         userEventEntry->EventCallbackFunction,
                                                                         (ULONG_PTR)userEventEntry->EventCallbackContext,
                                                                         userEventEntry->NtStatus);

    if (0 == numberOfRequestsCompleted)
    {
        // Failed to complete the request.
        //
        ntStatus = STATUS_INTERNAL_ERROR;
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "NotifyUserWithRequest_EventRequestReturn failed to complete request.");
        moduleConfig = DMF_CONFIG_GET(DmfModule);
#if defined(DMF_USER_MODE)
        DMF_Utility_EventLogEntryWriteUserMode(moduleConfig->ClientDriverProviderName,
                                               EVENTLOG_INFORMATION_TYPE,
                                               MSG_USERMODE_EVENT_REQUEST_NOT_FOUND,
                                               0,
                                               NULL,
                                               0);
#endif // defined(DMF_USER_MODE)
        goto Exit;
    }

Exit:

    if (NULL != clientBuffer)
    {
        DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueue,
                              clientBuffer);
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_BufferQueue_Reuse");
    }

    if (TRUE == lockAcquired)
    {
        DMF_ModuleUnlock(DmfModule);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;

}
#pragma code_seg()

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
_Must_inspect_result_
static
NTSTATUS
DMF_NotifyUserWithRequest_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type NotifyUserWithRequest.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_NotifyUserWithRequest* moduleContext;
    WDF_IO_QUEUE_CONFIG ioQueueConfig;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES queueAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    // This queue will hold requests that are asynchronously completed.
    // 
    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig,
                             WdfIoQueueDispatchManual);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueAttributes,
                                            DMFMODULE);
    queueAttributes.ParentObject = DmfModule;

    ioQueueConfig.PowerManaged = WdfFalse;
    ioQueueConfig.EvtIoCanceledOnQueue = EvtIoCanceledOnQueue;
    ntStatus = WdfIoQueueCreate(device,
                                &ioQueueConfig,
                                &queueAttributes,
                                &moduleContext->EventRequestQueue);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "WdfIoQueueCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // NOTE: It is not possible to get the parent of a WDFIOQUEUE.
    // Therefore, it is necessary to save the DmfModule in its context area.
    //
    DMF_ModuleInContextSave(moduleContext->EventRequestQueue,
                            DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_NotifyUserWithRequest_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type NotifyUserWithRequest.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_NotifyUserWithRequest* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Flush any requests held by this object.
    //
    DMF_NotifyUserWithRequest_RequestReturnAll(DmfModule,
                                               NULL,
                                               0,
                                               STATUS_CANCELLED);

    WdfObjectDelete(moduleContext->EventRequestQueue);
    moduleContext->EventRequestQueue = NULL;

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_ChildModulesAdd(
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
    DMF_CONTEXT_NotifyUserWithRequest* moduleContext;
    DMF_CONFIG_NotifyUserWithRequest* moduleConfig;
    DMF_CONFIG_BufferQueue moduleBufferQueueConfigList;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // BufferQueue
    // -----------
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&moduleBufferQueueConfigList,
                                               &moduleAttributes);
    moduleBufferQueueConfigList.SourceSettings.EnableLookAside = FALSE;
    moduleBufferQueueConfigList.SourceSettings.BufferCount = moduleConfig->MaximumNumberOfPendingDataBuffers;
    moduleBufferQueueConfigList.SourceSettings.BufferSize = sizeof(USEREVENT_ENTRY) +
                                                            moduleConfig->SizeOfDataBuffer;
    if (DmfParentModuleAttributes->PassiveLevel)
    {
        moduleBufferQueueConfigList.SourceSettings.PoolType = PagedPool;
    }
    else
    {
        moduleBufferQueueConfigList.SourceSettings.PoolType = NonPagedPoolNx;
    }
    moduleAttributes.ClientModuleInstanceName = "NotifyUserWithRequestBufferQueue";
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
DMF_NotifyUserWithRequest_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type NotifyUserWithRequest.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_NotifyUserWithRequest;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_NotifyUserWithRequest;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_NotifyUserWithRequest);
    dmfCallbacksDmf_NotifyUserWithRequest.ChildModulesAdd = DMF_NotifyUserWithRequest_ChildModulesAdd;
    dmfCallbacksDmf_NotifyUserWithRequest.DeviceOpen = DMF_NotifyUserWithRequest_Open;
    dmfCallbacksDmf_NotifyUserWithRequest.DeviceClose = DMF_NotifyUserWithRequest_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_NotifyUserWithRequest,
                                            NotifyUserWithRequest,
                                            DMF_CONTEXT_NotifyUserWithRequest,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_NotifyUserWithRequest.CallbacksDmf = &dmfCallbacksDmf_NotifyUserWithRequest;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_NotifyUserWithRequest,
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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_DataProcess(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequeset_Complete* EventCallbackFunction,
    _In_opt_ VOID* EventCallbackContext,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    Processes the User-mode event. Stores it in this object's consumer list and calls NotifyUserWithRequest_CompleteRequestWithEventData
    which tries to complete the request with event data.

Arguments:

    DmfModule - This Module's handle.
    EventCallbackFunction - Client callback function invoked by passing a request and usermode event data.
    EventCallbackContext - Event callback context.
    NtStatus - Status associated with the User-mode event.

Return Value:

    None

++*/
{
    // Store the data event in the buffer list.
    //
    USEREVENT_ENTRY* userEventEntry;
    DMF_CONTEXT_NotifyUserWithRequest* moduleContext;
    DMF_CONFIG_NotifyUserWithRequest* moduleConfig;
    VOID* clientBuffer;
    VOID* clientBufferContext;
    NTSTATUS ntStatus;
    BOOLEAN isLocked;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    isLocked = FALSE;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ASSERT(((EventCallbackContext != NULL) && moduleConfig->SizeOfDataBuffer > 0) ||
        (NULL == EventCallbackContext));

    DMF_ModuleLock(DmfModule);
    isLocked = TRUE;

    // Retrieve the next buffer. 
    // This call should always succeed. The buffer list is created with a fixed number of buffers.
    // The consumer locks the DMFMODULE, consumes the content of the buffer and returns it back to the list.
    // The producer here, locks the DMFMODULE and then gets the next buffer.
    //

    ntStatus = DMF_BufferQueue_Fetch(moduleContext->DmfModuleBufferQueue,
                                     &clientBuffer,
                                     &clientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        // Failed to get buffer from producer list.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_BufferQueue_Fetch fails: ntStatus=%!STATUS!", ntStatus);

        // Get the buffer from consumer list. This will overwrite stale data.
        //
        ntStatus = DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueue,
                                           &clientBuffer,
                                           &clientBufferContext);
        if (! NT_SUCCESS(ntStatus))
        {
            // This should never happen.
            //
            ASSERT(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferQueue_Dequeue fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Populate the client buffer with event data.
    //
    userEventEntry = (USEREVENT_ENTRY*)clientBuffer;

    userEventEntry->EventCallbackFunction = EventCallbackFunction;
    userEventEntry->NtStatus = NtStatus;
    userEventEntry->EventCallbackContext = ((BYTE*)userEventEntry) + sizeof(USEREVENT_ENTRY);
    // It is not necessary to check if EventCallbackContext is NULL because if that is the case
    // SizeOfDataBuffer is asserted to be 0.
    // 'warning C6387: 'EventCallbackContext' could be '0':  this does not adhere to the specification for the function 'memcpy'. '.
    //
    #pragma warning(suppress:6387)
    RtlCopyMemory(userEventEntry->EventCallbackContext,
                  EventCallbackContext,
                  moduleConfig->SizeOfDataBuffer);

    DMF_BufferQueue_Enqueue(moduleContext->DmfModuleBufferQueue,
                            clientBuffer);

    DMF_ModuleUnlock(DmfModule);

    isLocked = FALSE;

    // Complete request with user event.
    //
    ntStatus = NotifyUserWithRequest_CompleteRequestWithEventData(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NotifyUserWithRequest_CompleteRequestWithEventData fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    if (isLocked)
    {
        DMF_ModuleUnlock(DmfModule);
    }

    FuncExitVoid(DMF_TRACE);

}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequest_EventRequestAdd(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Add an event request to this object's queue.

Arguments:

    DmfModule - This Module's handle.
    Request - The request to add to this object's queue.

Return Value:

    NTSTATUS

++*/
{
    DMF_CONTEXT_NotifyUserWithRequest* moduleContext;
    DMF_CONFIG_NotifyUserWithRequest* moduleConfig;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (InterlockedIncrement(&moduleContext->EventCountHeld) > moduleConfig->MaximumNumberOfPendingRequests)
    {
        // The maximum number of pending events allowed is exceeded.
        //
        InterlockedDecrement(&moduleContext->EventCountHeld);
        ntStatus = STATUS_INVALID_DEVICE_STATE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Too many events (%d): Request=%p", moduleConfig->MaximumNumberOfPendingRequests, Request);
        goto Exit;
    }

    // When a process comes or goes this request will be dequeued and completed.
    //
    ntStatus = WdfRequestForwardToIoQueue(Request,
                                          moduleContext->EventRequestQueue);
    if (NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ENQUEUE Request=0x%p EventsHeld=%d", Request, moduleContext->EventCountHeld);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Unable to enqueue Request=%p ntStatus=%!STATUS!", Request, ntStatus);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequest_RequestProcess(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Stores the request in this object's request queue and calls NotifyUserWithRequest_CompleteRequestWithEventData
    which tries to complete the request with event data.

Arguments:

    DmfModule - This Module's handle.
    Request - The request to add to this object's queue.

Return Value:

    Status of the operation.

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    // Store the request.
    //
    ntStatus = DMF_NotifyUserWithRequest_EventRequestAdd(DmfModule,
                                                         Request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_NotifyUserWithRequest_EventRequestAdd failed with ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Complete request with user event.
    //
    ntStatus = NotifyUserWithRequest_CompleteRequestWithEventData(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NotifyUserWithRequest_CompleteRequestWithEventData failed with ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_RequestReturn(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequeset_Complete* EventCallbackFunction,
    _In_opt_ ULONG_PTR EventCallbackContext,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    Dequeue a single request from this object's queue and complete it using a specif
    completion handler.
    NOTE: This dequeues a single request which means that caller must have opened this
    channel in Exclusive Mode.

Arguments:

    DmfModule - This Module's handle.
    EventCallbackFunction - The function to call with dequeued request.
    EventCallbackContext - Context to pass to EventCallbackFunction.
    NtStatus - The status to send in completed request.

Return Value:

    None

++*/
{
    ULONG numberOfRequestsCompleted;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    numberOfRequestsCompleted = NotifyUserWithRequest_EventRequestReturn(DmfModule,
                                                                         EventCallbackFunction,
                                                                         EventCallbackContext,
                                                                         NtStatus);
    if (0 == numberOfRequestsCompleted)
    {
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Event lost because there are no pending requests!");
    }
    else
    {
        ASSERT(1 == numberOfRequestsCompleted);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_RequestReturnAll(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequeset_Complete* EventCallbackFunction,
    _In_opt_ ULONG_PTR EventCallbackContext,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    Dequeue all requests from this object's queue and complete it using a specific
    completion handler.

Arguments:

    DmfModule - This Module's handle.
    EventCallbackFunction - The function to call with dequeued requests.
    EventCallbackContext - Context to pass to EventCallbackFunction.
    NtStatus - The status to send in completed request.

Return Value:

    None

++*/
{
    ULONG numberOfRequestsCompleted;
    ULONG numberOfRequestsCompletedThisCall;
    ULONG numberOfRequestsToComplete;
    DMF_CONTEXT_NotifyUserWithRequest* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // By design this Method can be called by Close callback.
    // (This Method is called to flush any remaining requests when Module is closed.)
    //
    DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(DmfModule,
                                            NotifyUserWithRequest);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Find number of requests in queue (There will be at least 1 request per Application).
    //
    numberOfRequestsToComplete = moduleContext->EventCountHeld;

    // Complete all the requests in the queue at this time.
    //
    numberOfRequestsCompleted = 0;
    numberOfRequestsCompletedThisCall = 0;

    do
    {
        numberOfRequestsCompletedThisCall = NotifyUserWithRequest_EventRequestReturn(DmfModule,
                                                                                     EventCallbackFunction,
                                                                                     EventCallbackContext,
                                                                                     NtStatus);
        numberOfRequestsCompleted += numberOfRequestsCompletedThisCall;
        numberOfRequestsToComplete--;
    } while ((numberOfRequestsCompletedThisCall > 0) && (numberOfRequestsToComplete > 0));

    if (0 == numberOfRequestsCompleted)
    {
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Event lost because there are no pending requests!");
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Number of requests completed = %u", numberOfRequestsCompleted);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_NotifyUserWithRequest_RequestReturnEx(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequeset_Complete* EventCallbackFunction,
    _In_opt_ ULONG_PTR EventCallbackContext,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    This is a variation of DMF_NotifyUserWithRequest_RequestReturn just by adding operation status.
    Use case for variated function is when Client does not know if there is a request in a queue
    but must still get data stored until a request arrives to carry the new data back.

Arguments:

    DmfModule - This Module's handle.
    EventCallbackFunction - The function to call with dequeued request.
    EventCallbackContext - Context to pass to EventCallbackFunction.
    NtStatus - The status to send in completed request.

Return Value:

    STATUS_SUCCESS - A request was completed normally.
    STATUS_UNSUCCESSFUL - There was no request in the queue.

++*/
{
    ULONG numberOfRequestsCompleted;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    numberOfRequestsCompleted = NotifyUserWithRequest_EventRequestReturn(DmfModule,
                                                                         EventCallbackFunction,
                                                                         EventCallbackContext,
                                                                         NtStatus);
    if (0 == numberOfRequestsCompleted)
    {
        ntStatus = STATUS_UNSUCCESSFUL;   
    }
    else
    {
        ntStatus = STATUS_SUCCESS;
    }

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

// eof: Dmf_NotifyUserWithRequest.c
//
