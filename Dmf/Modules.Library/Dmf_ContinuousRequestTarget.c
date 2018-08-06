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
    // Completion routine for single asynchronous requests.
    //
    EVT_WDF_REQUEST_COMPLETION_ROUTINE* CompletionRoutineSingle;
    // IO Target to Send Requests to.
    //
    WDFIOTARGET IoTarget;
} DMF_CONTEXT_ContinuousRequestTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(ContinuousRequestTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ContinuousRequestTarget)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define DEFAULT_NUMBER_OF_PENDING_PASSIVE_LEVEL_COMPLETION_ROUTINES 4

typedef struct
{
    DMFMODULE DmfModule;
    ContinuousRequestTarget_RequestType SingleAsynchronousRequestType;
    EVT_DMF_ContinuousRequestTarget_SingleAsynchronousBufferOutput* EvtContinuousRequestTargetSingleAsynchronousRequest;
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

#if defined(DEBUG) || defined(SELFHOST)
    ULONG bufferIndex;
    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_ContinuousRequestTarget, "BufferStart");
    for (bufferIndex = 0; bufferIndex < Length; bufferIndex++)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_ContinuousRequestTarget, "%02X", *(Buffer + bufferIndex));
    }
    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_ContinuousRequestTarget, "BufferEnd");
#endif // defined(DEBUG) || defined(SELFHOST)
}

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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

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
            *OutputBufferSize = CompletionParams->Parameters.Write.Length;
            outputMemory = CompletionParams->Parameters.Write.Buffer;
            // Get the write buffer.
            //
            if (outputMemory != NULL)
            {
                *OutputBuffer = WdfMemoryGetBuffer(outputMemory,
                                                   NULL);
                ASSERT(*OutputBuffer != NULL);
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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    inputBuffer = NULL;
    outputBuffer = NULL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = WdfRequestGetStatus(Request);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfRequestGetStatus Request=0x%p fails: ntStatus=%!STATUS!", Request, ntStatus);
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

    FuncExitVoid(DMF_TRACE_ContinuousRequestTarget);
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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    singleAsynchronousRequestContext = (ContinuousRequestTarget_SingleAsynchronousRequestContext*)Context;
    ASSERT(singleAsynchronousRequestContext != NULL);

    dmfModule = singleAsynchronousRequestContext->DmfModule;
    ASSERT(dmfModule != NULL);

    ContinuousRequestTarget_ProcessAsynchronousRequestSingle(dmfModule,
                                                             Request,
                                                             CompletionParams,
                                                             singleAsynchronousRequestContext);

    FuncExitVoid(DMF_TRACE_ContinuousRequestTarget);
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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    singleAsynchronousRequestContext = (ContinuousRequestTarget_SingleAsynchronousRequestContext*)Context;
    ASSERT(singleAsynchronousRequestContext != NULL);

    dmfModule = singleAsynchronousRequestContext->DmfModule;
    ASSERT(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    workitemContext.Request = Request;
    workitemContext.RequestCompletionParams = *CompletionParams;
    workitemContext.SingleAsynchronousRequestContext = singleAsynchronousRequestContext;

    DMF_QueuedWorkItem_Enqueue(moduleContext->DmfModuleQueuedWorkitemSingle,
                               (VOID*)&workitemContext,
                               sizeof(ContinuousRequestTarget_QueuedWorkitemContext));

    FuncExitVoid(DMF_TRACE_ContinuousRequestTarget);
}

// The completion routine calls this function so it needs to be declared here.
//
static
NTSTATUS
ContinuousRequestTarget_StreamRequestCreateAndSend(
    _In_ DMFMODULE DmfModule,
    _In_opt_ WDFREQUEST Request
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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    inputBuffer = NULL;
    outputBuffer = NULL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = WdfRequestGetStatus(Request);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_ContinuousRequestTarget,
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

    DMF_BufferPool_ContextGet(moduleContext->DmfModuleBufferPoolOutput,
                              outputBuffer,
                              &clientBufferContextOutput);

    // Call the Client's callback function to give the Client Buffer a chance to use the output buffer.
    // The Client returns TRUE if Client expects this Module to return the buffer to its own list.
    // Otherwise, the Client will take ownership of the buffer and return it later using a Module Methods.
    //
    ContinuousRequestTarget_PrintDataReceived((BYTE*)outputBuffer,
                                              (ULONG)outputBufferSize);
    bufferDisposition = moduleConfig->EvtContinuousRequestTargetBufferOutput(DmfModule,
                                                                             outputBuffer,
                                                                             outputBufferSize,
                                                                             clientBufferContextOutput,
                                                                             ntStatus);

    ASSERT(bufferDisposition > ContinuousRequestTarget_BufferDisposition_Invalid);
    ASSERT(bufferDisposition < ContinuousRequestTarget_BufferDisposition_Maximum);

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
        // Reuse the request and send.
        //
        ntStatus = ContinuousRequestTarget_StreamRequestCreateAndSend(DmfModule,
                                                                      Request);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "ContinuousRequestTarget_StreamRequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
        }
    }
    else
    {
        // Cause the request to be deleted by design.
        //
        ntStatus = STATUS_UNSUCCESSFUL;
    }

    if (!NT_SUCCESS(ntStatus))
    {
        // Delete the completed request. It is not being reused.
        //
        WdfObjectDelete(Request);
    }

    FuncExitVoid(DMF_TRACE_ContinuousRequestTarget);
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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    dmfModule = DMFMODULEVOID_TO_MODULE(Context);

    ContinuousRequestTarget_ProcessAsynchronousRequestStream(dmfModule,
                                                             Request,
                                                             CompletionParams);

    FuncExitVoid(DMF_TRACE_ContinuousRequestTarget);
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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

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
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfIoTargetFormatRequestForWrite fails: ntStatus=%!STATUS!", ntStatus);
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
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfIoTargetFormatRequestForRead fails: ntStatus=%!STATUS!", ntStatus);
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
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfIoTargetFormatRequestForIoctl fails: ntStatus=%!STATUS!", ntStatus);
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
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfIoTargetFormatRequestForInternalIoctl fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
            break;
        }
#endif // !defined(DMF_USER_MODE)
        default:
        {
            ntStatus = STATUS_INVALID_PARAMETER;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "Invalid RequestType:%d fails: ntStatus=%!STATUS!", RequestType, ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE_ContinuousRequestTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

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
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "DMF_BufferPool_GetWithMemory fails: ntStatus=%!STATUS!", ntStatus);
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
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "DMF_BufferPool_GetWithMemory fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "ContinuousRequestTarget_FormatRequestForRequestType fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE_ContinuousRequestTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
ContinuousRequestTarget_StreamRequestCreateAndSend(
    _In_ DMFMODULE DmfModule,
    _In_opt_ WDFREQUEST Request
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
    NTSTATUS ntStatus = STATUS_SUCCESS;
    WDF_REQUEST_REUSE_PARAMS  requestParams;
    WDF_OBJECT_ATTRIBUTES requestAttributes;
    BOOLEAN requestSendResult;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_AttachedDeviceGet(DmfModule);

    // If Request is NULL, it means that a request should be created. Otherwise, it means
    // that Request should be reused.
    //
    if (NULL == Request)
    {
        WDF_OBJECT_ATTRIBUTES_INIT(&requestAttributes);
        requestAttributes.ParentObject = DmfModule;

        ntStatus = WdfRequestCreate(&requestAttributes,
                                    moduleContext->IoTarget,
                                    &Request);
        if (! NT_SUCCESS(ntStatus))
        {
             TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfRequestCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }
    else
    {
        WDF_REQUEST_REUSE_PARAMS_INIT(&requestParams,
                                      WDF_REQUEST_REUSE_NO_FLAGS,
                                      STATUS_SUCCESS);

        ntStatus = WdfRequestReuse(Request,
                                   &requestParams);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfRequestReuse fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    ntStatus = ContinuousRequestTarget_CreateBuffersAndFormatRequestForRequestType(DmfModule,
                                                                                   Request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "ContinuousRequestTarget_CreateBuffersAndFormatRequestForRequestType fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Set a CompletionRoutine callback function. It goes back into this Module which will
    // dispatch to the Client.
    //
    WdfRequestSetCompletionRoutine(Request,
                                   moduleContext->CompletionRoutineStream,
                                   (WDFCONTEXT)(DmfModule));

    // Send the request - Asynchronous call, so check for Status if it fails.
    // If it succeeds, the Status will be checked in Completion Routine.
    //
    requestSendResult = WdfRequestSend(Request,
                                       moduleContext->IoTarget,
                                       WDF_NO_SEND_OPTIONS);
    if (requestSendResult == FALSE)
    {
        ntStatus = WdfRequestGetStatus(Request);
        ASSERT(! NT_SUCCESS(ntStatus));
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfRequestSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    if (! NT_SUCCESS(ntStatus))
    {
        if (Request != NULL)
        {
            WdfObjectDelete(Request);
            Request = NULL;
        }
    }

    FuncExit(DMF_TRACE_ContinuousRequestTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
#pragma warning(suppress:6101)
// Prevent SAL "returning uninitialized memory" error.
// Buffer is associated with request object.
//
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
    _Out_opt_ size_t* BytesWritten,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SingleAsynchronousBufferOutput* EvtContinuousRequestTargetSingleAsynchronousRequest,
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
    BytesWritten - Bytes returned by the transaction.

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
    ContinuousRequestTarget_SingleAsynchronousRequestContext* singleAsynchronousRequestContext;
    VOID* singleBufferContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    outputBufferSize = 0;
    requestSendResult = FALSE;

    ASSERT((IsSynchronousRequest && (EvtContinuousRequestTargetSingleAsynchronousRequest == NULL)) ||
           (! IsSynchronousRequest));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->IoTarget != NULL);

    device = DMF_AttachedDeviceGet(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT(&requestAttributes);
    requestAttributes.ParentObject = DmfModule;
    request = NULL;
    ntStatus = WdfRequestCreate(&requestAttributes,
                                moduleContext->IoTarget,
                                &request);
    if (! NT_SUCCESS(ntStatus))
    {
        request = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfRequestCreate fails: ntStatus=%!STATUS!", ntStatus);
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
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
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
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfMemoryCreate for position fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "ContinuousRequestTarget_FormatRequestForRequestType fails: ntStatus=%!STATUS!", ntStatus);
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
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "DMF_BufferPool_GetWithMemory fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        singleAsynchronousRequestContext->DmfModule = DmfModule;
        singleAsynchronousRequestContext->SingleAsynchronousCallbackClientContext = SingleAsynchronousRequestClientContext;
        singleAsynchronousRequestContext->EvtContinuousRequestTargetSingleAsynchronousRequest = EvtContinuousRequestTargetSingleAsynchronousRequest;
        singleAsynchronousRequestContext->SingleAsynchronousRequestType = RequestType;

        // Set the completion routine to internal completion routine of this Module.
        //
        WdfRequestSetCompletionRoutine(request,
                                       moduleContext->CompletionRoutineSingle,
                                       singleAsynchronousRequestContext);
    }

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                         WDF_REL_TIMEOUT_IN_MS(RequestTimeoutMilliseconds));

    ntStatus = WdfRequestAllocateTimer(request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfRequestAllocateTimer fails: ntStatus=%!STATUS!", ntStatus);
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
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfRequestGetStatus returned ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
        else
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_ContinuousRequestTarget, "WdfRequestSend completed with ntStatus=%!STATUS!", ntStatus);
            outputBufferSize = WdfRequestGetInformation(request);
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
    else if (! IsSynchronousRequest && 
             ! NT_SUCCESS(ntStatus) && 
             request != NULL)
    {
        // Delete the request if Asynchronous request failed.
        //
        WdfObjectDelete(request);
        request = NULL;
    }

    FuncExit(DMF_TRACE_ContinuousRequestTarget, "ntStatus=%!STATUS!", ntStatus);

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

    ContinuousRequestTarget_ProcessAsynchronousRequestStream(dmfModuleParent,
                                                             workitemContext->Request,
                                                             &workitemContext->RequestCompletionParams);

    return ScheduledTask_WorkResult_Success;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Start the target on any power transition other than cold boot.
    // if the PurgeAndStartTargetInD0Callbacks is set to true.
    //
    if (moduleConfig->PurgeAndStartTargetInD0Callbacks &&
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

    FuncExit(DMF_TRACE_ContinuousRequestTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    UNREFERENCED_PARAMETER(TargetState);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (moduleConfig->PurgeAndStartTargetInD0Callbacks &&
        (moduleContext->IoTarget != NULL))
    {
        WdfIoTargetPurge(moduleContext->IoTarget,
                         WdfIoTargetPurgeIoAndWait);
    }

    FuncExit(DMF_TRACE_ContinuousRequestTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_ContinuousRequestTarget;
static DMF_CALLBACKS_WDF DmfCallbacksWdf_ContinuousRequestTarget;

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
    DMFMODULE dmfModule;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    DMF_CONFIG_BufferPool moduleConfigBufferPool;
    DMF_CONFIG_QueuedWorkItem moduleConfigQueuedWorkItem;
    WDF_OBJECT_ATTRIBUTES attributes;
    DMF_MODULE_ATTRIBUTES moduleAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    DMF_CALLBACKS_WDF_INIT(&DmfCallbacksWdf_ContinuousRequestTarget);
    DmfCallbacksWdf_ContinuousRequestTarget.ModuleD0Entry = DMF_ContinuousRequestTarget_ModuleD0Entry;
    DmfCallbacksWdf_ContinuousRequestTarget.ModuleD0Exit = DMF_ContinuousRequestTarget_ModuleD0Exit;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_ContinuousRequestTarget,
                                            ContinuousRequestTarget,
                                            DMF_CONTEXT_ContinuousRequestTarget,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_ContinuousRequestTarget.CallbacksWdf = &DmfCallbacksWdf_ContinuousRequestTarget;
    DmfModuleDescriptor_ContinuousRequestTarget.ModuleConfigSize = sizeof(DMF_CONFIG_ContinuousRequestTarget);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_ContinuousRequestTarget,
                                &dmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    // dmfModule will be set as ParentObject for all child modules.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = dmfModule;

    // BufferPoolInput
    // ---------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = FALSE;
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = moduleConfig->BufferCountInput;
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = moduleConfig->PoolTypeInput;
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = moduleConfig->BufferInputSize;
    moduleConfigBufferPool.Mode.SourceSettings.BufferContextSize = moduleConfig->BufferContextInputSize;
    moduleAttributes.ClientModuleInstanceName = "BufferPoolInput";
    moduleAttributes.PassiveLevel = DmfModuleAttributes->PassiveLevel;
    ntStatus = DMF_BufferPool_Create(Device,
                                     &moduleAttributes,
                                     &attributes,
                                     &moduleContext->DmfModuleBufferPoolInput);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "DMF_BufferPool_Create fails: ntStatus=%!STATUS!", ntStatus);
        DMF_Module_Destroy(dmfModule);
        dmfModule = NULL;
        goto Exit;
    }

    // BufferPoolOutput
    // ----------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = moduleConfig->EnableLookAsideOutput;
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = moduleConfig->BufferCountOutput;
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = moduleConfig->PoolTypeOutput;
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = moduleConfig->BufferOutputSize;
    moduleConfigBufferPool.Mode.SourceSettings.BufferContextSize = moduleConfig->BufferContextOutputSize;
    moduleAttributes.ClientModuleInstanceName = "BufferPoolOutput";
    moduleAttributes.PassiveLevel = DmfModuleAttributes->PassiveLevel;
    ntStatus = DMF_BufferPool_Create(Device,
                                     &moduleAttributes,
                                     &attributes,
                                     &moduleContext->DmfModuleBufferPoolOutput);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "DMF_BufferPool_Create fails: ntStatus=%!STATUS!", ntStatus);
        DMF_Module_Destroy(dmfModule);
        dmfModule = NULL;
        goto Exit;
    }

    // BufferPoolContext
    // -----------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = 1;
    if (DmfModuleAttributes->PassiveLevel)
    {
        moduleConfigBufferPool.Mode.SourceSettings.PoolType = PagedPool;
    }
    else
    {
        moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    }
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = sizeof(ContinuousRequestTarget_SingleAsynchronousRequestContext);
    moduleAttributes.ClientModuleInstanceName = "BufferPoolContext";
    moduleAttributes.PassiveLevel = DmfModuleAttributes->PassiveLevel;
    ntStatus = DMF_BufferPool_Create(Device,
                                     &moduleAttributes,
                                     &attributes,
                                     &moduleContext->DmfModuleBufferPoolContext);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "DMF_BufferPool_Create fails: ntStatus=%!STATUS!", ntStatus);
        DMF_Module_Destroy(dmfModule);
        dmfModule = NULL;
        goto Exit;
    }

    if (DmfModuleAttributes->PassiveLevel)
    {
        moduleContext->CompletionRoutineSingle = ContinuousRequestTarget_CompletionRoutinePassive;
        moduleContext->CompletionRoutineStream = ContinuousRequestTarget_StreamCompletionRoutinePassive;
        // QueuedWorkItemStream
        // --------------------
        //
        DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT(&moduleConfigQueuedWorkItem,
                                                      &moduleAttributes);
        moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.BufferCount = DEFAULT_NUMBER_OF_PENDING_PASSIVE_LEVEL_COMPLETION_ROUTINES;
        moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.BufferSize = sizeof(ContinuousRequestTarget_QueuedWorkitemContext);
        // This has to be NonPagedPoolNx because completion routine runs at dispatch level.
        //
        moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
        moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
        moduleConfigQueuedWorkItem.EvtQueuedWorkitemFunction = ContinuousRequestTarget_QueuedWorkitemCallbackStream;
        ntStatus = DMF_QueuedWorkItem_Create(Device,
                                             &moduleAttributes,
                                             &attributes,
                                             &moduleContext->DmfModuleQueuedWorkitemStream);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "DMF_QueuedWorkItem_Create fails: ntStatus=%!STATUS!", ntStatus);
            DMF_Module_Destroy(dmfModule);
            dmfModule = NULL;
            goto Exit;
        }

        // QueuedWorkItemSingle
        // --------------------
        //
        DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT(&moduleConfigQueuedWorkItem,
                                                      &moduleAttributes);
        moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.BufferCount = DEFAULT_NUMBER_OF_PENDING_PASSIVE_LEVEL_COMPLETION_ROUTINES;
        moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.BufferSize = sizeof(ContinuousRequestTarget_QueuedWorkitemContext);
        // This has to be NonPagedPoolNx because completion routine runs at dispatch level.
        //
        moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
        moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
        moduleConfigQueuedWorkItem.EvtQueuedWorkitemFunction = ContinuousRequestTarget_QueuedWorkitemCallbackSingle;
        ntStatus = DMF_QueuedWorkItem_Create(Device,
                                             &moduleAttributes,
                                             &attributes,
                                             &moduleContext->DmfModuleQueuedWorkitemSingle);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "DMF_QueuedWorkItem_Create fails: ntStatus=%!STATUS!", ntStatus);
            DMF_Module_Destroy(dmfModule);
            dmfModule = NULL;
            goto Exit;
        }
    }
    else
    {
        moduleContext->CompletionRoutineSingle = ContinuousRequestTarget_CompletionRoutine;
        moduleContext->CompletionRoutineStream = ContinuousRequestTarget_StreamCompletionRoutine;
    }

Exit:

    *DmfModule = dmfModule;

    FuncExit(DMF_TRACE_ContinuousRequestTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_ContinuousRequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolOutput,
                       ClientBuffer);

    FuncExitVoid(DMF_TRACE_ContinuousRequestTarget);
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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_ContinuousRequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(moduleContext->IoTarget != NULL);

    moduleContext->IoTarget = NULL;

    FuncExitVoid(DMF_TRACE_ContinuousRequestTarget);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
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
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_ContinuousRequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(IoTarget != NULL);
    ASSERT(moduleContext->IoTarget == NULL);

    moduleContext->IoTarget = IoTarget;

    FuncExitVoid(DMF_TRACE_ContinuousRequestTarget);
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
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SingleAsynchronousBufferOutput* EvtContinuousRequestTargetSingleAsynchronousRequest,
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
    NTSTATUS ntStatus = STATUS_SUCCESS;

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_ContinuousRequestTarget);

    ntStatus = ContinuousRequestTarget_RequestCreateAndSend(DmfModule,
                                                            FALSE,
                                                            RequestBuffer,
                                                            RequestLength,
                                                            ResponseBuffer,
                                                            ResponseLength,
                                                            RequestType,
                                                            RequestIoctl,
                                                            RequestTimeoutMilliseconds,
                                                            NULL,
                                                            EvtContinuousRequestTargetSingleAsynchronousRequest,
                                                            SingleAsynchronousRequestClientContext);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "ContinuousRequestTarget_RequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
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

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_ContinuousRequestTarget);

    ntStatus = ContinuousRequestTarget_RequestCreateAndSend(DmfModule,
                                                            TRUE,
                                                            RequestBuffer,
                                                            RequestLength,
                                                            ResponseBuffer,
                                                            ResponseLength,
                                                            RequestType,
                                                            RequestIoctl,
                                                            RequestTimeoutMilliseconds,
                                                            BytesWritten,
                                                            NULL,
                                                            NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "ContinuousRequestTarget_RequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
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
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONFIG_ContinuousRequestTarget* moduleConfig;
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_ContinuousRequestTarget);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "WdfIoTargetStart fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    for (UINT requestIndex = 0; requestIndex < moduleConfig->ContinuousRequestCount; requestIndex++)
    {
        ntStatus = ContinuousRequestTarget_StreamRequestCreateAndSend(DmfModule,
                                                                      NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_ContinuousRequestTarget, "ContinuousRequestTarget_StreamRequestCreateAndSend fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
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

    VOID.

--*/
{
    DMF_CONTEXT_ContinuousRequestTarget* moduleContext;

    FuncEntry(DMF_TRACE_ContinuousRequestTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_ContinuousRequestTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->IoTarget != NULL);
}

// eof: Dmf_ContinuousRequestTarget.c
//
