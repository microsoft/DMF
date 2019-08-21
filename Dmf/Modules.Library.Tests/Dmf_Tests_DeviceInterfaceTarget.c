/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_DeviceInterfaceTarget.c

Abstract:

    Functional tests for Dmf_DeviceInterfaceTarget Module

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_DeviceInterfaceTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define THREAD_COUNT                            (2)
#define MAXIMUM_SLEEP_TIME_MS                   (15000)
// This timeout is necessary for causing asynchronous single requests to complete fast so that
// driver disable works well (since it is not possible to cancel asynchronous requests at this time.
// using DMF).
//
#define ASYNCHRONOUS_REQUEST_TIMEOUT_MS         (50)
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
    DMFMODULE DmfModuleDeviceInterfaceTargetDispatchInput;
    DMFMODULE DmfModuleDeviceInterfaceTargetPassiveInput;
    DMFMODULE DmfModuleDeviceInterfaceTargetPassiveOutput;
    // Work threads that perform actions on the DeviceInterfaceTarget Module.
    // +1 makes it easy to set THREAD_COUNT = 0 for test purposes.
    //
    DMFMODULE DmfModuleThreadAuto[THREAD_COUNT + 1];
    DMFMODULE DmfModuleThreadManualInput[THREAD_COUNT + 1];
    DMFMODULE DmfModuleThreadManualOutput[THREAD_COUNT + 1];
    // Use alertable sleep to allow driver to unload faster.
    //
    DMFMODULE DmfModuleAlertableSleepAuto[THREAD_COUNT + 1];
    DMFMODULE DmfModuleAlertableSleepManualInput[THREAD_COUNT + 1];
    DMFMODULE DmfModuleAlertableSleepManualOutput[THREAD_COUNT + 1];
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
} THREAD_INDEX_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE(THREAD_INDEX_CONTEXT);

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

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(ClientBuferContextInput);

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0,
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    RtlCopyMemory(InputBuffer,
                  &sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));
    *InputBufferSize = sizeof(sleepIoctlBuffer);
}

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
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientBufferContextOutput);
    UNREFERENCED_PARAMETER(DmfModule);

    if (!NT_SUCCESS(CompletionStatus))
    {
        ASSERT(FALSE);
    }
    if (OutputBufferSize != sizeof(DWORD))
    {
        ASSERT(FALSE);
    }
    if (*((DWORD*)OutputBuffer) != 0)
    {
        ASSERT(FALSE);
    }
    return ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;
}

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceTarget_ThreadAction_Synchronous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep
    )
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;

    UNREFERENCED_PARAMETER(DmfModuleAlertableSleep);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendSynchronously(moduleContext->DmfModuleDeviceInterfaceTargetDispatchInput,
                                                           &sleepIoctlBuffer,
                                                           sizeof(sleepIoctlBuffer),
                                                           NULL,
                                                           NULL,
                                                           ContinuousRequestTarget_RequestType_Ioctl,
                                                           IOCTL_Tests_IoctlHandler_SLEEP,
                                                           0,
                                                           &bytesWritten);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    // TODO: Get time and compare with send time.
    //

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendSynchronously(moduleContext->DmfModuleDeviceInterfaceTargetPassiveInput,
                                                           &sleepIoctlBuffer,
                                                           sizeof(sleepIoctlBuffer),
                                                           NULL,
                                                           NULL,
                                                           ContinuousRequestTarget_RequestType_Ioctl,
                                                           IOCTL_Tests_IoctlHandler_SLEEP,
                                                           0,
                                                           &bytesWritten);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
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
    // TODO: Get time and compare with send time.
    //
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientRequestContext);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferBytesWritten);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferBytesRead);
    UNREFERENCED_PARAMETER(CompletionStatus);
}

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceTarget_ThreadAction_Asynchronous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep
    )
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;

    UNREFERENCED_PARAMETER(DmfModuleAlertableSleep);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(moduleContext->DmfModuleDeviceInterfaceTargetDispatchInput,
                                              &sleepIoctlBuffer,
                                              sizeof(sleepIoctlBuffer),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                              Tests_DeviceInterfaceTarget_SendCompletion,
                                              NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(moduleContext->DmfModuleDeviceInterfaceTargetPassiveInput,
                                              &sleepIoctlBuffer,
                                              sizeof(sleepIoctlBuffer),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                              Tests_DeviceInterfaceTarget_SendCompletion,
                                              NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
}
#pragma code_seg()

// TODO: Module does not support cancellation of individual requests.
//
#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceTarget_ThreadAction_AsynchronousCancel(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep
    )
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(moduleContext->DmfModuleDeviceInterfaceTargetDispatchInput,
                                              &sleepIoctlBuffer,
                                              sizeof(sleepIoctlBuffer),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                              Tests_DeviceInterfaceTarget_SendCompletion,
                                              NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);
    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out.
        //
        goto Exit;
    }

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(moduleContext->DmfModuleDeviceInterfaceTargetPassiveInput,
                                              &sleepIoctlBuffer,
                                              sizeof(sleepIoctlBuffer),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                              Tests_DeviceInterfaceTarget_SendCompletion,
                                              NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    DMF_AlertableSleep_ResetForReuse(DmfModuleAlertableSleep,
                                     0);
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);

Exit:
    ;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DeviceInterfaceTarget_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    TEST_ACTION testAction;
    THREAD_INDEX_CONTEXT* threadIndex;

    PAGED_CODE();

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
            Tests_DeviceInterfaceTarget_ThreadAction_Synchronous(dmfModule,
                                                                 threadIndex->DmfModuleAlertableSleep);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DeviceInterfaceTarget_ThreadAction_Asynchronous(dmfModule,
                                                                  threadIndex->DmfModuleAlertableSleep);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DeviceInterfaceTarget_ThreadAction_AsynchronousCancel(dmfModule,
                                                                        threadIndex->DmfModuleAlertableSleep);
            break;
        default:
            ASSERT(FALSE);
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
Tests_DeviceInterfaceTarget_NonContinousStartAuto(
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
Tests_DeviceInterfaceTarget_NonContinousStopAuto(
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
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepAuto[threadIndex],
                                 0);
        // Stop thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThreadAuto[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Tests_DeviceInterfaceTarget_NonContinousStartManualInput(
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
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadManualInput[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThreadManualInput[threadIndex]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Tests_DeviceInterfaceTarget_NonContinousStartManualOutput(
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
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadManualOutput[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThreadManualOutput[threadIndex]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_NonContinousStopManualInput(
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
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepManualInput[threadIndex],
                                 0);
        // Stop thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThreadManualInput[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_NonContinousStopManualOutput(
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
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepManualOutput[threadIndex],
                                 0);
        // Stop thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThreadManualOutput[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_AutoContinous(
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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

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
        // Reset in case target comes and goes and comes back.
        //
        DMF_AlertableSleep_ResetForReuse(threadIndexContext->DmfModuleAlertableSleep,
                                         0);
    }

    ntStatus = Tests_DeviceInterfaceTarget_NonContinousStartAuto(dmfModuleParent);
    ASSERT(NT_SUCCESS(ntStatus));

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_AutoContinous(
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

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    Tests_DeviceInterfaceTarget_NonContinousStopAuto(dmfModuleParent);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_ManualContinousInput(
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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        THREAD_INDEX_CONTEXT* threadIndexContext;
        WDF_OBJECT_ATTRIBUTES objectAttributes;

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                               THREAD_INDEX_CONTEXT);
        WdfObjectAllocateContext(moduleContext->DmfModuleThreadManualInput[threadIndex],
                                 &objectAttributes,
                                 (PVOID*)&threadIndexContext);
        threadIndexContext->DmfModuleAlertableSleep = moduleContext->DmfModuleAlertableSleepManualInput[threadIndex];
        // Reset in case target comes and goes and comes back.
        //
        DMF_AlertableSleep_ResetForReuse(threadIndexContext->DmfModuleAlertableSleep,
                                         0);
    }

    ntStatus = DMF_DeviceInterfaceTarget_StreamStart(DmfModule);
    if (NT_SUCCESS(ntStatus))
    {
        Tests_DeviceInterfaceTarget_NonContinousStartManualInput(dmfModuleParent);
    }
    ASSERT(NT_SUCCESS(ntStatus));

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_ManualContinousOutput(
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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        THREAD_INDEX_CONTEXT* threadIndexContext;
        WDF_OBJECT_ATTRIBUTES objectAttributes;

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                               THREAD_INDEX_CONTEXT);
        WdfObjectAllocateContext(moduleContext->DmfModuleThreadManualOutput[threadIndex],
                                 &objectAttributes,
                                 (PVOID*)&threadIndexContext);
        threadIndexContext->DmfModuleAlertableSleep = moduleContext->DmfModuleAlertableSleepManualOutput[threadIndex];
        // Reset in case target comes and goes and comes back.
        //
        DMF_AlertableSleep_ResetForReuse(threadIndexContext->DmfModuleAlertableSleep,
                                         0);
    }

    ntStatus = DMF_DeviceInterfaceTarget_StreamStart(DmfModule);
    if (NT_SUCCESS(ntStatus))
    {
        Tests_DeviceInterfaceTarget_NonContinousStartManualOutput(dmfModuleParent);
    }
    ASSERT(NT_SUCCESS(ntStatus));

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_ManualContinousInput(
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

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    DMF_DeviceInterfaceTarget_StreamStop(DmfModule);
    Tests_DeviceInterfaceTarget_NonContinousStopManualInput(dmfModuleParent);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_ManualContinousOutput(
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

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    DMF_DeviceInterfaceTarget_StreamStop(DmfModule);
    Tests_DeviceInterfaceTarget_NonContinousStopManualOutput(dmfModuleParent);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
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

    // DeviceInterfaceTarget (DISPATCH_LEVEL)
    // Processes Input Buffers.
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

    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_AutoContinous;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_AutoContinous;
    moduleAttributes.ClientCallbacks = &moduleEventCallbacks;
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
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DeviceInterfaceTarget_BufferInput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Manual;

    moduleAttributes.PassiveLevel = TRUE;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_ManualContinousInput;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_ManualContinousInput;
    moduleAttributes.ClientCallbacks = &moduleEventCallbacks;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceTargetPassiveInput);

    // DeviceInterfaceTarget (PASSIVE_LEVEL)
    // Processes Output Buffers.
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = 32;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(DWORD);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 32;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_ZEROBUFFER;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = Tests_DeviceInterfaceTarget_BufferOutput;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Manual;
    moduleAttributes.PassiveLevel = TRUE;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_ManualContinousOutput;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_ManualContinousOutput;
    moduleAttributes.ClientCallbacks = &moduleEventCallbacks;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceTargetPassiveOutput);

    // Thread
    // ------
    //
    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_DeviceInterfaceTarget_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThreadAuto[threadIndex]);

        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_DeviceInterfaceTarget_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThreadManualInput[threadIndex]);

        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_DeviceInterfaceTarget_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThreadManualOutput[threadIndex]);

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

        // AlertableSleep Manual (Input)
        // ---------------------
        //
        DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                      &moduleAttributes);
        moduleConfigAlertableSleep.EventCount = 1;
        moduleAttributes.ClientModuleInstanceName = "AlertableSleep.Manual";
        DMF_DmfModuleAdd(DmfModuleInit,
                            &moduleAttributes,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &moduleContext->DmfModuleAlertableSleepManualInput[threadIndex]);

        // AlertableSleep Manual (Output)
        // ---------------------
        //
        DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                      &moduleAttributes);
        moduleConfigAlertableSleep.EventCount = 1;
        moduleAttributes.ClientModuleInstanceName = "AlertableSleep.Manual";
        DMF_DmfModuleAdd(DmfModuleInit,
                            &moduleAttributes,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &moduleContext->DmfModuleAlertableSleepManualOutput[threadIndex]);
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
