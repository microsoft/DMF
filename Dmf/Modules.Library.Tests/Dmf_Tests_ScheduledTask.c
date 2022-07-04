/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_ScheduledTask.c

Abstract:

    Functional tests for Dmf_ScheduledTask Module.

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
#include "Dmf_Tests_ScheduledTask.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define MANUAL_TASK_EXECUTE_COUNT   (50)

// Task configuration and validation data.
//
typedef struct
{   
    ScheduledTask_Result_Type ResultOnFirstCall;
    ScheduledTask_Persistence_Type PersistenceType;
    ScheduledTask_ExecuteWhen_Type ExecuteWhen;
    ScheduledTask_ExecutionMode_Type ExecutionMode;
    LONG TimesShouldExecute;
} Tests_ScheduledTask_TaskDescription;

// Context data for a scheduled task
//
typedef struct
{
    // A number of times this task was executed.
    //
    LONG TimesExecuted;
    // And taskDescriptionIndex for this task's description in TaskDescriptionArray.
    //
    ULONG DescriptionIndex;
} Tests_ScheduledTask_TaskContext;

// Array of descriptions for all the scheduled tasks we're running.
//
static
Tests_ScheduledTask_TaskDescription
TaskDescriptionArray[] = 
{
    // Non-persistent immediate tasks.
    // Don't test Fail cases, since it'll block driver from loading.
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Immediate,
        1
    },
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Immediate,
        2
    },
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Immediate,
        1
    },
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Immediate,
        2
    },
    // Non-persistent deferred tasks.
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        1
    },
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        2
    },
    {   ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        1
    },
    {   ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        2
    },
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        1
    },
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        2
    },
    {   ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        1
    },
    {   ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        2
    },
    // Persistent tasks.
    // Only one of them should execute, since ScheduledTask module
    // does not support per-instance persistence tracking.
    //
    {
        ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_PersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Immediate,
        1
    },
    { 
        ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_PersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Immediate,
        0
    },
    // Tasks with manual execution.
    //
    {
        ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        (MANUAL_TASK_EXECUTE_COUNT * 2)
    },
    {
        ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        (MANUAL_TASK_EXECUTE_COUNT * 2)
    },
    {
        ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        (MANUAL_TASK_EXECUTE_COUNT * 2)
    },
    {
        ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        (MANUAL_TASK_EXECUTE_COUNT * 2)
    }
};

static
Tests_ScheduledTask_TaskDescription
TaskDescriptionArrayEx[] = 
{
    // Non-persistent immediate tasks.
    // Don't test Fail cases, since it'll block driver from loading.
    // 0x0
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Immediate,
        1
    },
    // 0x1
    //
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Immediate,
        2
    },
    // 0x2
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Immediate,
        1
    },
    // 0x3
    //
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Immediate,
        2
    },
    // Non-persistent deferred tasks.
    //
    // 0x4
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        1
    },
    // 0x5
    //
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        2
    },
    // 0x6
    //
    {   ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        1
    },
    // 0x7
    //
    {   ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        2
    },
    // 0x8
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        1
    },
    // 0x9
    //
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        2
    },
    // 0xA
    //
    {   ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        1
    },
    // 0xB
    //
    {   ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        2
    },
    // Persistent tasks.
    // (It won't executed since it already executed in legacy test.)
    //
    // 0xC
    //
    {
        ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_PersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Immediate,
        0
    },
    // 0xD
    //
    { 
        ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_PersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Immediate,
        0
    },
    // Tasks with manual execution.
    //
    // 0xE
    //
    {
        ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        (1)
    },
    // 0xF
    //
    {
        ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        (2)
    },
    // 0x10
    //
    {
        ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        (2)
    },
    // 0x11
    //
    {
        ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        (2)
    }
};

static
Tests_ScheduledTask_TaskDescription
TaskDescriptionArrayExCancel[] = 
{
    // Non-persistent immediate tasks.
    // Don't test Fail cases, since it'll block driver from loading.
    // 0x0
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x1
    //
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x2
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x3
    //
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // Non-persistent deferred tasks.
    //
    // 0x4
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x5
    //
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x6
    //
    {   ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x7
    //
    {   ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x8
    //
    {   ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x9
    //
    {   ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0xA
    //
    {   ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0xB
    //
    {   ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // Persistent tasks.
    // (It won't executed since it already executed in legacy test.)
    //
    // 0xC
    //
    {
        ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_PersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_PrepareHardware,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0xD
    //
    { 
        ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_PersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_D0Entry,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // Tasks with manual execution.
    //
    // 0xE
    //
    {
        ScheduledTask_WorkResult_Success,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0xF
    //
    {
        ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x10
    //
    {
        ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Deferred,
        0
    },
    // 0x11
    //
    {
        ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Deferred,
        0
    }
};

#define TASK_COUNT                  (ARRAYSIZE(TaskDescriptionArray))
#define TASK_DELAY_MS               (1000)
#define TASK_DELAY_CANCEL_MS        (4000)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Tests_ScheduledTask
{
    // ScheduledTask Modules to test.
    //
    DMFMODULE DmfModuleScheduledTask[TASK_COUNT];
    // ScheduledTask Modules to test.
    //
    DMFMODULE DmfModuleScheduledTaskEx[TASK_COUNT];
    // ScheduledTask Modules to test. (Cancel)
    //
    DMFMODULE DmfModuleScheduledTaskExCancel[TASK_COUNT];
    // Callback contexts for scheduled tasks
    //
    Tests_ScheduledTask_TaskContext TaskContext[TASK_COUNT];
    Tests_ScheduledTask_TaskContext TaskContextEx[TASK_COUNT];
    Tests_ScheduledTask_TaskContext TaskContextExCancel[TASK_COUNT];
    // Timer for delayed validation.
    //
    WDFTIMER ValidationTimer;
    WDFTIMER ValidationTimerEx;
    WDFTIMER ValidationTimerExCancel;
} DMF_CONTEXT_Tests_ScheduledTask;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_ScheduledTask)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_ScheduledTask)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_ScheduledTask_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
ScheduledTask_Result_Type 
Tests_ScheduledTask_TaskCallback(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* CallbackContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    Tests_ScheduledTask_TaskContext* taskContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ScheduledTask_Result_Type result;
    BOOLEAN firstCall;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(PreviousState);

    PAGED_CODE();

    taskContext = (Tests_ScheduledTask_TaskContext*)CallbackContext;
    DmfAssert(taskContext != NULL);
    
    DmfAssert(taskContext->DescriptionIndex < TASK_COUNT);
    taskDescription = &TaskDescriptionArray[taskContext->DescriptionIndex];

    firstCall = (1 == InterlockedIncrement(&taskContext->TimesExecuted));

    // From the first call return the value from task description.
    // From the second call always return success.
    //
    result = (firstCall) ? taskDescription->ResultOnFirstCall
                         : ScheduledTask_WorkResult_Success;

    return result;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_ScheduledTask_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
ScheduledTask_Result_Type 
Tests_ScheduledTask_TaskCallbackEx(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* CallbackContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    Tests_ScheduledTask_TaskContext* taskContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ScheduledTask_Result_Type result;
    BOOLEAN firstCall;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(PreviousState);

    PAGED_CODE();

    taskContext = (Tests_ScheduledTask_TaskContext*)CallbackContext;
    DmfAssert(taskContext != NULL);
    
    DmfAssert(taskContext->DescriptionIndex < TASK_COUNT);
    taskDescription = &TaskDescriptionArrayEx[taskContext->DescriptionIndex];

    firstCall = (1 == InterlockedIncrement(&taskContext->TimesExecuted));

    // From the first call return the value from task description.
    // From the second call always return success.
    //
    result = (firstCall) ? taskDescription->ResultOnFirstCall
                         : ScheduledTask_WorkResult_Success;

    return result;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_ScheduledTask_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
ScheduledTask_Result_Type 
Tests_ScheduledTask_TaskCallbackExCancel(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* CallbackContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    Tests_ScheduledTask_TaskContext* taskContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ScheduledTask_Result_Type result;
    BOOLEAN firstCall;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(PreviousState);

    PAGED_CODE();

    // This callback should never happen since it should have been canceled.
    //
    DmfAssert(FALSE);

    taskContext = (Tests_ScheduledTask_TaskContext*)CallbackContext;
    DmfAssert(taskContext != NULL);
    
    DmfAssert(taskContext->DescriptionIndex < TASK_COUNT);
    taskDescription = &TaskDescriptionArrayExCancel[taskContext->DescriptionIndex];

    firstCall = (1 == InterlockedIncrement(&taskContext->TimesExecuted));

    // From the first call return the value from task description.
    // From the second call always return success.
    //
    result = (firstCall) ? taskDescription->ResultOnFirstCall
                         : ScheduledTask_WorkResult_Success;

    return result;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_ScheduledTask_ValidationTimerCallback(
    _In_ WDFTIMER WdfTimer
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    Tests_ScheduledTask_TaskContext* taskContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ULONG index;

    PAGED_CODE();

    dmfModule = (DMFMODULE)WdfTimerGetParentObject(WdfTimer);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArray[index];
        taskContext = &moduleContext->TaskContext[index];

        DmfAssert(taskContext->DescriptionIndex == index);

        // All the tasks should execute the specified amount of times at this point.
        //
        DmfAssert(taskDescription->TimesShouldExecute == taskContext->TimesExecuted);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_ScheduledTask_ValidationTimerCallbackEx(
    _In_ WDFTIMER WdfTimer
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    Tests_ScheduledTask_TaskContext* taskContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ULONG index;

    PAGED_CODE();

    dmfModule = (DMFMODULE)WdfTimerGetParentObject(WdfTimer);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArrayEx[index];
        taskContext = &moduleContext->TaskContextEx[index];

        DmfAssert(taskContext->DescriptionIndex == index);

        // All the tasks should execute the specified amount of times at this point.
        //
        DmfAssert(taskDescription->TimesShouldExecute == taskContext->TimesExecuted);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_ScheduledTask_ValidationTimerCallbackExCancel(
    _In_ WDFTIMER WdfTimer
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    Tests_ScheduledTask_TaskContext* taskContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ULONG index;

    PAGED_CODE();

    dmfModule = (DMFMODULE)WdfTimerGetParentObject(WdfTimer);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArrayExCancel[index];
        taskContext = &moduleContext->TaskContextExCancel[index];

        DmfAssert(taskContext->DescriptionIndex == index);

        // All the tasks should execute the specified amount of times at this point.
        //
        DmfAssert(taskDescription->TimesShouldExecute == taskContext->TimesExecuted);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
Tests_ScheduledTask_TestTimesRun(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    DMFMODULE scheduledTaskModule;
    NTSTATUS ntStatus;
    ULONG timesRun;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(TASK_COUNT > 0);

    // NOTE: No need to test in both legacy and Ex versions.
    //
    scheduledTaskModule = moduleContext->DmfModuleScheduledTask[0];
    DmfAssert(scheduledTaskModule != NULL);

    // Set to zero, validate results.
    //
    ntStatus = DMF_ScheduledTask_TimesRunSet(scheduledTaskModule,
                                             1);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    ntStatus = DMF_ScheduledTask_TimesRunGet(scheduledTaskModule,
                                             &timesRun);
    DmfAssert(STATUS_SUCCESS == ntStatus);
    DmfAssert(1 == timesRun);

    // Set to zero again, so that persistent tasks can run. Validate results.
    //
    ntStatus = DMF_ScheduledTask_TimesRunSet(scheduledTaskModule,
                                             0);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    ntStatus = DMF_ScheduledTask_TimesRunGet(scheduledTaskModule,
                                             &timesRun);
    DmfAssert(STATUS_SUCCESS == ntStatus);
    DmfAssert(0 == timesRun);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
Tests_ScheduledTask_RunManualTasks(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ULONG taskDescriptionIndex;
    ULONG timesExecuted;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (taskDescriptionIndex = 0; taskDescriptionIndex < TASK_COUNT; taskDescriptionIndex++)
    {
        taskDescription = &TaskDescriptionArray[taskDescriptionIndex];

        if (taskDescription->ExecuteWhen != ScheduledTask_ExecuteWhen_Other)
        {
            continue;
        }

        for (timesExecuted = 0; timesExecuted < MANUAL_TASK_EXECUTE_COUNT; timesExecuted++)
        {
            DMFMODULE moduleUnderTest = moduleContext->DmfModuleScheduledTask[taskDescriptionIndex];

            // Schedule deferred execution (legacy version with bug).
            //
            ntStatus = DMF_ScheduledTask_ExecuteNowDeferred(moduleUnderTest,
                                                            &moduleContext->TaskContext[taskDescriptionIndex]);
            DmfAssert(STATUS_SUCCESS == ntStatus);

            // Schedule immediate execution.
            //
            DMF_ScheduledTask_ExecuteNow(moduleUnderTest,
                                         &moduleContext->TaskContext[taskDescriptionIndex]);
        }
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
Tests_ScheduledTask_RunManualTasksEx(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ULONG taskDescriptionIndex;
    ULONG timesExecuted;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (taskDescriptionIndex = 0; taskDescriptionIndex < TASK_COUNT; taskDescriptionIndex++)
    {
        taskDescription = &TaskDescriptionArrayEx[taskDescriptionIndex];

        if (taskDescription->ExecuteWhen != ScheduledTask_ExecuteWhen_Other)
        {
            continue;
        }

        for (timesExecuted = 0; timesExecuted < MANUAL_TASK_EXECUTE_COUNT; timesExecuted++)
        {
            DMFMODULE moduleUnderTest = moduleContext->DmfModuleScheduledTaskEx[taskDescriptionIndex];

            // Schedule deferred execution (Ex version that is correct).
            //
            ntStatus = DMF_ScheduledTask_ExecuteNowDeferredEx(moduleUnderTest);
            DmfAssert(STATUS_SUCCESS == ntStatus);
        }
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
Tests_ScheduledTask_RunManualTasksExCancel(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ULONG taskDescriptionIndex;
    ULONG timesExecuted;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (taskDescriptionIndex = 0; taskDescriptionIndex < TASK_COUNT; taskDescriptionIndex++)
    {
        taskDescription = &TaskDescriptionArrayExCancel[taskDescriptionIndex];

        if (taskDescription->ExecuteWhen != ScheduledTask_ExecuteWhen_Other)
        {
            continue;
        }

        for (timesExecuted = 0; timesExecuted < MANUAL_TASK_EXECUTE_COUNT; timesExecuted++)
        {
            DMFMODULE moduleUnderTest = moduleContext->DmfModuleScheduledTaskEx[taskDescriptionIndex];

            // Schedule deferred execution.
            //
            if (timesExecuted > 0)
            {
                // If previously canceled, restart.
                //
                DMF_ScheduledTask_Restart(moduleUnderTest);
            }
            ntStatus = DMF_ScheduledTask_ExecuteNowDeferredEx(moduleUnderTest);
            DmfAssert(STATUS_SUCCESS == ntStatus);

            // Wait a short time...not too long otherwise start up takes a long time.
            // TODO: Run in different thread.
            //
            DMF_Utility_DelayMilliseconds(30);

            // Cancel it.
            //
            DMF_ScheduledTask_Cancel(moduleUnderTest);
        }
    }
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ModulePrepareHardware)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
Tests_ScheduledTask_ModulePrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    Tests_ScheduledTask_TaskContext* taskContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ULONG index;
    
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArray[index];
        taskContext = &moduleContext->TaskContext[index];

        DmfAssert(taskContext->DescriptionIndex == index);

        if (taskDescription->ExecutionMode != ScheduledTask_ExecutionMode_Immediate)
        {
            continue;
        }

        switch (taskDescription->ExecuteWhen)
        {
        case ScheduledTask_ExecuteWhen_PrepareHardware:
            // Each immediate PrepareHardware task 
            // should execute at least once by this time.
            //
            DmfAssert((taskDescription->TimesShouldExecute == 0) && (taskContext->TimesExecuted == 0) ||
                      (taskDescription->TimesShouldExecute > 0) && (taskContext->TimesExecuted > 0));
            break;

        case ScheduledTask_ExecuteWhen_D0Entry:
            // No immediate D0Enty tasks should be executed yet.
            //
            DmfAssert(0 == taskContext->TimesExecuted);
            break;

        default:
            break;
        }
    }

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArrayEx[index];
        taskContext = &moduleContext->TaskContextEx[index];

        DmfAssert(taskContext->DescriptionIndex == index);

        if (taskDescription->ExecutionMode != ScheduledTask_ExecutionMode_Immediate)
        {
            continue;
        }

        switch (taskDescription->ExecuteWhen)
        {
        case ScheduledTask_ExecuteWhen_PrepareHardware:
            // Each immediate PrepareHardware task 
            // should execute at least once by this time.
            //
            DmfAssert((taskDescription->TimesShouldExecute == 0) && (taskContext->TimesExecuted == 0) ||
                      (taskDescription->TimesShouldExecute > 0) && (taskContext->TimesExecuted > 0));
            break;

        case ScheduledTask_ExecuteWhen_D0Entry:
            // No immediate D0Enty tasks should be executed yet.
            //
            DmfAssert(0 == taskContext->TimesExecuted);
            break;

        default:
            break;
        }
    }

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArrayExCancel[index];
        taskContext = &moduleContext->TaskContextExCancel[index];

        DmfAssert(taskContext->DescriptionIndex == index);

        switch (taskDescription->ExecuteWhen)
        {
        case ScheduledTask_ExecuteWhen_PrepareHardware:
            DMF_ScheduledTask_Cancel(moduleContext->DmfModuleScheduledTaskExCancel[index]);
            break;

        case ScheduledTask_ExecuteWhen_D0Entry:
            // No immediate D0Enty tasks should be executed yet.
            //
            DmfAssert(0 == taskContext->TimesExecuted);
            break;

        default:
            break;
        }
    }

    return STATUS_SUCCESS;
}
#pragma code_seg()

_Function_class_(DMF_ModuleD0Entry)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Tests_ScheduledTask_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    Tests_ScheduledTask_TaskContext* taskContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ULONG index;

    UNREFERENCED_PARAMETER(PreviousState);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArray[index];
        taskContext = &moduleContext->TaskContext[index];

        DmfAssert(taskContext->DescriptionIndex == index);

        if (taskDescription->ExecutionMode != ScheduledTask_ExecutionMode_Immediate)
        {
            continue;
        }

        switch (taskDescription->ExecuteWhen)
        {
        case ScheduledTask_ExecuteWhen_PrepareHardware:
        case ScheduledTask_ExecuteWhen_D0Entry:
            // Each immediate PrepareHardware and D0Entry task 
            // should execute at least once by this time.
            //
            DmfAssert((taskDescription->TimesShouldExecute == 0) && (taskContext->TimesExecuted == 0) ||
                      (taskDescription->TimesShouldExecute > 0) && (taskContext->TimesExecuted > 0));
            break;

        default:
            break;
        }
    }

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArrayEx[index];
        taskContext = &moduleContext->TaskContextEx[index];

        DmfAssert(taskContext->DescriptionIndex == index);

        if (taskDescription->ExecutionMode != ScheduledTask_ExecutionMode_Immediate)
        {
            continue;
        }

        switch (taskDescription->ExecuteWhen)
        {
        case ScheduledTask_ExecuteWhen_PrepareHardware:
        case ScheduledTask_ExecuteWhen_D0Entry:
            // Each immediate PrepareHardware and D0Entry task 
            // should execute at least once by this time.
            //
            DmfAssert((taskDescription->TimesShouldExecute == 0) && (taskContext->TimesExecuted == 0) ||
                      (taskDescription->TimesShouldExecute > 0) && (taskContext->TimesExecuted > 0));
            break;

        default:
            break;
        }
    }

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArrayExCancel[index];
        taskContext = &moduleContext->TaskContextExCancel[index];

        DmfAssert(taskContext->DescriptionIndex == index);

        switch (taskDescription->ExecuteWhen)
        {
        case ScheduledTask_ExecuteWhen_PrepareHardware:
            DmfAssert(0 == taskContext->TimesExecuted);
            break;
        case ScheduledTask_ExecuteWhen_D0Entry:
            DMF_ScheduledTask_Cancel(moduleContext->DmfModuleScheduledTaskExCancel[index]);
            break;

        default:
            break;
        }
    }

    // Set up a timer to validate final tasks status.
    // Double the delay, to make sure all the retried tasks are complete.
    //
    WdfTimerStart(moduleContext->ValidationTimer,
                  WDF_REL_TIMEOUT_IN_MS(TASK_DELAY_MS * 2));
    WdfTimerStart(moduleContext->ValidationTimerEx,
                  WDF_REL_TIMEOUT_IN_MS(TASK_DELAY_MS * 10));
    WdfTimerStart(moduleContext->ValidationTimerExCancel,
                  WDF_REL_TIMEOUT_IN_MS(TASK_DELAY_MS * 10));

    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleD0Exit)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Tests_ScheduledTask_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
{
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;

    UNREFERENCED_PARAMETER(TargetState);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfTimerStop(moduleContext->ValidationTimer,
                 TRUE);
    WdfTimerStop(moduleContext->ValidationTimerEx,
                 TRUE);
    WdfTimerStop(moduleContext->ValidationTimerExCancel,
                 TRUE);

    return STATUS_SUCCESS;
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
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Tests_ScheduledTask_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_ScheduledTask.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Create a timer object to validate if tasks were executed properly.
    //
    WDF_TIMER_CONFIG_INIT(&timerConfig,
                          Tests_ScheduledTask_ValidationTimerCallback);
    timerConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    objectAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    ntStatus = WdfTimerCreate(&timerConfig,
                              &objectAttributes,
                              &moduleContext->ValidationTimer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    
    // Create a timer object to validate if tasks were executed properly.
    //
    WDF_TIMER_CONFIG_INIT(&timerConfig,
                          Tests_ScheduledTask_ValidationTimerCallbackEx);
    timerConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    objectAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    ntStatus = WdfTimerCreate(&timerConfig,
                              &objectAttributes,
                              &moduleContext->ValidationTimerEx);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Create a timer object to validate if tasks were executed properly.
    //
    WDF_TIMER_CONFIG_INIT(&timerConfig,
                          Tests_ScheduledTask_ValidationTimerCallbackExCancel);
    timerConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    objectAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    ntStatus = WdfTimerCreate(&timerConfig,
                              &objectAttributes,
                              &moduleContext->ValidationTimerExCancel);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Test APIs to get/set TimesRun value.
    //
    Tests_ScheduledTask_TestTimesRun(DmfModule);
    
    // Run tasks that are manually scheduled.
    //
    Tests_ScheduledTask_RunManualTasks(DmfModule);
    Tests_ScheduledTask_RunManualTasksEx(DmfModule);
    Tests_ScheduledTask_RunManualTasksExCancel(DmfModule);

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_ScheduledTask_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_ScheduledTask.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
 
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfObjectDelete(moduleContext->ValidationTimer);
    moduleContext->ValidationTimer = NULL;
    WdfObjectDelete(moduleContext->ValidationTimerEx);
    moduleContext->ValidationTimerEx = NULL;
    WdfObjectDelete(moduleContext->ValidationTimerExCancel);
    moduleContext->ValidationTimerEx = NULL;

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_ScheduledTask_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    DMF_CONFIG_ScheduledTask moduleConfigScheduledTask;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Thread
    // ------
    //
    for (ULONG scheduledTaskIndex = 0; scheduledTaskIndex < TASK_COUNT; scheduledTaskIndex++)
    {
        // Used for legacy deferred Method.
        //
        DMF_CONFIG_ScheduledTask_AND_ATTRIBUTES_INIT(&moduleConfigScheduledTask,
                                                     &moduleAttributes);
        moduleContext->TaskContext[scheduledTaskIndex].DescriptionIndex = scheduledTaskIndex;
        moduleContext->TaskContext[scheduledTaskIndex].TimesExecuted = 0;
        moduleConfigScheduledTask.EvtScheduledTaskCallback = Tests_ScheduledTask_TaskCallback;
        moduleConfigScheduledTask.CallbackContext = &moduleContext->TaskContext[scheduledTaskIndex];
        moduleConfigScheduledTask.PersistenceType = TaskDescriptionArray[scheduledTaskIndex].PersistenceType;
        moduleConfigScheduledTask.ExecutionMode = TaskDescriptionArray[scheduledTaskIndex].ExecutionMode;
        moduleConfigScheduledTask.ExecuteWhen = TaskDescriptionArray[scheduledTaskIndex].ExecuteWhen;
        moduleConfigScheduledTask.TimerPeriodMsOnSuccess = TASK_DELAY_MS;
        moduleConfigScheduledTask.TimerPeriodMsOnFail = TASK_DELAY_MS;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleScheduledTask[scheduledTaskIndex]);

        // Used for Ex deferred Method.
        //
        DMF_CONFIG_ScheduledTask_AND_ATTRIBUTES_INIT(&moduleConfigScheduledTask,
                                                     &moduleAttributes);
        moduleContext->TaskContextEx[scheduledTaskIndex].DescriptionIndex = scheduledTaskIndex;
        moduleContext->TaskContextEx[scheduledTaskIndex].TimesExecuted = 0;
        moduleConfigScheduledTask.EvtScheduledTaskCallback = Tests_ScheduledTask_TaskCallbackEx;
        moduleConfigScheduledTask.CallbackContext = &moduleContext->TaskContextEx[scheduledTaskIndex];
        moduleConfigScheduledTask.PersistenceType = TaskDescriptionArrayEx[scheduledTaskIndex].PersistenceType;
        moduleConfigScheduledTask.ExecutionMode = TaskDescriptionArrayEx[scheduledTaskIndex].ExecutionMode;
        moduleConfigScheduledTask.ExecuteWhen = TaskDescriptionArrayEx[scheduledTaskIndex].ExecuteWhen;
        moduleConfigScheduledTask.TimerPeriodMsOnSuccess = TASK_DELAY_MS;
        moduleConfigScheduledTask.TimerPeriodMsOnFail = TASK_DELAY_MS;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleScheduledTaskEx[scheduledTaskIndex]);

        // Used for Cancel Method.
        //
        DMF_CONFIG_ScheduledTask_AND_ATTRIBUTES_INIT(&moduleConfigScheduledTask,
                                                     &moduleAttributes);
        moduleContext->TaskContextExCancel[scheduledTaskIndex].DescriptionIndex = scheduledTaskIndex;
        moduleContext->TaskContextExCancel[scheduledTaskIndex].TimesExecuted = 0;
        moduleConfigScheduledTask.EvtScheduledTaskCallback = Tests_ScheduledTask_TaskCallbackExCancel;
        moduleConfigScheduledTask.CallbackContext = &moduleContext->TaskContextExCancel[scheduledTaskIndex];
        moduleConfigScheduledTask.PersistenceType = TaskDescriptionArrayExCancel[scheduledTaskIndex].PersistenceType;
        moduleConfigScheduledTask.ExecutionMode = TaskDescriptionArrayExCancel[scheduledTaskIndex].ExecutionMode;
        moduleConfigScheduledTask.ExecuteWhen = TaskDescriptionArrayExCancel[scheduledTaskIndex].ExecuteWhen;
        moduleConfigScheduledTask.TimerPeriodMsOnSuccess = TASK_DELAY_CANCEL_MS;
        moduleConfigScheduledTask.TimerPeriodMsOnFail = TASK_DELAY_CANCEL_MS;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleScheduledTaskExCancel[scheduledTaskIndex]);
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
DMF_Tests_ScheduledTask_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_ScheduledTask.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where we return the created DMFMODULE handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_ScheduledTask;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_ScheduledTask;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_Tests_ScheduledTask;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_ScheduledTask);
    dmfCallbacksDmf_Tests_ScheduledTask.DeviceOpen = Tests_ScheduledTask_Open;
    dmfCallbacksDmf_Tests_ScheduledTask.DeviceClose = Tests_ScheduledTask_Close;


    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_Tests_ScheduledTask);
    dmfCallbacksDmf_Tests_ScheduledTask.ChildModulesAdd = DMF_Tests_ScheduledTask_ChildModulesAdd;
    dmfCallbacksWdf_Tests_ScheduledTask.ModulePrepareHardware = Tests_ScheduledTask_ModulePrepareHardware;
    dmfCallbacksWdf_Tests_ScheduledTask.ModuleD0Entry = Tests_ScheduledTask_ModuleD0Entry;
    dmfCallbacksWdf_Tests_ScheduledTask.ModuleD0Exit = Tests_ScheduledTask_ModuleD0Exit;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_ScheduledTask,
                                            Tests_ScheduledTask,
                                            DMF_CONTEXT_Tests_ScheduledTask,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Tests_ScheduledTask.CallbacksDmf = &dmfCallbacksDmf_Tests_ScheduledTask;
    dmfModuleDescriptor_Tests_ScheduledTask.CallbacksWdf = &dmfCallbacksWdf_Tests_ScheduledTask;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_ScheduledTask,
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

// eof: Dmf_Tests_ScheduledTask.c
//
