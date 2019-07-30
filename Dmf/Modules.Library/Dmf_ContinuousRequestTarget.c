/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ContinuousRequestTarget.c

Abstract:

    Creates a stream of asynchronous requests to a specific IO Target. Also, there is support
    for sending synchronous requests to the same IO Target.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_ContinuousRequestTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Input Buffer List.
    //
    DMFMODULE DmfModuleBufferPoolInput;
    // Output Buffer List.
    //
    DMFMODULE DmfModuleBufferPoolOutput;
    // Context Buffer List.
    //
    DMFMODULE DmfModuleBufferPoolContext;
    // Queued workitem for passive level completion routine.
    // Stream Asynchronous Request.
    //
    DMFMODULE DmfModuleQueuedWorkitemStream;
    // Queued workitem for passive level completion routine.
    // Single Asynchronous Request.
    //
    DMFMODULE DmfModuleQueuedWorkitemSingle;
    // Completion routine for stream asynchronous requests.
    //
    EVT_WDF_REQUEST_COMPLETION_ROUTINE* CompletionRoutineStream;
    // IO Target to Send Requests to.
    //
    WDFIOTARGET IoTarget;
    // Indicates that the Client has stopped streaming. This flag prevents new requests from 
    // being sent to the underlying target.
    //
    BOOLEAN Stopping;
    // Count of requests in lower driver so that Module can shutdown gracefully.
    // NOTE: This is for User-mode rundown support. Once Rundown support is unified for
    //       Kernel and user-modes, this can be removed.
    //
    LONG PendingStreamingRequests;
    // Count of streaming requests so that Module can shutdown gracefully.
    //
    LONG StreamingRequestCount;
    // Collection of asynchronous stream requests. This is the Collection of requests that is created
    // when the Module is instantiated.
    //
    WDFCOLLECTION CreatedStreamRequestsCollection;
    // Collection of asynchronous transient stream requests. Requests are added to this collection when
    // streaming starts and are removed when streaming stops.
    //
    WDFCOLLECTION TransientStreamRequestsCollection;
    // Rundown for sending stream requests.
    //
    DMF_PORTABLE_RUNDOWN_REF StreamRequestsRundown;
    // Rundown for in-flight stream requests.
    //
    DMF_PORTABLE_EVENT StreamRequestsRundownCompletionEvent;
} DMF_CONTEXT_ContinuousRequestTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(ContinuousRequestTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ContinuousRequestTarget)

// Memory Pool Tag.
//
#define MemoryTag 'mTRC'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define DEFAULT_NUMBER_OF_PENDING_PASSIVE_LEVEL_COMPLETION_ROUTINES 4

typedef struct
{
    DMFMODULE DmfModule;
    ContinuousRequestTarget_RequestType SingleAsynchronousRequestType;
    EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest;
    VOID* SingleAsynchronousCallbackClientContext;
} ContinuousRequestTarget_SingleAsynchronousRequestContext;

typedef struct
{
    WDFREQUEST Request;
    WDF_REQUEST_COMPLETION_PARAMS RequestCompletionParams;
    ContinuousRequestTarget_SingleAsynchronousRequestContext* SingleAsynchronousRequestContext;
} ContinuousRequestTarget_QueuedWorkitemContext;

static
VOID
ContinuousRequestTarget_PrintDataReceived(
    _In_reads_bytes_(Length) BYTE* Buffer,
    _In_ ULONG Length
    )
/*++

Routine Description:

    Prints every byte stored in buffer of a given length.

Arguments:

    Buffer: Pointer to a buffer
    Length: Length of the buffer

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);

#if defined(DEBUG)
    ULONG bufferIndex;
    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "BufferStart");
    for (bufferIndex = 0; bufferIndex < Length; bufferIndex++)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "%02X", *(Buffer + bufferIndex));
    }
    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "BufferEnd");
#endif // defined(DEBUG)
}

static
VOID
ContinuousRequestTarget_DeleteStreamRequestsFromCollection(
    _In_ DMF_CONTEXT_ContinuousRequestTarget* ModuleContext
)
/*++

Routine Description:

    Remove and delete requests collected in CreatedStreamRequestsCollection.

Arguments:

    ModuleContext - This Module's context.

Return Value:

    None

--*/
{
    WDFREQUEST request;
    while ((request = (WDFREQUEST)WdfCollectionGetFirstItem(ModuleContext->CreatedStreamRequestsCollection)) != NULL)
    {
        WdfCollectionRemoveItem(ModuleContext->CreatedStreamRequestsCollection,
                                0);
        WdfObjectDelete(request);
    }
}

#if !defined(DMF_USER_MODE)
static
VOID
ContinuousRequestTarget_DecreaseStreamRequestCount(
    _In_ DMF_CONTEXT_ContinuousRequestTarget* ModuleContext
    )
/*++

Routine Description:

    Decrease the total number of active streaming requests by 1. If the count
    reaches 0, signal the rundown completion event.

Arguments:

    ModuleContext - This Module's context.

Return Value:

    None

--*/
{
    LONG result;

    result = InterlockedDecrement(&ModuleContext->StreamingRequestCount);
    ASSERT(result >= 0);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE,
                "[%d -> %d]", 
                result + 1, 
                result);

    if (0 == result)
    {
        DMF_Portable_EventSet(&ModuleContext->StreamRequestsRundownCompletionEvent);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "StreamRequestsRundownCompletionEvent SET");
    }
}
#endif

static
VOID
ContinuousRequestTarget_CompletionParamsInputBufferAndOutputBufferGet(
    _In_ DMFMODULE DmfModule,
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
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    WDFMEMORY inputMemory;
    WDFMEMORY outputMemory;

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

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
                ASSERT(*OutputBuffer != NULL);
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
                ASSERT(*InputBuffer != NULL);
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
                ASSERT(*InputBuffer != NULL);
            }
            if (outputMemory != NULL)
            {
                *OutputBuffer = WdfMemoryGetBuffer(outputMemory,
                                                   OutputBufferSize);
                ASSERT(*OutputBufferSize >= CompletionParams->Parameters.Ioctl.Output.Length);
                *OutputBufferSize = CompletionParams->Parameters.Ioctl.Output.Length;
                ASSERT(*OutputBuffer != NULL);
            }
            break;
        }
        default:
        {
            ASSERT(FALSE);
        }
    }
}

VOID
ContinuousRequestTarget_ProcessAsynchronousRequestSingle(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ ContinuousRequestTarget_SingleAsynchronousRequestContext* SingleAsynchronousRequestContext
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
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    inputBuffer = NULL;
    outputBuffer = NULL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

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
    ContinuousRequestTarget_CompletionParamsInputBufferAndOutputBufferGet(DmfModule,
                                                                          CompletionParams,
                                                                          SingleAsynchronousRequestContext->SingleAsynchronousRequestType,
                                                                          &inputBuffer,
                                                                          &inputBufferSize,
                                                                          &outputBuffer,
                                                                          &outputBufferSize);

    // Call the Client's callback function
    //
    if (SingleAsynchronousRequestContext->EvtContinuousRequestTargetSingleAsynchronousRequest != NULL)
    {
        (SingleAsynchronousRequestContext->EvtContinuousRequestTargetSingleAsynchronousRequest)(DmfModule,
                                                                                                SingleAsynchronousRequestContext->SingleAsynchronousCallbackClientContext,
                                                                                                inputBuffer,
                                                                                                inputBufferSize,
                                                                                                outputBuffer,
                                                                                                outputBufferSize,
                                                                                                ntStatus);
    }

    // The Request is complete.  
    // Put the buffer associated with single asynchronous request back into BufferPool.
    //
    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolContext,
                       SingleAsynchronousRequestContext);

    WdfObjectDelete(Request);

    DMF_ModuleDereference(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_REQUEST_COMPLETION_ROUTINE ContinuousRequestTarget_CompletionRoutine;

_Function_class_(EVT_WDF_REQUEST_COMPLETION_ROUTINE)
_IRQL_requires_same_
VOID
ContinuousRequestTarget_CompletionRoutine(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ WDFCONTEXT Context
    )
/*++

Routine Description:

    It is the completion routine for the Single Asynchronous requests. This routine does all the work
    to extract the buffers that are returned from underlying target. Then it calls the Client's Output Buffer callback function with the buffers.

Arguments:

    Request - The completed request.
    Target - The Io Target that completed the request.
    CompletionParams - Information about the completion.
    Context - This Module's handle.

Return Value:

    None

--*/
{
    ContinuousRequestTarget_SingleAsynchronousRequestContext* singleAsynchronousRequestContext;
    DMFMODULE dmfModule;

    UNREFERENCED_PARAMETER(Target);

    FuncEntry(DMF_TRACE);

    singleAsynchronousRequestContext = (ContinuousRequestTarget_SingleAsynchronousRequestContext*)Context;
    ASSERT(singleAsynchronousRequestContext != NULL);

    dmfModule = singleAsynchronousRequestContext->DmfModule;
    ASSERT(dmfModule != NULL);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Request=0x%p [Completion Request]", Request);

    ContinuousRequestTarget_ProcessAsynchronousRequestSingle(dmfModule,
                                                             Request,
                                                             CompletionParams,
                                                             singleAsynchronousRequestContext);

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_REQUEST_COMPLETION_ROUTINE ContinuousRequestTarget_CompletionRoutinePassive;

_Function_class_(EVT_WDF_REQUEST_COMPLETION_ROUTINE)
_IRQL_requires_same_
VOID
ContinuousRequestTarget_CompletionRoutinePassive(
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
    ContinuousRequestTarget_SingleAsynchronousRequestContext* singleAsynchronousRequestContext;
    DMFMODULE dmfModule;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    ContinuousRequestTarget_QueuedWorkitemContext workitemContext;

    UNREFERENCED_PARAMETER(Target);

    FuncEntry(DMF_TRACE);

    singleAsynchronousRequestContext = (ContinuousRequestTarget_SingleAsynchronousRequestContext*)Context;
    ASSERT(singleAsynchronousRequestContext != NULL);

    dmfModule = singleAsynchronousRequestContext->DmfModule;
    ASSERT(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    workitemContext.Request = Request;
    workitemContext.RequestCompletionParams = *CompletionParams;
    workitemContext.SingleAsynchronousRequestContext = singleAsynchronousRequestContext;

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Request=0x%p [Enqueue Completion]", Request);

    DMF_QueuedWorkItem_Enqueue(moduleContext->DmfModuleQueuedWorkitemSingle,
                               (VOID*)&workitemContext,
                               sizeof(ContinuousRequestTarget_QueuedWorkitemContext));

    FuncExitVoid(DMF_TRACE);
}

// The completion routine calls this function so it needs to be declared here.
//
static
NTSTATUS
ContinuousRequestTarget_StreamRequestSend(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    );

VOID
ContinuousRequestTarget_ProcessAsynchronousRequestStream(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams
    )
/*++

Routine Description:

    This routine does all the work to extract the buffers that are returned from underlying target.
    Then it calls the Client's Output Buffer callback function with the buffers.

Arguments:

    DmfModule - The given Dmf Module.
    Request - The completed request.
    CompletionParams - Information about the completion.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    VOID* inputBuffer;
    size_t inputBufferSize;
    VOID* outputBuffer;
    size_t outputBufferSize;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    ContinuousRequestTarget_BufferDisposition bufferDisposition;
    VOID* clientBufferContextOutput;

    FuncEntry(DMF_TRACE);

    inputBuffer = NULL;
    outputBuffer = NULL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = WdfRequestGetStatus(Request);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE,
                "WdfRequestGetStatus Request=0x%p completes: ntStatus=%!STATUS!",
                Request, ntStatus);

    // Get information about the request completion.
    //
    WdfRequestGetCompletionParams(Request,
                                  CompletionParams);

    // Get the input and output buffers.
    // Input buffer will be NULL for request type read and write.
    //
    ContinuousRequestTarget_CompletionParamsInputBufferAndOutputBufferGet(DmfModule,
                                                                          CompletionParams,
                                                                          moduleConfig->RequestType,
                                                                          &inputBuffer,
                                                                          &inputBufferSize,
                                                                          &outputBuffer,
                                                                          &outputBufferSize);

    if (outputBuffer != NULL)
    {
        DMF_BufferPool_ContextGet(moduleContext->DmfModuleBufferPoolOutput,
                                  outputBuffer,
                                  &clientBufferContextOutput);

        // If Client has stopped streaming, then regardless of what the Client returns from the callback, return buffers
        // back to the original state and delete corresponding requests.
        //
        if (moduleContext->Stopping)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Request=0x%p [STOPPED]", Request);
            bufferDisposition = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        }
        else
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Request=0x%p [Not Stopped]", Request);
            if (NT_SUCCESS(ntStatus))
            {
                ContinuousRequestTarget_PrintDataReceived((BYTE*)outputBuffer,
                                                          (ULONG)outputBufferSize);
            }
            // Call the Client's callback function to give the Client Buffer a chance to use the output buffer.
            // The Client returns TRUE if Client expects this Module to return the buffer to its own list.
            // Otherwise, the Client will take ownership of the buffer and return it later using a Module Method.
            //
            bufferDisposition = moduleConfig->EvtContinuousRequestTargetBufferOutput(DmfModule,
                                                                                     outputBuffer,
                                                                                     outputBufferSize,
                                                                                     clientBufferContextOutput,
                                                                                     ntStatus);

            ASSERT(bufferDisposition > ContinuousRequestTarget_BufferDisposition_Invalid);
            ASSERT(bufferDisposition < ContinuousRequestTarget_BufferDisposition_Maximum);
        }

        if (((bufferDisposition == ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming) ||
            (bufferDisposition == ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming)) &&
             (outputBuffer != NULL))
        {
            // The Client indicates that it is finished with the buffer. So return it back to the
            // list of output buffers.
            //
            DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolOutput,
                               outputBuffer);
        }
    }
    else
    {
        if ((!NT_SUCCESS(ntStatus)) ||
            (moduleContext->Stopping))
        {
            bufferDisposition = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        }
        else
        {
            bufferDisposition = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;
        }
    }

    // Input buffer will be NULL for Request types Read and Write.
    //
    if (inputBuffer != NULL)
    {
        // Always return the Input Buffer back to the Input Buffer List.
        //
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolInput,
                           inputBuffer);
    }

    if (((bufferDisposition == ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming) ||
        (bufferDisposition == ContinuousRequestTarget_BufferDisposition_ClientAndContinueStreaming))
        )
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Request=0x%p Send again", Request);

        ntStatus = ContinuousRequestTarget_StreamRequestSend(DmfModule,
                                                             Request);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ContinuousRequestTarget_StreamRequestSend fails: ntStatus=%!STATUS! Request=0x%p", ntStatus, Request);
        }
        else
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "ContinuousRequestTarget_StreamRequestSend success: ntStatus=%!STATUS! Request=0x%p", ntStatus, Request);
        }
    }
    else
    {
        ntStatus = STATUS_CANCELLED;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Cancel due to callback: ntStatus=%!STATUS! Request=0x%p", ntStatus, Request);
    }

    if (!NT_SUCCESS(ntStatus))
    {
#if ! defined(DMF_USER_MODE)
        // This request stream has stopped so reduce the total count
        //
        ContinuousRequestTarget_DecreaseStreamRequestCount(moduleContext);
#endif
        // Remove on decrement so we know what requests are still outstanding.
        //
        WdfCollectionRemove(moduleContext->TransientStreamRequestsCollection, 
                            Request);
    }
    else
    {
#if ! defined(DMF_USER_MODE)
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Request=0x%p [No decrement]", Request);
#endif
    }

    // Request has returned. Decrement.
    //
    InterlockedDecrement(&moduleContext->PendingStreamingRequests);

    DMF_ModuleDereference(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_REQUEST_COMPLETION_ROUTINE ContinuousRequestTarget_StreamCompletionRoutine;

_Function_class_(EVT_WDF_REQUEST_COMPLETION_ROUTINE)
_IRQL_requires_same_
VOID
ContinuousRequestTarget_StreamCompletionRoutine(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ WDFCONTEXT Context
    )
/*++

Routine Description:

    It is the completion routine for the Asynchronous requests. This routine does all the work
    to extract the buffers that are returned from underlying target. Then it calls the Client's
    Output Buffer callback function with the buffers so that the Client can do Client specific processing.

Arguments:

    Request - The completed request.
    Target - The Io Target that completed the request.
    CompletionParams - Information about the completion.
    Context - This Module's handle.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;

    UNREFERENCED_PARAMETER(Target);

    FuncEntry(DMF_TRACE);

    dmfModule = DMFMODULEVOID_TO_MODULE(Context);

    ContinuousRequestTarget_ProcessAsynchronousRequestStream(dmfModule,
                                                             Request,
                                                             CompletionParams);

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_REQUEST_COMPLETION_ROUTINE ContinuousRequestTarget_StreamCompletionRoutinePassive;

_Function_class_(EVT_WDF_REQUEST_COMPLETION_ROUTINE)
_IRQL_requires_same_
VOID
ContinuousRequestTarget_StreamCompletionRoutinePassive(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
    _In_ WDFCONTEXT Context
)
/*++

Routine Description:

    It is the completion routine for the Asynchronous requests. This routine does all the work
    to extract the buffers that are returned from underlying target. Then it calls the Client's
    Output Buffer callback function with the buffers so that the Client can do Client specific processing.

Arguments:

    Request - The completed request.
    Target - The Io Target that completed the request.
    CompletionParams - Information about the completion.
    Context - This Module's handle.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    ContinuousRequestTarget_QueuedWorkitemContext workitemContext;

    UNREFERENCED_PARAMETER(Target);

    FuncEntry(DMF_TRACE);

    dmfModule = DMFMODULEVOID_TO_MODULE(Context);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    workitemContext.Request = Request;
    workitemContext.RequestCompletionParams = *CompletionParams;

    DMF_QueuedWorkItem_Enqueue(moduleContext->DmfModuleQueuedWorkitemStream,
                               (VOID*)&workitemContext,
                               sizeof(ContinuousRequestTarget_QueuedWorkitemContext));
}

static
NTSTATUS
ContinuousRequestTarget_FormatRequestForRequestType(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST   Request,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctlCode,
    _In_opt_ WDFMEMORY    InputMemory,
    _In_opt_ WDFMEMORY    OutputMemory
    )
/*++

Routine Description:

    Format the Request based on Request Type specified in Module Config.

Arguments:

    DmfModule - This Module's handle.
    Request - The request to format.
    RequestIoctlCode - IOCTL code for Request type ContinuousRequestTarget_RequestType_Ioctl or ContinuousRequestTarget_RequestType_InternalIoctl
    InputMemory - Handle to framework memory object which contains input data
    OutputMemory - Handle to framework memory object to receive output data

Return Value:

    None

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Prepare the request to be sent down.
    //
    ASSERT(moduleContext->IoTarget != NULL);
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

static
NTSTATUS
ContinuousRequestTarget_CreateBuffersAndFormatRequestForRequestType(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST   Request
    )
/*++

Routine Description:

    Create the required input and output buffers and format the
    Request based on Request Type specified in Module Config.

Arguments:

    DmfModule - This Module's handle.
    Request - The request to format.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    WDFMEMORY requestOutputMemory;
    WDFMEMORY requestInputMemory;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    VOID* inputBuffer;
    size_t inputBufferSize;
    VOID* outputBuffer;
    VOID* inputBufferContext;
    VOID* outputBufferContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Create the input buffer for the request if the Client needs one.
    //
    requestInputMemory = NULL;
    if (moduleConfig->BufferInputSize > 0)
    {
        // Get an input buffer from the input buffer list.
        // NOTE: This is fast operation that involves only pointer manipulation unless the buffer list is empty
        // (which should not happen).
        //
        ntStatus = DMF_BufferPool_GetWithMemory(moduleContext->DmfModuleBufferPoolInput,
                                                &inputBuffer,
                                                &inputBufferContext,
                                                &requestInputMemory);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferPool_GetWithMemory fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        inputBufferSize = moduleConfig->BufferInputSize;
        moduleConfig->EvtContinuousRequestTargetBufferInput(DmfModule,
                                                            inputBuffer,
                                                            &inputBufferSize,
                                                            inputBufferContext);
        ASSERT(inputBufferSize <= moduleConfig->BufferInputSize);
    }

    // Create the output buffer for the request if the Client needs one.
    //
    requestOutputMemory = NULL;
    if (moduleConfig->BufferOutputSize > 0)
    {
        // Get an output buffer from the output buffer list.
        // NOTE: This is fast operation that involves only pointer manipulation unless the buffer list is empty
        // (which should not happen).
        //
        ntStatus = DMF_BufferPool_GetWithMemory(moduleContext->DmfModuleBufferPoolOutput,
                                                &outputBuffer,
                                                &outputBufferContext,
                                                &requestOutputMemory);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferPool_GetWithMemory fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    ntStatus = ContinuousRequestTarget_FormatRequestForRequestType(DmfModule,
                                                                   Request,
                                                                   moduleConfig->RequestType,
                                                                   moduleConfig->ContinuousRequestTargetIoctl,
                                                                   requestInputMemory,
                                                                   requestOutputMemory);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ContinuousRequestTarget_FormatRequestForRequestType fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
ContinuousRequestTarget_StreamRequestSend(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Send a single asynchronous request down the stack.

Arguments:

    DmfModule - This Module's handle.
    Request - The request to send or NULL if the request should be created.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    BOOLEAN requestSendResult;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // A new request will be sent down the stack. Count it so we can verify when
    // it returns.
    //
    InterlockedIncrement(&moduleContext->PendingStreamingRequests);
    DMF_ModuleReference(DmfModule);

#if !defined(DMF_USER_MODE)
    // A new request will be sent down the stack. Increase Rundown ref until 
    // send request has been made.
    //
    if (DMF_Portable_Rundown_Acquire(&moduleContext->StreamRequestsRundown))
    {
#endif
        // Reuse the request
        //
        WDF_REQUEST_REUSE_PARAMS  requestParams;

        WDF_REQUEST_REUSE_PARAMS_INIT(&requestParams,
                                      WDF_REQUEST_REUSE_NO_FLAGS,
                                      STATUS_SUCCESS);
        ntStatus = WdfRequestReuse(Request,
                                   &requestParams);
        // Simple reuse cannot fail.
        //
        ASSERT(NT_SUCCESS(ntStatus));

        ntStatus = ContinuousRequestTarget_CreateBuffersAndFormatRequestForRequestType(DmfModule,
                                                                                       Request);
        if (NT_SUCCESS(ntStatus))
        {
            // Set a CompletionRoutine callback function. It goes back into this Module which will
            // dispatch to the Client.
            //
            WdfRequestSetCompletionRoutine(Request,
                                           moduleContext->CompletionRoutineStream,
                                           (WDFCONTEXT) (DmfModule));

            // Send the request - Asynchronous call, so check for Status if it fails.
            // If it succeeds, the Status will be checked in Completion Routine.
            //
            requestSendResult = WdfRequestSend(Request,
                                               moduleContext->IoTarget,
                                               WDF_NO_SEND_OPTIONS);
            if (! requestSendResult)
            {
                ntStatus = WdfRequestGetStatus(Request);
                ASSERT(!NT_SUCCESS(ntStatus));
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestSend fails: ntStatus=%!STATUS!", ntStatus);
            }
        }
        else
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "ContinuousRequestTarget_CreateBuffersAndFormatRequestForRequestType fails: ntStatus=%!STATUS!", ntStatus);
        }

#if !defined(DMF_USER_MODE)
        DMF_Portable_Rundown_Release(&moduleContext->StreamRequestsRundown);
    }
    else
    {
        ntStatus = STATUS_CANCELLED;
    }
#endif

    if (!NT_SUCCESS(ntStatus))
    {

        // Unable to send the request. Decrement to account for the increment above.
        //
        InterlockedDecrement(&moduleContext->PendingStreamingRequests);
        DMF_ModuleDereference(DmfModule);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// 'Returning uninitialized memory'
//
#pragma warning(suppress:6101)
static
NTSTATUS
ContinuousRequestTarget_RequestCreateAndSend(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN IsSynchronousRequest,
    _In_ VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_ VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_ ContinuousRequestTarget_CompletionOptions CompletionOption,
    _Out_opt_ size_t* BytesWritten,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    )
/*++

Routine Description:

    Creates and sends a synchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes to in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    CompletionOption - Completion option associated with the completion routine. 
    BytesWritten - Bytes returned by the transaction.
    EvtContinuousRequestTargetSingleAsynchronousRequest - Completion routine. 
    SingleAsynchronousRequestClientContext - Client context returned in completion routine. 

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
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    WDFDEVICE device;
    EVT_WDF_REQUEST_COMPLETION_ROUTINE* completionRoutineSingle;
    ContinuousRequestTarget_SingleAsynchronousRequestContext* singleAsynchronousRequestContext;
    VOID* singleBufferContext;

    FuncEntry(DMF_TRACE);

    outputBufferSize = 0;
    requestSendResult = FALSE;

    ASSERT((IsSynchronousRequest && (EvtContinuousRequestTargetSingleAsynchronousRequest == NULL)) ||
           (! IsSynchronousRequest));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->IoTarget != NULL);

    device = DMF_ParentDeviceGet(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT(&requestAttributes);
    requestAttributes.ParentObject = device;
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

    WDF_OBJECT_ATTRIBUTES_INIT(&memoryAttributes);
    memoryAttributes.ParentObject = request;

    memoryForRequest = NULL;
    if (RequestLength > 0)
    {
        ASSERT(RequestBuffer != NULL);
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
        ASSERT(ResponseBuffer != NULL);
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

    ntStatus = ContinuousRequestTarget_FormatRequestForRequestType(DmfModule,
                                                                   request,
                                                                   RequestType,
                                                                   RequestIoctl,
                                                                   memoryForRequest,
                                                                   memoryForResponse);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ContinuousRequestTarget_FormatRequestForRequestType fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (IsSynchronousRequest)
    {
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
            completionRoutineSingle = ContinuousRequestTarget_CompletionRoutine;
        }
        else if (CompletionOption == ContinuousRequestTarget_CompletionOptions_Passive)
        {
            completionRoutineSingle = ContinuousRequestTarget_CompletionRoutinePassive;
        }
        else
        {
            completionRoutineSingle = ContinuousRequestTarget_CompletionRoutine;
            ASSERT(FALSE);
        }

        singleAsynchronousRequestContext->DmfModule = DmfModule;
        singleAsynchronousRequestContext->SingleAsynchronousCallbackClientContext = SingleAsynchronousRequestClientContext;
        singleAsynchronousRequestContext->EvtContinuousRequestTargetSingleAsynchronousRequest = EvtContinuousRequestTargetSingleAsynchronousRequest;
        singleAsynchronousRequestContext->SingleAsynchronousRequestType = RequestType;

        // Set the completion routine to internal completion routine of this Module.
        //
        WdfRequestSetCompletionRoutine(request,
                                       completionRoutineSingle,
                                       singleAsynchronousRequestContext);
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

Exit:

    if (BytesWritten != NULL)
    {
        *BytesWritten = outputBufferSize;
    }

    if (IsSynchronousRequest && 
        request != NULL)
    {
        // Delete the request if its Synchronous.
        //
        WdfObjectDelete(request);
        request = NULL;
    }
    else if (! IsSynchronousRequest && 
             ! NT_SUCCESS(ntStatus) && 
             request != NULL)
    {
        // Delete the request if Asynchronous request failed.
        //
        WdfObjectDelete(request);
        request = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

ScheduledTask_Result_Type 
ContinuousRequestTarget_QueuedWorkitemCallbackSingle(
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
    ContinuousRequestTarget_QueuedWorkitemContext* workitemContext;

    UNREFERENCED_PARAMETER(ClientBufferContext);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    workitemContext = (ContinuousRequestTarget_QueuedWorkitemContext*)ClientBuffer;

    ContinuousRequestTarget_ProcessAsynchronousRequestSingle(dmfModuleParent,
                                                             workitemContext->Request,
                                                             &workitemContext->RequestCompletionParams,
                                                             workitemContext->SingleAsynchronousRequestContext);

    return ScheduledTask_WorkResult_Success;
}

ScheduledTask_Result_Type
ContinuousRequestTarget_QueuedWorkitemCallbackStream(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext
    )
/*++

Routine Description:

    This routine does the work of completion routine for stream asynchronous requests, at passive level.

Arguments:

    DmfModule - The QueuedWorkItem Dmf Module.
    ClientBuffer - The buffer that contains the context of work to be done.
    ClientBufferContext - Context associated with the buffer.

Return Value:

    None

--*/
{
    DMFMODULE dmfModuleParent;
    ContinuousRequestTarget_QueuedWorkitemContext* workitemContext;

    UNREFERENCED_PARAMETER(ClientBufferContext);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    workitemContext = (ContinuousRequestTarget_QueuedWorkitemContext*)ClientBuffer;

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Request=0x%p [Queued Callback]", workitemContext->Request);

    ContinuousRequestTarget_ProcessAsynchronousRequestStream(dmfModuleParent,
                                                             workitemContext->Request,
                                                             &workitemContext->RequestCompletionParams);

    return ScheduledTask_WorkResult_Success;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
ContinuousRequestTarget_RequestsCancel(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Cancel all the outstanding requests.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    WDFREQUEST request;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Tell the rest of the Module that Client has stopped streaming.
    // (It is possible this is called twice if removal of WDFIOTARGET occurs on stream that starts/stops
    // automatically.
    //
    moduleContext->Stopping = TRUE;

    // Cancel all requests from target. Do not wait until all pending requests have returned.
    //

#if !defined(DMF_USER_MODE)
    // 1. Make sure no new request will be sent.
    //
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Start Rundown");
    DMF_Portable_Rundown_WaitForRundownProtectionRelease(&moduleContext->StreamRequestsRundown);
    DMF_Portable_Rundown_Completed(&moduleContext->StreamRequestsRundown);
#endif

    // 2. Cancel any pending WDF requests.
    //
    // NOTE: There is not need to lock because these requests always exist in this list.
    // NOTE: Get total number from Config in case it has already started decrementing StreamRequestCount;
    //
    LONG requestsToCancel = (LONG)moduleConfig->ContinuousRequestCount;
    ASSERT(moduleContext->StreamingRequestCount <= requestsToCancel);
    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Cancel Pending Requests: START requestsToCancel=%d", requestsToCancel);
    for (LONG requestIndex = 0; requestIndex < requestsToCancel; requestIndex++)
    {
        request = (WDFREQUEST)WdfCollectionGetItem(moduleContext->CreatedStreamRequestsCollection,
                                                   requestIndex);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WdfRequestCancelSentRequest Request[%d]=0x%p", requestIndex, request);
        WdfRequestCancelSentRequest(request);
    }
    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Cancel Pending Requests: END");

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
ContinuousRequestTarget_StopAndWait(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops streaming Asynchronous requests to the IoTarget and waits for all pending requests to return.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ASSERT(moduleContext->IoTarget != NULL);

    // Tell the rest of the Module that Client has stopped streaming.
    // (It is possible this is called twice if removal of WDFIOTARGET occurs on stream that starts/stops
    // automatically.
    //
    moduleContext->Stopping = TRUE;

    // Cancel all the outstanding requests.
    //
    ContinuousRequestTarget_RequestsCancel(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Wait for in-flight callback");

    // 3. Wait for any in-flight callback to return.
    //
#if !defined(DMF_USER_MODE)
    DMF_Portable_EventWaitForSingleObject(&moduleContext->StreamRequestsRundownCompletionEvent,
                                          FALSE, 
                                          NULL);
#else
    // Once Rundown API is supported in User-mode, this code can be deleted.
    //
    while (moduleContext->PendingStreamingRequests > 0)
    {
        DMF_Utility_DelayMilliseconds(50);
    }
#endif

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Rundown Completed");

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_ContinuousRequestTarget_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Callback for ModuleD0Entry for a this Module. Some Clients require streaming
    to stop during D0Exit/D0Entry transitions. This code does that work on behalf of the
    Client.

Arguments:

    DmfModule - The given DMF Module.
    PreviousState - The WDF Power State that this DMF Module should exit from.

Return Value:

   NTSTATUS

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Send each WDFREQUEST this Module's instance has created to 
    // its WDFIOTARGET.
    //
    if ((moduleConfig->CancelAndResendRequestInD0Callbacks) &&
        (moduleContext->IoTarget != NULL))
    {
        if (PreviousState == WdfPowerDeviceD3Final)
        {
            ntStatus = STATUS_SUCCESS;
        }
        else
        {
            ntStatus = DMF_ContinuousRequestTarget_Start(DmfModule);
        }
    }

    // Start the target on any power transition other than cold boot.
    // if the PurgeAndStartTargetInD0Callbacks is set to true.
    //
    if ((moduleConfig->PurgeAndStartTargetInD0Callbacks) &&
        (moduleContext->IoTarget != NULL))
    {
        if (PreviousState == WdfPowerDeviceD3Final)
        {
            ntStatus = STATUS_SUCCESS;
        }
        else
        {
            ntStatus = WdfIoTargetStart(moduleContext->IoTarget);
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_ContinuousRequestTarget_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Callback for ModuleD0Exit for this Module. Some Clients require streaming
    to stop during D0Exit/D0Entry transitions. This code does that work on behalf of the
    Client.

Arguments:

    DmfModule - The given DMF Module.
    TargetState - The WDF Power State that this DMF Module will enter.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(TargetState);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if ((moduleConfig->CancelAndResendRequestInD0Callbacks) &&
        (moduleContext->IoTarget != NULL))
    {
        DMF_ContinuousRequestTarget_StopAndWait(DmfModule);
    }

    if ((moduleConfig->PurgeAndStartTargetInD0Callbacks) &&
        (moduleContext->IoTarget != NULL))
    {
        WdfIoTargetPurge(moduleContext->IoTarget,
                         WdfIoTargetPurgeIoAndWait);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContinuousRequestTarget_ChildModulesAdd(
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
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_BufferPool moduleConfigBufferPoolInput;
    DMF_CONFIG_BufferPool moduleConfigBufferPoolOutput;
    DMF_CONFIG_BufferPool moduleConfigBufferPoolContext;
    DMF_CONFIG_QueuedWorkItem moduleConfigQueuedWorkItemStream;
    DMF_CONFIG_QueuedWorkItem moduleConfigQueuedWorkItemSingle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Create buffer pools for input and output buffers only if they are needed.
    //
    if (moduleConfig->BufferInputSize > 0)
    {
        // BufferPoolInput
        // ---------------
        //
        DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPoolInput,
                                                  &moduleAttributes);
        moduleConfigBufferPoolInput.BufferPoolMode = BufferPool_Mode_Source;
        moduleConfigBufferPoolInput.Mode.SourceSettings.EnableLookAside = FALSE;
        moduleConfigBufferPoolInput.Mode.SourceSettings.BufferCount = moduleConfig->BufferCountInput;
        moduleConfigBufferPoolInput.Mode.SourceSettings.PoolType = moduleConfig->PoolTypeInput;
        moduleConfigBufferPoolInput.Mode.SourceSettings.BufferSize = moduleConfig->BufferInputSize;
        moduleConfigBufferPoolInput.Mode.SourceSettings.BufferContextSize = moduleConfig->BufferContextInputSize;
        moduleAttributes.ClientModuleInstanceName = "BufferPoolInput";
        moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleBufferPoolInput);
    }
    else
    {
        ASSERT(moduleConfig->BufferCountInput == 0);
    }

    if (moduleConfig->BufferOutputSize > 0)
    {
        // BufferPoolOutput
        // ----------------
        //
        DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPoolOutput,
                                                  &moduleAttributes);
        moduleConfigBufferPoolOutput.BufferPoolMode = BufferPool_Mode_Source;
        moduleConfigBufferPoolOutput.Mode.SourceSettings.EnableLookAside = moduleConfig->EnableLookAsideOutput;
        moduleConfigBufferPoolOutput.Mode.SourceSettings.BufferCount = moduleConfig->BufferCountOutput;
        moduleConfigBufferPoolOutput.Mode.SourceSettings.PoolType = moduleConfig->PoolTypeOutput;
        moduleConfigBufferPoolOutput.Mode.SourceSettings.BufferSize = moduleConfig->BufferOutputSize;
        moduleConfigBufferPoolOutput.Mode.SourceSettings.BufferContextSize = moduleConfig->BufferContextOutputSize;
        moduleAttributes.ClientModuleInstanceName = "BufferPoolOutput";
        moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleBufferPoolOutput);
    }
    else
    {
        ASSERT(moduleConfig->BufferCountOutput == 0);
    }

    // BufferPoolContext
    // -----------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPoolContext,
                                              &moduleAttributes);
    moduleConfigBufferPoolContext.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPoolContext.Mode.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferPoolContext.Mode.SourceSettings.BufferCount = 1;
    // NOTE: BufferPool context must always be NonPagedPool because it is accessed in the
    //       completion routine running at DISPATCH_LEVEL.
    //
    moduleConfigBufferPoolContext.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigBufferPoolContext.Mode.SourceSettings.BufferSize = sizeof(ContinuousRequestTarget_SingleAsynchronousRequestContext);
    moduleAttributes.ClientModuleInstanceName = "BufferPoolContext";
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolContext);

    // QueuedWorkItemSingle
    // --------------------
    //
    DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT(&moduleConfigQueuedWorkItemSingle,
                                                  &moduleAttributes);
    moduleConfigQueuedWorkItemSingle.BufferQueueConfig.SourceSettings.BufferCount = DEFAULT_NUMBER_OF_PENDING_PASSIVE_LEVEL_COMPLETION_ROUTINES;
    moduleConfigQueuedWorkItemSingle.BufferQueueConfig.SourceSettings.BufferSize = sizeof(ContinuousRequestTarget_QueuedWorkitemContext);
    // This has to be NonPagedPoolNx because completion routine runs at dispatch level.
    //
    moduleConfigQueuedWorkItemSingle.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigQueuedWorkItemSingle.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    moduleConfigQueuedWorkItemSingle.EvtQueuedWorkitemFunction = ContinuousRequestTarget_QueuedWorkitemCallbackSingle;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleQueuedWorkitemSingle);

    if (DmfParentModuleAttributes->PassiveLevel)
    {
        moduleContext->CompletionRoutineStream = ContinuousRequestTarget_StreamCompletionRoutinePassive;
        // QueuedWorkItemStream
        // --------------------
        //
        DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT(&moduleConfigQueuedWorkItemStream,
                                                      &moduleAttributes);
        moduleConfigQueuedWorkItemStream.BufferQueueConfig.SourceSettings.BufferCount = DEFAULT_NUMBER_OF_PENDING_PASSIVE_LEVEL_COMPLETION_ROUTINES;
        moduleConfigQueuedWorkItemStream.BufferQueueConfig.SourceSettings.BufferSize = sizeof(ContinuousRequestTarget_QueuedWorkitemContext);
        // This has to be NonPagedPoolNx because completion routine runs at dispatch level.
        //
        moduleConfigQueuedWorkItemStream.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
        moduleConfigQueuedWorkItemStream.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
        moduleConfigQueuedWorkItemStream.EvtQueuedWorkitemFunction = ContinuousRequestTarget_QueuedWorkitemCallbackStream;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleQueuedWorkitemStream);
    }
    else
    {
        moduleContext->CompletionRoutineStream = ContinuousRequestTarget_StreamCompletionRoutine;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_ContinuousRequestTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type ContinuousRequestTarget.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Streaming is not started yet.
    // 
    moduleContext->Stopping = TRUE;

#if !defined(DMF_USER_MODE)
    DMF_Portable_Rundown_Initialize(&moduleContext->StreamRequestsRundown);

    DMF_Portable_EventCreate(&moduleContext->StreamRequestsRundownCompletionEvent, 
                             NotificationEvent, 
                             FALSE);
#endif

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    // This Collection contains all the requests that are created for streaming. These requests remain
    // in this collection until the Module is Closed.
    //
    ntStatus = WdfCollectionCreate(&objectAttributes,
                                   &moduleContext->CreatedStreamRequestsCollection);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // These are the requests that need to be canceled prior to streaming stopping.
    //
    ntStatus = WdfCollectionCreate(&objectAttributes,
                                   &moduleContext->TransientStreamRequestsCollection);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // It is possible for Client to instantiate this Module without using streaming.
    //
    if (moduleConfig->ContinuousRequestCount > 0)
    {
        for (UINT requestIndex = 0; requestIndex < moduleConfig->ContinuousRequestCount; requestIndex++)
        {
            WDF_OBJECT_ATTRIBUTES requestAttributes;
            WDFREQUEST request;

            WDF_OBJECT_ATTRIBUTES_INIT(&requestAttributes);
            // The request is being parented to the device explicitly to handle deletion.
            // When a dynamic module tree is deleted, the child objects are deleted first before the parent.
            // So, if request is a child of this module and this module gets implicitly deleted, 
            // the requests get the delete operation first. And if the reqeust is already sent to an IO Target, 
            // WDF verifier complains about it.
            // Thus request is parented to device, and are deleted when the collection is deleted in DMF close callback. 
            //
            requestAttributes.ParentObject = device;

            ntStatus = WdfRequestCreate(&requestAttributes,
                                        moduleContext->IoTarget,
                                        &request);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }

            ntStatus = WdfCollectionAdd(moduleContext->CreatedStreamRequestsCollection, 
                                        request);
            if (!NT_SUCCESS(ntStatus))
            {
                WdfObjectDelete(request);
                goto Exit;
            }
        }
    }
#if !defined(DMF_USER_MODE)
    else
    {
        DMF_Portable_EventSet(&moduleContext->StreamRequestsRundownCompletionEvent);
        ntStatus = STATUS_SUCCESS;
    }
#endif

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        if(moduleContext->CreatedStreamRequestsCollection != NULL)
        {
            ContinuousRequestTarget_DeleteStreamRequestsFromCollection(moduleContext);
            WdfObjectDelete(moduleContext->CreatedStreamRequestsCollection);
            moduleContext->CreatedStreamRequestsCollection = NULL;
        }
        if(moduleContext->TransientStreamRequestsCollection != NULL)
        {
            WdfObjectDelete(moduleContext->TransientStreamRequestsCollection);
            moduleContext->TransientStreamRequestsCollection = NULL;
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_ContinuousRequestTarget_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type ContinuousRequestTarget.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // NOTE: Do not stop streaming here because this can happen after Release Hardware!.
    //       In that case, cancellation of requests works in an undefined manner.
    //       Streaming *must* be stopped when this callback happens!
    //
    ASSERT(moduleContext->Stopping);

    // There is no need to verify that IoTarget is NULL. Client may not clear it because it is
    // not necessary to do so.
    //

    // Clean up resources created in Open.
    //
    if (moduleContext->TransientStreamRequestsCollection != NULL)
    {
        ASSERT(WdfCollectionGetCount(moduleContext->TransientStreamRequestsCollection) == 0);
        WdfObjectDelete(moduleContext->TransientStreamRequestsCollection);
        moduleContext->TransientStreamRequestsCollection = NULL;
    }

    if (moduleContext->CreatedStreamRequestsCollection != NULL)
    {
        ContinuousRequestTarget_DeleteStreamRequestsFromCollection(moduleContext);
        WdfObjectDelete(moduleContext->CreatedStreamRequestsCollection);
        moduleContext->CreatedStreamRequestsCollection = NULL;
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
DMF_ContinuousRequestTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type ContinuousRequestTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_ContinuousRequestTarget;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_ContinuousRequestTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_ContinuousRequestTarget;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = (DMF_CONFIG_ContinuousRequestTarget*)DmfModuleAttributes->ModuleConfigPointer;

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_ContinuousRequestTarget);
    dmfCallbacksDmf_ContinuousRequestTarget.ChildModulesAdd = DMF_ContinuousRequestTarget_ChildModulesAdd;
    dmfCallbacksDmf_ContinuousRequestTarget.DeviceOpen = DMF_ContinuousRequestTarget_Open;
    dmfCallbacksDmf_ContinuousRequestTarget.DeviceClose = DMF_ContinuousRequestTarget_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_ContinuousRequestTarget,
                                            ContinuousRequestTarget,
                                            DMF_CONTEXT_ContinuousRequestTarget,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_ContinuousRequestTarget.CallbacksDmf = &dmfCallbacksDmf_ContinuousRequestTarget;

    if (moduleConfig->PurgeAndStartTargetInD0Callbacks)
    {
        ASSERT(DmfModuleAttributes->DynamicModule == FALSE);
        DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_ContinuousRequestTarget);
        dmfCallbacksWdf_ContinuousRequestTarget.ModuleD0Entry = DMF_ContinuousRequestTarget_ModuleD0Entry;
        dmfCallbacksWdf_ContinuousRequestTarget.ModuleD0Exit = DMF_ContinuousRequestTarget_ModuleD0Exit;
        dmfModuleDescriptor_ContinuousRequestTarget.CallbacksWdf = &dmfCallbacksWdf_ContinuousRequestTarget;
    }

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_ContinuousRequestTarget,
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
DMF_ContinuousRequestTarget_BufferPut(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Add the output buffer back to OutputBufferPool.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
    NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ContinuousRequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolOutput,
                       ClientBuffer);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ContinuousRequestTarget_IoTargetClear(
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
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ContinuousRequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(moduleContext->IoTarget != NULL);
    ASSERT(moduleContext->Stopping);

    moduleContext->IoTarget = NULL;

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_IoTargetSet(
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
    NTSTATUS ntStatus;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ContinuousRequestTarget);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(IoTarget != NULL);
    ASSERT(moduleContext->IoTarget == NULL);

    moduleContext->IoTarget = IoTarget;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    )
/*++

Routine Description:

    Creates and sends a Asynchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes to in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    EvtContinuousRequestTargetSingleAsynchronousRequest - Callback to be called in completion routine.
    SingleAsynchronousRequestClientContext - Client context sent in callback

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    ContinuousRequestTarget_CompletionOptions completionOption;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ContinuousRequestTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (DMF_IsModulePassiveLevel(DmfModule))
    {
        completionOption = ContinuousRequestTarget_CompletionOptions_Passive;
    }
    else
    {
        completionOption = ContinuousRequestTarget_CompletionOptions_Dispatch;
    }

    ntStatus = ContinuousRequestTarget_RequestCreateAndSend(DmfModule,
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
                                                            EvtContinuousRequestTargetSingleAsynchronousRequest,
                                                            SingleAsynchronousRequestClientContext);
    if (! NT_SUCCESS(ntStatus))
    {
        DMF_ModuleDereference(DmfModule);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ContinuousRequestTarget_RequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_ ContinuousRequestTarget_CompletionOptions CompletionOption,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    )
/*++

Routine Description:

    Creates and sends a Asynchronous request to the IoTarget given a buffer, IOCTL and other information.
    Once the request is complete, EvtContinuousRequestTargetSingleAsynchronousRequest will be called at passive level. 

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes to in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    EvtContinuousRequestTargetSingleAsynchronousRequest - Callback to be called in completion routine.
    SingleAsynchronousRequestClientContext - Client context sent in callback

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ContinuousRequestTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = ContinuousRequestTarget_RequestCreateAndSend(DmfModule,
                                                            FALSE,
                                                            RequestBuffer,
                                                            RequestLength,
                                                            ResponseBuffer,
                                                            ResponseLength,
                                                            RequestType,
                                                            RequestIoctl,
                                                            RequestTimeoutMilliseconds,
                                                            CompletionOption,
                                                            NULL,
                                                            EvtContinuousRequestTargetSingleAsynchronousRequest,
                                                            SingleAsynchronousRequestClientContext);
    if (! NT_SUCCESS(ntStatus))
    {
        DMF_ModuleDereference(DmfModule);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ContinuousRequestTarget_RequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
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
    RequestLength - Number of bytes to in RequestBuffer to send.
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
    NTSTATUS ntStatus = STATUS_SUCCESS;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ContinuousRequestTarget);

    ntStatus = ContinuousRequestTarget_RequestCreateAndSend(DmfModule,
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
                                                            NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ContinuousRequestTarget_RequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts streaming Asynchronous requests to the IoTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ContinuousRequestTarget);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    ASSERT(moduleContext->Stopping);

    // Clear the Stopped flag as streaming will now start.
    //
    moduleContext->Stopping = FALSE;

#if !defined(DMF_USER_MODE)
    // In case it was previous stopped, re-initialize fields used for rundown.
    //
    DMF_Portable_EventReset(&moduleContext->StreamRequestsRundownCompletionEvent);
    DMF_Portable_Rundown_Reinitialize(&moduleContext->StreamRequestsRundown);
#endif

    moduleContext->StreamingRequestCount = moduleConfig->ContinuousRequestCount;

    for (UINT requestIndex = 0; requestIndex < moduleConfig->ContinuousRequestCount; requestIndex++)
    {
        WDFREQUEST request;
        
        request = (WDFREQUEST)WdfCollectionGetItem(moduleContext->CreatedStreamRequestsCollection,
                                                   requestIndex);
        ASSERT(request != NULL);

        // Add it to the list of Transient requests a single time when Streaming starts.
        //
        ntStatus = WdfCollectionAdd(moduleContext->TransientStreamRequestsCollection,
                                    request);
        if (NT_SUCCESS(ntStatus))
        {
            // Actually send the Request down.
            //
            ntStatus = ContinuousRequestTarget_StreamRequestSend(DmfModule,
                                                                 request);
        }

        if (! NT_SUCCESS(ntStatus))
        {
#if !defined(DMF_USER_MODE)
            // Subtract the rest of stream requests yet to start.
            //
            while (requestIndex++ < moduleConfig->ContinuousRequestCount)
            {
                ContinuousRequestTarget_DecreaseStreamRequestCount(moduleContext);
            }
#endif
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ContinuousRequestTarget_StreamRequestSend fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContinuousRequestTarget_Stop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops streaming Asynchronous requests to the IoTarget and Cancels all the existing requests.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ContinuousRequestTarget);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        ASSERT(FALSE);
        goto Exit;
    }

    // Tell the rest of the Module that Client has stopped streaming.
    // (It is possible this is called twice if removal of WDFIOTARGET occurs on stream that starts/stops
    // automatically.
    //
    moduleContext->Stopping = TRUE;

    // Cancel all requests from target. Do not wait until all pending requests have returned.
    //
    ContinuousRequestTarget_RequestsCancel(DmfModule);

    DMF_ModuleDereference(DmfModule);

    ASSERT(moduleContext->IoTarget != NULL);

Exit:
    ;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContinuousRequestTarget_StopAndWait(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops streaming Asynchronous requests to the IoTarget and waits for all pending requests to return.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ContinuousRequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        ASSERT(FALSE);
        goto Exit;
    }

    // Stop Streaming. This is an internal function in case needs to be called in the future.
    //
    ContinuousRequestTarget_StopAndWait(DmfModule);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
}

// eof: Dmf_ContinuousRequestTarget.c
//
