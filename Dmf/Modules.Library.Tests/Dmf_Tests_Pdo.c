/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_Pdo.c

Abstract:

    Functional tests for Dmf_Pdo Module.

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
#include "Dmf_Tests_Pdo.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define THREAD_COUNT                            (2)
// Don't use an ever increasing serial number because those serial numbers will be remembered
// by Windows and will slow down the test computer eventually.
//
#define MAXIMUM_PDO_SERIAL_NUMBER               (THREAD_COUNT)

// For test purposes to easily enable/disable types of PDOs.
//
#define PDO_ENABLE_KERNELMODE
#define PDO_ENABLE_USERMODE

#define NO_FAST_REMOVE
#if !defined(FAST_REMOVE)

// Remove PDOs slowly.
//
#define PDO_SLOW_TIMEOUT_ONLY
#define NO_PDO_FAST_TIMEOUT_ONLY

#define MINIMUM_PDO_TIMEOUT_SECONDS     (5)
#define FAST_PDO_TIMEOUT_SECONDS        (60)
#define SLOW_PDO_TIMEOUT_SECONDS        (3600)  // 60 minutes for PnPDTest

#else

// Remove PDOs fast.
//
#define NO_PDO_SLOW_TIMEOUT_ONLY
#define PDO_FAST_TIMEOUT_ONLY

#define MINIMUM_PDO_TIMEOUT_SECONDS     (5)
#define FAST_PDO_TIMEOUT_SECONDS        (30)
#define SLOW_PDO_TIMEOUT_SECONDS        (3600)

#endif

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

typedef struct _DMF_CONTEXT_Tests_Pdo
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
    BOOLEAN SerialNumbersInUse[MAXIMUM_PDO_SERIAL_NUMBER + 1];
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
    // This instance will only be accessed from attached targets. Do not create a device interface.
    // (To be clear, this is the target for DMF_Tests_DefaultTarget.)
    //
    moduleConfigTests_IoctlHandler.CreateDeviceInterface = FALSE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);
}

// {71D84E2B-73E5-4235-B16E-706BF96AAD37}
DEFINE_GUID(GUID_Test1, 
0x71d84e2b, 0x73e5, 0x4235, 0xb1, 0x6e, 0x70, 0x6b, 0xf9, 0x6a, 0xad, 0x37);

// {7A7907AC-D445-4E38-B444-9382AADF97AF}
DEFINE_GUID(GUID_Test2, 
0x7a7907ac, 0xd445, 0x4e38, 0xb4, 0x44, 0x93, 0x82, 0xaa, 0xdf, 0x97, 0xaf);

DEFINE_DEVPROPKEY(DEVPKEY_Test1, 0x3696efa5, 0x5f52, 0x4fb8, 0xad, 0x89, 0x1a, 0xfd, 0xb1, 0x91, 0xb3, 0x36, 1);

DEFINE_DEVPROPKEY(DEVPKEY_Test2, 0xd80a5b3c, 0x4e5c, 0x4f1e, 0x84, 0x34, 0xc5, 0x3a, 0x1a, 0x41, 0xe3, 0x95, 2);

#include <devpkey.h>

#pragma code_seg("PAGE")
static
void
Tests_Pdo_ThreadAction(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG MinimumTimeMilliseconds,
    _In_ ULONG MaximumTimeMilliseconds,
    _In_ ULONG ThreadIndex
    )
{
    DMF_CONTEXT_Tests_Pdo* moduleContext;
    NTSTATUS ntStatus;
    ULONG timeToSleepMilliSeconds;
    USHORT serialNumberPair;
    WDFDEVICE devicePair[2];
    PDO_RECORD pdoRecord;
    BOOLEAN waitAgain;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    waitAgain = TRUE;
    serialNumberPair = 0;

    // Each iteration requires two PDOs with unique serial numbers.
    //
    DMF_ModuleLock(DmfModule);
    for (USHORT serialNumber = 1; serialNumber < MAXIMUM_PDO_SERIAL_NUMBER; serialNumber += 2)
    {
        if (! moduleContext->SerialNumbersInUse[serialNumber])
        {
            // Each iteration requires two serial numbers, one for Kernel-mode
            // function driver and one for User-mode function driver.
            //
            moduleContext->SerialNumbersInUse[serialNumber + 0] = TRUE;
            DmfAssert(!moduleContext->SerialNumbersInUse[serialNumber + 1]);
            moduleContext->SerialNumbersInUse[serialNumber + 1] = TRUE;
            serialNumberPair = serialNumber;
            break;
        }
    }
    DMF_ModuleUnlock(DmfModule);

    if (0 == serialNumberPair)
    {
        // No more serial numbers left. Just get out and retry later.
        //
        goto Exit;
    }

#if defined(PDO_ENABLE_KERNELMODE)
    // Create the Kernel-mode function driver PDO.
    //
    const int numberOfProperties = 2;
    ULONG propertyValue[numberOfProperties] = { 0x010002000, 0x020003000 };
    Pdo_DevicePropertyEntry propertyTableEntry[numberOfProperties];
    Pdo_DeviceProperty_Table propertyTable;

    RtlZeroMemory(&propertyTableEntry,
                  sizeof(propertyTableEntry));

    propertyTableEntry[0].ValueData = (VOID*)&propertyValue[0];
    propertyTableEntry[0].ValueSize = sizeof(ULONG);
    propertyTableEntry[0].ValueType = DEVPROP_TYPE_UINT32;
    propertyTableEntry[0].DeviceInterfaceGuid = (GUID*)&GUID_Test1;
    propertyTableEntry[0].RegisterDeviceInterface = TRUE;
    propertyTableEntry[0].DevicePropertyData.Flags = 0;
    propertyTableEntry[0].DevicePropertyData.Lcid = LOCALE_NEUTRAL;
    propertyTableEntry[0].DevicePropertyData.Size = sizeof(propertyTableEntry[0].DevicePropertyData);
    propertyTableEntry[0].DevicePropertyData.PropertyKey = &DEVPKEY_Test1;

    propertyTableEntry[1].ValueData = (VOID*)&propertyValue[1];
    propertyTableEntry[1].ValueSize = sizeof(ULONG);
    propertyTableEntry[1].ValueType = DEVPROP_TYPE_UINT32;
    propertyTableEntry[1].DeviceInterfaceGuid = (GUID*)&GUID_Test2;
    propertyTableEntry[1].RegisterDeviceInterface = TRUE;
    propertyTableEntry[1].DevicePropertyData.Flags = 0;
    propertyTableEntry[1].DevicePropertyData.Lcid = LOCALE_NEUTRAL;
    propertyTableEntry[1].DevicePropertyData.Size = sizeof(propertyTableEntry[1].DevicePropertyData);
    propertyTableEntry[1].DevicePropertyData.PropertyKey = &DEVPKEY_Test2;

    RtlZeroMemory(&propertyTable,
                  sizeof(propertyTable));
    propertyTable.ItemCount = numberOfProperties;
    propertyTable.TableEntries = propertyTableEntry;

    RtlZeroMemory(&pdoRecord,
                  sizeof(pdoRecord));
    pdoRecord.HardwareIds[0] = L"{0ACF873A-242F-4C8B-A97D-8CA4DD9F86F1}\\DmfKTestFunction";
    pdoRecord.Description = L"DMF Test Function Driver (Kernel)";
    pdoRecord.HardwareIdsCount = 1;
    pdoRecord.SerialNumber = serialNumberPair + 0;
    pdoRecord.EnableDmf = TRUE;
    pdoRecord.EvtDmfDeviceModulesAdd = Tests_Pdo_DmfModulesAdd;
    pdoRecord.DeviceProperties = &propertyTable;

    ntStatus = DMF_Pdo_DevicePlugEx(moduleContext->DmfModulePdo,
                                    &pdoRecord,
                                    &devicePair[0]);
    // It can fail during low memory situations.
    //
    if (!NT_SUCCESS(ntStatus))
    {
        DMF_ModuleLock(DmfModule);
        moduleContext->SerialNumbersInUse[serialNumberPair + 0] = FALSE;
        moduleContext->SerialNumbersInUse[serialNumberPair + 1] = FALSE;
        DMF_ModuleUnlock(DmfModule);

        goto Exit;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "PDO: PLUG [%S] device=0x%p", pdoRecord.Description, devicePair[0]);
#endif

#if defined(PDO_ENABLE_USERMODE)
    // Create the User-mode function driver PDO.
    //
    RtlZeroMemory(&pdoRecord,
                  sizeof(pdoRecord));
    pdoRecord.HardwareIds[0] = L"{5F30A572-D79D-43EC-BD35-D5556F09CE21}\\DmfUTestFunction";
    pdoRecord.Description = L"DMF Test Function Driver (User)";
    pdoRecord.HardwareIdsCount = 1;
    pdoRecord.SerialNumber = serialNumberPair + 1;
    pdoRecord.EnableDmf = TRUE;
    pdoRecord.EvtDmfDeviceModulesAdd = Tests_Pdo_DmfModulesAdd;
    ntStatus = DMF_Pdo_DevicePlugEx(moduleContext->DmfModulePdo,
                                    &pdoRecord,
                                    &devicePair[1]);
    // It can fail during low memory situations.
    //
    if (!NT_SUCCESS(ntStatus))
    {
        // Undo the successful plug in above.
        //
        ntStatus = DMF_Pdo_DeviceUnplug(moduleContext->DmfModulePdo,
                                        devicePair[0]);
        DMF_ModuleLock(DmfModule);
        moduleContext->SerialNumbersInUse[serialNumberPair + 0] = FALSE;
        moduleContext->SerialNumbersInUse[serialNumberPair + 1] = FALSE;
        DMF_ModuleUnlock(DmfModule);

        goto Exit;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "PDO: PLUG [%S] device=0x%p", pdoRecord.Description, devicePair[1]);
#endif

    // Wait some time.
    //
    timeToSleepMilliSeconds = TestsUtility_GenerateRandomNumber(MinimumTimeMilliseconds, 
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

    // Destroy the PDOs.
    //
#if defined(PDO_ENABLE_KERNELMODE)
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "PDO: UNPLUG START device=0x%p", devicePair[0]);
    ntStatus = DMF_Pdo_DeviceUnplug(moduleContext->DmfModulePdo,
                                    devicePair[0]);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "PDO: UNPLUG END device=0x%p", devicePair[0]);
#endif
#if defined(PDO_ENABLE_USERMODE)
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "PDO: UNPLUG START device=0x%p", devicePair[1]);
    // NOTE: This can fail when driver is unloading as WDF deletes the PDO automatically.
    //
    ntStatus = DMF_Pdo_DeviceUnplug(moduleContext->DmfModulePdo,
                                    devicePair[1]);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "PDO: UNPLUG END device=0x%p", devicePair[1]);
#endif

    DMF_ModuleLock(DmfModule);
    moduleContext->SerialNumbersInUse[serialNumberPair + 0] = FALSE;
    moduleContext->SerialNumbersInUse[serialNumberPair + 1] = FALSE;
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
                           1000 * MINIMUM_PDO_TIMEOUT_SECONDS,
                           1000 * FAST_PDO_TIMEOUT_SECONDS,
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
                           1000 * FAST_PDO_TIMEOUT_SECONDS,
                           1000 * SLOW_PDO_TIMEOUT_SECONDS,
                           ThreadIndex);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
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
#if defined(PDO_SLOW_TIMEOUT_ONLY)
    testAction = TEST_ACTION_SLOW;
#elif defined(PDO_FAST_TIMEOUT_ONLY)
    testAction = TEST_ACTION_FAST;
#else
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);
#endif

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

        // Reset the alertable sleep in case it was stopped.
        //
        DMF_AlertableSleep_ResetForReuse(moduleContext->DmfModuleAlertableSleep[threadIndex],
                                         0);

        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
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
_Function_class_(DMF_ModuleSelfManagedIoInit)
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

_Function_class_(DMF_ModuleSelfManagedIoSuspend)
_IRQL_requires_max_(PASSIVE_LEVEL)
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

_Function_class_(DMF_ModuleSelfManagedIoRestart)
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

    FuncEntry(DMF_TRACE);

    ntStatus = Tests_Pdo_Start(DmfModule);
    
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
