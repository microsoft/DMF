/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_SelfTarget.c

Abstract:

    Functional tests for Dmf_SelfTarget Module.

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
#include "Dmf_Tests_SelfTarget.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define THREAD_COUNT                            (1)         // Set to 32 for stress
#define MAXIMUM_SLEEP_TIME_MS                   (15000)
// This timeout is necessary for causing asynchronous single requests to complete fast so that
// driver disable works well (since it is not possible to cancel asynchronous requests at this time.
// using DMF).
//
#define ASYNCHRONOUS_REQUEST_TIMEOUT_MS         (50)
// Keep synchronous maximum time short to make driver disable faster.
//
#define MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS       (1000)

// Random timeouts for IOCTLs sent.
//
#define TIMEOUT_FAST_MS             100
#define TIMEOUT_SLOW_MS             5000
#define TIMEOUT_TRAFFIC_DELAY_MS    1000

#define NO_TEST_SIMPLE
#define NO_TEST_SYNCHRONOUS_ONLY
#define NO_TEST_ASYNCHRONOUS_ONLY

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

typedef struct _DMF_CONTEXT_Tests_SelfTarget
{
    // Module under test.
    //
    DMFMODULE DmfModuleSelfTargetDispatch;
#if !defined(TEST_SIMPLE)
    DMFMODULE DmfModuleSelfTargetPassive;
#endif
    // Source of buffers sent asynchronously.
    //
    DMFMODULE DmfModuleBufferPool;
    // Work threads that perform actions on the SelfTarget Module.
    //
    DMFMODULE DmfModuleThread[THREAD_COUNT + 1];
    // Use alertable sleep to allow driver to unload faster.
    //
    DMFMODULE DmfModuleAlertableSleep[THREAD_COUNT + 1];
    // Used to Purge Target as needed.
    //
    WDFIOTARGET IoTargetDispatch;
    WDFIOTARGET IoTargetPassive;
} DMF_CONTEXT_Tests_SelfTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_SelfTarget)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_SelfTarget)

// Memory Pool Tag.
//
#define MemoryTag 'TaHT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Stores the Module threadIndex so that the corresponding alterable sleep
// can be retrieved inside the thread's callback.
//
typedef struct
{
    ULONG ThreadIndex;
} Tests_Selfarget_THREAD_INDEX_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE(Tests_Selfarget_THREAD_INDEX_CONTEXT);

#pragma code_seg("PAGE")
static
void
Tests_SelfTarget_ThreadAction_Synchronous(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG ThreadIndex
    )
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
    NTSTATUS ntStatus;
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;

    UNREFERENCED_PARAMETER(ThreadIndex);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Tests_SelfTarget_ThreadAction_Synchronous");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    sleepIoctlBuffer.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);

    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_SendSynchronously(moduleContext->DmfModuleSelfTargetDispatch,
                                                &sleepIoctlBuffer,
                                                sizeof(Tests_IoctlHandler_Sleep),
                                                NULL,
                                                NULL,
                                                ContinuousRequestTarget_RequestType_Ioctl,
                                                IOCTL_Tests_IoctlHandler_SLEEP,
                                                0,
                                                &bytesWritten);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    // TODO: Get time and compare with send time.
    //

#if !defined(TEST_SIMPLE)
    sleepIoctlBuffer.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_SendSynchronously(moduleContext->DmfModuleSelfTargetPassive,
                                                &sleepIoctlBuffer,
                                                sizeof(Tests_IoctlHandler_Sleep),
                                                NULL,
                                                NULL,
                                                ContinuousRequestTarget_RequestType_Ioctl,
                                                IOCTL_Tests_IoctlHandler_SLEEP,
                                                0,
                                                &bytesWritten);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    // TODO: Get time and compare with send time.
    //
#endif
}
#pragma code_seg()

_Function_class_(EVT_DMF_ContinuousRequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_SelfTarget_SendCompletion(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    )
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;

    // TODO: Get time and compare with send time.
    //
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferBytesWritten);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferBytesRead);
    UNREFERENCED_PARAMETER(CompletionStatus);

    moduleContext = (DMF_CONTEXT_Tests_SelfTarget*)ClientRequestContext;
    sleepIoctlBuffer = (Tests_IoctlHandler_Sleep*)InputBuffer;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ST: RECEIVE sleepIoctlBuffer->TimeToSleepMilliseconds=%d InputBuffer=0x%p", 
                sleepIoctlBuffer->TimeToSleepMilliseconds,
                InputBuffer);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                       (VOID*)sleepIoctlBuffer);
}

#pragma code_seg("PAGE")
static
void
Tests_SelfTarget_ThreadAction_Asynchronous(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG ThreadIndex
    )
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
    NTSTATUS ntStatus;
    size_t bytesWritten;
    ULONG timeoutMs;

    UNREFERENCED_PARAMETER(ThreadIndex);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Tests_SelfTarget_ThreadAction_Asynchronous");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;
    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    RtlZeroMemory(sleepIoctlBuffer,
                  sizeof(Tests_IoctlHandler_Sleep));

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

    sleepIoctlBuffer->TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                  MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetDispatch,
                                   sleepIoctlBuffer,
                                   sizeof(Tests_IoctlHandler_Sleep),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   timeoutMs,
                                   Tests_SelfTarget_SendCompletion,
                                   moduleContext);
    if (!NT_SUCCESS(ntStatus))
    {
        // Completion routine will not happen. Return buffer now or a leak happens.
        //
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                           sleepIoctlBuffer);
    }

#if !defined(TEST_SIMPLE)
    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    RtlZeroMemory(sleepIoctlBuffer,
                  sizeof(Tests_IoctlHandler_Sleep));

    sleepIoctlBuffer->TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                  MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetPassive,
                                   sleepIoctlBuffer,
                                   sizeof(Tests_IoctlHandler_Sleep),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   timeoutMs,
                                   Tests_SelfTarget_SendCompletion,
                                   moduleContext);
    if (!NT_SUCCESS(ntStatus))
    {
        // Completion routine will not happen. Return buffer now or a leak happens.
        //
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                           sleepIoctlBuffer);
    }
#endif
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_SelfTarget_ThreadAction_AsynchronousCancel(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG ThreadIndex
    )
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
    NTSTATUS ntStatus;
    size_t bytesWritten;
    LONG timeToSleepMilliseconds;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Tests_SelfTarget_ThreadAction_AsynchronousCancel");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;
    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    RtlZeroMemory(sleepIoctlBuffer,
                  sizeof(Tests_IoctlHandler_Sleep));

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;

    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetDispatch,
                                   sleepIoctlBuffer,
                                   sizeof(Tests_IoctlHandler_Sleep),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                   Tests_SelfTarget_SendCompletion,
                                   moduleContext);
    if (!NT_SUCCESS(ntStatus))
    {
        // Completion routine will not happen. Return buffer now or a leak happens.
        //
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                           sleepIoctlBuffer);
    }
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep[ThreadIndex],
                                        0,
                                        timeToSleepMilliseconds / 2);
    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out.
        //
        goto Exit;
    }

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

#if !defined(TEST_SIMPLE)
    RtlZeroMemory(sleepIoctlBuffer,
                  sizeof(Tests_IoctlHandler_Sleep));

    sleepIoctlBuffer->TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetPassive,
                                   sleepIoctlBuffer,
                                   sizeof(Tests_IoctlHandler_Sleep),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                   Tests_SelfTarget_SendCompletion,
                                   moduleContext);
    if (!NT_SUCCESS(ntStatus))
    {
        // Completion routine will not happen. Return buffer now or a leak happens.
        //
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                           sleepIoctlBuffer);
    }
    DMF_AlertableSleep_ResetForReuse(moduleContext->DmfModuleAlertableSleep[ThreadIndex],
                                     0);
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep[ThreadIndex],
                                        0,
                                        timeToSleepMilliseconds / 2);
#endif
Exit:
    ;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_SelfTarget_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
    TEST_ACTION testAction;
    Tests_Selfarget_THREAD_INDEX_CONTEXT* threadIndex;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Tests_SelfTarget_WorkThread");

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    threadIndex = WdfObjectGet_Tests_Selfarget_THREAD_INDEX_CONTEXT(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);
#if defined(TEST_SYNCHRONOUS_ONLY)
    testAction = TEST_ACTION_SYNCHRONOUS;
#elif defined(TEST_ASYNCHRONOUS_ONLY)
    testAction = TEST_ACTION_ASYNCHRONOUS;
#endif

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SYNCHRONOUS:
            Tests_SelfTarget_ThreadAction_Synchronous(dmfModule,
                                                      threadIndex->ThreadIndex);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_SelfTarget_ThreadAction_Asynchronous(dmfModule,
                                                       threadIndex->ThreadIndex);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_SelfTarget_ThreadAction_AsynchronousCancel(dmfModule,
                                                             threadIndex->ThreadIndex);
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

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(DMF_ModuleD0Entry)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Tests_SelfTarget_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Starts non-continuous threads. Also starts the manual instance of DMF_DeviceInterfaceTarget.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
    LONG threadIndex;

    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (PreviousState == WdfPowerDeviceD3Final)
    {
        DMF_SelfTarget_Get(moduleContext->DmfModuleSelfTargetDispatch,
                           &moduleContext->IoTargetDispatch);
        DMF_SelfTarget_Get(moduleContext->DmfModuleSelfTargetPassive,
                           &moduleContext->IoTargetPassive);
    }
    else
    {
        // Targets are started by default.
        //
        WdfIoTargetStart(moduleContext->IoTargetDispatch);
        WdfIoTargetStart(moduleContext->IoTargetPassive);
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // TODO: Do this another way. 
        //
        if (PreviousState == WdfPowerDeviceD3Final)
        {
            Tests_Selfarget_THREAD_INDEX_CONTEXT* threadIndexContext;
            WDF_OBJECT_ATTRIBUTES objectAttributes;

            WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
            WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                                   Tests_Selfarget_THREAD_INDEX_CONTEXT);
            ntStatus = WdfObjectAllocateContext(moduleContext->DmfModuleThread[threadIndex],
                                                &objectAttributes,
                                                (PVOID*)&threadIndexContext);
            if (!NT_SUCCESS(ntStatus))
            {
                break;
            }
    
            threadIndexContext->ThreadIndex = threadIndex;
        }

        // Starts thread.
        //
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread[threadIndex]);
        DmfAssert(NT_SUCCESS(ntStatus));
        DMF_Thread_WorkReady(moduleContext->DmfModuleThread[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);

    return STATUS_SUCCESS;
}

_Function_class_(DMF_ModuleD0Exit)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Tests_SelfTarget_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Stops non-continuous threads. Also stops the manual instance of DMF_DeviceInterfaceTarget.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
    LONG threadIndex;

    UNREFERENCED_PARAMETER(TargetState);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Purge Targets to prevent stuck IO during synchronous transactions.
    //
    WdfIoTargetPurge(moduleContext->IoTargetDispatch,
                     WdfIoTargetPurgeIoAndWait);
    WdfIoTargetPurge(moduleContext->IoTargetPassive,
                     WdfIoTargetPurgeIoAndWait);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // Interrupt any long sleeps.
        //
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleep[threadIndex],
                                 0);
        // Stop thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThread[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);

    return STATUS_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_SelfTarget_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
    DMF_CONFIG_Thread moduleConfigThread;

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

    // SelfTarget (DISPATCH_LEVEL)
    //
    DMF_SelfTarget_ATTRIBUTES_INIT(&moduleAttributes);
    moduleAttributes.ClientModuleInstanceName = "DmfModuleSelfTargetDispatch";
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleSelfTargetDispatch);

#if !defined(TEST_SIMPLE)
    // SelfTarget (PASSIVE_LEVEL)
    //
    DMF_SelfTarget_ATTRIBUTES_INIT(&moduleAttributes);
    moduleAttributes.PassiveLevel = TRUE;
    moduleAttributes.ClientModuleInstanceName = "DmfModuleSelfTargetPassive";
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleSelfTargetPassive);
#endif

    // Thread
    // ------
    //
    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_SelfTarget_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThread[threadIndex]);

        // AlertableSleep
        // --------------
        //
        DMF_CONFIG_AlertableSleep moduleConfigAlertableSleep;
        DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                      &moduleAttributes);
        moduleConfigAlertableSleep.EventCount = 1;
        DMF_DmfModuleAdd(DmfModuleInit,
                            &moduleAttributes,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &moduleContext->DmfModuleAlertableSleep[threadIndex]);
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
DMF_Tests_SelfTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_SelfTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_SelfTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_SelfTarget;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_Tests_SelfTarget;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_SelfTarget);
    dmfCallbacksDmf_Tests_SelfTarget.ChildModulesAdd = DMF_Tests_SelfTarget_ChildModulesAdd;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_Tests_SelfTarget);
    dmfCallbacksWdf_Tests_SelfTarget.ModuleD0Entry = DMF_Tests_SelfTarget_ModuleD0Entry;
    dmfCallbacksWdf_Tests_SelfTarget.ModuleD0Exit = DMF_Tests_SelfTarget_ModuleD0Exit;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_SelfTarget,
                                            Tests_SelfTarget,
                                            DMF_CONTEXT_Tests_SelfTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_Tests_SelfTarget.CallbacksDmf = &dmfCallbacksDmf_Tests_SelfTarget;
    dmfModuleDescriptor_Tests_SelfTarget.CallbacksWdf = &dmfCallbacksWdf_Tests_SelfTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_SelfTarget,
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

// eof: Dmf_Tests_SelfTarget.c
//
