/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_IoctlHandler.c

Abstract:

    Functional tests for Dmf_IoctlHandler Module.
    NOTE: This Module simply instantiates an instance of DMF_IoctlHandler. It provides a target
          for other Test Modules to send and receive data via an IOCTL interface.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_Tests_IoctlHandler.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    WDFREQUEST Request;
    Tests_IoctlHandler_Sleep SleepRequest;
} SleepContext;

typedef struct
{
    DMFMODULE DmfModuleTestIoctlHandler;
    LONG TimeToSleepMilliseconds;
} REQUEST_CONTEXT, *PREQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(REQUEST_CONTEXT);

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Tests_IoctlHandler
{
    // Allocates buffers to be inserted into pending pool.
    //
    DMFMODULE DmfModuleBufferPoolFree;
    // Module that stores all pending sleep contexts.
    //
    DMFMODULE DmfModuleBufferPoolPending;
    // Holds pending requests.
    //
    WDFQUEUE CancelableQueue;
    // To enable/disable interface.
    //
    DMFMODULE DmfModuleIoctlHandler;
    // To enable/disable interface.
    //
    DMFMODULE DmfModuleThread;
    // Helper for thread work.
    //
    DMFMODULE DmfModuleAlertableSleep;
    // Value get/set via direct call.
    //
    UCHAR InterfaceValue;
} DMF_CONTEXT_Tests_IoctlHandler;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_IoctlHandler)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(Tests_IoctlHandler)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This setting disables the code that disables/enables the device interface.
//
#define NO_DISABLE_INTERFACE_THREAD

typedef
BOOLEAN
(*Tests_IoctlHandler_RequestCompare)(_In_ WDFREQUEST, _In_ VOID*);

WDFREQUEST
Tests_IoctHandler_FindRequestWithMatchingData(
    _In_ DMFMODULE DmfModuleIoctlHandler,
    _In_ WDFQUEUE Queue,
    _In_  Tests_IoctlHandler_RequestCompare CallbackCompare,
    _In_ VOID* CallbackCompareContext
    )
{
    WDFREQUEST previousTagRequest;
    WDFREQUEST tagRequest;
    WDFREQUEST outRequest;
    NTSTATUS ntStatus;
    SleepContext* sleepContext;
    DMFMODULE dmfModule;

    previousTagRequest = NULL;
    tagRequest = NULL;
    outRequest = NULL;
    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    sleepContext = (SleepContext*)CallbackCompareContext;
    dmfModule = DMF_ParentModuleGet(DmfModuleIoctlHandler);

    DMF_ModuleLock(dmfModule);

    do 
    {
        ntStatus = WdfIoQueueFindRequest(Queue,
                                         previousTagRequest,
                                         NULL,
                                         NULL,
                                         &tagRequest);
        if (previousTagRequest)
        {
            // WdfIoQueueFindRequest incremented the reference count of the prevTagRequest object,
            // so decrement the count here.
            //
            WdfObjectDereference(previousTagRequest);
        }
        if (ntStatus == STATUS_NO_MORE_ENTRIES) 
        {
            break;
        }
        if (ntStatus == STATUS_NOT_FOUND) 
        {
            // The previousTagRequest object is no longer
            // in the queue.
            //
            previousTagRequest = tagRequest = NULL;
            continue;
        }
        if ( !NT_SUCCESS(ntStatus))
        {
            break;
        }

        // Determine if this is the request that is being searched for.
        //
        if (CallbackCompare(tagRequest,
                            &(sleepContext->Request)))
        {
            // Found a match.
            //
            // The timer expiration callback can be called for a request that has been cancelled.
            // This means the same request object may be re-used. Check TimeToSleepMilliseconds to ensure request matches.
            // 
            REQUEST_CONTEXT* requestContext;
            requestContext = (REQUEST_CONTEXT*)WdfObjectGetTypedContext(tagRequest,
                                                                        REQUEST_CONTEXT);
            if (requestContext->TimeToSleepMilliseconds != sleepContext->SleepRequest.TimeToSleepMilliseconds)
            {
                TraceEvents(TRACE_LEVEL_WARNING,
                            DMF_TRACE,
                            "Request has been reused: Request=0x%p TimeToSleepMilliseconds=%d ActualTimeToSleepMilliseconds=%d",
                            sleepContext->Request,
                            sleepContext->SleepRequest.TimeToSleepMilliseconds,
                            requestContext->TimeToSleepMilliseconds);

                // The request did not match our criteria. Get another request.
                //
                previousTagRequest = tagRequest;
                continue;
            }

            // Found a match. Get the request handle.
            // 
            ntStatus = WdfIoQueueRetrieveFoundRequest(Queue,
                                                      tagRequest,
                                                      &outRequest);
            // WdfIoQueueRetrieveFoundRequest incremented the
            // reference count of the TagRequest object,
            // so we decrement the count here.
            //
            WdfObjectDereference(tagRequest);

            if (ntStatus == STATUS_NOT_FOUND) 
            {
                // The TagRequest object is no longer in the queue. But other requests might
                // match our criteria, so we restart the search.
                //
                previousTagRequest = tagRequest = NULL;
                continue;
            }

            // We found the request we were looking for. 
            //
            break;
        } 
        else 
        {
            // The request did not match our criteria. Get another request.
            //
            previousTagRequest = tagRequest;
            continue;
        }
    }
    while (TRUE);

    DMF_ModuleUnlock(dmfModule);

    return outRequest;
 }

BOOLEAN
Test_IoctlHandler_RequestCompare(
    __in WDFREQUEST Request,
    __in VOID* CallbackCompareContext
    )
{
    BOOLEAN returnValue;
    WDFREQUEST* lookForRequest;

    lookForRequest = (WDFREQUEST*)CallbackCompareContext;
    if (Request == *lookForRequest)
    {
        returnValue = TRUE;
    }
    else 
    {
        returnValue = FALSE;
    }

    return returnValue;
}

BOOLEAN
Test_IoctlHandler_SelectAll(
    __in WDFREQUEST Request,
    __in VOID* CallbackCompareContext
    )
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(CallbackCompareContext);

    return TRUE;
}

_Function_class_(EVT_DMF_BufferPool_TimerCallback)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Test_IoctlHandler_BufferPool_TimerCallback(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    SleepContext* sleepContext;
    WDFREQUEST request;

    UNREFERENCED_PARAMETER(ClientBufferContext);
    UNREFERENCED_PARAMETER(ClientDriverCallbackContext);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    sleepContext = (SleepContext*)ClientBuffer;

    request = Tests_IoctHandler_FindRequestWithMatchingData(DmfModule,
                                                            moduleContext->CancelableQueue,
                                                            Test_IoctlHandler_RequestCompare,
                                                            (VOID*)sleepContext);
    if (request == NULL)
    {
        // Request has been canceled or will be canceled soon.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WdfIoQueueFindRequest: pending or already canceled Request=0x%p TimeToSleepMilliseconds=%d", sleepContext->Request, sleepContext->SleepRequest.TimeToSleepMilliseconds);
        goto Exit;
    }

    // Cancel routine will not be called. Complete request now.
    //
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Complete: Request=0x%p TimeToSleepMilliseconds=%d", sleepContext->Request, sleepContext->SleepRequest.TimeToSleepMilliseconds);  
    WdfRequestComplete(sleepContext->Request,
                       STATUS_SUCCESS);

    // Reference count increased when it was put in list with timer.
    //
    WdfObjectDereference(sleepContext->Request);

Exit:

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolFree,
                        ClientBuffer);
}

// Context for passing to enumeration function.
//
typedef struct
{
    // The request to look for in the list.
    //
    WDFREQUEST Request;
    BOOLEAN Found;
} ENUMERATION_CONTEXT;

_Function_class_(EVT_DMF_BufferPool_Enumeration)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BufferPool_EnumerationDispositionType
Test_IoctlHandler_BufferPool_EnumerationToCancel(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    SleepContext* sleepContext;
    BufferPool_EnumerationDispositionType enumerationDispositionType;
    ENUMERATION_CONTEXT* enumerationContext;

    UNREFERENCED_PARAMETER(ClientBufferContext);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    sleepContext = (SleepContext*)ClientBuffer;
    enumerationContext = (ENUMERATION_CONTEXT*)ClientDriverCallbackContext;

    // 'Dereferencing NULL pointer. 'enumerationContext' contains the same NULL value as 'ClientDriverCallbackContext' did. '
    //
    #pragma warning(suppress:28182)
    if (sleepContext->Request == enumerationContext->Request)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "BufferPool=0x%p Test_IoctlHandler_BufferPool_EnumerationToCancel: found Request=0x%p (stop searching)", DmfModule, sleepContext->Request);
        enumerationContext->Found = TRUE;
        enumerationDispositionType = BufferPool_EnumerationDisposition_RemoveAndStopEnumeration;
    }
    else
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "BufferPool=0x%p Test_IoctlHandler_BufferPool_EnumerationToCancel: Request=0x%p not found. currentRequest=0x%p (keep searching)", DmfModule, enumerationContext->Request, sleepContext->Request);
        enumerationDispositionType = BufferPool_EnumerationDisposition_ContinueEnumeration;
    }

    return enumerationDispositionType;
}

_Function_class_(EVT_DMF_BufferPool_Enumeration)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BufferPool_EnumerationDispositionType
Test_IoctlHandler_BufferPool_EnumerationToDelete(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
{
    BufferPool_EnumerationDispositionType enumerationDispositionType;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientBufferContext);

    if (ClientBuffer == ClientDriverCallbackContext)
    {
        enumerationDispositionType = BufferPool_EnumerationDisposition_RemoveAndStopEnumeration;
    }
    else
    {
        enumerationDispositionType = BufferPool_EnumerationDisposition_ContinueEnumeration;
    }

    return enumerationDispositionType;
}

_Function_class_(EVT_WDF_IO_QUEUE_IO_CANCELED_ON_QUEUE)
VOID
Tests_IoctlHandler_CancelOnQueue(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request
    )
{
    REQUEST_CONTEXT* requestContext;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    SleepContext* sleepContext;
    ENUMERATION_CONTEXT enumerationContext;

    UNREFERENCED_PARAMETER(Queue);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Tests_IoctlHandler_RequestCancel: Request=0x%p", Request);

    requestContext = (REQUEST_CONTEXT*)WdfObjectGetTypedContext(Request,
                                                                REQUEST_CONTEXT);

    DmfAssert(requestContext->DmfModuleTestIoctlHandler != NULL);
    moduleContext = DMF_CONTEXT_GET(requestContext->DmfModuleTestIoctlHandler);
    
    RtlZeroMemory(&enumerationContext,
                  sizeof(enumerationContext));
    enumerationContext.Request = Request;

    // In case the request is in the list, remove its associated data from that list.
    //
    DMF_BufferPool_Enumerate(moduleContext->DmfModuleBufferPoolPending,
                             Test_IoctlHandler_BufferPool_EnumerationToCancel,
                             (VOID*)&enumerationContext,
                             (VOID**)&sleepContext,
                             NULL);

    if (enumerationContext.Found)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Tests_IoctlHandler_CancelOnQueue: Request=0x%p FOUND", Request);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Tests_IoctlHandler_CancelOnQueue: Request=0x%p NOT FOUND", Request);
    }

    WdfRequestComplete(Request,
                       STATUS_CANCELLED);
    // Reference count increased when it was put in list with timer.
    //
    WdfObjectDereference(Request);

    // This buffer may or may not have been removed by the timer callback.
    //
    if (sleepContext != NULL)
    {
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolFree,
                            sleepContext);
    }
}

NTSTATUS
Tests_IoctlHandler_Enqueue(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ VOID* ClientBuffer,
    _In_ ULONG TimeoutMs
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Tests_IoctlHandler_Enqueue: Request=0x%p TimeoutMs=%d", Request, TimeoutMs);

    WdfObjectReference(Request);
    DMF_BufferPool_PutInSinkWithTimer(moduleContext->DmfModuleBufferPoolPending,
                                      ClientBuffer,
                                      TimeoutMs,
                                      Test_IoctlHandler_BufferPool_TimerCallback,
                                      NULL);
    ntStatus = WdfRequestForwardToIoQueue(Request,
                                          moduleContext->CancelableQueue);
    if (NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "WdfRequestForwardToIoQueue success: Request=0x%p", Request);
        ntStatus = STATUS_PENDING;
    }
    else
    {
        // It will not be pended so remove the entry added just above.
        //
        void* returnedClientBuffer;
        DMF_BufferPool_Enumerate(moduleContext->DmfModuleBufferPoolPending,
                                 Test_IoctlHandler_BufferPool_EnumerationToDelete,
                                 ClientBuffer,
                                 &returnedClientBuffer,
                                 NULL);
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolFree,
                           ClientBuffer);
        WdfObjectDereference(Request);
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "WdfRequestForwardToIoQueue fails: Request=0x%p ntStatus=%!STATUS!", Request, ntStatus);
    }

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

NTSTATUS
Tests_IoctlHandler_Callback(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    NTSTATUS ntStatus;
    VOID* clientBuffer;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferSize);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);
    ntStatus = STATUS_NOT_SUPPORTED;
    *BytesReturned = 0;

    switch(IoControlCode) 
    {
        case IOCTL_Tests_IoctlHandler_SLEEP:
        {
            Tests_IoctlHandler_Sleep* sleepRequestBuffer;
            SleepContext* sleepContext;
            WDF_OBJECT_ATTRIBUTES objectAttributes;
            REQUEST_CONTEXT* requestContext;

            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IOCTL_Tests_IoctlHandler_SLEEP: Request=0x%p", Request);

            WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes,
                                                    REQUEST_CONTEXT);
            ntStatus = WdfObjectAllocateContext(Request,
                                                &objectAttributes,
                                                (VOID**)&requestContext);
            if (!NT_SUCCESS(ntStatus))
            {
                goto Exit;
            }

            sleepRequestBuffer = (Tests_IoctlHandler_Sleep*)InputBuffer;

            // Save the Module in private context for cancel routine.
            // It is necessary so that it can be removed from lists.
            //
            requestContext->DmfModuleTestIoctlHandler = dmfModuleParent;
            requestContext->TimeToSleepMilliseconds = sleepRequestBuffer->TimeToSleepMilliseconds;

            ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolFree,
                                          &clientBuffer,
                                          NULL);
            DmfAssert(NT_SUCCESS(ntStatus));

            sleepContext = (SleepContext*)clientBuffer;
            sleepContext->Request = Request;

            RtlCopyMemory(&sleepContext->SleepRequest,
                          sleepRequestBuffer,
                          sizeof(Tests_IoctlHandler_Sleep));

            ntStatus = Tests_IoctlHandler_Enqueue(dmfModuleParent,
                                                  Request,
                                                  clientBuffer,
                                                  sleepRequestBuffer->TimeToSleepMilliseconds);
            break;
        }
        case IOCTL_Tests_IoctlHandler_ZEROBUFFER:
        {
            SleepContext* sleepContext;
            WDF_OBJECT_ATTRIBUTES objectAttributes;
            REQUEST_CONTEXT* requestContext;

            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "IOCTL_Tests_IoctlHandler_ZEROBUFFER: Request=0x%p", Request);

            // Perform the work.
            //
            RtlZeroMemory(OutputBuffer,
                          OutputBufferSize);
            WdfRequestSetInformation(Request,
                                     OutputBufferSize);

            WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes,
                                                    REQUEST_CONTEXT);
            ntStatus = WdfObjectAllocateContext(Request,
                                                &objectAttributes,
                                                (VOID**)&requestContext);
            if (!NT_SUCCESS(ntStatus))
            {
                goto Exit;
            }

            // Save the Module in private context for cancel routine.
            // It is necessary so that it can be removed from lists.
            //
            requestContext->DmfModuleTestIoctlHandler = dmfModuleParent;

            ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolFree,
                                          &clientBuffer,
                                          NULL);
            DmfAssert(NT_SUCCESS(ntStatus));

            sleepContext = (SleepContext*)clientBuffer;
            sleepContext->Request = Request;
            sleepContext->SleepRequest.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0,
                                                                                                   5000);
            requestContext->TimeToSleepMilliseconds = sleepContext->SleepRequest.TimeToSleepMilliseconds;
            ntStatus = Tests_IoctlHandler_Enqueue(dmfModuleParent,
                                                  Request,
                                                  clientBuffer,
                                                  sleepContext->SleepRequest.TimeToSleepMilliseconds);
            break;
        }
        case IOCTL_Tests_IoctlHandler_ZEROSIZE:
        {
            SleepContext* sleepContext;
            WDF_OBJECT_ATTRIBUTES objectAttributes;
            REQUEST_CONTEXT* requestContext;

            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "IOCTL_Tests_IoctlHandler_ZEROSIZE: Request=0x%p", Request);

            // Perform the work.
            //
            WdfRequestSetInformation(Request,
                                     OutputBufferSize);

            WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes,
                                                    REQUEST_CONTEXT);
            ntStatus = WdfObjectAllocateContext(Request,
                                                &objectAttributes,
                                                (VOID**)&requestContext);
            if (!NT_SUCCESS(ntStatus))
            {
                goto Exit;
            }

            // Save the Module in private context for cancel routine.
            // It is necessary so that it can be removed from lists.
            //
            requestContext->DmfModuleTestIoctlHandler = dmfModuleParent;

            ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolFree,
                                          &clientBuffer,
                                          NULL);
            DmfAssert(NT_SUCCESS(ntStatus));

            sleepContext = (SleepContext*)clientBuffer;
            sleepContext->Request = Request;
            sleepContext->SleepRequest.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0,
                                                                                                   5000);
            requestContext->TimeToSleepMilliseconds = sleepContext->SleepRequest.TimeToSleepMilliseconds;
            ntStatus = Tests_IoctlHandler_Enqueue(dmfModuleParent,
                                                  Request,
                                                  clientBuffer,
                                                  sleepContext->SleepRequest.TimeToSleepMilliseconds);
            break;
        }
    }

Exit:

    return ntStatus;
}

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_IoctlHandler_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    ULONG timeToWaitMilliseconds;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Pick a random time that interface will be disabled.
    //
    timeToWaitMilliseconds = TestsUtility_GenerateRandomNumber(1000,
                                                               5000);

    // Disable interface.
    //
    DMF_IoctlHandler_IoctlStateSet(moduleContext->DmfModuleIoctlHandler,
                                   FALSE);

    // Wait for a while.
    //
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep,
                                        0,
                                        timeToWaitMilliseconds);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Enable again.
    //
    DMF_IoctlHandler_IoctlStateSet(moduleContext->DmfModuleIoctlHandler,
                                   TRUE);

    // Wait for a while.
    //
    timeToWaitMilliseconds = TestsUtility_GenerateRandomNumber(5000,
                                                               30000);
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep,
                                        0,
                                        timeToWaitMilliseconds);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Repeat the test, until stop is signaled.
    //
    if (!DMF_Thread_IsStopPending(DmfModuleThread))
    {
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();

Exit:
    ;
}
#pragma code_seg()

VOID
Tests_IoctlHandler_InterfaceReference(
    _In_ VOID* DmfModuleVoid
    )
/*++

Routine Description:

Routine Description:

    Reference the interface. Module will not close until count is zero.

Arguments:

    DmfModuleVoid - This Module's handle.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;

    dmfModule = DMFMODULEVOID_TO_MODULE(DmfModuleVoid);
    DMF_ModuleReference(dmfModule);
}

VOID
Tests_IoctlHandler_InterfaceDereference(
    _In_ VOID* DmfModuleVoid
    )
/*++

Routine Description:

    Dereference the interface. Module will not close until count is zero.

Arguments:

    DmfModuleVoid - This Module's handle.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;

    dmfModule = DMFMODULEVOID_TO_MODULE(DmfModuleVoid);
    DMF_ModuleDereference(dmfModule);
}

BOOLEAN
Tests_IoctlHandler_Get(
    _In_ VOID* DmfModuleVoid,
    _Out_ UCHAR* Value
    )
/*++

Routine Description:

    Gets the interface value from Module Context.

Arguments:

    DmfModule - This Module's handle.
    Value - Value from Module Context.

Return Value:

    TRUE

--*/
{
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    DMFMODULE dmfModule;

    dmfModule = DMFMODULEVOID_TO_MODULE(DmfModuleVoid);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    *Value = moduleContext->InterfaceValue;

    return TRUE;
}

BOOLEAN
Tests_IoctlHandler_Set(
    _In_ VOID* DmfModuleVoid,
    _In_ UCHAR Value
    )
/*++

Routine Description:

    Sets the interface value in Module Context.

Arguments:

    DmfModule - This Module's handle.
    Value - Value to set in Module Context.

Return Value:

    TRUE

--*/
{
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    DMFMODULE dmfModule;

    dmfModule = DMFMODULEVOID_TO_MODULE(DmfModuleVoid);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    moduleContext->InterfaceValue = Value;

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

IoctlHandler_IoctlRecord Tests_IoctlHandlerTable[] =
{
    { (LONG)IOCTL_Tests_IoctlHandler_SLEEP,         sizeof(Tests_IoctlHandler_Sleep), 0, Tests_IoctlHandler_Callback, FALSE },
    { (LONG)IOCTL_Tests_IoctlHandler_ZEROBUFFER,    0,                                0, Tests_IoctlHandler_Callback, FALSE },
    { (LONG)IOCTL_Tests_IoctlHandler_ZEROSIZE,      0,                                0, Tests_IoctlHandler_Callback, FALSE },
};

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_IoctlHandler_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;
    DMF_CONFIG_BufferPool moduleConfigBufferPool;
    DMF_CONFIG_Tests_IoctlHandler* moduleConfig;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // BufferPool Source
    // -----------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = sizeof(SleepContext);
    moduleConfigBufferPool.Mode.SourceSettings.CreateWithTimer = TRUE;
#if defined(DMF_KERNEL_MODE)
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = 32;
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
#else
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = 512;
    // Lookaside is not supported in User-mode.
    //
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = FALSE;
#endif
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolFree);

    // BufferPool Sink
    // ---------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Sink;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolPending);

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.IoctlRecords = Tests_IoctlHandlerTable;
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(Tests_IoctlHandlerTable);
    if (moduleConfig->CreateDeviceInterface)
    {
        moduleConfigIoctlHandler.DeviceInterfaceGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    }
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &moduleContext->DmfModuleIoctlHandler);

    // TODO: Add second instance for Internal IOCTL.
    //

    // Thread
    // ------
    //
    DMF_CONFIG_Thread moduleConfigThread;
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_IoctlHandler_WorkThread;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThread);

    // AlertableSleep Manual (Output)
    // ---------------------
    //
    DMF_CONFIG_AlertableSleep moduleConfigAlertableSleep;
    DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                  &moduleAttributes);
    moduleConfigAlertableSleep.EventCount = 1;
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleAlertableSleep);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Tests_IoctlHandler_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Tests_IoctlHandler.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_IO_QUEUE_CONFIG ioQueueConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    WDFDEVICE device;
    WDFQUEUE queue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig,
                             WdfIoQueueDispatchManual);
    ioQueueConfig.EvtIoCanceledOnQueue = Tests_IoctlHandler_CancelOnQueue;
    ntStatus = WdfIoQueueCreate(device,
                                &ioQueueConfig,
                                WDF_NO_OBJECT_ATTRIBUTES,
                                &moduleContext->CancelableQueue);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoQueueCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

#if defined(DMF_KERNEL_MODE)
    // Create an interface that allows Client to call directly into this driver.
    // This allows the sample to show how to use DMF_ModuleReference()/DMF_ModuleDereference()
    // with such an interface. Drivers access this interface using IRP_MN_QUERY_INTERFACE.
    //
    WDF_QUERY_INTERFACE_CONFIG  queryInterrfaceConfig;
    Tests_IoctlHandler_INTERFACE_STANDARD  testInterfaceStandard;

    RtlZeroMemory(&testInterfaceStandard,
                  sizeof(testInterfaceStandard));

    testInterfaceStandard.InterfaceHeader.Size = sizeof(testInterfaceStandard);
    testInterfaceStandard.InterfaceHeader.Version = 1;
    testInterfaceStandard.InterfaceHeader.Context = (VOID*)DmfModule;

    testInterfaceStandard.InterfaceHeader.InterfaceReference =  Tests_IoctlHandler_InterfaceReference;
    testInterfaceStandard.InterfaceHeader.InterfaceDereference = Tests_IoctlHandler_InterfaceDereference;

    testInterfaceStandard.InterfaceValueGet = Tests_IoctlHandler_Get;
    testInterfaceStandard.InterfaceValueSet = Tests_IoctlHandler_Set;

    WDF_QUERY_INTERFACE_CONFIG_INIT(&queryInterrfaceConfig,
                                    (PINTERFACE) &testInterfaceStandard,
                                    &GUID_Tests_IoctlHandler_INTERFACE_STANDARD,
                                    NULL);
    ntStatus = WdfDeviceAddQueryInterface(device,
                                          &queryInterrfaceConfig);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }
#endif // defined(DMF_KERNEL_MODE)

    queue = WdfDeviceGetDefaultQueue(device);
    WdfIoQueueStart(queue);
    WdfIoQueueStart(moduleContext->CancelableQueue);

#if !defined(DISABLE_INTERFACE_THREAD)
    // Start the thread that disables/enables device interface.
    //
    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    DMF_Thread_WorkReady(moduleContext->DmfModuleThread);
#endif

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
DMF_Tests_IoctlHandler_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_IoctlHandler.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    WDFDEVICE device;
    WDFQUEUE queue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleep,
                             0);
#if !defined(DISABLE_INTERFACE_THREAD)
    DMF_Thread_Stop(moduleContext->DmfModuleThread);
#endif

    device = DMF_ParentDeviceGet(DmfModule);
    queue = WdfDeviceGetDefaultQueue(device);

    WdfIoQueuePurge(queue,
                    NULL,
                    NULL);
    WdfIoQueuePurge(moduleContext->CancelableQueue,
                    NULL,
                    NULL);

    WdfObjectDelete(moduleContext->CancelableQueue);

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
DMF_Tests_IoctlHandler_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_IoctlHandler.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_IoctlHandler;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_IoctlHandler;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_IoctlHandler);
    dmfCallbacksDmf_Tests_IoctlHandler.ChildModulesAdd = DMF_Tests_IoctlHandler_ChildModulesAdd;
    dmfCallbacksDmf_Tests_IoctlHandler.DeviceOpen = DMF_Tests_IoctlHandler_Open;
    dmfCallbacksDmf_Tests_IoctlHandler.DeviceClose = DMF_Tests_IoctlHandler_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_IoctlHandler,
                                            Tests_IoctlHandler,
                                            DMF_CONTEXT_Tests_IoctlHandler,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_Tests_IoctlHandler.CallbacksDmf = &dmfCallbacksDmf_Tests_IoctlHandler;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_IoctlHandler,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_Tests_IoctlHandler.c
//
