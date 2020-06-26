/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_AlertableSleep.c

Abstract:

    Functional tests for Dmf_AlertableSleep Module.

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
#include "Dmf_Tests_AlertableSleep.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Tests_AlertableSleep
{
    // AlertableSleep Module. (Test Module)
    //
    DMFMODULE DmfModuleAlertableSleepTest;
    // AlertableSleep Module. (Internal Use)
    //
    DMFMODULE DmfModuleAlertableSleepInternal;
    // Thread that sleep.
    //
    DMFMODULE DmfModuleThreadSleep;
    // Thread that interrupts.
    //
    DMFMODULE DmfModuleThreadInterrupt;
    // Indicates Module has started started running or is stopping.
    // When it is stopping, all sleeps are interrupted.
    //
    BOOLEAN Closing;
} DMF_CONTEXT_Tests_AlertableSleep;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_AlertableSleep)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_AlertableSleep)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define TIMEOUT_MS_MAXIMUM  15000

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_AlertableSleep_WorkThreadSleep(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_AlertableSleep* moduleContext;
    ULONG timeout;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    timeout = TestsUtility_GenerateRandomNumber(0, 
                                                TIMEOUT_MS_MAXIMUM);

    // Wait for a while. The wait may or may not be interrupted based on what the
    // other thread does.
    //
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleepTest,
                                        0,
                                        timeout);

    // Reset from previous iteration.
    //
    DMF_AlertableSleep_ResetForReuse(moduleContext->DmfModuleAlertableSleepTest,
                                     0);

    // Repeat the test, until stop is signaled or the function stopped because the
    // driver is stopping.
    //
    if ((! DMF_Thread_IsStopPending(DmfModuleThread)) &&
        (! moduleContext->Closing))
    {
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_AlertableSleep_WorkThreadInterrupt(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_AlertableSleep* moduleContext;
    ULONG timeout;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    timeout = TestsUtility_GenerateRandomNumber(0, 
                                                TIMEOUT_MS_MAXIMUM);

    // Wait for a while.
    // 
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Waiting to interrupt...");
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleepInternal,
                                        0,
                                        timeout);

    if (NT_SUCCESS(ntStatus))
    {
        // Reset for next time.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "RESET Wait to interrupt...");
        DMF_AlertableSleep_ResetForReuse(moduleContext->DmfModuleAlertableSleepInternal,
                                         0);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "INTERRUPTED Wait to interrupt...");
    }

    // Abort the current sleep.
    //
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interrupt...");
    DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepTest,
                             0);

    // Repeat the test, until stop is signaled or the function stopped because the
    // driver is stopping.
    //
    if ((! DMF_Thread_IsStopPending(DmfModuleThread)) &&
        (! moduleContext->Closing))
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
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Tests_AlertableSleep_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_AlertableSleep.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Tests_AlertableSleep* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Start the threads.
    //
    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadSleep);
    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThreadInterrupt);

    // Tell the threads they have work to do.
    //
    DMF_Thread_WorkReady(moduleContext->DmfModuleThreadSleep);
    DMF_Thread_WorkReady(moduleContext->DmfModuleThreadInterrupt);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Tests_AlertableSleep_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Close an instance of a DMF Module of type Test_AlertableSleep.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_AlertableSleep* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleContext->Closing = TRUE;

    DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepInternal,
                             0);
    DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleepTest,
                             0);

    DMF_Thread_Stop(moduleContext->DmfModuleThreadSleep);
    DMF_Thread_Stop(moduleContext->DmfModuleThreadInterrupt);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_AlertableSleep_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_AlertableSleep* moduleContext;
    DMF_CONFIG_Thread moduleConfigThread;
    DMF_CONFIG_AlertableSleep moduleConfigAlertableSleep;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // AlertableSleep (Test)
    // ---------------------
    //
    DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                  &moduleAttributes);
    moduleConfigAlertableSleep.EventCount = 1;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleAlertableSleepTest);

    // AlertableSleep (Internal)
    // -------------------------
    //
    DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                  &moduleAttributes);
    moduleConfigAlertableSleep.EventCount = 1;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleAlertableSleepInternal);

    // Thread (Sleeps)
    // ---------------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_AlertableSleep_WorkThreadSleep;
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleThreadSleep);

    // Thread (Interrupts)
    // -------------------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_AlertableSleep_WorkThreadInterrupt;
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleThreadInterrupt);

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
DMF_Tests_AlertableSleep_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_AlertableSleep.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_AlertableSleep;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_AlertableSleep;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_AlertableSleep);
    dmfCallbacksDmf_Tests_AlertableSleep.ChildModulesAdd = DMF_Tests_AlertableSleep_ChildModulesAdd;
    dmfCallbacksDmf_Tests_AlertableSleep.DeviceOpen = DMF_Tests_AlertableSleep_Open;
    dmfCallbacksDmf_Tests_AlertableSleep.DeviceClose = DMF_Tests_AlertableSleep_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_AlertableSleep,
                                            Tests_AlertableSleep,
                                            DMF_CONTEXT_Tests_AlertableSleep,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Tests_AlertableSleep.CallbacksDmf = &dmfCallbacksDmf_Tests_AlertableSleep;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_AlertableSleep,
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

// eof: Dmf_Tests_AlertableSleep.c
//
