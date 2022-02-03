/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_DeviceInterfaceTarget.c

Abstract:

    Functional tests for Dmf_DeviceInterfaceTarget Module.

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
#include "Dmf_Tests_DeviceInterfaceTarget.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include <initguid.h>

// Define TEST_SIMPLE to only execute simple tests that test cancel.
//
#define NO_TEST_CANCEL_NORMAL
#define NO_TEST_SIMPLE

// Choose one of these or none of these.
//
#define NO_TEST_SYNCHRONOUS_ONLY
#define NO_TEST_ASYNCHRONOUS_ONLY
#define NO_TEST_ASYNCHRONOUSCANCEL_ONLY

// {5F4F3758-D11E-4684-B5AD-FE6D19D82A51}
//
DEFINE_GUID(GUID_NO_DEVICE, 0x5f4f3758, 0xd11e, 0x4684, 0xb5, 0xad, 0xfe, 0x6d, 0x19, 0xd8, 0x2a, 0x51);

#define THREAD_COUNT                            (1)
#define MAXIMUM_SLEEP_TIME_MS                   (15000)
// Keep synchronous maximum time short to make driver disable faster.
//
#if !defined(TEST_SIMPLE)
#define MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS       (1000)
#else
#define MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS       (30000)
#endif

// Asynchronous minimum sleep time to make sure request can be canceled.
//
#define MINIMUM_SLEEP_TIME_MS                   (4000)

// Random timeouts for IOCTLs sent.
//
#define TIMEOUT_FAST_MS             100
#define TIMEOUT_SLOW_MS             5000
#define TIMEOUT_TRAFFIC_DELAY_MS    1000
#define TIMEOUT_CANCEL_MS           15
#define TIMEOUT_CANCEL_LONG_MS      250

#define NUMBER_OF_CONTINUOUS_REQUESTS   32

typedef enum _TEST_ACTION
{
    TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUSCANCEL,
    TEST_ACTION_DYNAMIC,
    TEST_ACTION_COUNT,
    TEST_ACTION_MINIUM = TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_MAXIMUM = TEST_ACTION_DYNAMIC
} TEST_ACTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Tests_DeviceInterfaceTarget
{
    // Modules under test.
    //
    DMFMODULE DmfModuleDeviceInterfaceTargetDispatchInput;
    DMFMODULE DmfModuleDeviceInterfaceTargetPassiveInput;
    DMFMODULE DmfModuleDeviceInterfaceTargetPassiveOutput;
    // Source of buffers sent asynchronously.
    //
    DMFMODULE DmfModuleBufferPool;
    // Work threads that perform actions on the DeviceInterfaceTarget Module.
    // +1 makes it easy to set THREAD_COUNT = 0 for test purposes.
    //
    DMFMODULE DmfModuleThreadDispatchInput[THREAD_COUNT + 1];
    DMFMODULE DmfModuleThreadPassiveInput[THREAD_COUNT + 1];
    DMFMODULE DmfModuleThreadPassiveOutput[THREAD_COUNT + 1];
    // Use alertable sleep to allow driver to unload faster.
    //
    DMFMODULE DmfModuleAlertableSleepDispatchInput[THREAD_COUNT + 1];
    DMFMODULE DmfModuleAlertableSleepPassiveInput[THREAD_COUNT + 1];
    DMFMODULE DmfModuleAlertableSleepPassiveOutput[THREAD_COUNT + 1];
} DMF_CONTEXT_Tests_DeviceInterfaceTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_DeviceInterfaceTarget)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_DeviceInterfaceTarget)

// Memory Pool Tag.
//
#define MemoryTag 'TiDT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Stores the Module threadIndex so that the corresponding alterable sleep
// can be retrieved inside the thread's callback.
//
typedef struct
{
    DMFMODULE DmfModuleAlertableSleep;
} Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE(Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT);

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferInput)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceTarget_BufferInput(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(*InputBufferSize) VOID* InputBuffer,
    _Out_ size_t* InputBufferSize,
    _In_ VOID* ClientBuferContextInput
    )
{
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    GUID guid;

    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(ClientBuferContextInput);

    DMF_DeviceInterfaceTarget_GuidGet(DmfModule,
                                      &guid);

    sleepIoctlBuffer.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0,
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    RtlCopyMemory(InputBuffer,
                  &sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));
    *InputBufferSize = sizeof(sleepIoctlBuffer);
}

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferOutput)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ContinuousRequestTarget_BufferDisposition
Tests_DeviceInterfaceTarget_BufferOutput(
    _In_ DMFMODULE DmfModule,
    _In_reads_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _In_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    )
{
    GUID guid;
    ContinuousRequestTarget_BufferDisposition bufferDisposition;

    UNREFERENCED_PARAMETER(ClientBufferContextOutput);
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(CompletionStatus);

    bufferDisposition = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;

    DMF_DeviceInterfaceTarget_GuidGet(DmfModule,
                                      &guid);

    DmfAssert(NT_SUCCESS(CompletionStatus) ||
              (CompletionStatus == STATUS_CANCELLED) ||
              (CompletionStatus == STATUS_INVALID_DEVICE_STATE));
    if (OutputBufferSize != sizeof(DWORD) &&
        CompletionStatus != STATUS_INVALID_DEVICE_STATE)
    {
        // Request can be completed with InformationSize of 0 by framework.
        // This can happen during suspend/resume of machine.
        //
    }
    if (NT_SUCCESS(CompletionStatus))
    {
        if (*((DWORD*)OutputBuffer) != 0)
        {
            DmfAssert(FALSE);
        }
    }

    // If IoTarget is closing but streaming has not been stopped, ContinuousRequestTarget will continue to send the request
    // back to the closing IoTarget if we don't stop streaming here.
    //
    if (! NT_SUCCESS(CompletionStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Completed Request CompletionStatus=%!STATUS!", CompletionStatus);
        bufferDisposition = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
    }

    return bufferDisposition;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
Tests_DeviceInterfaceTarget_ThreadAction_Synchronous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DMFMODULE DmfModuleDeviceInterfaceTarget
    )
{
    NTSTATUS ntStatus;
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;
    ULONG timeoutMs;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(DmfModuleAlertableSleep);

    PAGED_CODE();

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

#if defined(TEST_CANCEL_NORMAL)
    // Test buffer never completes, always cancels.
    //
    sleepIoctlBuffer.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(MINIMUM_SLEEP_TIME_MS, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    timeoutMs = TIMEOUT_CANCEL_MS;
#else
    sleepIoctlBuffer.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    timeoutMs = 0;
#endif
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendSynchronously(DmfModuleDeviceInterfaceTarget,
                                                           &sleepIoctlBuffer,
                                                           sizeof(Tests_IoctlHandler_Sleep),
                                                           NULL,
                                                           NULL,
                                                           ContinuousRequestTarget_RequestType_Ioctl,
                                                           IOCTL_Tests_IoctlHandler_SLEEP,
                                                           timeoutMs,
                                                           &bytesWritten);
    DmfAssert(NT_SUCCESS(ntStatus) ||
              (ntStatus == STATUS_CANCELLED) ||
              (ntStatus == STATUS_INVALID_DEVICE_STATE) ||
              (ntStatus == STATUS_DELETE_PENDING));
    // TODO: Get time and compare with send time.
    //

#if defined(TEST_CANCEL_NORMAL)
    // Test buffer always completes, no timeout.
    //
    sleepIoctlBuffer.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 TIMEOUT_CANCEL_LONG_MS);
    timeoutMs = MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS;
#else
    sleepIoctlBuffer.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    timeoutMs = 0;
#endif
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendSynchronously(DmfModuleDeviceInterfaceTarget,
                                                           &sleepIoctlBuffer,
                                                           sizeof(Tests_IoctlHandler_Sleep),
                                                           NULL,
                                                           NULL,
                                                           ContinuousRequestTarget_RequestType_Ioctl,
                                                           IOCTL_Tests_IoctlHandler_SLEEP,
                                                           timeoutMs,
                                                           &bytesWritten);
    DmfAssert(NT_SUCCESS(ntStatus) ||
              (ntStatus == STATUS_CANCELLED) ||
              (ntStatus == STATUS_INVALID_DEVICE_STATE) ||
              (ntStatus == STATUS_DELETE_PENDING));
    // TODO: Get time and compare with send time.
    //
}
#pragma code_seg()

_Function_class_(EVT_DMF_ContinuousRequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceTarget_SendCompletion(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    )
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;

    // TODO: Get time and compare with send time.
    //
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferBytesWritten);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferBytesRead);
    UNREFERENCED_PARAMETER(CompletionStatus);

    moduleContext = (DMF_CONTEXT_Tests_DeviceInterfaceTarget*)ClientRequestContext;
    sleepIoctlBuffer = (Tests_IoctlHandler_Sleep*)InputBuffer;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DI: RECEIVE sleepIoctlBuffer->TimeToSleepMilliseconds=%d InputBuffer=0x%p CompletionStatus=%!STATUS!", 
                sleepIoctlBuffer->TimeToSleepMilliseconds,
                InputBuffer,
                CompletionStatus);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                       (VOID*)sleepIoctlBuffer);
}

_Function_class_(EVT_DMF_ContinuousRequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceTarget_SendCompletionMustBeCancelled(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    )
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;

    // TODO: Get time and compare with send time.
    //
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(InputBufferBytesWritten);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferBytesRead);
    UNREFERENCED_PARAMETER(CompletionStatus);

    moduleContext = (DMF_CONTEXT_Tests_DeviceInterfaceTarget*)ClientRequestContext;
    sleepIoctlBuffer = (Tests_IoctlHandler_Sleep*)InputBuffer;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DI: CANCELED sleepIoctlBuffer->TimeToSleepMilliseconds=%d InputBuffer=0x%p", 
                sleepIoctlBuffer->TimeToSleepMilliseconds,
                InputBuffer);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                       (VOID*)sleepIoctlBuffer);

#if !defined(DMF_WIN32_MODE)
    DmfAssert(STATUS_CANCELLED == CompletionStatus);
#endif
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
Tests_DeviceInterfaceTarget_ThreadAction_Asynchronous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DMFMODULE DmfModuleDeviceInterfaceTarget
    )
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    size_t bytesWritten;
    ULONG timeoutMs;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;
    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    RtlZeroMemory(sleepIoctlBuffer,
                  sizeof(Tests_IoctlHandler_Sleep));

#if !defined(TEST_CANCEL_NORMAL)
    if (TestsUtility_GenerateRandomNumber(0,
                                          1))
    {
        timeoutMs = TestsUtility_GenerateRandomNumber(TIMEOUT_FAST_MS,
                                                      TIMEOUT_SLOW_MS);
    }
    else
    {
        timeoutMs = 0;
    }
#else
    timeoutMs = TIMEOUT_CANCEL_MS;
#endif

    sleepIoctlBuffer->TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                  MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(DmfModuleDeviceInterfaceTarget,
                                              sleepIoctlBuffer,
                                              sizeof(Tests_IoctlHandler_Sleep),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              timeoutMs,
                                              Tests_DeviceInterfaceTarget_SendCompletion,
                                              moduleContext);
    DmfAssert(NT_SUCCESS(ntStatus) ||
              (ntStatus == STATUS_CANCELLED) ||
              (ntStatus == STATUS_INVALID_DEVICE_STATE) ||
              (ntStatus == STATUS_DELETE_PENDING));

    // Reduce traffic to reduce CPU usage and make debugging easier.
    //
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        1000);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:
    ;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
Tests_DeviceInterfaceTarget_ThreadAction_AsynchronousCancel(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DMFMODULE DmfModuleDeviceInterfaceTarget
    )
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    size_t bytesWritten;
    RequestTarget_DmfRequest DmfRequestId;
    BOOLEAN requestCanceled;
    LONG timeToSleepMilliseconds;
    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request after it is normally completed. It should never cancel unless driver is shutting down.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(MINIMUM_SLEEP_TIME_MS, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DI: SEND: sleepIoctlBuffer->TimeToSleepMilliseconds=%d sleepIoctlBuffer=0x%p", 
                timeToSleepMilliseconds,
                sleepIoctlBuffer);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendEx(DmfModuleDeviceInterfaceTarget,
                                                sleepIoctlBuffer,
                                                sizeof(Tests_IoctlHandler_Sleep),
                                                NULL,
                                                NULL,
                                                ContinuousRequestTarget_RequestType_Ioctl,
                                                IOCTL_Tests_IoctlHandler_SLEEP,
                                                0,
                                                Tests_DeviceInterfaceTarget_SendCompletion,
                                                moduleContext,
                                                &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) ||
              (ntStatus == STATUS_CANCELLED) ||
              (ntStatus == STATUS_INVALID_DEVICE_STATE) ||
              (ntStatus == STATUS_DELETE_PENDING));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToSleepMilliseconds * 4);
    // Cancel the request if possible.
    // It should never cancel since the time just waited is 4 times what was sent above.
    //
    requestCanceled = DMF_DeviceInterfaceTarget_Cancel(DmfModuleDeviceInterfaceTarget,
                                                       DmfRequestId);
    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out.
        //
        goto Exit;
    }

#if !defined(DMF_WIN32_MODE)
    DmfAssert(!requestCanceled);
#endif

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DI: CANCELED: sleepIoctlBuffer->TimeToSleepMilliseconds=%d sleepIoctlBuffer=0x%p", 
                timeToSleepMilliseconds,
                sleepIoctlBuffer);

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request after waiting for a while. It may or may not be canceled.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendEx(DmfModuleDeviceInterfaceTarget,
                                                sleepIoctlBuffer,
                                                sizeof(Tests_IoctlHandler_Sleep),
                                                NULL,
                                                NULL,
                                                ContinuousRequestTarget_RequestType_Ioctl,
                                                IOCTL_Tests_IoctlHandler_SLEEP,
                                                0,
                                                Tests_DeviceInterfaceTarget_SendCompletion,
                                                moduleContext,
                                                &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) ||
              (ntStatus == STATUS_CANCELLED) ||
              (ntStatus == STATUS_INVALID_DEVICE_STATE) ||
              (ntStatus == STATUS_DELETE_PENDING));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToSleepMilliseconds);

    // Cancel the request if possible.
    //
    requestCanceled = DMF_DeviceInterfaceTarget_Cancel(DmfModuleDeviceInterfaceTarget,
                                                       DmfRequestId);

    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out.
        //
        goto Exit;
    }

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request after waiting the same time sent in timeout. 
    // It may or may not be canceled.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendEx(DmfModuleDeviceInterfaceTarget,
                                                sleepIoctlBuffer,
                                                sizeof(Tests_IoctlHandler_Sleep),
                                                NULL,
                                                NULL,
                                                ContinuousRequestTarget_RequestType_Ioctl,
                                                IOCTL_Tests_IoctlHandler_SLEEP,
                                                0,
                                                Tests_DeviceInterfaceTarget_SendCompletion,
                                                moduleContext,
                                                &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) ||
              (ntStatus == STATUS_CANCELLED) ||
              (ntStatus == STATUS_INVALID_DEVICE_STATE) ||
              (ntStatus == STATUS_DELETE_PENDING));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToSleepMilliseconds);

    // Cancel the request if possible.
    //
    requestCanceled = DMF_DeviceInterfaceTarget_Cancel(DmfModuleDeviceInterfaceTarget,
                                                       DmfRequestId);

    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out.
        //
        goto Exit;
    }

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request immediately after sending it. It may or may not be canceled.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendEx(DmfModuleDeviceInterfaceTarget,
                                                sleepIoctlBuffer,
                                                sizeof(Tests_IoctlHandler_Sleep),
                                                NULL,
                                                NULL,
                                                ContinuousRequestTarget_RequestType_Ioctl,
                                                IOCTL_Tests_IoctlHandler_SLEEP,
                                                0,
                                                Tests_DeviceInterfaceTarget_SendCompletion,
                                                moduleContext,
                                                &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) ||
              (ntStatus == STATUS_CANCELLED) ||
              (ntStatus == STATUS_INVALID_DEVICE_STATE) ||
              (ntStatus == STATUS_DELETE_PENDING));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Cancel the request immediately after sending it.
    //
    requestCanceled = DMF_DeviceInterfaceTarget_Cancel(DmfModuleDeviceInterfaceTarget,
                                                       DmfRequestId);

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request before it is normally completed. It should always cancel.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(MINIMUM_SLEEP_TIME_MS, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendEx(DmfModuleDeviceInterfaceTarget,
                                                sleepIoctlBuffer,
                                                sizeof(Tests_IoctlHandler_Sleep),
                                                NULL,
                                                NULL,
                                                ContinuousRequestTarget_RequestType_Ioctl,
                                                IOCTL_Tests_IoctlHandler_SLEEP,
                                                0,
                                                Tests_DeviceInterfaceTarget_SendCompletionMustBeCancelled,
                                                moduleContext,
                                                &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) ||
              (ntStatus == STATUS_CANCELLED) ||
              (ntStatus == STATUS_INVALID_DEVICE_STATE) ||
              (ntStatus == STATUS_DELETE_PENDING));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToSleepMilliseconds / 4);

    // Cancel the request if possible.
    // It should always cancel since the time just waited is 1/4 the time that was sent above.
    //
    requestCanceled = DMF_DeviceInterfaceTarget_Cancel(DmfModuleDeviceInterfaceTarget,
                                                       DmfRequestId);
    // Even though the attempt to cancel happens in 1/4 of the total time out, it is possible
    // that the cancel call happens just as the underlying driver is going away. In that case,
    // the request is not canceled by this call, but it will be canceled by the underlying
    // driver. (In this case the call to cancel returns FALSE.) Thus, no assert is possible here.
    // This case happens often as the underlying driver comes and goes every second.
    //

Exit:
    ;
}
#pragma code_seg()

// Forward declarations for Dynamic Module Test.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_DispatchInput(
    _In_ DMFMODULE DmfModule
    );
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_DispatchInput(
    _In_ DMFMODULE DmfModule
    );

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DeviceInterfaceTarget_ThreadAction_Dynamic(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep
    )
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    size_t bytesWritten;
    ULONG timeoutMs;
    WDFMEMORY memory;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    // Create a parent object for the Module Under Test.
    // Size does not matter because it is just used for parent object.
    //
    memory = NULL;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               '1234',
                               sizeof(VOID*),
                               &memory,
                               NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;
    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    RtlZeroMemory(sleepIoctlBuffer,
                  sizeof(Tests_IoctlHandler_Sleep));

#if !defined(TEST_CANCEL_NORMAL)
    if (TestsUtility_GenerateRandomNumber(0,
                                          1))
    {
        timeoutMs = TestsUtility_GenerateRandomNumber(TIMEOUT_FAST_MS,
                                                      TIMEOUT_SLOW_MS);
    }
    else
    {
        timeoutMs = 0;
    }
#else
    timeoutMs = TIMEOUT_CANCEL_MS;
#endif

    DMFMODULE dynamicDeviceInterfaceTarget;
    DMF_CONFIG_DeviceInterfaceTarget moduleConfigDeviceInterfaceTarget;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    WDFDEVICE device;

    device = DMF_ParentDeviceGet(DmfModule);
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = memory;

    // DeviceInterfaceTarget (DISPATCH_LEVEL)
    // Processes Input Buffers.
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
#if !defined(TEST_SIMPLE)
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DeviceInterfaceTarget_BufferInput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
#endif
    ntStatus = DMF_DeviceInterfaceTarget_Create(device,
                                                &moduleAttributes,
                                                &objectAttributes,
                                                &dynamicDeviceInterfaceTarget);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Wait for underlying target to open.
    //
    ULONG timeToWaitMs = TestsUtility_GenerateRandomNumber(0, 
                                                           MAXIMUM_SLEEP_TIME_MS);
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToWaitMs);

    // Send it some data asynchronously..
    //
    sleepIoctlBuffer->TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                  MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(dynamicDeviceInterfaceTarget,
                                              sleepIoctlBuffer,
                                              sizeof(Tests_IoctlHandler_Sleep),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              timeoutMs,
                                              Tests_DeviceInterfaceTarget_SendCompletion,
                                              moduleContext);
    DmfAssert(NT_SUCCESS(ntStatus) ||
              (ntStatus == STATUS_CANCELLED) ||
              (ntStatus == STATUS_INVALID_DEVICE_STATE) ||
              (ntStatus == STATUS_DELETE_PENDING));

    timeToWaitMs = TestsUtility_GenerateRandomNumber(0, 
                                                     MAXIMUM_SLEEP_TIME_MS);

    // Wait for a while.
    //
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToWaitMs);

Exit:

    if (memory != NULL)
    {
        // Delete the Dynamic Module by deleting its parent to execute the hardest path.
        //
        WdfObjectDelete(memory);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DeviceInterfaceTarget_WorkThreadDispatchInput(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    TEST_ACTION testAction;
    Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT* threadIndex;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    threadIndex = WdfObjectGet_Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

#if defined(TEST_SYNCHRONOUS_ONLY)
    testAction = TEST_ACTION_SYNCHRONOUS;
#elif defined(TEST_ASYNCHRONOUS_ONLY)
    testAction = TEST_ACTION_ASYNCHRONOUS;
#elif defined(TEST_ASYNCHRONOUSCANCEL_ONLY)
    testAction = TEST_ACTION_ASYNCHRONOUSCANCEL;
#else
    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);
#endif

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SYNCHRONOUS:
            Tests_DeviceInterfaceTarget_ThreadAction_Synchronous(dmfModule,
                                                                 threadIndex->DmfModuleAlertableSleep,
                                                                 moduleContext->DmfModuleDeviceInterfaceTargetDispatchInput);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DeviceInterfaceTarget_ThreadAction_Asynchronous(dmfModule,
                                                                  threadIndex->DmfModuleAlertableSleep,
                                                                  moduleContext->DmfModuleDeviceInterfaceTargetDispatchInput);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DeviceInterfaceTarget_ThreadAction_AsynchronousCancel(dmfModule,
                                                                        threadIndex->DmfModuleAlertableSleep,
                                                                        moduleContext->DmfModuleDeviceInterfaceTargetDispatchInput);
            break;
        case TEST_ACTION_DYNAMIC:
            Tests_DeviceInterfaceTarget_ThreadAction_Dynamic(dmfModule,
                                                             threadIndex->DmfModuleAlertableSleep);
            break;
        default:
            DmfAssert(FALSE);
            break;
    }

    // Repeat the test, until stop is signaled.
    //
    if (!DMF_Thread_IsStopPending(DmfModuleThread))
    {
        // Short delay to reduce traffic.
        //
        DMF_Utility_DelayMilliseconds(TIMEOUT_TRAFFIC_DELAY_MS);
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DeviceInterfaceTarget_WorkThreadPassiveInput(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    TEST_ACTION testAction;
    Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT* threadIndex;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    threadIndex = WdfObjectGet_Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

#if defined(TEST_SYNCHRONOUS_ONLY)
    testAction = TEST_ACTION_SYNCHRONOUS;
#elif defined(TEST_ASYNCHRONOUS_ONLY)
    testAction = TEST_ACTION_ASYNCHRONOUS;
#elif defined(TEST_ASYNCHRONOUSCANCEL_ONLY)
    testAction = TEST_ACTION_ASYNCHRONOUSCANCEL;
#else
    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);
#endif

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SYNCHRONOUS:
            Tests_DeviceInterfaceTarget_ThreadAction_Synchronous(dmfModule,
                                                                 threadIndex->DmfModuleAlertableSleep,
                                                                 moduleContext->DmfModuleDeviceInterfaceTargetPassiveInput);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DeviceInterfaceTarget_ThreadAction_Asynchronous(dmfModule,
                                                                  threadIndex->DmfModuleAlertableSleep,
                                                                  moduleContext->DmfModuleDeviceInterfaceTargetPassiveInput);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DeviceInterfaceTarget_ThreadAction_AsynchronousCancel(dmfModule,
                                                                        threadIndex->DmfModuleAlertableSleep,
                                                                        moduleContext->DmfModuleDeviceInterfaceTargetPassiveInput);
            break;
        case TEST_ACTION_DYNAMIC:
            Tests_DeviceInterfaceTarget_ThreadAction_Dynamic(dmfModule,
                                                             threadIndex->DmfModuleAlertableSleep);
            break;
        default:
            DmfAssert(FALSE);
            break;
    }

    // Repeat the test, until stop is signaled.
    //
    if (!DMF_Thread_IsStopPending(DmfModuleThread))
    {
        // Short delay to reduce traffic.
        //
        DMF_Utility_DelayMilliseconds(TIMEOUT_TRAFFIC_DELAY_MS);
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DeviceInterfaceTarget_WorkThreadPassiveOutput(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    TEST_ACTION testAction;
    Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT* threadIndex;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    threadIndex = WdfObjectGet_Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

#if defined(TEST_SYNCHRONOUS_ONLY)
    testAction = TEST_ACTION_SYNCHRONOUS;
#elif defined(TEST_ASYNCHRONOUS_ONLY)
    testAction = TEST_ACTION_ASYNCHRONOUS;
#elif defined(TEST_ASYNCHRONOUSCANCEL_ONLY)
    testAction = TEST_ACTION_ASYNCHRONOUSCANCEL;
#else
    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);
#endif

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SYNCHRONOUS:
            Tests_DeviceInterfaceTarget_ThreadAction_Synchronous(dmfModule,
                                                                 threadIndex->DmfModuleAlertableSleep,
                                                                 moduleContext->DmfModuleDeviceInterfaceTargetPassiveOutput);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DeviceInterfaceTarget_ThreadAction_Asynchronous(dmfModule,
                                                                  threadIndex->DmfModuleAlertableSleep,
                                                                  moduleContext->DmfModuleDeviceInterfaceTargetPassiveOutput);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DeviceInterfaceTarget_ThreadAction_AsynchronousCancel(dmfModule,
                                                                        threadIndex->DmfModuleAlertableSleep,
                                                                        moduleContext->DmfModuleDeviceInterfaceTargetPassiveOutput);
            break;
        case TEST_ACTION_DYNAMIC:
            Tests_DeviceInterfaceTarget_ThreadAction_Dynamic(dmfModule,
                                                             threadIndex->DmfModuleAlertableSleep);
            break;
        default:
            DmfAssert(FALSE);
            break;
    }

    // Repeat the test, until stop is signaled.
    //
    if (!DMF_Thread_IsStopPending(DmfModuleThread))
    {
        // Short delay to reduce traffic.
        //
        DMF_Utility_DelayMilliseconds(TIMEOUT_TRAFFIC_DELAY_MS);
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Tests_DeviceInterfaceTarget_StartDispatchInput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts the threads that send asynchronous data to the automatically started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceTarget.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadDispatchInput[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThreadDispatchInput[threadIndex]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_StopDispatchInput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops the threads that send asynchronous data to the automatically started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceTarget.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // Interrupt any long sleeps.
        //
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepDispatchInput[threadIndex],
                                 0);
        // Stop thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThreadDispatchInput[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Tests_DeviceInterfaceTarget_StartPassiveInput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts the threads that send asynchronous data to the manually started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceTarget.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadPassiveInput[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThreadPassiveInput[threadIndex]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Tests_DeviceInterfaceTarget_StartPassiveOutput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts the threads that send asynchronous data to the manually started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceTarget.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadPassiveOutput[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThreadPassiveOutput[threadIndex]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_StopPassiveInput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops the threads that send asynchronous data to the manually started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceTarget.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // Interrupt any long sleeps.
        //
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepPassiveInput[threadIndex],
                                 0);
        // Stop thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThreadPassiveInput[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_StopPassiveOutput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops the threads that send asynchronous data to the manually started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceTarget.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // Interrupt any long sleeps.
        //
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepPassiveOutput[threadIndex],
                                 0);
        // Stop thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThreadPassiveOutput[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_DispatchInput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Arrival Notification.
    This function starts the threads that send asynchronous data to automatically started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    PAGED_CODE();

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT* threadIndexContext;
        WDF_OBJECT_ATTRIBUTES objectAttributes;

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                               Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT);
        ntStatus = WdfObjectAllocateContext(moduleContext->DmfModuleThreadDispatchInput[threadIndex],
                                            &objectAttributes,
                                            (PVOID*)&threadIndexContext);
        DmfAssert(NT_SUCCESS(ntStatus));
        threadIndexContext->DmfModuleAlertableSleep = moduleContext->DmfModuleAlertableSleepDispatchInput[threadIndex];
        // Reset in case target comes and goes and comes back.
        //
        DMF_AlertableSleep_ResetForReuse(threadIndexContext->DmfModuleAlertableSleep,
                                         0);
    }

    // Start the threads. Streaming is automatically started.
    //
    ntStatus = Tests_DeviceInterfaceTarget_StartDispatchInput(dmfModuleParent);
    DmfAssert(NT_SUCCESS(ntStatus));
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_DispatchInput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Removal Notification.
    This function stops the threads that send asynchronous data to automatically started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModuleParent;
    NTSTATUS ntStatus;
    WDFIOTARGET ioTarget;

    PAGED_CODE();

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DMF_DeviceInterfaceTarget_Get(DmfModule,
                                             &ioTarget);
    if (NT_SUCCESS(ntStatus))
    {
        WdfIoTargetPurge(ioTarget,
                         WdfIoTargetPurgeIoAndWait);
    }

    DMF_ModuleDereference(DmfModule);

Exit:

    // Stop the threads. Streaming is automatically stopped.
    //
    Tests_DeviceInterfaceTarget_StopDispatchInput(dmfModuleParent);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_PassiveInput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Arrival Notification.
    Manually starts the manual DMF_DeviceInterfaceTarget Module.
    This function starts the threads that send asynchronous data to manually started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    PAGED_CODE();

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

#if !defined(TEST_SIMPLE)
    NTSTATUS ntStatus;

    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT* threadIndexContext;
        WDF_OBJECT_ATTRIBUTES objectAttributes;

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                               Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT);
        ntStatus = WdfObjectAllocateContext(moduleContext->DmfModuleThreadPassiveInput[threadIndex],
                                            &objectAttributes,
                                            (PVOID*)&threadIndexContext);
        DmfAssert(NT_SUCCESS(ntStatus));
        threadIndexContext->DmfModuleAlertableSleep = moduleContext->DmfModuleAlertableSleepPassiveInput[threadIndex];
        // Reset in case target comes and goes and comes back.
        //
        DMF_AlertableSleep_ResetForReuse(threadIndexContext->DmfModuleAlertableSleep,
                                         0);
    }

    // Start streaming.
    //
    ntStatus = DMF_DeviceInterfaceTarget_StreamStart(DmfModule);
    if (NT_SUCCESS(ntStatus))
    {
        // Start threads.
        //
        Tests_DeviceInterfaceTarget_StartPassiveInput(dmfModuleParent);
    }
    DmfAssert(NT_SUCCESS(ntStatus));

#endif
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_PassiveOutput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Arrival Notification.
    Manually starts the manual DMF_DeviceInterfaceTarget Module.
    This function starts the threads that send asynchronous data to manually started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    PAGED_CODE();

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT* threadIndexContext;
        WDF_OBJECT_ATTRIBUTES objectAttributes;

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                               Tests_DeviceInterfaceTarget_THREAD_INDEX_CONTEXT);
        ntStatus = WdfObjectAllocateContext(moduleContext->DmfModuleThreadPassiveOutput[threadIndex],
                                            &objectAttributes,
                                            (PVOID*)&threadIndexContext);
        DmfAssert(NT_SUCCESS(ntStatus));
        threadIndexContext->DmfModuleAlertableSleep = moduleContext->DmfModuleAlertableSleepPassiveOutput[threadIndex];
        // Reset in case target comes and goes and comes back.
        //
        DMF_AlertableSleep_ResetForReuse(threadIndexContext->DmfModuleAlertableSleep,
                                         0);
    }

    // Start streaming.
    //
    ntStatus = DMF_DeviceInterfaceTarget_StreamStart(DmfModule);
    if (NT_SUCCESS(ntStatus))
    {
        // Start threads.
        //
        Tests_DeviceInterfaceTarget_StartPassiveOutput(dmfModuleParent);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_PassiveInput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Removal Notification.
    Manually stops the manual DMF_DeviceInterfaceTarget Module.
    This function stops the threads that send asynchronous data to manually started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModuleParent;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

#if !defined(TEST_SIMPLE)
    WDFIOTARGET ioTarget;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DMF_DeviceInterfaceTarget_Get(DmfModule,
                                             &ioTarget);
    if (NT_SUCCESS(ntStatus))
    {
        WdfIoTargetPurge(ioTarget,
                         WdfIoTargetPurgeIoAndWait);
    }

    DMF_ModuleDereference(DmfModule);

Exit:

    // Stop streaming.
    //
    DMF_DeviceInterfaceTarget_StreamStop(DmfModule);
    // Stop threads.
    //
    Tests_DeviceInterfaceTarget_StopPassiveInput(dmfModuleParent);
#endif
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_PassiveOutput(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Removal Notification.
    Manually stops the manual DMF_DeviceInterfaceTarget Module.
    This function stops the threads that send asynchronous data to manually started
    DMF_DeviceInterfaceTarget Modules.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModuleParent;
    NTSTATUS ntStatus;
    WDFIOTARGET ioTarget;

    PAGED_CODE();

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DMF_DeviceInterfaceTarget_Get(DmfModule,
                                             &ioTarget);
    if (NT_SUCCESS(ntStatus))
    {
        WdfIoTargetPurge(ioTarget,
                         WdfIoTargetPurgeIoAndWait);
    }

    DMF_ModuleDereference(DmfModule);

Exit:

    // Stop streaming.
    //
    DMF_DeviceInterfaceTarget_StreamStop(DmfModule);
    // Stop threads.
    //
    Tests_DeviceInterfaceTarget_StopPassiveOutput(dmfModuleParent);
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
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_DeviceInterfaceTarget_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_Thread moduleConfigThread;
    DMF_CONFIG_DeviceInterfaceTarget moduleConfigDeviceInterfaceTarget;
    DMF_MODULE_EVENT_CALLBACKS moduleEventCallbacks;
    DMF_CONFIG_AlertableSleep moduleConfigAlertableSleep;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // General purpose buffers for asynchronous transactions.
    //
    DMF_CONFIG_BufferPool moduleConfigBufferPool;
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = 10;
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPool);

    // Thread
    // ------
    //
    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_DeviceInterfaceTarget_WorkThreadDispatchInput;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThreadDispatchInput[threadIndex]);

#if !defined(TEST_SIMPLE)
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_DeviceInterfaceTarget_WorkThreadPassiveInput;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThreadPassiveInput[threadIndex]);

        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_DeviceInterfaceTarget_WorkThreadPassiveOutput;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThreadPassiveOutput[threadIndex]);
#endif

        // AlertableSleep Auto
        // -------------------
        //
        DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                      &moduleAttributes);
        moduleConfigAlertableSleep.EventCount = 1;
        moduleAttributes.ClientModuleInstanceName = "AlertableSleep.Auto";
        DMF_DmfModuleAdd(DmfModuleInit,
                            &moduleAttributes,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &moduleContext->DmfModuleAlertableSleepDispatchInput[threadIndex]);

        // AlertableSleep Manual (Input)
        // ---------------------
        //
        DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                      &moduleAttributes);
        moduleConfigAlertableSleep.EventCount = 1;
        moduleAttributes.ClientModuleInstanceName = "AlertableSleep.ManualInput";
        DMF_DmfModuleAdd(DmfModuleInit,
                            &moduleAttributes,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &moduleContext->DmfModuleAlertableSleepPassiveInput[threadIndex]);

        // AlertableSleep Manual (Output)
        // ---------------------
        //
        DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                      &moduleAttributes);
        moduleConfigAlertableSleep.EventCount = 1;
        moduleAttributes.ClientModuleInstanceName = "AlertableSleep.ManualOutput";
        DMF_DmfModuleAdd(DmfModuleInit,
                            &moduleAttributes,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &moduleContext->DmfModuleAlertableSleepPassiveOutput[threadIndex]);
    }

    // DeviceInterfaceTarget (DISPATCH_LEVEL)
    // Processes Input Buffers.
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
#if !defined(TEST_SIMPLE)
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DeviceInterfaceTarget_BufferInput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
#endif
    DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(&moduleAttributes,
                                               &moduleEventCallbacks);
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_DispatchInput;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_DispatchInput;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceTargetDispatchInput);

    // DeviceInterfaceTarget (PASSIVE_LEVEL)
    // Processes Input Buffers.
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
#if !defined(TEST_SIMPLE)
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DeviceInterfaceTarget_BufferInput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Manual;
#endif
    DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(&moduleAttributes,
                                               &moduleEventCallbacks);
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_PassiveInput;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_PassiveInput;
    moduleAttributes.PassiveLevel = TRUE;
    moduleAttributes.ClientCallbacks = &moduleEventCallbacks;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceTargetPassiveInput);

#if !defined(TEST_SIMPLE)

    // DeviceInterfaceTarget (PASSIVE_LEVEL)
    // Processes Output Buffers.
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = NUMBER_OF_CONTINUOUS_REQUESTS;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(DWORD);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = NUMBER_OF_CONTINUOUS_REQUESTS;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_ZEROBUFFER;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = Tests_DeviceInterfaceTarget_BufferOutput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Manual;

    DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(&moduleAttributes,
                                               &moduleEventCallbacks);
    moduleAttributes.PassiveLevel = TRUE;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_PassiveOutput;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_PassiveOutput;
    moduleAttributes.ClientCallbacks = &moduleEventCallbacks;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceTargetPassiveOutput);

    // Test valid combinations of pool types.
    //

    // BufferPool=Paged
    // DeviceInterfaceTarget=Paged
    // Input
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = PagedPool;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DeviceInterfaceTarget_BufferInput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = TRUE;
    moduleAttributes.PassiveLevel = TRUE;
    moduleAttributes.ClientModuleInstanceName = "Input/Paged/Paged";
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // BufferPool=NonPagedNx
    // DeviceInterfaceTarget=Paged
    // Input
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DeviceInterfaceTarget_BufferInput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = TRUE;
    moduleAttributes.PassiveLevel = TRUE;
    moduleAttributes.ClientModuleInstanceName = "Input/NonPaged/Paged";
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // BufferPool=NonPagedNx
    // DeviceInterfaceTarget=NonPaged
    // Input
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DeviceInterfaceTarget_BufferInput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = TRUE;
    moduleAttributes.ClientModuleInstanceName = "Input/NonPaged/NonPaged";
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // BufferPool=Paged
    // DeviceInterfaceTarget=Paged
    // Output
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = NUMBER_OF_CONTINUOUS_REQUESTS;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(DWORD);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = NUMBER_OF_CONTINUOUS_REQUESTS;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = PagedPool;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_ZEROBUFFER;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = Tests_DeviceInterfaceTarget_BufferOutput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = TRUE;
    moduleAttributes.PassiveLevel = TRUE;
    moduleAttributes.ClientModuleInstanceName = "Output/Paged/Paged";
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // BufferPool=NonPaged
    // DeviceInterfaceTarget=Paged
    // Output
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = NUMBER_OF_CONTINUOUS_REQUESTS;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(DWORD);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = NUMBER_OF_CONTINUOUS_REQUESTS;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_ZEROBUFFER;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = Tests_DeviceInterfaceTarget_BufferOutput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = TRUE;
    moduleAttributes.PassiveLevel = TRUE;
    moduleAttributes.ClientModuleInstanceName = "Output/NonPaged/Paged";
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // BufferPool=NonPaged
    // DeviceInterfaceTarget=NonPaged
    // Output
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = NUMBER_OF_CONTINUOUS_REQUESTS;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(DWORD);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = NUMBER_OF_CONTINUOUS_REQUESTS;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_ZEROBUFFER;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = Tests_DeviceInterfaceTarget_BufferOutput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = TRUE;
    moduleAttributes.ClientModuleInstanceName = "Output/NonPaged/NonPaged";
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // This instance allows for arrival/removal on demand.
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_DISK;
    moduleConfigDeviceInterfaceTarget.OpenMode = GENERIC_READ;
    moduleConfigDeviceInterfaceTarget.ShareAccess = FILE_SHARE_READ;
 
    // NOTE: No Module handle is needed since no Methods are called.
    //
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // This instance tests for no device attached ever.
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_NO_DEVICE;
    moduleConfigDeviceInterfaceTarget.OpenMode = GENERIC_READ;
    moduleConfigDeviceInterfaceTarget.ShareAccess = FILE_SHARE_READ;
 
    // NOTE: No Module handle is needed since no Methods are called.
    //
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

#endif

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
DMF_Tests_DeviceInterfaceTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_DeviceInterfaceTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_DeviceInterfaceTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_DeviceInterfaceTarget;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_DeviceInterfaceTarget);
    dmfCallbacksDmf_Tests_DeviceInterfaceTarget.ChildModulesAdd = DMF_Tests_DeviceInterfaceTarget_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_DeviceInterfaceTarget,
                                            Tests_DeviceInterfaceTarget,
                                            DMF_CONTEXT_Tests_DeviceInterfaceTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_Create);

    dmfModuleDescriptor_Tests_DeviceInterfaceTarget.CallbacksDmf = &dmfCallbacksDmf_Tests_DeviceInterfaceTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_DeviceInterfaceTarget,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_Tests_DeviceInterfaceTarget.c
//
