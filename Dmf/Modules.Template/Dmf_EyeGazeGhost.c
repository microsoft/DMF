/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_EyeGazeGhost.c

Abstract:

    DMF version of Eye Gaze Ghost sample.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_EyeGazeGhost.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_EyeGazeGhost
{
    // Underlying VHIDMINI2 support.
    //
    DMFMODULE DmfModuleVirtualEyeGaze;
    DMFMODULE DmfModuleThread;

    LONG X;
    LONG Y;
} DMF_CONTEXT_EyeGazeGhost;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(EyeGazeGhost)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(EyeGazeGhost)

// MemoryTag.
//
#define MemoryTag 'mDHV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include <time.h>

_Function_class_(EVT_DMF_Thread_Function)
VOID
EyeGazeGhost_ThreadWork(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Child DMF Module Thread.
    "Work" is to perform firmware update protocol as per the specification.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    None

--*/
{
    DMFMODULE dmfModuleEyeGazeGhost;
    DMF_CONTEXT_EyeGazeGhost* moduleContext;
    DMF_CONFIG_EyeGazeGhost* moduleConfig;

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    dmfModuleEyeGazeGhost = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleEyeGazeGhost);
    moduleConfig = DMF_CONFIG_GET(dmfModuleEyeGazeGhost);

    GAZE_REPORT gazeReport = { 0 };

    gazeReport.ReportId = HID_USAGE_TRACKING_DATA;

    __time64_t ltime;
    _time64(&ltime);

    gazeReport.TimeStamp = (ULONGLONG)ltime;
    gazeReport.GazePoint.X = moduleContext->X;
    gazeReport.GazePoint.Y = moduleContext->Y;
    //KdPrint(("GazePoint = [%1.5f, %1.5f]\n", gazeReport.GazePoint.X, gazeReport.GazePoint.Y));
    DMF_VirtualEyeGaze_GazeReportSend(moduleContext->DmfModuleVirtualEyeGaze,
                                      &gazeReport);

    if (moduleContext->X > 600)
    {
        moduleContext->X = 0;
        if (moduleContext->Y > 600)
        {
            moduleContext->Y = 0;
        }
        else
        {
            ++(moduleContext->Y);
        }
    }
    else
    {
        ++(moduleContext->X);
    }

    if (! DMF_Thread_IsStopPending(DmfModule))
    {
        DMF_Utility_DelayMilliseconds(15);
        DMF_Thread_WorkReady(moduleContext->DmfModuleThread);
    }
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
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_EyeGazeGhost_ChildModulesAdd(
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
    DMF_CONFIG_EyeGazeGhost* moduleConfig;
    DMF_CONTEXT_EyeGazeGhost* moduleContext;
    DMF_CONFIG_VirtualEyeGaze moduleConfigVirtualEyeGaze;
    DMF_CONFIG_Thread moduleConfigThread;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // VirtualEyeGaze
    // --------------
    //
    DMF_CONFIG_VirtualEyeGaze_AND_ATTRIBUTES_INIT(&moduleConfigVirtualEyeGaze,
                                                  &moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleVirtualEyeGaze);

    // Thread
    // ------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = EyeGazeGhost_ThreadWork;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThread);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_EyeGazeGhost_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type EyeGazeGhost.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_EyeGazeGhost* moduleConfig;
    DMF_CONTEXT_EyeGazeGhost* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread);

    DMF_Thread_WorkReady(moduleContext->DmfModuleThread);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_EyeGazeGhost_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type EyeGazeGhost.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_EyeGazeGhost* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Thread_Stop(moduleContext->DmfModuleThread);

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
DMF_EyeGazeGhost_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type EyeGazeGhost.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_EyeGazeGhost;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_EyeGazeGhost;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_EyeGazeGhost);
    dmfCallbacksDmf_EyeGazeGhost.ChildModulesAdd = DMF_EyeGazeGhost_ChildModulesAdd;
    dmfCallbacksDmf_EyeGazeGhost.DeviceOpen = DMF_EyeGazeGhost_Open;
    dmfCallbacksDmf_EyeGazeGhost.DeviceClose = DMF_EyeGazeGhost_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_EyeGazeGhost,
                                            EyeGazeGhost,
                                            DMF_CONTEXT_EyeGazeGhost,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_EyeGazeGhost.CallbacksDmf = &dmfCallbacksDmf_EyeGazeGhost;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_EyeGazeGhost,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_EyeGazeGhost.c
//
