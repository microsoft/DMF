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

#include "Dmf_RequestTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define DEFAULT_NUMBER_OF_PENDING_PASSIVE_LEVEL_COMPLETION_ROUTINES 4

typedef struct
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
    ASSERT(singleAsynchronousRequestContext != NULL);

    dmfModule = singleAsynchronousRequestContext->DmfModule;
    ASSERT(dmfModule != NULL);

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
    ASSERT(singleAsynchronousRequestContext != NULL);

    dmfModule = singleAsynchronousRequestContext->DmfModule;
    ASSERT(dmfModule != NULL);

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
    _In_opt_ WDFMEMORY    InputMemory,
    _In_opt_ WDFMEMORY    OutputMemory
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
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONTEXT_RequestTarget* moduleContext;

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

#pragma code_seg("PAGE")
#pragma warning(suppress:6101)
// Prevent SAL "returning uninitialized memory" error.
// Buffer is associated with request object.
//
static
NTSTATUS
RequestTarget_RequestCreateAndSend(
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
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
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
    DMF_CONTEXT_RequestTarget* moduleContext;
    WDFDEVICE device;
    EVT_WDF_REQUEST_COMPLETION_ROUTINE* completionRoutineSingle;
    RequestTarget_SingleAsynchronousRequestContext* singleAsynchronousRequestContext;
    VOID* singleBufferContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    outputBufferSize = 0;
    requestSendResult = FALSE;

    ASSERT((IsSynchronousRequest && (EvtRequestTargetSingleAsynchronousRequest == NULL)) ||
           (! IsSynchronousRequest));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->IoTarget != NULL);

    device = DMF_ParentDeviceGet(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT(&requestAttributes);
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
            ASSERT(FALSE);
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
#pragma code_seg()

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
    if (DmfParentModuleAttributes->PassiveLevel)
    {
        moduleConfigBufferPool.Mode.SourceSettings.PoolType = PagedPool;
    }
    else
    {
        moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    }
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
    ASSERT(IoTarget != NULL);
    ASSERT(moduleContext->IoTarget == NULL);

    moduleContext->IoTarget = IoTarget;

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
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
    RequestLength - Number of bytes to in RequestBuffer to send.
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
    ContinuousRequestTarget_CompletionOptions completionOption;
    NTSTATUS ntStatus;

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
                                                  SingleAsynchronousRequestClientContext);
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
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_ ContinuousRequestTarget_CompletionOptions CompletionOption,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    )
/*++

Routine Description:

    Creates and sends a Asynchronous request to the IoTarget given a buffer, IOCTL and other information.
    And once the request is complete, EvtContinuousRequestTargetSingleAsynchronousRequest will be called at Passive level.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes to in RequestBuffer to send.
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
    NTSTATUS ntStatus = STATUS_SUCCESS;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 RequestTarget);

    ntStatus = RequestTarget_RequestCreateAndSend(DmfModule,
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
                                                  EvtRequestTargetSingleAsynchronousRequest,
                                                  SingleAsynchronousRequestClientContext);
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
