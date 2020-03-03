/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_DefaultTarget.c

Abstract:

    Functional tests for Dmf_DefaultTarget Module.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_DefaultTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define THREAD_COUNT                            (2)
#define MAXIMUM_SLEEP_TIME_MS                   (15000)

// Keep synchronous maximum time short to make driver disable faster.
//
#define MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS       (1000)

typedef enum _TEST_ACTION
{
    TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUSCANCEL,
    TEST_ACTION_COUNT,
    TEST_ACTION_MINIUM = TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_MAXIMUM = TEST_ACTION_ASYNCHRONOUSCANCEL
} TEST_ACTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Modules under test.
    //
    DMFMODULE DmfModuleDefaultTargetDispatchInput;
    DMFMODULE DmfModuleDefaultTargetPassiveInput;
    DMFMODULE DmfModuleDefaultTargetPassiveOutput;
    // Work threads that perform actions on the DefaultTarget Module.
    // +1 makes it easy to set THREAD_COUNT = 0 for test purposes.
    //
    DMFMODULE DmfModuleThreadAuto[THREAD_COUNT + 1];
    DMFMODULE DmfModuleThreadManual[THREAD_COUNT + 1];
    // Use alertable sleep to allow driver to unload faster.
    //
    DMFMODULE DmfModuleAlertableSleepAuto[THREAD_COUNT + 1];
    DMFMODULE DmfModuleAlertableSleepManual[THREAD_COUNT + 1];
} DMF_CONTEXT_Tests_DefaultTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_DefaultTarget)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_DefaultTarget)

// Memory Pool Tag.
//
#define MemoryTag 'TeDT'

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
} THREAD_INDEX_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE(THREAD_INDEX_CONTEXT);

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferInput)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_DefaultTarget_BufferInput(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(*InputBufferSize) VOID* InputBuffer,
    _Out_ size_t* InputBufferSize,
    _In_ VOID* ClientBuferContextInput
    )
{
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(ClientBuferContextInput);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0,
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
Tests_DefaultTarget_BufferOutput(
    _In_ DMFMODULE DmfModule,
    _In_reads_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _In_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    )
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(ClientBufferContextOutput);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "%!FUNC!:  ntStatus=%!STATUS!", CompletionStatus);

    DmfAssert(NT_SUCCESS(CompletionStatus));
    DmfAssert(OutputBufferSize == sizeof(DWORD));
    DmfAssert(OutputBuffer != NULL);

    return ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;
}

#pragma code_seg("PAGE")
static
void
Tests_DefaultTarget_ThreadAction_Synchronous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep
    )
{
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    NTSTATUS ntStatus;
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;

    UNREFERENCED_PARAMETER(DmfModuleAlertableSleep);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_SendSynchronously(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                                   &sleepIoctlBuffer,
                                                   sizeof(sleepIoctlBuffer),
                                                   NULL,
                                                   NULL,
                                                   ContinuousRequestTarget_RequestType_Ioctl,
                                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                                   0,
                                                   &bytesWritten);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    // TODO: Get time and compare with send time.
    //

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_SendSynchronously(moduleContext->DmfModuleDefaultTargetPassiveInput,
                                                   &sleepIoctlBuffer,
                                                   sizeof(sleepIoctlBuffer),
                                                   NULL,
                                                   NULL,
                                                   ContinuousRequestTarget_RequestType_Ioctl,
                                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                                   0,
                                                   &bytesWritten);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    // TODO: Get time and compare with send time.
    //
}
#pragma code_seg()

_Function_class_(EVT_DMF_ContinuousRequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_DefaultTarget_SendCompletion(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    )
{
    // TODO: Get time and compare with send time.
    //
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientRequestContext);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferBytesWritten);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferBytesRead);
    UNREFERENCED_PARAMETER(CompletionStatus);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");
}

_Function_class_(EVT_DMF_ContinuousRequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_DefaultTarget_SendCompletionMustBeCancelled(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    )
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientRequestContext);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferBytesWritten);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferBytesRead);
    UNREFERENCED_PARAMETER(CompletionStatus);

    //DmfAssert(STATUS_CANCELLED == CompletionStatus);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");
}

#pragma code_seg("PAGE")
static
void
Tests_DefaultTarget_ThreadAction_Asynchronous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep
    )
{
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    NTSTATUS ntStatus;
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;
    ULONG timeoutMs;

    UNREFERENCED_PARAMETER(DmfModuleAlertableSleep);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    if (TestsUtility_GenerateRandomNumber(0,
                                          1))
    {
        timeoutMs = TestsUtility_GenerateRandomNumber(1,
                                                      5);
    }
    else
    {
        timeoutMs = 0;
    }

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_Send(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                      &sleepIoctlBuffer,
                                      sizeof(sleepIoctlBuffer),
                                      NULL,
                                      NULL,
                                      ContinuousRequestTarget_RequestType_Ioctl,
                                      IOCTL_Tests_IoctlHandler_SLEEP,
                                      timeoutMs,
                                      Tests_DefaultTarget_SendCompletion,
                                      NULL);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_Send(moduleContext->DmfModuleDefaultTargetPassiveInput,
                                      &sleepIoctlBuffer,
                                      sizeof(sleepIoctlBuffer),
                                      NULL,
                                      NULL,
                                      ContinuousRequestTarget_RequestType_Ioctl,
                                      IOCTL_Tests_IoctlHandler_SLEEP,
                                      timeoutMs,
                                      Tests_DefaultTarget_SendCompletion,
                                      NULL);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DefaultTarget_ThreadAction_AsynchronousCancel(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep
    )
{
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    NTSTATUS ntStatus;
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;
    RequestTarget_DmfRequest DmfRequest;
    BOOLEAN requestCancelled;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request after waiting for a while. It may or may not be canceled.
    //

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_SendEx(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                        &sleepIoctlBuffer,
                                        sizeof(sleepIoctlBuffer),
                                        NULL,
                                        NULL,
                                        ContinuousRequestTarget_RequestType_Ioctl,
                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                        0,
                                        ContinuousRequestTarget_CompletionOptions_Default,
                                        Tests_DefaultTarget_SendCompletion,
                                        NULL,
                                        &DmfRequest);

    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);
    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out.
        //
        goto Exit;
    }

    // Cancel the request if possible.
    //
    requestCancelled = DMF_DefaultTarget_Cancel(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                                DmfRequest);

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_SendEx(moduleContext->DmfModuleDefaultTargetPassiveInput,
                                        &sleepIoctlBuffer,
                                        sizeof(sleepIoctlBuffer),
                                        NULL,
                                        NULL,
                                        ContinuousRequestTarget_RequestType_Ioctl,
                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                        0,
                                        ContinuousRequestTarget_CompletionOptions_Default,
                                        Tests_DefaultTarget_SendCompletion,
                                        NULL,
                                        &DmfRequest);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    DMF_AlertableSleep_ResetForReuse(DmfModuleAlertableSleep,
                                     0);
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);

    // Cancel the request if possible.
    //
    requestCancelled = DMF_DefaultTarget_Cancel(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                                DmfRequest);

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request immediately after sending it. It may or may not be canceled.
    //

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_SendEx(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                        &sleepIoctlBuffer,
                                        sizeof(sleepIoctlBuffer),
                                        NULL,
                                        NULL,
                                        ContinuousRequestTarget_RequestType_Ioctl,
                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                        0,
                                        ContinuousRequestTarget_CompletionOptions_Default,
                                        Tests_DefaultTarget_SendCompletion,
                                        NULL,
                                        &DmfRequest);

    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));

    // Cancel the request immediately after sending it.
    //
    requestCancelled = DMF_DefaultTarget_Cancel(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                                DmfRequest);

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_SendEx(moduleContext->DmfModuleDefaultTargetPassiveInput,
                                        &sleepIoctlBuffer,
                                        sizeof(sleepIoctlBuffer),
                                        NULL,
                                        NULL,
                                        ContinuousRequestTarget_RequestType_Ioctl,
                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                        0,
                                        ContinuousRequestTarget_CompletionOptions_Default,
                                        Tests_DefaultTarget_SendCompletion,
                                        NULL,
                                        &DmfRequest);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));

    // Cancel the request if possible right after sending it.
    //
    requestCancelled = DMF_DefaultTarget_Cancel(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                                DmfRequest);

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel request the before it is normally completed. It should always cancel.
    //

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(4, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_SendEx(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                        &sleepIoctlBuffer,
                                        sizeof(sleepIoctlBuffer),
                                        NULL,
                                        NULL,
                                        ContinuousRequestTarget_RequestType_Ioctl,
                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                        0,
                                        ContinuousRequestTarget_CompletionOptions_Default,
                                        Tests_DefaultTarget_SendCompletionMustBeCancelled,
                                        NULL,
                                        &DmfRequest);

    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);
    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out.
        //
        goto Exit;
    }

    // Cancel the request if possible.
    // It should always cancel since the time just waited is 1/2 the time that was sent above.
    //
    requestCancelled = DMF_DefaultTarget_Cancel(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                                DmfRequest);
    DmfAssert(requestCancelled);

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(4, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DefaultTarget_SendEx(moduleContext->DmfModuleDefaultTargetPassiveInput,
                                        &sleepIoctlBuffer,
                                        sizeof(sleepIoctlBuffer),
                                        NULL,
                                        NULL,
                                        ContinuousRequestTarget_RequestType_Ioctl,
                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                        0,
                                        ContinuousRequestTarget_CompletionOptions_Default,
                                        Tests_DefaultTarget_SendCompletion,
                                        NULL,
                                        &DmfRequest);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    DMF_AlertableSleep_ResetForReuse(DmfModuleAlertableSleep,
                                     0);
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);

    // Cancel the request if possible.
    // It should always cancel since the time just waited is 1/2 the time that was sent above.
    //
    requestCancelled = DMF_DefaultTarget_Cancel(moduleContext->DmfModuleDefaultTargetDispatchInput,
                                                DmfRequest);
    //DmfAssert(requestCancelled);

Exit:
    ;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DefaultTarget_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    TEST_ACTION testAction;
    THREAD_INDEX_CONTEXT* threadIndex;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    threadIndex = WdfObjectGet_THREAD_INDEX_CONTEXT(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SYNCHRONOUS:
            Tests_DefaultTarget_ThreadAction_Synchronous(dmfModule,
                                                         threadIndex->DmfModuleAlertableSleep);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DefaultTarget_ThreadAction_Asynchronous(dmfModule,
                                                          threadIndex->DmfModuleAlertableSleep);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DefaultTarget_ThreadAction_AsynchronousCancel(dmfModule,
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
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Tests_DefaultTarget_NonContinousStartAuto(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts the threads that send asynchronous data to the automatically started
    DMF_DefaultTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DefaultTarget.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    NTSTATUS ntStatus;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadAuto[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThreadAuto[threadIndex]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DefaultTarget_NonContinousStopAuto(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops the threads that send asynchronous data to the automatically started
    DMF_DefaultTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DefaultTarget.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // Interrupt any long sleeps.
        //
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepAuto[threadIndex],
                                 0);
        // Stop the thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThreadAuto[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Tests_DefaultTarget_NonContinousStartManual(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts the threads that send asynchronous data to the manually started
    DMF_DefaultTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DefaultTarget.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    NTSTATUS ntStatus;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadManual[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThreadManual[threadIndex]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DefaultTarget_NonContinousStopManual(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops the threads that send asynchronous data to the manually started
    DMF_DefaultTarget Modules.

Arguments:

    DmfModule - DMF_Tests_DefaultTarget.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // Interrupt any long sleeps.
        //
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepManual[threadIndex],
                                 0);
        // Stop thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThreadManual[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(DMF_ModuleD0Entry)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Tests_DefaultTarget_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Start all threads.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;

    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // The PASSIVE_LEVEL targets are manually started (although they don't need to be).
    //
    ntStatus = DMF_DefaultTarget_StreamStart(moduleContext->DmfModuleDefaultTargetPassiveInput);
    DmfAssert(NT_SUCCESS(ntStatus));
    ntStatus = DMF_DefaultTarget_StreamStart(moduleContext->DmfModuleDefaultTargetPassiveOutput);
    DmfAssert(NT_SUCCESS(ntStatus));

    ntStatus = Tests_DefaultTarget_NonContinousStartAuto(DmfModule);
    DmfAssert(NT_SUCCESS(ntStatus));

    ntStatus = Tests_DefaultTarget_NonContinousStartManual(DmfModule);
    DmfAssert(NT_SUCCESS(ntStatus));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Function_class_(DMF_ModuleD0Exit)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Tests_DefaultTarget_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Stop all threads.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;

    UNREFERENCED_PARAMETER(TargetState);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DefaultTarget_NonContinousStopAuto(DmfModule);
    Tests_DefaultTarget_NonContinousStopManual(DmfModule);

    // The PASSIVE_LEVEL targets are manually started (although they don't need to be).
    //
    DMF_DefaultTarget_StreamStop(moduleContext->DmfModuleDefaultTargetPassiveInput);
    DMF_DefaultTarget_StreamStop(moduleContext->DmfModuleDefaultTargetPassiveOutput);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_DefaultTarget_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    DMF_CONFIG_Thread moduleConfigThread;
    DMF_CONFIG_DefaultTarget moduleConfigDefaultTarget;
    DMF_CONFIG_AlertableSleep moduleConfigAlertableSleep;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // DefaultTarget (DISPATCH_LEVEL)
    // Processes Input Buffers.
    //
    DMF_CONFIG_DefaultTarget_AND_ATTRIBUTES_INIT(&moduleConfigDefaultTarget,
                                                 &moduleAttributes);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DefaultTarget_BufferInput;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;

    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleDefaultTargetDispatchInput);

    // DefaultTarget (PASSIVE_LEVEL)
    // Processes Input Buffers.
    //
    DMF_CONFIG_DefaultTarget_AND_ATTRIBUTES_INIT(&moduleConfigDefaultTarget,
                                                 &moduleAttributes);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DefaultTarget_BufferInput;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Manual;

    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDefaultTargetPassiveInput);

    // DefaultTarget (PASSIVE_LEVEL)
    // Processes Output Buffers.
    //
    DMF_CONFIG_DefaultTarget_AND_ATTRIBUTES_INIT(&moduleConfigDefaultTarget,
                                                 &moduleAttributes);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(DWORD);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_ZEROBUFFER;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = Tests_DefaultTarget_BufferOutput;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Manual;
    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDefaultTargetPassiveOutput);

    // Thread
    // ------
    //
    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_DefaultTarget_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThreadAuto[threadIndex]);

        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_DefaultTarget_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThreadManual[threadIndex]);

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
                            &moduleContext->DmfModuleAlertableSleepAuto[threadIndex]);

        // AlertableSleep Manual
        // ---------------------
        //
        DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                      &moduleAttributes);
        moduleConfigAlertableSleep.EventCount = 1;
        moduleAttributes.ClientModuleInstanceName = "AlertableSleep.Manual";
        DMF_DmfModuleAdd(DmfModuleInit,
                            &moduleAttributes,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &moduleContext->DmfModuleAlertableSleepManual[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Tests_DefaultTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_DefaultTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_DefaultTarget* moduleContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        THREAD_INDEX_CONTEXT* threadIndexContext;
        WDF_OBJECT_ATTRIBUTES objectAttributes;

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                               THREAD_INDEX_CONTEXT);
        WdfObjectAllocateContext(moduleContext->DmfModuleThreadAuto[threadIndex],
                                 &objectAttributes,
                                 (PVOID*)&threadIndexContext);
        threadIndexContext->DmfModuleAlertableSleep = moduleContext->DmfModuleAlertableSleepAuto[threadIndex];

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                               THREAD_INDEX_CONTEXT);
        WdfObjectAllocateContext(moduleContext->DmfModuleThreadManual[threadIndex],
                                 &objectAttributes,
                                 (PVOID*)&threadIndexContext);
        threadIndexContext->DmfModuleAlertableSleep = moduleContext->DmfModuleAlertableSleepManual[threadIndex];
    }
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
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
DMF_Tests_DefaultTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_DefaultTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_DefaultTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_DefaultTarget;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_Tests_DefaultTarget;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_DefaultTarget);
    dmfCallbacksDmf_Tests_DefaultTarget.ChildModulesAdd = DMF_Tests_DefaultTarget_ChildModulesAdd;
    dmfCallbacksDmf_Tests_DefaultTarget.DeviceOpen = DMF_Tests_DefaultTarget_Open;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_Tests_DefaultTarget);
    dmfCallbacksWdf_Tests_DefaultTarget.ModuleD0Entry = DMF_Tests_DefaultTarget_ModuleD0Entry;
    dmfCallbacksWdf_Tests_DefaultTarget.ModuleD0Exit = DMF_Tests_DefaultTarget_ModuleD0Exit;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_DefaultTarget,
                                            Tests_DefaultTarget,
                                            DMF_CONTEXT_Tests_DefaultTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Tests_DefaultTarget.CallbacksDmf = &dmfCallbacksDmf_Tests_DefaultTarget;
    dmfModuleDescriptor_Tests_DefaultTarget.CallbacksWdf = &dmfCallbacksWdf_Tests_DefaultTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_DefaultTarget,
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

// eof: Dmf_Tests_DefaultTarget.c
//
