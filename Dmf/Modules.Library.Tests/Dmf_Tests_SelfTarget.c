/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_SelfTarget.c

Abstract:

    Functional tests for Dmf_SelfTarget Module

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

#define THREAD_COUNT                (2)

typedef enum _TEST_ACTION
{
    TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUSCANCEL,
    TEST_ACTION_CONTINUOUS,
    TEST_ACTION_COUNT,
    TEST_ACTION_MINIUM      = TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_MAXIMUM     = TEST_ACTION_CONTINUOUS
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
    DMFMODULE DmfModuleThread[THREAD_COUNT];
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

#pragma code_seg("PAGE")
static
void
Tests_SelfTarget_ThreadAction_Synchronous(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
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
    ntStatus = DMF_SelfTarget_SendSynchronously(moduleContext->DmfModuleSelfTargetDispatch,
                                                &sleepIoctlBuffer,
                                                sizeof(sleepIoctlBuffer),
                                                NULL,
                                                NULL,
                                                ContinuousRequestTarget_RequestType_Ioctl,
                                                IOCTL_Tests_IoctlHandler_SLEEP,
                                                0,
                                                &bytesWritten);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED));
    // TODO: Get time and compare with send time.
    //

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 15000);
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
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED));
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
}

#pragma code_seg("PAGE")
static
void
Tests_SelfTarget_ThreadAction_Asynchronous(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
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
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetDispatch,
                                   &sleepIoctlBuffer,
                                   sizeof(sleepIoctlBuffer),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   0,
                                   Tests_SelfTarget_SendCompletion,
                                   NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED));

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 15000);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetPassive,
                                   &sleepIoctlBuffer,
                                   sizeof(sleepIoctlBuffer),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   0,
                                   Tests_SelfTarget_SendCompletion,
                                   NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED));
}
#pragma code_seg()

// TODO: Module does not support cancellation of individual requests.
//
#pragma code_seg("PAGE")
static
void
Tests_SelfTarget_ThreadAction_AsynchronousCancel(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
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
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetDispatch,
                                   &sleepIoctlBuffer,
                                   sizeof(sleepIoctlBuffer),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   0,
                                   Tests_SelfTarget_SendCompletion,
                                   NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED));
    DMF_Utility_DelayMilliseconds(sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);

    sleepIoctlBuffer.TimeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 15000);
    bytesWritten = 0;
    ntStatus = DMF_SelfTarget_Send(moduleContext->DmfModuleSelfTargetPassive,
                                   &sleepIoctlBuffer,
                                   sizeof(sleepIoctlBuffer),
                                   NULL,
                                   NULL,
                                   ContinuousRequestTarget_RequestType_Ioctl,
                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                   0,
                                   Tests_SelfTarget_SendCompletion,
                                   NULL);
    ASSERT(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED));
    DMF_Utility_DelayMilliseconds(sleepIoctlBuffer.TimeToSleepMilliSeconds / 2);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_SelfTarget_ThreadAction_Continous(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

}
#pragma code_seg()

#pragma code_seg("PAGE")
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
            Tests_SelfTarget_ThreadAction_Synchronous(dmfModule);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_SelfTarget_ThreadAction_Asynchronous(dmfModule);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_SelfTarget_ThreadAction_AsynchronousCancel(dmfModule);
            break;
        case TEST_ACTION_CONTINUOUS:
            Tests_SelfTarget_ThreadAction_Continous(dmfModule);
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

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_Tests_SelfTarget_SelfManagedIoInit(
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
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
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
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread[index]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThread[index]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Tests_SelfTarget_SelfManagedIoCleanup(
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
    DMF_CONTEXT_Tests_SelfTarget* moduleContext;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_Stop(moduleContext->DmfModuleThread[index]);
    }

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
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
    for (ULONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_SelfTarget_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThread[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Tests_SelfTarget;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Tests_SelfTarget;
static DMF_CALLBACKS_WDF DmfCallbacksWdf_Tests_SelfTarget;

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

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Tests_SelfTarget);
    DmfCallbacksDmf_Tests_SelfTarget.ChildModulesAdd = DMF_Tests_SelfTarget_ChildModulesAdd;

    DMF_CALLBACKS_WDF_INIT(&DmfCallbacksWdf_Tests_SelfTarget);
    DmfCallbacksWdf_Tests_SelfTarget.ModuleSelfManagedIoInit = DMF_Tests_SelfTarget_SelfManagedIoInit;
    DmfCallbacksWdf_Tests_SelfTarget.ModuleSelfManagedIoCleanup = DMF_Tests_SelfTarget_SelfManagedIoCleanup;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Tests_SelfTarget,
                                            Tests_SelfTarget,
                                            DMF_CONTEXT_Tests_SelfTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_D0Entry);

    DmfModuleDescriptor_Tests_SelfTarget.CallbacksDmf = &DmfCallbacksDmf_Tests_SelfTarget;
    DmfModuleDescriptor_Tests_SelfTarget.CallbacksWdf = &DmfCallbacksWdf_Tests_SelfTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Tests_SelfTarget,
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
