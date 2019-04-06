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

#define THREAD_COUNT                (1)

typedef enum _TEST_ACTION
{
    TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUSCANCEL,
    TEST_ACTION_COUNT,
    TEST_ACTION_MINIUM      = TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_MAXIMUM     = TEST_ACTION_ASYNCHRONOUSCANCEL
} TEST_ACTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Module under test.
    //
    DMFMODULE DmfModuleDeviceInterfaceTargetDispatch;
    DMFMODULE DmfModuleDeviceInterfaceTargetPassive;
    // Work threads that perform actions on the DeviceInterfaceTarget Module.
    //
    DMFMODULE DmfModuleThreadAuto[THREAD_COUNT];
    DMFMODULE DmfModuleThreadManual[THREAD_COUNT];
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
#define MemoryTag 'TaHT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

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

    PAGED_CODE();

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 15000);
    RtlCopyMemory(InputBuffer,
                  &sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));
}

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceTarget_ThreadAction_Synchronous(
    _In_ DMFMODULE DmfModule
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
                                                                                 15000);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendSynchronously(moduleContext->DmfModuleDeviceInterfaceTargetDispatch,
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
                                                                                 15000);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_SendSynchronously(moduleContext->DmfModuleDeviceInterfaceTargetPassive,
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
    _In_ DMFMODULE DmfModule
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
                                                                                 15000);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(moduleContext->DmfModuleDeviceInterfaceTargetDispatch,
                                              &sleepIoctlBuffer,
                                              sizeof(sleepIoctlBuffer),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              0,
                                              Tests_DeviceInterfaceTarget_SendCompletion,
                                              NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 15000);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(moduleContext->DmfModuleDeviceInterfaceTargetPassive,
                                              &sleepIoctlBuffer,
                                              sizeof(sleepIoctlBuffer),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              0,
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
    _In_ DMFMODULE DmfModule
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
                                                                                 15000);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(moduleContext->DmfModuleDeviceInterfaceTargetDispatch,
                                              &sleepIoctlBuffer,
                                              sizeof(sleepIoctlBuffer),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              0,
                                              Tests_DeviceInterfaceTarget_SendCompletion,
                                              NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    DMF_Utility_DelayMilliseconds(sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 15000);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceTarget_Send(moduleContext->DmfModuleDeviceInterfaceTargetPassive,
                                              &sleepIoctlBuffer,
                                              sizeof(sleepIoctlBuffer),
                                              NULL,
                                              NULL,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_Tests_IoctlHandler_SLEEP,
                                              0,
                                              Tests_DeviceInterfaceTarget_SendCompletion,
                                              NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    DMF_Utility_DelayMilliseconds(sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);
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

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
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
            Tests_DeviceInterfaceTarget_ThreadAction_Synchronous(dmfModule);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DeviceInterfaceTarget_ThreadAction_Asynchronous(dmfModule);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DeviceInterfaceTarget_ThreadAction_AsynchronousCancel(dmfModule);
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

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
Tests_DeviceInterfaceTarget_NonContinousStartAuto(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSelfManagedIoInit for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // Create threads that read with expected success, read with expected failure
    // and enumerate.
    //
    for (index = 0; index < THREAD_COUNT; index++)
    {
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadAuto[index]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThreadAuto[index]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
Tests_DeviceInterfaceTarget_NonContinousStopAuto(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSelfManagedIoCleanup for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
DbgBreakPoint();
    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_Stop(moduleContext->DmfModuleThreadAuto[index]);
    }

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
Tests_DeviceInterfaceTarget_NonContinousStartManual(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSelfManagedIoInit for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // Create threads that read with expected success, read with expected failure
    // and enumerate.
    //
    for (index = 0; index < THREAD_COUNT; index++)
    {
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadManual[index]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThreadManual[index]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
Tests_DeviceInterfaceTarget_NonContinousStopManual(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSelfManagedIoCleanup for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceTarget* moduleContext;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
DbgBreakPoint();
    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_Stop(moduleContext->DmfModuleThreadManual[index]);
    }

    FuncExitVoid(DMF_TRACE);
}

VOID
Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_AutoContinous(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Arrival Notification.
    In this case, this driver starts the continuous reader and makes sure that the lightbar is
    set correctly per the state of the switches.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModuleParent;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");
    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    ntStatus = Tests_DeviceInterfaceTarget_NonContinousStartAuto(dmfModuleParent);
    ASSERT(NT_SUCCESS(ntStatus));

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
}

VOID
Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_AutoContinous(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Removal Notification.
    In this case, this driver stops the continuous reader.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModuleParent;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");
    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
DbgBreakPoint();
    Tests_DeviceInterfaceTarget_NonContinousStopAuto(dmfModuleParent);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
}

VOID
Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_ManualContinous(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Arrival Notification.
    In this case, this driver starts the continuous reader and makes sure that the lightbar is
    set correctly per the state of the switches.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModuleParent;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    ntStatus = DMF_DeviceInterfaceTarget_StreamStart(DmfModule);
    if (NT_SUCCESS(ntStatus))
    {
        Tests_DeviceInterfaceTarget_NonContinousStartManual(dmfModuleParent);
    }
    ASSERT(NT_SUCCESS(ntStatus));

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
}

VOID
Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_ManualContinous(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Removal Notification.
    In this case, this driver stops the continuous reader.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModuleParent;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");
    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
DbgBreakPoint();
    DMF_DeviceInterfaceTarget_StreamStop(DmfModule);
    Tests_DeviceInterfaceTarget_NonContinousStartManual(dmfModuleParent);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "<--%!FUNC!");
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

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // DeviceInterfaceTarget (DISPATCH_LEVEL)
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
    // Work around due to lack of support for input buffers only.
    // (To be fixed.)
    //
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(DWORD);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;

    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_AutoContinous;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_AutoContinous;
    moduleAttributes.ClientCallbacks = &moduleEventCallbacks;
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleDeviceInterfaceTargetDispatch);

    // DeviceInterfaceTarget (PASSIVE_LEVEL)
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
    // Work around due to lack of support for input buffers only.
    // (To be fixed.)
    //
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = 1;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(DWORD);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;

    moduleAttributes.PassiveLevel = TRUE;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = Tests_DeviceInterfaceTarget_OnDeviceArrivalNotification_ManualContinous;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = Tests_DeviceInterfaceTarget_OnDeviceRemovalNotification_ManualContinous;
    moduleAttributes.ClientCallbacks = &moduleEventCallbacks;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceTargetPassive);

    // Thread
    // ------
    //
    for (ULONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
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
                         &moduleContext->DmfModuleThreadManual[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Tests_DeviceInterfaceTarget;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Tests_DeviceInterfaceTarget;

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

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Tests_DeviceInterfaceTarget);
    DmfCallbacksDmf_Tests_DeviceInterfaceTarget.ChildModulesAdd = DMF_Tests_DeviceInterfaceTarget_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Tests_DeviceInterfaceTarget,
                                            Tests_DeviceInterfaceTarget,
                                            DMF_CONTEXT_Tests_DeviceInterfaceTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_Create);

    DmfModuleDescriptor_Tests_DeviceInterfaceTarget.CallbacksDmf = &DmfCallbacksDmf_Tests_DeviceInterfaceTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Tests_DeviceInterfaceTarget,
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
