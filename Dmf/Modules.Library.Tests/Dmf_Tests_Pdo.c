/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_Pdo.c

Abstract:

    Functional tests for Dmf_Pdo Module

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_Pdo.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define THREAD_COUNT                (2)

typedef enum _TEST_ACTION
{
    TEST_ACTION_SLOW,
    TEST_ACTION_FAST,
    TEST_ACTION_COUNT,
    TEST_ACTION_MINIUM      = TEST_ACTION_SLOW,
    TEST_ACTION_MAXIMUM     = TEST_ACTION_FAST
} TEST_ACTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Module under test.
    //
    DMFMODULE DmfModulePdo;
    // Current Serial number of PDO.
    //
    USHORT SerialNumber;
    // Work threads that perform actions on the Pdo Module.
    //
    DMFMODULE DmfModuleThread[THREAD_COUNT];
} DMF_CONTEXT_Tests_Pdo;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_Pdo)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_Pdo)

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
Tests_Pdo_ThreadAction(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG MaximumTimeMilliseconds
    )
{
    DMF_CONTEXT_Tests_Pdo* moduleContext;
    NTSTATUS ntStatus;
    ULONG timeToSleepMilliSeconds;
    PWSTR hardwareIds[] = { L"{0ACF873A-242F-4C8B-A97D-8CA4DD9F86F1}\\DmfKTestFunction" };
    USHORT serialNumber;
    WDFDEVICE device;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);
    moduleContext->SerialNumber++;
    serialNumber = moduleContext->SerialNumber;
    DMF_ModuleUnlock(DmfModule);

    // Create the PDO.
    //
    ntStatus = DMF_Pdo_DevicePlug(moduleContext->DmfModulePdo,
                                  hardwareIds,
                                  1,
                                  NULL,
                                  0,
                                  L"DMF Test Function Driver (Kernel)",
                                  serialNumber,
                                  &device);
    ASSERT(NT_SUCCESS(ntStatus));

    // Wait some time.
    //
    timeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(1000, 
                                                                MaximumTimeMilliseconds);
    DMF_Utility_DelayMilliseconds(timeToSleepMilliSeconds);

    // Destroy the PDO.
    //
    ntStatus = DMF_Pdo_DeviceUnplug(moduleContext->DmfModulePdo,
                                    device);
    // NOTE: This can fail when driver is unloading as WDF deletes the PDO automatically.
    //
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_Pdo_ThreadAction_Fast(
    _In_ DMFMODULE DmfModule
    )
{
    PAGED_CODE();

    Tests_Pdo_ThreadAction(DmfModule,
                           1000);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_Pdo_ThreadAction_Slow(
    _In_ DMFMODULE DmfModule
    )
{
    PAGED_CODE();

    Tests_Pdo_ThreadAction(DmfModule,
                           15000);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_Pdo_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_Pdo* moduleContext;
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
        case TEST_ACTION_SLOW:
            Tests_Pdo_ThreadAction_Slow(dmfModule);
            break;
        case TEST_ACTION_FAST:
            Tests_Pdo_ThreadAction_Fast(dmfModule);
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

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Tests_Pdo_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_Pdo.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_Pdo* moduleContext;
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
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_Pdo_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_Pdo.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_Pdo* moduleContext;
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
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_Pdo_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_Pdo* moduleContext;
    DMF_CONFIG_Thread moduleConfigThread;
    DMF_CONFIG_Pdo moduleConfigPdo;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Pdo
    // ---
    //
    DMF_CONFIG_Pdo_AND_ATTRIBUTES_INIT(&moduleConfigPdo,
                                       &moduleAttributes);
    moduleConfigPdo.InstanceIdFormatString = L"DmfKFunctionPdo(%d)";
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &moduleContext->DmfModulePdo);


    // Thread
    // ------
    //
    for (ULONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_Pdo_WorkThread;
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

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Tests_Pdo;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Tests_Pdo;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Tests_Pdo_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_Pdo.

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

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Tests_Pdo);
    DmfCallbacksDmf_Tests_Pdo.ChildModulesAdd = DMF_Tests_Pdo_ChildModulesAdd;
    DmfCallbacksDmf_Tests_Pdo.DeviceOpen = Tests_Pdo_Open;
    DmfCallbacksDmf_Tests_Pdo.DeviceClose = Tests_Pdo_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Tests_Pdo,
                                            Tests_Pdo,
                                            DMF_CONTEXT_Tests_Pdo,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_D0Entry);

    DmfModuleDescriptor_Tests_Pdo.CallbacksDmf = &DmfCallbacksDmf_Tests_Pdo;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Tests_Pdo,
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

// eof: Dmf_Tests_Pdo.c
//
