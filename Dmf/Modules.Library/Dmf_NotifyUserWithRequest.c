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

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_NotifyUserWithRequest.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_NotifyUserWithRequest
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

    // Used to timestamp requests and data buffers.
    //
    DMFMODULE DmfModuleTime;
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
    EVT_DMF_NotifyUserWithRequest_Complete* EventCallbackFunction;
    // Event Callback context.
    //
    VOID* EventCallbackContext;
    // Status used to complete the request.
    //
    NTSTATUS NtStatus;

    // Time that data is received.
    //
    LONGLONG Timestamp;
} USEREVENT_ENTRY;

struct REQUESTCONTEXT 
{
    LONGLONG Timestamp;
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(REQUESTCONTEXT, RequestContextGet)

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
    DmfAssert(moduleContext != NULL);

    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);
    DmfAssert(moduleConfig != NULL);

    // Cancel the request and decrement our held event count.
    //
    DmfAssert(moduleContext->EventCountHeld > 0);
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

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
NotifyUserWithRequest_EventRequestReturn(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequest_Complete* EventCallbackFunction,
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

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Complete all the requests in the queue at this time.
    //
    numberOfRequestsCompleted = 0;

    request = NULL;
    ntStatus = WdfIoQueueRetrieveNextRequest(moduleContext->EventRequestQueue,
                                             &request);
    if (NT_SUCCESS(ntStatus))
    {
        // NOTE: The decrement must happen before the request returns because
        //       the caller may immediately enqueue another request.
        //
        DmfAssert(moduleContext->EventCountHeld > 0);
        InterlockedDecrement(&moduleContext->EventCountHeld);
        numberOfRequestsCompleted++;
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DEQUEUE request=0x%p PendingEvents=%d", request, moduleContext->EventCountHeld);
        if (NULL == EventCallbackFunction)
        {
            // Complete the request on behalf of Client Driver.
            // NOTE: NtStatus can be STATUS_CANCELLED or any other NTSTATUS.
            //
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Complete request=0x%p", request);
            WdfRequestComplete(request,
                               NtStatus);
        }
        else
        {
            // Allow Client Driver to complete this request.
            //
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Pass request=0x%p to Client Driver", request);
            EventCallbackFunction(DmfModule,
                                  request,
                                  EventCallbackContext,
                                  NtStatus);
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Cannot find request ntStatus:%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "numberOfRequestsCompleted=%d", numberOfRequestsCompleted);

    return numberOfRequestsCompleted;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NotifyUserWithRequest_EventRequestReturnAll(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequest_Complete* EventCallbackFunction,
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

    FuncEntry(DMF_TRACE);

    numberOfRequestsCompleted = 0;
    numberOfRequestsCompletedThisCall = 0;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Find number of requests in queue (There will be at least 1 request per Application).
    //
    numberOfRequestsToComplete = moduleContext->EventCountHeld;

    // Complete all the requests in the queue.
    //
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

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
NotifyUserWithRequest_CompleteRequestWithEventData(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Looks for both a pending request and pending data. If both exist, completes the
    pending request using the pending data (in the manner specified by the client).

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
    BOOLEAN clientBufferExtracted;

    FuncEntry(DMF_TRACE);

    numberOfRequestsCompleted = 0;
    ntStatus = STATUS_SUCCESS;
    clientBuffer = NULL;
    clientBufferExtracted = FALSE;

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleConfig->MaximumNumberOfPendingDataBuffers > 0);

    DMF_ModuleLock(DmfModule);

    // Check if request is available.
    //
    request = NULL;
    ntStatus = WdfIoQueueFindRequest(moduleContext->EventRequestQueue,
                                     NULL,
                                     NULL,
                                     NULL,
                                     &request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "WdfIoQueueFindRequest fails: ntStatus=%!STATUS!", ntStatus);
        // Correct the error status.
        //
        ntStatus = STATUS_SUCCESS;
        goto ExitWithUnlock;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "WdfIoQueueFindRequest success request=0x%p", request);

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
        goto ExitWithUnlock;
    }

    DMF_ModuleUnlock(DmfModule);

    userEventEntry = (USEREVENT_ENTRY*)clientBuffer;

    // Now a request and a valid event are available. Complete the request with the event data.
    //
    numberOfRequestsCompleted = NotifyUserWithRequest_EventRequestReturn(DmfModule,
                                                                         userEventEntry->EventCallbackFunction,
                                                                         (ULONG_PTR)userEventEntry->EventCallbackContext,
                                                                         userEventEntry->NtStatus);

    if (0 == numberOfRequestsCompleted)
    {
        // This path can happen in cases of stress where the single request in the queue has been completed/canceled
        // after the find above has happened. In this case the dequeued data buffer will be lost unless it is 
        // enqueued again.
        //
        ntStatus = STATUS_SUCCESS;
        // This is an unexpected path that should not, but can, happen. Re-queue the buffer at the head
        // and clear it so it is not reused. It will be completed later when a new request is available.
        //
        DMF_BufferQueue_EnqueueAtHead(moduleContext->DmfModuleBufferQueue,
                                      clientBuffer);
        // Don't reuse this buffer at end of this function.
        //
        clientBuffer = NULL;
        // It means producer is not sending requests fast enough.
        //
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "NotifyUserWithRequest_EventRequestReturn fails to complete request.");
#if defined(DMF_USER_MODE)
        DMF_Utility_LogEmitString(DmfModule,
                                  DmfLogDataSeverity_Informational,
                                  L"Request not found.");
#endif // defined(DMF_USER_MODE)
    }

    goto Exit;

ExitWithUnlock:

    DMF_ModuleUnlock(DmfModule);

Exit:

    if (NULL != clientBuffer)
    {
        DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueue,
                              clientBuffer);
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_BufferQueue_Reuse");
    }

    if (request != NULL)
    {
        // Dereferencing the request object because every successful call to WdfIoQueueFindRequest
        // increments the reference count of the request object.
        // NOTE: Do this after all the code that depends on the request has executed.
        //
        WdfObjectDereference(request);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
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
_Function_class_(DMF_Open)
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
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
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
    NotifyUserWithRequest_EventRequestReturnAll(DmfModule,
                                                NULL,
                                                0,
                                                STATUS_CANCELLED);

    WdfObjectDelete(moduleContext->EventRequestQueue);
    moduleContext->EventRequestQueue = NULL;

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
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

    if (moduleConfig->MaximumNumberOfPendingDataBuffers > 0)
    {
        // BufferQueue
        // -----------
        //
        DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&moduleBufferQueueConfigList,
                                                   &moduleAttributes);
        moduleBufferQueueConfigList.SourceSettings.EnableLookAside = moduleConfig->EnableLookAside;
        moduleBufferQueueConfigList.SourceSettings.BufferCount = moduleConfig->MaximumNumberOfPendingDataBuffers;
        moduleBufferQueueConfigList.SourceSettings.BufferSize = sizeof(USEREVENT_ENTRY) + moduleConfig->SizeOfDataBuffer;
        if (DmfParentModuleAttributes->PassiveLevel)
        {
            moduleBufferQueueConfigList.SourceSettings.PoolType = PagedPool;
        }
        else
        {
            moduleBufferQueueConfigList.SourceSettings.PoolType = NonPagedPoolNx;
        }
        moduleBufferQueueConfigList.EvtBufferQueueReuseCleanup = moduleConfig->EvtDataCleanup;
        moduleAttributes.ClientModuleInstanceName = "NotifyUserWithRequestBufferQueue";
        moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleBufferQueue);
    }

    if (moduleConfig->TimeStamping)
    {
        DMF_Time_ATTRIBUTES_INIT(&moduleAttributes);
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleTime);
    }

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

_IRQL_requires_max_(DISPATCH_LEVEL)
LONGLONG
DMF_NotifyUserWithRequest_DataBufferTimestampGet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* DataBuffer
    )
/*++

Routine Description:

    Given a dequeued data buffer, this Method returns its corresponding time stamp.
    NOTE: Only call this Method from the callback.

Arguments:

    DmfModule - This Module's handle.
    DataBuffer - The given data buffer.

Return Value:

    The data buffer's time stamp.

++*/
{
    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

#if defined(DEBUG)
    DMF_CONFIG_NotifyUserWithRequest* moduleConfig = DMF_CONFIG_GET(DmfModule);
    DmfAssert(moduleConfig->TimeStamping);
#endif

    USEREVENT_ENTRY* userEventEntry = (USEREVENT_ENTRY*)(((CHAR*)DataBuffer) - sizeof(USEREVENT_ENTRY));

    FuncExit(DMF_TRACE,"timeStamp=%lld", userEventEntry->Timestamp);

    return userEventEntry->Timestamp;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_NotifyUserWithRequest_DataProcess(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequest_Complete* EventCallbackFunction,
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

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoDereference;
    }

    ntStatus = STATUS_SUCCESS;
    isLocked = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(moduleConfig->MaximumNumberOfPendingDataBuffers > 0);
    DmfAssert(((EventCallbackContext != NULL) && moduleConfig->SizeOfDataBuffer > 0) ||
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
            DmfAssert(FALSE);
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
    if (moduleConfig->TimeStamping)
    {
        userEventEntry->Timestamp = DMF_Time_TickCountGet(moduleContext->DmfModuleTime);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "userEventEntry Timestamp=%lld", userEventEntry->Timestamp);
    }
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

    DMF_ModuleDereference(DmfModule);

    if (isLocked)
    {
        DMF_ModuleUnlock(DmfModule);
    }

ExitNoDereference:

    FuncExitVoid(DMF_TRACE);

}

_IRQL_requires_max_(DISPATCH_LEVEL)
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

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoDereference;
    }

    if (InterlockedIncrement(&moduleContext->EventCountHeld) > moduleConfig->MaximumNumberOfPendingRequests)
    {
        // The maximum number of pending events allowed is exceeded.
        //
        InterlockedDecrement(&moduleContext->EventCountHeld);
        ntStatus = STATUS_INVALID_DEVICE_STATE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Too many events (%d): Request=%p", moduleConfig->MaximumNumberOfPendingRequests, Request);
        goto Exit;
    }

    if (moduleConfig->TimeStamping)
    {
        WDF_OBJECT_ATTRIBUTES objectAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes, 
                                                REQUESTCONTEXT);

        REQUESTCONTEXT* requestContext;
        ntStatus = WdfObjectAllocateContext(Request, 
                                            &objectAttributes, 
                                            (VOID**)&requestContext);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfObjectAllocateContext fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        requestContext->Timestamp = DMF_Time_TickCountGet(moduleContext->DmfModuleTime);
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "requestContext Timestamp=%lld", requestContext->Timestamp);
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

    DMF_ModuleDereference(DmfModule);

ExitNoDereference:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
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
    DMF_CONFIG_NotifyUserWithRequest* moduleConfig;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoDereference;
    }

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    DmfAssert(moduleConfig->MaximumNumberOfPendingDataBuffers > 0);

    // Store the request.
    //
    ntStatus = DMF_NotifyUserWithRequest_EventRequestAdd(DmfModule,
                                                         Request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_NotifyUserWithRequest_EventRequestAdd fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Complete request with user event.
    //
    ntStatus = NotifyUserWithRequest_CompleteRequestWithEventData(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NotifyUserWithRequest_CompleteRequestWithEventData fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoDereference:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_NotifyUserWithRequest_RequestReturn(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequest_Complete* EventCallbackFunction,
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
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

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
        DmfAssert(1 == numberOfRequestsCompleted);
    }

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_NotifyUserWithRequest_RequestReturnAll(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequest_Complete* EventCallbackFunction,
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
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    NotifyUserWithRequest_EventRequestReturnAll(DmfModule,
                                                EventCallbackFunction,
                                                EventCallbackContext,
                                                NtStatus);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequest_RequestReturnEx(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequest_Complete* EventCallbackFunction,
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

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

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

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
LONGLONG
DMF_NotifyUserWithRequest_RequestTimestampGet(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Given a dequeued WDFREQUEST, this Method returns its corresponding time stamp.
    NOTE: Only call this Method from the callback.

Arguments:

    DmfModule - This Module's handle.
    Request - The given WDFREQUEST.

Return Value:

    The given request's time stamp.

++*/
{
    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequest);

#if defined(DEBUG)
    DMF_CONFIG_NotifyUserWithRequest* moduleConfig = DMF_CONFIG_GET(DmfModule);
    DmfAssert(moduleConfig->TimeStamping);
#endif

    REQUESTCONTEXT* requestContext = RequestContextGet(Request);

    FuncExit(DMF_TRACE,"timeStamp=%lld", requestContext->Timestamp);

    return requestContext->Timestamp;
}

// eof: Dmf_NotifyUserWithRequest.c
//
