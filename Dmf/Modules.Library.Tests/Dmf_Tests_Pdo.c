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

#define THREAD_COUNT                            (2)
// Don't use an ever increasing serial number because those serial numbers will be remembered
// by Windows and will slow down the test computer eventually.
//
#define MAXIMUM_NUMBER_OF_PDO_SERIAL_NUMBERS    (THREAD_COUNT)

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
    // Work threads that perform actions on the Pdo Module.
    //
    DMFMODULE DmfModuleThread[THREAD_COUNT + 1];
    // Use alertable sleep to allow driver to unload faster.
    //
    DMFMODULE DmfModuleAlertableSleep[THREAD_COUNT + 1];
    // Serial number in use table.
    //
    BOOLEAN SerialNumbersInUse[MAXIMUM_NUMBER_OF_PDO_SERIAL_NUMBERS + 1];
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

// Stores the Module threadIndex so that the corresponding alterable sleep
// can be retrieved inside the thread's callback.
//
typedef struct
{
    ULONG ThreadIndex;
} THREAD_INDEX_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE(THREAD_INDEX_CONTEXT);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_Pdo_DmfModulesAdd(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_Tests_IoctlHandler moduleConfigTests_IoctlHandler;

    UNREFERENCED_PARAMETER(Device);

    // Tests_IoctlHandler
    // ------------------
    //
    DMF_CONFIG_Tests_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigTests_IoctlHandler,
                                                      &moduleAttributes);
    // This instance will only be access from attached targets. Do not create a device interface.
    //
    moduleConfigTests_IoctlHandler.CreateDeviceInterface = FALSE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);
}

#pragma code_seg("PAGE")
static
void
Tests_Pdo_ThreadAction(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG MaximumTimeMilliseconds,
    _In_ ULONG ThreadIndex
    )
{
    DMF_CONTEXT_Tests_Pdo* moduleContext;
    NTSTATUS ntStatus;
    ULONG timeToSleepMilliSeconds;
    USHORT serialNumber;
    WDFDEVICE device;
    PDO_RECORD pdoRecord;
    BOOLEAN waitAgain;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    waitAgain = TRUE;
    serialNumber = 0;

    DMF_ModuleLock(DmfModule);
    for (USHORT serialNumberIndex = 0; serialNumberIndex < MAXIMUM_NUMBER_OF_PDO_SERIAL_NUMBERS; serialNumberIndex++)
    {
        if (! moduleContext->SerialNumbersInUse[serialNumberIndex])
        {
            moduleContext->SerialNumbersInUse[serialNumberIndex] = TRUE;
            serialNumber = serialNumberIndex + 1;
            break;
        }
    }
    DMF_ModuleUnlock(DmfModule);

    if (0 == serialNumber)
    {
        // No more serial numbers left. Just get out and retry later.
        //
        goto Exit;
    }

    RtlZeroMemory(&pdoRecord,
                  sizeof(pdoRecord));

    pdoRecord.HardwareIds[0] = L"{0ACF873A-242F-4C8B-A97D-8CA4DD9F86F1}\\DmfKTestFunction";
    pdoRecord.Description = L"DMF Test Function Driver (Kernel)";
    pdoRecord.HardwareIdsCount = 1;
    pdoRecord.SerialNumber = serialNumber;
    pdoRecord.EnableDmf = TRUE;
    pdoRecord.EvtDmfDeviceModulesAdd = Tests_Pdo_DmfModulesAdd;

    // Create the PDO.
    //
    ntStatus = DMF_Pdo_DevicePlugEx(moduleContext->DmfModulePdo,
                                    &pdoRecord,
                                    &device);
    ASSERT(NT_SUCCESS(ntStatus));

    // Wait some time.
    //
    timeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(1000, 
                                                                MaximumTimeMilliseconds);
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep[ThreadIndex],
                                        0,
                                        timeToSleepMilliSeconds);
    if (!NT_SUCCESS(ntStatus))
    {
        // Continue to remove the PDO, but do not wait after removing the PDO.
        //
        waitAgain = FALSE;
    }

    // Destroy the PDO.
    //
    ntStatus = DMF_Pdo_DeviceUnplug(moduleContext->DmfModulePdo,
                                    device);
    // NOTE: This can fail when driver is unloading as WDF deletes the PDO automatically.
    //

    DMF_ModuleLock(DmfModule);
    moduleContext->SerialNumbersInUse[serialNumber - 1] = FALSE;
    DMF_ModuleUnlock(DmfModule);

Exit:

    if (waitAgain)
    {
        // Wait some time.
        //
        timeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(1000, 
                                                                    MaximumTimeMilliseconds);
        DMF_AlertableSleep_ResetForReuse(moduleContext->DmfModuleAlertableSleep[ThreadIndex],
                                         0);
        ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep[ThreadIndex],
                                            0,
                                            timeToSleepMilliSeconds);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_Pdo_ThreadAction_Fast(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG ThreadIndex
    )
{
    PAGED_CODE();

    Tests_Pdo_ThreadAction(DmfModule,
                           1000 * 1,
                           ThreadIndex);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_Pdo_ThreadAction_Slow(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG ThreadIndex
    )
{
    PAGED_CODE();

    Tests_Pdo_ThreadAction(DmfModule,
                           1000 * 60,
                           ThreadIndex);
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
    THREAD_INDEX_CONTEXT* threadIndex;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    threadIndex = WdfObjectGet_THREAD_INDEX_CONTEXT(DmfModuleThread);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SLOW:
            Tests_Pdo_ThreadAction_Slow(dmfModule,
                                        threadIndex->ThreadIndex);
            break;
        case TEST_ACTION_FAST:
            Tests_Pdo_ThreadAction_Fast(dmfModule,
                                        threadIndex->ThreadIndex);
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
_Must_inspect_result_
static
NTSTATUS
Tests_Pdo_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Start the threads that create and destroy PDOs.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_Pdo* moduleContext;
    NTSTATUS ntStatus;
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // Create threads that read with expected success, read with expected failure
    // and enumerate.
    //
    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
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

        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThread[threadIndex]);
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
Tests_Pdo_Stop(
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
    LONG threadIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // Interrupt any long sleeps.
        //
        DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleep[threadIndex],
                                 0);
        // Stop the thread.
        //
        DMF_Thread_Stop(moduleContext->DmfModuleThread[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Tests_Pdo_SelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Tests_Pdo callback for ModuleSelfManagedIoInit for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = Tests_Pdo_Start(DmfModule);
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_Tests_Pdo_SelfManagedIoSuspend(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Tests_Pdo callback for ModuleSelfManagedIoSuspend for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    Tests_Pdo_Stop(DmfModule);
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Tests_Pdo_SelfManagedIoRestart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Tests_Pdo callback for ModuleSelfManagedIoRestart for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = Tests_Pdo_Start(DmfModule);
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

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
    for (LONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_Pdo_WorkThread;
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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_Pdo;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_Pdo;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_Tests_Pdo;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_Pdo);
    dmfCallbacksDmf_Tests_Pdo.ChildModulesAdd = DMF_Tests_Pdo_ChildModulesAdd;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_Tests_Pdo);
    dmfCallbacksWdf_Tests_Pdo.ModuleSelfManagedIoInit = DMF_Tests_Pdo_SelfManagedIoInit;
    dmfCallbacksWdf_Tests_Pdo.ModuleSelfManagedIoRestart = DMF_Tests_Pdo_SelfManagedIoRestart;
    dmfCallbacksWdf_Tests_Pdo.ModuleSelfManagedIoSuspend = DMF_Tests_Pdo_SelfManagedIoSuspend;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_Pdo,
                                            Tests_Pdo,
                                            DMF_CONTEXT_Tests_Pdo,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_D0Entry);

    dmfModuleDescriptor_Tests_Pdo.CallbacksDmf = &dmfCallbacksDmf_Tests_Pdo;
    dmfModuleDescriptor_Tests_Pdo.CallbacksWdf = &dmfCallbacksWdf_Tests_Pdo;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_Pdo,
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
