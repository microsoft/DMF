/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ContinuousRequestTarget.h

Abstract:

    Companion file to Dmf_ContinuousRequestTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Enum to specify the type of request
//
typedef enum
{
    ContinuousRequestTarget_RequestType_Invalid,
    ContinuousRequestTarget_RequestType_InternalIoctl,
    ContinuousRequestTarget_RequestType_Ioctl,
    ContinuousRequestTarget_RequestType_Read,
    ContinuousRequestTarget_RequestType_Write
} ContinuousRequestTarget_RequestType;

// Enum to specify who owns the buffer and whether to continue streaming the request or stop
// The streaming callback function returns this value.
//
typedef enum
{
    ContinuousRequestTarget_BufferDisposition_Invalid,
    // Module owns the buffer and continue streaming.
    //
    ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming,
    // Module owns the buffer and stop streaming.
    //
    ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming,
    // Client owns the buffer and continue streaming.
    //
    ContinuousRequestTarget_BufferDisposition_ClientAndContinueStreaming,
    // Client owns the buffer and stop streaming.
    //
    ContinuousRequestTarget_BufferDisposition_ClientAndStopStreaming,
    ContinuousRequestTarget_BufferDisposition_Maximum
} ContinuousRequestTarget_BufferDisposition;

// Client Driver callback function to fill input buffer.
//
typedef
_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferInput)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ContinuousRequestTarget_BufferInput(_In_ DMFMODULE DmfModule,
                                            _Out_writes_(*InputBufferSize) VOID* InputBuffer,
                                            _Out_ size_t* InputBufferSize,
                                            _In_ VOID* ClientBuferContextInput);

// Client Driver callback function to be called from pending request completion routine.
//
typedef
_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferOutput)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ContinuousRequestTarget_BufferDisposition
EVT_DMF_ContinuousRequestTarget_BufferOutput(_In_ DMFMODULE DmfModule,
                                             _In_reads_(OutputBufferSize) VOID* OutputBuffer,
                                             _In_ size_t OutputBufferSize,
                                             _In_ VOID* ClientBufferContextOutput,
                                             _In_ NTSTATUS CompletionStatus);

// Client Driver callback function to be called from single request completion routine.
//
typedef
_Function_class_(EVT_DMF_ContinuousRequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ContinuousRequestTarget_SendCompletion(_In_ DMFMODULE DmfModule,
                                               _In_ VOID* ClientRequestContext,
                                               _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
                                               _In_ size_t InputBufferBytesWritten,
                                               _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
                                               _In_ size_t OutputBufferBytesRead,
                                               _In_ NTSTATUS CompletionStatus);

typedef enum
{
    // EVT_DMF_ContinuousRequestTarget_SendCompletion will be called at dispatch level.
    //
    ContinuousRequestTarget_CompletionOptions_Dispatch = 0,
    ContinuousRequestTarget_CompletionOptions_Default = 0,
    // EVT_DMF_ContinuousRequestTarget_SendCompletion will be called at passive level.
    //
    ContinuousRequestTarget_CompletionOptions_Passive,
    ContinuousRequestTarget_CompletionOptions_Maximum,
} ContinuousRequestTarget_CompletionOptions;

// These definitions indicate the mode of ContinuousRequestTarget.
// Indicates how and when the Requests start and stop streaming.
//
typedef enum
{
    // DMF_ContinuousRequestTarget_Start and DMF_ContinuousRequestTarget_Stop must be called explicitly
    // by the Client when needed.
    //
    ContinuousRequestTarget_Mode_Manual = 0,
    // DMF_ContinuousRequestTarget_Start invoked automatically on DMF_ContinuousRequestTarget_IoTargetSet and
    // DMF_ContinuousRequestTarget_Stop invoked automatically on DMF_ContinuousRequestTarget_IoTargetClear.
    //
    ContinuousRequestTarget_Mode_Automatic,
    ContinuousRequestTarget_Mode_Maximum,
} ContinuousRequestTarget_ModeType;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Number of Asynchronous requests.
    //
    ULONG ContinuousRequestCount;
    // Number of Input Buffers.
    //
    ULONG BufferCountInput;
    // Number of Output Buffers.
    //
    ULONG BufferCountOutput;
    // Request type.
    //
    ContinuousRequestTarget_RequestType RequestType;
    // Size of input buffer for each request.
    //
    ULONG BufferInputSize;
    // Size of Client Driver Input Buffer Context.
    //
    ULONG BufferContextInputSize;
    // Size of output buffer for each request.
    //
    ULONG BufferOutputSize;
    // Size of Client Driver Output Buffer Context.
    //
    ULONG BufferContextOutputSize;
    // Indicates if a look aside list should be used for output buffer
    // This is not required for input buffer since input is not cached.
    //
    ULONG EnableLookAsideOutput;
    // Pool Type for Input Buffer.
    //
    POOL_TYPE PoolTypeInput;
    // Pool Type for Output Buffer.
    //
    POOL_TYPE PoolTypeOutput;
    // Input buffer callback.
    //
    EVT_DMF_ContinuousRequestTarget_BufferInput* EvtContinuousRequestTargetBufferInput;
    // Output buffer callback.
    //
    EVT_DMF_ContinuousRequestTarget_BufferOutput* EvtContinuousRequestTargetBufferOutput;
    // IOCTL to send for ContinuousRequestTarget_RequestType_Ioctl or ContinuousRequestTarget_RequestType_InternalIoctl.
    //
    ULONG ContinuousRequestTargetIoctl;
    // Flag to indicate whether to Purge target in D0Exit and Start in D0Entry
    // This flag should be set to TRUE, if IO target needs to process all the requests
    // before entering low power.
    // NOTE: This flag will affect all instances of the Module on running on the same target.
    //
    BOOLEAN PurgeAndStartTargetInD0Callbacks;
    // Flag to indicate whether to Cancel all this Module's instance' WDFREQUESTS target in D0Exit 
    // and send them down again in D0Entry. When, possible use this flag as it only affect a single
    // instance of the Module.
    //
    BOOLEAN CancelAndResendRequestInD0Callbacks;
    // Indicates the mode of ContinuousRequestTarget.
    //
    ContinuousRequestTarget_ModeType ContinuousRequestTargetMode;
} DMF_CONFIG_ContinuousRequestTarget;

// This macro declares the following functions:
// DMF_ContinuousRequestTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_ContinuousRequestTarget_AND_ATTRIBUTES_INIT()
// DMF_ContinuousRequestTarget_Create()
//
DECLARE_DMF_MODULE(ContinuousRequestTarget)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ContinuousRequestTarget_BufferPut(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ContinuousRequestTarget_IoTargetClear(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_IoTargetSet(
    _In_ DMFMODULE DmfModule,
    _In_ WDFIOTARGET IoTarget
    );

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
    );

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
    );

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
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_Start(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContinuousRequestTarget_Stop(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContinuousRequestTarget_StopAndWait(
    _In_ DMFMODULE DmfModule
    );

// eof: Dmf_ContinuousRequestTarget.h
//
