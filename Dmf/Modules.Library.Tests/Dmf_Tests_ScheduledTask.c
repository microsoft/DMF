/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_ScheduledTask.c

Abstract:

    Functional tests for Dmf_ScheduledTask Module

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_ScheduledTask.tmh"

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
    // And index for this task's description in TaskDescriptionArray.
    //
    ULONG DescriptionIndex;
} Tests_ScheduledTask_TaskContext;

// Array of descriptions for all the scheduled tasks we're running.
//
static
Tests_ScheduledTask_TaskDescription
TaskDescriptionArray[] = {
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
        MANUAL_TASK_EXECUTE_COUNT * 2
    },
    {
        ScheduledTask_WorkResult_SuccessButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        MANUAL_TASK_EXECUTE_COUNT * 2
    },
    {
        ScheduledTask_WorkResult_Fail,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        MANUAL_TASK_EXECUTE_COUNT * 2
    },
    {
        ScheduledTask_WorkResult_FailButTryAgain,
        ScheduledTask_Persistence_NotPersistentAcrossReboots,
        ScheduledTask_ExecuteWhen_Other,
        ScheduledTask_ExecutionMode_Immediate,
        MANUAL_TASK_EXECUTE_COUNT * 2
    }
};

#define TASK_COUNT                  (ARRAYSIZE(TaskDescriptionArray))
#define TASK_DELAY_MS               (1000)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // ScheduledTask Modules to test
    //
    DMFMODULE DmfModuleScheduledTask[TASK_COUNT];
    // Callback contexts for scheduled tasks
    //
    Tests_ScheduledTask_TaskContext TaskContext[TASK_COUNT];
    // Timer for delayed validation.
    //
    WDFTIMER ValidationTimer;
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
_IRQL_requires_max_(PASSIVE_LEVEL)
static
ScheduledTask_Result_Type 
Tests_ScheduledTask_TaskCallback(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* CallbackContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState)
{
    Tests_ScheduledTask_TaskContext* taskContext;
    Tests_ScheduledTask_TaskDescription* taskDescription;
    ScheduledTask_Result_Type result;
    BOOLEAN firstCall;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(PreviousState);

    PAGED_CODE();

    taskContext = (Tests_ScheduledTask_TaskContext*)CallbackContext;
    ASSERT(taskContext != NULL);
    
    ASSERT(taskContext->DescriptionIndex < TASK_COUNT);
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
    ASSERT(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArray[index];
        taskContext = &moduleContext->TaskContext[index];

        ASSERT(taskContext->DescriptionIndex == index);

        // All the tasks should execute the specified amount of times at this point.
        //
        ASSERT(taskDescription->TimesShouldExecute == taskContext->TimesExecuted);
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

    ASSERT(TASK_COUNT > 0);

    scheduledTaskModule = moduleContext->DmfModuleScheduledTask[0];
    ASSERT(scheduledTaskModule != NULL);

    // Set to zero, validate results.
    //
    ntStatus = DMF_ScheduledTask_TimesRunSet(scheduledTaskModule,
                                             0);
    ASSERT(STATUS_SUCCESS == ntStatus);

    ntStatus = DMF_ScheduledTask_TimesRunGet(scheduledTaskModule,
                                             &timesRun);
    ASSERT(STATUS_SUCCESS == ntStatus);
    ASSERT(0 == timesRun);

    // Increment, validate results.
    //
    ntStatus = DMF_ScheduledTask_TimesRunIncrement(scheduledTaskModule,
                                                   &timesRun);
    ASSERT(STATUS_SUCCESS == ntStatus);
    ASSERT(0 == timesRun);

    ntStatus = DMF_ScheduledTask_TimesRunGet(scheduledTaskModule,
                                             &timesRun);
    ASSERT(STATUS_SUCCESS == ntStatus);
    ASSERT(1 == timesRun);

    // Set to zero again, so that persistent tasks can run. Validate results.
    //
    ntStatus = DMF_ScheduledTask_TimesRunSet(scheduledTaskModule,
                                             0);
    ASSERT(STATUS_SUCCESS == ntStatus);

    ntStatus = DMF_ScheduledTask_TimesRunGet(scheduledTaskModule,
                                             &timesRun);
    ASSERT(STATUS_SUCCESS == ntStatus);
    ASSERT(0 == timesRun);
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
    ULONG index;
    ULONG timesExecuted;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (index = 0; index < TASK_COUNT; index++)
    {
        taskDescription = &TaskDescriptionArray[index];

        if (taskDescription->ExecuteWhen != ScheduledTask_ExecuteWhen_Other)
        {
            continue;
        }
    
        for (timesExecuted = 0; timesExecuted < MANUAL_TASK_EXECUTE_COUNT; timesExecuted++)
        {
            // Schedule deferred execution
            //
            ntStatus = DMF_ScheduledTask_ExecuteNowDeferred(moduleContext->DmfModuleScheduledTask[index],
                                                            &moduleContext->TaskContext[index]);
            ASSERT(STATUS_SUCCESS == ntStatus);

            // Schedule immediate execution
            //
            DMF_ScheduledTask_ExecuteNow(moduleContext->DmfModuleScheduledTask[index],
                                         &moduleContext->TaskContext[index]);
        }
    }
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
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

        ASSERT(taskContext->DescriptionIndex == index);

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
            ASSERT((taskDescription->TimesShouldExecute == 0) && (taskContext->TimesExecuted == 0) ||
                   (taskDescription->TimesShouldExecute > 0) && (taskContext->TimesExecuted > 0));
            break;

        case ScheduledTask_ExecuteWhen_D0Entry:
            // No immediate D0Enty tasks should be executed yet.
            //
            ASSERT(0 == taskContext->TimesExecuted);
            break;

        default:
            break;
        }
    }

    return STATUS_SUCCESS;
}
#pragma code_seg()

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

        ASSERT(taskContext->DescriptionIndex == index);

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
            ASSERT((taskDescription->TimesShouldExecute == 0) && (taskContext->TimesExecuted == 0) ||
                   (taskDescription->TimesShouldExecute > 0) && (taskContext->TimesExecuted > 0));
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

    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
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

    return STATUS_SUCCESS;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
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

    FuncEntry(DMF_TRACE_Tests_ScheduledTask);

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_Tests_ScheduledTask, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Test APIs to get/set TimesRun value.
    //
    Tests_ScheduledTask_TestTimesRun(DmfModule);
    
    // Run tasks that are manually scheduled.
    //
    Tests_ScheduledTask_RunManualTasks(DmfModule);

Exit:
    
    FuncExit(DMF_TRACE_Tests_ScheduledTask, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
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

    FuncEntry(DMF_TRACE_Tests_ScheduledTask);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfObjectDelete(moduleContext->ValidationTimer);
    moduleContext->ValidationTimer = NULL;

    FuncExitVoid(DMF_TRACE_Tests_ScheduledTask);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Tests_ScheduledTask;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Tests_ScheduledTask;
static DMF_CALLBACKS_WDF DmfCallbacksWdf_Tests_ScheduledTask;

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
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_ScheduledTask* moduleContext;
    DMF_CONFIG_ScheduledTask moduleConfigScheduledTask;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    LONG index;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfModule = NULL;

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Tests_ScheduledTask);
    DmfCallbacksDmf_Tests_ScheduledTask.DeviceOpen = Tests_ScheduledTask_Open;
    DmfCallbacksDmf_Tests_ScheduledTask.DeviceClose = Tests_ScheduledTask_Close;


    DMF_CALLBACKS_WDF_INIT(&DmfCallbacksWdf_Tests_ScheduledTask);
    DmfCallbacksWdf_Tests_ScheduledTask.ModulePrepareHardware = Tests_ScheduledTask_ModulePrepareHardware;
    DmfCallbacksWdf_Tests_ScheduledTask.ModuleD0Entry = Tests_ScheduledTask_ModuleD0Entry;
    DmfCallbacksWdf_Tests_ScheduledTask.ModuleD0Exit = Tests_ScheduledTask_ModuleD0Exit;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Tests_ScheduledTask,
                                            Tests_ScheduledTask,
                                            DMF_CONTEXT_Tests_ScheduledTask,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_Tests_ScheduledTask.CallbacksDmf = &DmfCallbacksDmf_Tests_ScheduledTask;
    DmfModuleDescriptor_Tests_ScheduledTask.CallbacksWdf = &DmfCallbacksWdf_Tests_ScheduledTask;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Tests_ScheduledTask,
                                &dmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_Tests_ScheduledTask, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Create child modules
    //

    // DmfModule will be set as ParentObject for all child modules.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = dmfModule;

    // ScheduledTask array
    // -------------------
    //
    for (index = 0; index < TASK_COUNT; index++)
    {
        DMF_CONFIG_ScheduledTask_AND_ATTRIBUTES_INIT(&moduleConfigScheduledTask,
                                                     &moduleAttributes);
        moduleContext->TaskContext[index].DescriptionIndex = index;
        moduleContext->TaskContext[index].TimesExecuted = 0;
        moduleConfigScheduledTask.EvtScheduledTaskCallback = Tests_ScheduledTask_TaskCallback;
        moduleConfigScheduledTask.CallbackContext = &moduleContext->TaskContext[index];
        moduleConfigScheduledTask.PersistenceType = TaskDescriptionArray[index].PersistenceType;
        moduleConfigScheduledTask.ExecutionMode = TaskDescriptionArray[index].ExecutionMode;
        moduleConfigScheduledTask.ExecuteWhen = TaskDescriptionArray[index].ExecuteWhen;
        moduleConfigScheduledTask.TimerPeriodMsOnSuccess = TASK_DELAY_MS;
        moduleConfigScheduledTask.TimerPeriodMsOnFail = TASK_DELAY_MS;
        ntStatus = DMF_ScheduledTask_Create(Device,
                                         &moduleAttributes,
                                         &objectAttributes,
                                         &moduleContext->DmfModuleScheduledTask[index]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_Tests_ScheduledTask, "DMF_ScheduledTask_Create fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }
    
    *DmfModule = dmfModule;

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        if (NULL != dmfModule)
        {
            DMF_ModuleDestroy(dmfModule);
            dmfModule = NULL;
        }
    }    

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_Tests_ScheduledTask.c
//
