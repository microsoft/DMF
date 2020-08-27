/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_RequestTarget.c

Abstract:

    Support for sending IOCTLS and Read/Write requests to an IOTARGET.

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
#include "Dmf_RequestTarget.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define DEFAULT_NUMBER_OF_PENDING_PASSIVE_LEVEL_COMPLETION_ROUTINES 4

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_RequestTarget
{
    // Context Buffer List.
    //
    DMFMODULE DmfModuleBufferPoolContext;
    // Queued workitem for passive level completion routine.
    //
    DMFMODULE DmfModuleQueuedWorkitemSingle;
    // IO Target to Send Requests to.
    //
    WDFIOTARGET IoTarget;
    // Pending asynchronous requests.
    //
    WDFCOLLECTION PendingAsynchronousRequests;
} DMF_CONTEXT_RequestTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(RequestTarget)

DMF_MODULE_DECLARE_NO_CONFIG(RequestTarget)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// WDFREQUEST handles have the potential for being reused depending on the allocation
// strategy used by WDF. To prevent that from being a problem this globally unique 
// counter is used. Only a single counter should be used per driver since WDFREQUESTS
// potentially come from that same pool for all instances of all Modules.
//
extern LONGLONG g_ContinuousRequestTargetUniqueId;

typedef struct
{
    LONGLONG UniqueRequestId;
} UNIQUE_REQUEST;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(UNIQUE_REQUEST, UniqueRequestContextGet)

typedef struct
{
    DMFMODULE DmfModule;
    ContinuousRequestTarget_RequestType SingleAsynchronousRequestType;
    EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest;
    VOID* SingleAsynchronousCallbackClientContext;
} RequestTarget_SingleAsynchronousRequestContext;

typedef struct
{
    WDFREQUEST Request;
    WDF_REQUEST_COMPLETION_PARAMS CompletionParams;
    RequestTarget_SingleAsynchronousRequestContext* SingleAsynchronousRequestContext;
} RequestTarget_QueuedWorkitemContext;

static
VOID
RequestTarget_CompletionParamsInputBufferAndOutputBufferGet(
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _Out_ VOID** InputBuffer,
    _Out_ size_t* InputBufferSize,
    _Out_ VOID** OutputBuffer,
    _Out_ size_t* OutputBufferSize
    )
/*++

Routine Description:

    This routine is called in Completion routine of Asynchronous requests. It returns the
    right input buffer and output buffer pointers based on the Request type (Read/Write/Ioctl)
    specified in Module Config. It also returns the input and output buffer sizes

Arguments:

    DmfModule - This Module's handle.
    CompletionParams - Information about the completion.
    RequestType - Type of request.
    InputBuffer - Pointer to Input buffer.
    InputBufferSize - Size of Input buffer.
    OutputBuffer - Pointer to Output buffer.
    OutputBufferSize - Size of Output buffer.

Return Value:

    None

--*/
{
    WDFMEMORY inputMemory;
    WDFMEMORY outputMemory;

    FuncEntry(DMF_TRACE);

    *InputBufferSize = 0;
    *InputBuffer = NULL;

    *OutputBufferSize = 0;
    *OutputBuffer = NULL;

    switch (RequestType)
    {
        case ContinuousRequestTarget_RequestType_Read:
        {
            // Get the read buffer memory handle.
            //
            *OutputBufferSize = CompletionParams->Parameters.Read.Length;
            outputMemory = CompletionParams->Parameters.Read.Buffer;
            // Get the read buffer.
            //
            if (outputMemory != NULL)
            {
                *OutputBuffer = WdfMemoryGetBuffer(outputMemory,
                                                   NULL);
                DmfAssert(*OutputBuffer != NULL);
            }
            break;
        }
        case ContinuousRequestTarget_RequestType_Write:
        {
            // Get the write buffer memory handle.
            //
            *InputBufferSize = CompletionParams->Parameters.Write.Length;
            inputMemory = CompletionParams->Parameters.Write.Buffer;
            // Get the write buffer.
            //
            if (inputMemory != NULL)
            {
                *InputBuffer = WdfMemoryGetBuffer(inputMemory,
                                                  NULL);
                DmfAssert(*InputBuffer != NULL);
            }
            break;
        }
        case ContinuousRequestTarget_RequestType_Ioctl:
        case ContinuousRequestTarget_RequestType_InternalIoctl:
        {
            // Get the input and output buffers' memory handles.
            //
            inputMemory = CompletionParams->Parameters.Ioctl.Input.Buffer;
            outputMemory = CompletionParams->Parameters.Ioctl.Output.Buffer;
            // Get the input and output buffers.
            //
            if (inputMemory != NULL)
            {
                *InputBuffer = WdfMemoryGetBuffer(inputMemory,
                                                  InputBufferSize);
                DmfAssert(*InputBuffer != NULL);
            }
            if (outputMemory != NULL)
            {
                *OutputBuffer = WdfMemoryGetBuffer(outputMemory,
                                                   OutputBufferSize);
                DmfAssert(*OutputBufferSize >= CompletionParams->Parameters.Ioctl.Output.Length);
                *OutputBufferSize = CompletionParams->Parameters.Ioctl.Output.Length;
                DmfAssert(*OutputBuffer != NULL);
            }
            break;
        }
        default:
        {
            DmfAssert(FALSE);
        }
    }
}

static
NTSTATUS
RequestTarget_PendingCollectionListAdd(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Add the given WDFREQUEST to the list of pending asynchronous requests.

Arguments:

    DmfModule - This Module's handle.
    Request - The given request.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_RequestTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);
    ntStatus = WdfCollectionAdd(moduleContext->PendingAsynchronousRequests,
                                Request);
    DMF_ModuleUnlock(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCollectionAdd fails: ntStatus=%!STATUS!", ntStatus);
    }

    return ntStatus;
}

static
BOOLEAN 
RequestTarget_PendingCollectionListSearchAndRemove(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    If the given WDFREQUEST is in the pending asynchronous request list, remove it.

Arguments:

    DmfModule - This Module's handle.
    Request - The given request.

Return Value:

    TRUE if the WDFREQUEST was found and removed.
    FALSE if the given WDFREQUEST was not found.

--*/
{
    DMF_CONTEXT_RequestTarget* moduleContext;
    WDFREQUEST currentRequestFromList;
    ULONG currentItemIndex;
    BOOLEAN returnValue;

    returnValue = FALSE;
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // In case Client sends a NULL, don't try to remove the first request if there are
    // no requests.
    //
    if (NULL == Request)
    {
        DmfAssert(FALSE);
        goto Exit;
    }

    DMF_ModuleLock(DmfModule);
    
    currentItemIndex = 0;
    do 
    {
        currentRequestFromList = (WDFREQUEST)WdfCollectionGetItem(moduleContext->PendingAsynchronousRequests,
                                                                  currentItemIndex);
        if (currentRequestFromList == Request)
        {
            WdfCollectionRemoveItem(moduleContext->PendingAsynchronousRequests,
                                    currentItemIndex);
            returnValue = TRUE;
            break;
        }
        currentItemIndex++;
    } while (currentRequestFromList != NULL);
    
    DMF_ModuleUnlock(DmfModule);

Exit:

    return returnValue;
}

static
BOOLEAN 
RequestTarget_PendingCollectionListSearchAndReference(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequest UniqueRequestId,
    _Out_ WDFREQUEST* RequestToCancel
    )
/*++

Routine Description:

    If the given UniqueRequestId is in the pending asynchronous request list, add a reference to it.

Arguments:

    DmfModule - This Module's handle.
    UniqueRequestId - The given unique request id.

    NOTE: DmfRequestId is an ever increasing integer, so it is always safe to use as
          a comparison value in the list.

Return Value:

    TRUE if the UniqueRequestId was found and a reference added to it.
    FALSE if the given UniqueRequestId was not found in the list or is invalid.

--*/
{
    DMF_CONTEXT_RequestTarget* moduleContext;
    WDFREQUEST currentRequestFromList;
    ULONG currentItemIndex;
    BOOLEAN returnValue;

    *RequestToCancel = NULL;
    returnValue = FALSE;
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);
    
    currentItemIndex = 0;
    do 
    {
        currentRequestFromList = (WDFREQUEST)WdfCollectionGetItem(moduleContext->PendingAsynchronousRequests,
                                                                  currentItemIndex);
        if (NULL == currentRequestFromList)
        {
            // No request left in the list.
            //
            break;
        }

        UNIQUE_REQUEST* uniqueRequestId = UniqueRequestContextGet(currentRequestFromList);

        if (uniqueRequestId->UniqueRequestId == (LONGLONG)UniqueRequestId)
        {
            // Acquire a reference to the request so that if its completion routine
            // happens just after the unlock before the caller can cancel the request
            // the caller can still cancel the request safely.
            //
            WdfObjectReferenceWithTag(currentRequestFromList,
                                      (VOID*)DmfModule);
            *RequestToCancel = currentRequestFromList;
            returnValue = TRUE;
            break;
        }
        currentItemIndex++;
    } while (currentRequestFromList != NULL);
    
    DMF_ModuleUnlock(DmfModule);

    return returnValue;
}

VOID
RequestTarget_ProcessAsynchronousRequestSingle(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ RequestTarget_SingleAsynchronousRequestContext* SingleAsynchronousRequestContext
    )
/*++

Routine Description:

    This routine does all the work to extract the buffers that are returned from underlying target.
    Then it calls the Client's Output Buffer callback function with the buffers.

Arguments:

    DmfModule - The given Dmf Module.
    Request - The completed request.
    CompletionParams - Information about the completion.
    SingleAsynchronousRequestContext - Single asynchronous request context.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    VOID* inputBuffer;
    size_t inputBufferSize;
    VOID* outputBuffer;
    size_t outputBufferSize;
    DMF_CONTEXT_RequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    inputBuffer = NULL;
    outputBuffer = NULL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Request may or may not be in this list. Remove it if it is.
    // Caller may have removed it by calling the cancel Method.
    //
    RequestTarget_PendingCollectionListSearchAndRemove(DmfModule,
                                                       Request);

    ntStatus = WdfRequestGetStatus(Request);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestGetStatus Request=0x%p fails: ntStatus=%!STATUS!", Request, ntStatus);
    }

    // Get information about the request completion.
    //
    WdfRequestGetCompletionParams(Request,
                                  CompletionParams);

    // Get the input and output buffers.
    // Input buffer will be NULL for request type read and write.
    //
    RequestTarget_CompletionParamsInputBufferAndOutputBufferGet(CompletionParams,
                                                                SingleAsynchronousRequestContext->SingleAsynchronousRequestType,
                                                                &inputBuffer,
                                                                &inputBufferSize,
                                                                &outputBuffer,
                                                                &outputBufferSize);

    // Call the Client's callback function.
    //
    if (SingleAsynchronousRequestContext->EvtRequestTargetSingleAsynchronousRequest != NULL)
    {
        (SingleAsynchronousRequestContext->EvtRequestTargetSingleAsynchronousRequest)(DmfModule,
                                                                                      SingleAsynchronousRequestContext->SingleAsynchronousCallbackClientContext,
                                                                                      inputBuffer,
                                                                                      inputBufferSize,
                                                                                      outputBuffer,
                                                                                      outputBufferSize,
                                                                                      ntStatus);
    }

    // Put the single buffer back to single buffer list.
    //
    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolContext,
                       SingleAsynchronousRequestContext);

    WdfObjectDelete(Request);

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_REQUEST_COMPLETION_ROUTINE RequestTarget_CompletionRoutine;

_Function_class_(EVT_WDF_REQUEST_COMPLETION_ROUTINE)
_IRQL_requires_same_
VOID
RequestTarget_CompletionRoutine(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ WDFCONTEXT Context
    )
/*++

Routine Description:

    It is the completion routine for the Single Asynchronous requests. This routine does all the work
    to extract the buffers that are returned from underlying target. Then it calls the Client's
    Output Buffer callback function with the buffers.

Arguments:

    Request - The completed request.
    Target - The Io Target that completed the request.
    CompletionParams - Information about the completion.
    Context - This Module's handle.

Return Value:

    None

--*/
{
    RequestTarget_SingleAsynchronousRequestContext* singleAsynchronousRequestContext;
    DMFMODULE dmfModule;

    UNREFERENCED_PARAMETER(Target);

    FuncEntry(DMF_TRACE);

    singleAsynchronousRequestContext = (RequestTarget_SingleAsynchronousRequestContext*)Context;
    DmfAssert(singleAsynchronousRequestContext != NULL);

    dmfModule = singleAsynchronousRequestContext->DmfModule;
    DmfAssert(dmfModule != NULL);

    RequestTarget_ProcessAsynchronousRequestSingle(dmfModule,
                                                   Request,
                                                   CompletionParams,
                                                   singleAsynchronousRequestContext);

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_REQUEST_COMPLETION_ROUTINE RequestTarget_CompletionRoutinePassive;

_Function_class_(EVT_WDF_REQUEST_COMPLETION_ROUTINE)
_IRQL_requires_same_
VOID
RequestTarget_CompletionRoutinePassive(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ WDFCONTEXT Context
)
/*++

Routine Description:

    It is the completion routine for the Single Asynchronous requests. This routine does all the work
    to extract the buffers that are returned from underlying target. Then it calls the Client's
    Output Buffer callback function with the buffers.

Arguments:

    Request - The completed request.
    Target - The Io Target that completed the request.
    CompletionParams - Information about the completion.
    Context - This Module's handle.

Return Value:

    None

--*/
{
    RequestTarget_SingleAsynchronousRequestContext* singleAsynchronousRequestContext;
    DMFMODULE dmfModule;
    DMF_CONTEXT_RequestTarget* moduleContext;
    RequestTarget_QueuedWorkitemContext workitemContext;

    UNREFERENCED_PARAMETER(Target);

    FuncEntry(DMF_TRACE);

    singleAsynchronousRequestContext = (RequestTarget_SingleAsynchronousRequestContext*)Context;
    DmfAssert(singleAsynchronousRequestContext != NULL);

    dmfModule = singleAsynchronousRequestContext->DmfModule;
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    workitemContext.Request = Request;
    workitemContext.CompletionParams = *CompletionParams;
    workitemContext.SingleAsynchronousRequestContext = singleAsynchronousRequestContext;

    DMF_QueuedWorkItem_Enqueue(moduleContext->DmfModuleQueuedWorkitemSingle,
                               (VOID*)&workitemContext,
                               sizeof(RequestTarget_QueuedWorkitemContext));

    FuncExitVoid(DMF_TRACE);
}

static
NTSTATUS
RequestTarget_FormatRequestForRequestType(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST   Request,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctlCode,
    _In_opt_ WDFMEMORY InputMemory,
    _In_opt_ WDFMEMORY OutputMemory
    )
/*++

Routine Description:

    Format the Request based on Request Type specified in Module Config.

Arguments:

    DmfModule - This Module's handle.
    Request - The request to format.
    RequestIoctlCode - IOCTL code for Request type RequestTarget_RequestType_Ioctl or RequestTarget_RequestType_InternalIoctl
    InputMemory - Handle to framework memory object which contains input data
    OutputMemory - Handle to framework memory object to receive output data

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_RequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Prepare the request to be sent down.
    //
    DmfAssert(moduleContext->IoTarget != NULL);
    switch (RequestType)
    {
        case ContinuousRequestTarget_RequestType_Write:
        {
            ntStatus = WdfIoTargetFormatRequestForWrite(moduleContext->IoTarget,
                                                        Request,
                                                        InputMemory,
                                                        NULL,
                                                        NULL);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetFormatRequestForWrite fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
            break;
        }
        case ContinuousRequestTarget_RequestType_Read:
        {
            ntStatus = WdfIoTargetFormatRequestForRead(moduleContext->IoTarget,
                                                       Request,
                                                       OutputMemory,
                                                       NULL,
                                                       NULL);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetFormatRequestForRead fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
            break;
        }
        case ContinuousRequestTarget_RequestType_Ioctl:
        {

            ntStatus = WdfIoTargetFormatRequestForIoctl(moduleContext->IoTarget,
                                                        Request,
                                                        RequestIoctlCode,
                                                        InputMemory,
                                                        NULL,
                                                        OutputMemory,
                                                        NULL);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetFormatRequestForIoctl fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
            break;
        }
#if !defined(DMF_USER_MODE)
        case ContinuousRequestTarget_RequestType_InternalIoctl:
        {
            ntStatus = WdfIoTargetFormatRequestForInternalIoctl(moduleContext->IoTarget,
                                                                Request,
                                                                RequestIoctlCode,
                                                                InputMemory,
                                                                NULL,
                                                                OutputMemory,
                                                                NULL);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetFormatRequestForInternalIoctl fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
            break;
        }
#endif // !defined(DMF_USER_MODE)
        default:
        {
            ntStatus = STATUS_INVALID_PARAMETER;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid RequestType:%d fails: ntStatus=%!STATUS!", RequestType, ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma warning(suppress:6101)
// Prevent SAL "returning uninitialized memory" error.
// Buffer is associated with request object.
//
static
NTSTATUS
RequestTarget_RequestCreateAndSend(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN IsSynchronousRequest,
    _In_reads_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_ ContinuousRequestTarget_CompletionOptions CompletionOption,
    _Out_opt_ size_t* BytesWritten,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequest* DmfRequestId
    )
/*++

Routine Description:

    Creates and sends a synchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    CompletionOption - Completion option associated with the completion routine. 
    BytesWritten - Bytes returned by the transaction.
    EvtContinuousRequestTargetSingleAsynchronousRequest - Completion routine. 
    SingleAsynchronousRequestClientContext - Client context returned in completion routine. 
    DmfRequestId - Contains a unique request Id that is sent back by the Client to cancel the asynchronous transaction.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    WDFREQUEST request;
    WDFMEMORY memoryForRequest;
    WDFMEMORY memoryForResponse;
    WDF_OBJECT_ATTRIBUTES requestAttributes;
    WDF_OBJECT_ATTRIBUTES memoryAttributes;
    WDF_REQUEST_SEND_OPTIONS sendOptions;
    size_t outputBufferSize;
    BOOLEAN requestSendResult;
    DMF_CONTEXT_RequestTarget* moduleContext;
    WDFDEVICE device;
    EVT_WDF_REQUEST_COMPLETION_ROUTINE* completionRoutineSingle;
    RequestTarget_SingleAsynchronousRequestContext* singleAsynchronousRequestContext;
    VOID* singleBufferContext;
    RequestTarget_DmfRequest dmfRequestId;

    FuncEntry(DMF_TRACE);

    outputBufferSize = 0;
    requestSendResult = FALSE;

    DmfAssert((IsSynchronousRequest && (EvtRequestTargetSingleAsynchronousRequest == NULL)) ||
              (! IsSynchronousRequest));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    device = DMF_ParentDeviceGet(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&requestAttributes,
                                            UNIQUE_REQUEST);
    requestAttributes.ParentObject = DmfModule;
    request = NULL;
    ntStatus = WdfRequestCreate(&requestAttributes,
                                moduleContext->IoTarget,
                                &request);
    if (! NT_SUCCESS(ntStatus))
    {
        request = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestCreate fails: ntStatus=%!STATUS!", ntStatus);
        return ntStatus;
    }

    UNIQUE_REQUEST* uniqueRequestId = UniqueRequestContextGet(request);
    dmfRequestId = 0;

    WDF_OBJECT_ATTRIBUTES_INIT(&memoryAttributes);
    memoryAttributes.ParentObject = request;

    memoryForRequest = NULL;
    if (RequestLength > 0)
    {
        DmfAssert(RequestBuffer != NULL);
        ntStatus = WdfMemoryCreatePreallocated(&memoryAttributes,
                                               RequestBuffer,
                                               RequestLength,
                                               &memoryForRequest);
        if (! NT_SUCCESS(ntStatus))
        {
            memoryForRequest = NULL;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    memoryForResponse = NULL;
    if (ResponseLength > 0)
    {
        DmfAssert(ResponseBuffer != NULL);
        ntStatus = WdfMemoryCreatePreallocated(&memoryAttributes,
                                               ResponseBuffer,
                                               ResponseLength,
                                               &memoryForResponse);
        if (! NT_SUCCESS(ntStatus))
        {
            memoryForResponse = NULL;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate for position fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    ntStatus = RequestTarget_FormatRequestForRequestType(DmfModule,
                                                         request,
                                                         RequestType,
                                                         RequestIoctl,
                                                         memoryForRequest,
                                                         memoryForResponse);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RequestTarget_FormatRequestForRequestType fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (IsSynchronousRequest)
    {
        DmfAssert(NULL == DmfRequestId);
        WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                      WDF_REQUEST_SEND_OPTION_SYNCHRONOUS | WDF_REQUEST_SEND_OPTION_TIMEOUT);
    }
    else
    {
        WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                      WDF_REQUEST_SEND_OPTION_TIMEOUT);

        // Get a single buffer from the single buffer list.
        // NOTE: This is fast operation that involves only pointer manipulation unless the buffer list is empty
        // (which should not happen).
        //
        ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolContext,
                                      (VOID**)&singleAsynchronousRequestContext,
                                      &singleBufferContext);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferPool_GetWithMemory fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        if (CompletionOption == ContinuousRequestTarget_CompletionOptions_Default)
        {
            completionRoutineSingle = RequestTarget_CompletionRoutine;
        }
        else if (CompletionOption == ContinuousRequestTarget_CompletionOptions_Passive)
        {
            completionRoutineSingle = RequestTarget_CompletionRoutinePassive;
        }
        else
        {
            completionRoutineSingle = RequestTarget_CompletionRoutine;
            DmfAssert(FALSE);
        }

        singleAsynchronousRequestContext->DmfModule = DmfModule;
        singleAsynchronousRequestContext->SingleAsynchronousCallbackClientContext = SingleAsynchronousRequestClientContext;
        singleAsynchronousRequestContext->EvtRequestTargetSingleAsynchronousRequest = EvtRequestTargetSingleAsynchronousRequest;
        singleAsynchronousRequestContext->SingleAsynchronousRequestType = RequestType;

        // Set the completion routine to internal completion routine of this Module.
        //
        WdfRequestSetCompletionRoutine(request,
                                       completionRoutineSingle,
                                       singleAsynchronousRequestContext);

        // Add to list of pending requests so that when Client cancels the request it can be done safely
        // in case this Module has already deleted the request.
        //
        if (DmfRequestId != NULL)
        {
            // Generate and save a globally unique request id in the context so that the Module can guard
            // against requests that are assigned the same handle value.
            //
            uniqueRequestId->UniqueRequestId = InterlockedIncrement64(&g_ContinuousRequestTargetUniqueId);
            // Prepare to write to caller's return address when function succeeds.
            //
            dmfRequestId = (RequestTarget_DmfRequest)uniqueRequestId->UniqueRequestId;

            ntStatus = RequestTarget_PendingCollectionListAdd(DmfModule,
                                                              request);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RequestTarget_PendingCollectionListAdd fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }
    }

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                         WDF_REL_TIMEOUT_IN_MS(RequestTimeoutMilliseconds));

    ntStatus = WdfRequestAllocateTimer(request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestAllocateTimer fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    requestSendResult = WdfRequestSend(request,
                                       moduleContext->IoTarget,
                                       &sendOptions);

    if (! requestSendResult || IsSynchronousRequest)
    {
        if (DmfRequestId != NULL)
        {
            // Request is not pending, so remove it from the list.
            //
            RequestTarget_PendingCollectionListSearchAndRemove(DmfModule,
                                                               request);
        }

        ntStatus = WdfRequestGetStatus(request);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestGetStatus returned ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
        else
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "WdfRequestSend completed with ntStatus=%!STATUS!", ntStatus);
            outputBufferSize = WdfRequestGetInformation(request);
        }
    }
    else
    {
        if (DmfRequestId != NULL)
        {
            // Return an ever increasing number so that in case WDF allocates the same handle
            // in rapid succession cancellation still works. The Client cancels using this
            // number so that we are certain to cancel exactly the correct WDFREQUEST even
            // if there is a collision in the handle value.
            // (Do not access the request's context because the request may no longer exist,
            // so used value saved in local variable.)
            //
            *DmfRequestId = dmfRequestId;
        }
    }

Exit:

    if (BytesWritten != NULL)
    {
        *BytesWritten = outputBufferSize;
    }

    if (IsSynchronousRequest && request != NULL)
    {
        // Delete the request if its Synchronous.
        //
        WdfObjectDelete(request);
        request = NULL;
    }
    else if (! IsSynchronousRequest && ! NT_SUCCESS(ntStatus) && request != NULL)
    {
        // Delete the request if Asynchronous request failed.
        //
        WdfObjectDelete(request);
        request = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Function_class_(EVT_DMF_QueuedWorkItem_Callback)
ScheduledTask_Result_Type
RequestTarget_QueuedWorkitemCallbackSingle(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext
    )
/*++

Routine Description:

    This routine does the work of completion routine for single asynchronous request, at passive level.

Arguments:

    DmfModule - The QueuedWorkItem Dmf Module.
    ClientBuffer - The buffer that contains the context of work to be done.
    ClientBufferContext - Context associated with the buffer.

Return Value:

    None

--*/
{
    DMFMODULE dmfModuleParent;
    RequestTarget_QueuedWorkitemContext* workitemContext;

    UNREFERENCED_PARAMETER(ClientBufferContext);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    workitemContext = (RequestTarget_QueuedWorkitemContext*)ClientBuffer;

    RequestTarget_ProcessAsynchronousRequestSingle(dmfModuleParent,
                                                   workitemContext->Request,
                                                   &workitemContext->CompletionParams,
                                                   workitemContext->SingleAsynchronousRequestContext);

    return ScheduledTask_WorkResult_Success;
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
DMF_RequestTarget_ChildModulesAdd(
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
    DMF_CONTEXT_RequestTarget* moduleContext;
    DMF_CONFIG_BufferPool moduleConfigBufferPool;
    DMF_CONFIG_QueuedWorkItem moduleConfigQueuedWorkItem;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // BufferPoolContext
    // -----------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = 1;
    // NOTE: BufferPool context must always be NonPagedPool because it is accessed in the
    //       completion routine running at DISPATCH_LEVEL.
    //
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = sizeof(RequestTarget_SingleAsynchronousRequestContext);
    moduleAttributes.ClientModuleInstanceName = "BufferPoolContext";
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolContext);

    // QueuedWorkItemSingle
    // --------------------
    //
    DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT(&moduleConfigQueuedWorkItem,
                                                    &moduleAttributes);
    moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.BufferCount = DEFAULT_NUMBER_OF_PENDING_PASSIVE_LEVEL_COMPLETION_ROUTINES;
    moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.BufferSize = sizeof(RequestTarget_QueuedWorkitemContext);
    // This has to be NonPagedPoolNx because completion routine runs at dispatch level.
    //
    moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    moduleConfigQueuedWorkItem.EvtQueuedWorkitemFunction = RequestTarget_QueuedWorkitemCallbackSingle;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleQueuedWorkitemSingle);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_RequestTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type RequestTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_RequestTarget* moduleContext;
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfCollectionCreate(&objectAttributes,
                                   &moduleContext->PendingAsynchronousRequests);
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_RequestTarget_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type RequestTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_RequestTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // There should be no outstanding requests in the target.
    //
    ASSERT((moduleContext->PendingAsynchronousRequests == NULL) ||
            (WdfCollectionGetCount(moduleContext->PendingAsynchronousRequests)) == 0);

    if (moduleContext->PendingAsynchronousRequests != NULL)
    {
        WdfObjectDelete(moduleContext->PendingAsynchronousRequests);
        moduleContext->PendingAsynchronousRequests = NULL;
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
DMF_RequestTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type RequestTarget.

Arguments:

    Device - Client's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_RequestTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_RequestTarget;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_RequestTarget);
    dmfCallbacksDmf_RequestTarget.ChildModulesAdd = DMF_RequestTarget_ChildModulesAdd;
    dmfCallbacksDmf_RequestTarget.DeviceOpen = DMF_RequestTarget_Open;
    dmfCallbacksDmf_RequestTarget.DeviceClose = DMF_RequestTarget_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_RequestTarget,
                                            RequestTarget,
                                            DMF_CONTEXT_RequestTarget,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_RequestTarget.CallbacksDmf = &dmfCallbacksDmf_RequestTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_RequestTarget,
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
BOOLEAN
DMF_RequestTarget_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequest DmfRequestId
    )
/*++

Routine Description:

    Cancels a given WDFREQUEST associated with DmfRequestId.

Arguments:

    DmfModule - This Module's handle.
    DmfRequestId - The given DmfRequestId.

Return Value:

    TRUE if the given WDFREQUEST was has been canceled.
    FALSE if the given WDFREQUEST is not canceled because it has already been completed or deleted.

--*/
{
    BOOLEAN returnValue;
    WDFREQUEST requestToCancel;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 RequestTarget);

    // NOTE: DmfRequestId is an ever increasing integer, so it is always safe to use as
    //       a comparison value in the list.
    //
    returnValue = RequestTarget_PendingCollectionListSearchAndReference(DmfModule,
                                                                        DmfRequestId,
                                                                        &requestToCancel);
    if (returnValue)
    {
        // Even if the request has been canceled or completed after the above call
        // since a the above call acquired a reference count, it is still safe to try to cancel it.
        //
        returnValue = WdfRequestCancelSentRequest(requestToCancel);
        WdfObjectDereferenceWithTag(requestToCancel,
                                    (VOID*)DmfModule);
    }

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestTarget_IoTargetClear(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Clears the IoTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    VOID

--*/
{
    DMF_CONTEXT_RequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 RequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    
    // NOTE: Sometimes Close callbacks call this Method when the IoTarget
    //       is NULL because the underlying target did not asynchronously
    //       appear. Therefore, there is no assert for it (as there was before).
    //

    moduleContext->IoTarget = NULL;

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestTarget_IoTargetSet(
    _In_ DMFMODULE DmfModule,
    _In_ WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Set the IoTarget to Send Requests to.

Arguments:

    DmfModule - This Module's handle.
    IoTarget - IO Target to send requests to.

Return Value:

    VOID

--*/
{
    DMF_CONTEXT_RequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 RequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    DmfAssert(IoTarget != NULL);
    DmfAssert(moduleContext->IoTarget == NULL);

    moduleContext->IoTarget = IoTarget;

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    )
/*++

Routine Description:

    Creates and sends a Asynchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    EvtRequestTargetSingleAsynchronousRequest - Callback to be called in completion routine.
    SingleAsynchronousRequestClientContext - Client context sent in callback

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    ContinuousRequestTarget_CompletionOptions completionOption;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 RequestTarget);

    ntStatus = STATUS_SUCCESS;

    if (DMF_IsModulePassiveLevel(DmfModule))
    {
        completionOption = ContinuousRequestTarget_CompletionOptions_Passive;
    }
    else
    {
        completionOption = ContinuousRequestTarget_CompletionOptions_Dispatch;
    }

    ntStatus = RequestTarget_RequestCreateAndSend(DmfModule,
                                                  FALSE,
                                                  RequestBuffer,
                                                  RequestLength,
                                                  ResponseBuffer,
                                                  ResponseLength,
                                                  RequestType,
                                                  RequestIoctl,
                                                  RequestTimeoutMilliseconds,
                                                  completionOption,
                                                  NULL,
                                                  EvtRequestTargetSingleAsynchronousRequest,
                                                  SingleAsynchronousRequestClientContext,
                                                  NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RequestTarget_RequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequest* DmfRequestId
    )
/*++

Routine Description:

    Creates and sends a Asynchronous request to the IoTarget given a buffer, IOCTL and other information.
    And once the request is complete, EvtContinuousRequestTargetSingleAsynchronousRequest will be called at Passive level.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    EvtRequestTargetSingleAsynchronousRequest - Callback to be called in completion routine.
    SingleAsynchronousRequestClientContext - Client context sent in callback.
    DmfRequestId - Allows Client to retrieve asynchronous request for possible later cancellation.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    ContinuousRequestTarget_CompletionOptions completionOption;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 RequestTarget);

    if (DMF_IsModulePassiveLevel(DmfModule))
    {
        completionOption = ContinuousRequestTarget_CompletionOptions_Passive;
    }
    else
    {
        completionOption = ContinuousRequestTarget_CompletionOptions_Dispatch;
    }

    ntStatus = RequestTarget_RequestCreateAndSend(DmfModule,
                                                  FALSE,
                                                  RequestBuffer,
                                                  RequestLength,
                                                  ResponseBuffer,
                                                  ResponseLength,
                                                  RequestType,
                                                  RequestIoctl,
                                                  RequestTimeoutMilliseconds,
                                                  completionOption,
                                                  NULL,
                                                  EvtRequestTargetSingleAsynchronousRequest,
                                                  SingleAsynchronousRequestClientContext,
                                                  DmfRequestId);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RequestTarget_RequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _Out_opt_ size_t* BytesWritten
    )
/*++

Routine Description:

    Creates and sends a synchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    BytesWritten - Bytes returned by the transaction.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 RequestTarget);

    ntStatus = RequestTarget_RequestCreateAndSend(DmfModule,
                                                  TRUE,
                                                  RequestBuffer,
                                                  RequestLength,
                                                  ResponseBuffer,
                                                  ResponseLength,
                                                  RequestType,
                                                  RequestIoctl,
                                                  RequestTimeoutMilliseconds,
                                                  ContinuousRequestTarget_CompletionOptions_Default,
                                                  BytesWritten,
                                                  NULL,
                                                  NULL,
                                                  NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RequestTarget_RequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

// eof: Dmf_RequestTarget.c
//
