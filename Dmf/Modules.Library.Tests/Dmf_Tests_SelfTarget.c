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

#include "Dmf_Tests_SelfTarget.tmh"

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
    DMFMODULE DmfModuleSelfTargetDispatch;
    DMFMODULE DmfModuleSelfTargetPassive;
    // Work threads that perform actions on the SelfTarget Module.
    //
    DMFMODULE DmfModuleThread[THREAD_COUNT + 1];
    // Use alertable sleep to allow driver to unload faster.
    //
    DMFMODULE DmfModuleAlertableSleep[THREAD_COUNT + 1];
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
} THREAD_INDEX_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE(THREAD_INDEX_CONTEXT);

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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_SendSynchronously(moduleContext->DmfModuleSelfTargetDispatch,
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
    ntStatus = DMF_SelfTarget_SendSynchronously(moduleContext->DmfModuleSelfTargetPassive,
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
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;

    UNREFERENCED_PARAMETER(ThreadIndex);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetDispatch,
                                   &sleepIoctlBuffer,
                                   sizeof(sleepIoctlBuffer),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                   Tests_SelfTarget_SendCompletion,
                                   NULL);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetPassive,
                                   &sleepIoctlBuffer,
                                   sizeof(sleepIoctlBuffer),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                   Tests_SelfTarget_SendCompletion,
                                   NULL);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
}
#pragma code_seg()

// TODO: Module does not support cancellation of individual requests.
//
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
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "-->%!FUNC!");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetDispatch,
                                   &sleepIoctlBuffer,
                                   sizeof(sleepIoctlBuffer),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                   Tests_SelfTarget_SendCompletion,
                                   NULL);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep[ThreadIndex],
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
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetPassive,
                                   &sleepIoctlBuffer,
                                   sizeof(sleepIoctlBuffer),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   ASYNCHRONOUS_REQUEST_TIMEOUT_MS,
                                   Tests_SelfTarget_SendCompletion,
                                   NULL);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    DMF_AlertableSleep_ResetForReuse(moduleContext->DmfModuleAlertableSleep[ThreadIndex],
                                     0);
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep[ThreadIndex],
                                        0,
                                        sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);

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

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // Starts thread.
        //
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread[threadIndex]);
        DmfAssert(NT_SUCCESS(ntStatus));
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

    // SelfTarget (DISPATCH_LEVEL)
    //
    DMF_SelfTarget_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleSelfTargetDispatch);

    // SelfTarget (PASSIVE_LEVEL)
    //
    DMF_SelfTarget_ATTRIBUTES_INIT(&moduleAttributes);
    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleSelfTargetPassive);

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

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Tests_SelfTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_SelfTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
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
        WdfObjectAllocateContext(moduleContext->DmfModuleThread[threadIndex],
                                 &objectAttributes,
                                 (PVOID*)&threadIndexContext);
        threadIndexContext->ThreadIndex = threadIndex;
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
    dmfCallbacksDmf_Tests_SelfTarget.DeviceOpen = DMF_Tests_SelfTarget_Open;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_Tests_SelfTarget);
    dmfCallbacksWdf_Tests_SelfTarget.ModuleD0Entry = DMF_Tests_SelfTarget_ModuleD0Entry;
    dmfCallbacksWdf_Tests_SelfTarget.ModuleD0Exit = DMF_Tests_SelfTarget_ModuleD0Exit;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_SelfTarget,
                                            Tests_SelfTarget,
                                            DMF_CONTEXT_Tests_SelfTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_D0Entry);

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
