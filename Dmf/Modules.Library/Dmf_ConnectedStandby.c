/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ConnectedStandby.c

Abstract:

    Provides Connected Standby Notification Facilities.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_ConnectedStandby.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_ConnectedStandby
{
    // Handle power state changes.
    //
    VOID* PowerSettingHandle;
} DMF_CONTEXT_ConnectedStandby;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(ConnectedStandby)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ConnectedStandby)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
NTSTATUS
ConnectedStandby_OnConnectedStandby(
    LPCGUID SettingGuid,
    VOID* Value,
    ULONG ValueLength,
    VOID* Context
    )
/*++

Routine Description:

    This callback is called whenever the system enters or exits Connected
    Standby.

Arguments:

    SettingGuid - Supplies the pointer to the GUID for the power setting being updated.
    Value - Supplies the pointer to power setting buffer.
    ValueLength - Supplies the length of the power setting buffer.
    Context - Supplies the pointer to the context supplied when registering
              the power setting notification callback (the parent device context in
              this case).

Return Value:

    NTSTATUS indicating whether the callback succeeded.

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONFIG_ConnectedStandby* moduleConfig;
    ULONG state;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "ConnectedStandby_OnConnectedStandby entered.");

    ntStatus = STATUS_SUCCESS;

    dmfModule = (DMFMODULE)Context;
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    if (RtlCompareMemory(SettingGuid,
                         (LPCGUID)&GUID_CONSOLE_DISPLAY_STATE,
                         sizeof(GUID)) != sizeof(GUID))
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    if ((ValueLength != sizeof(ULONG)) || (Value == NULL))
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    RtlCopyMemory(&state, 
                  Value, 
                  ValueLength);

    switch (state)
    {
        // The system entered Connected Standby.
        //
        case PowerMonitorOff:
            moduleConfig->ConnectedStandbyStateChangedCallback(dmfModule, 
                                                               TRUE);
            break;

        // The system exited Connected Standby.
        //
        case PowerMonitorOn:
            moduleConfig->ConnectedStandbyStateChangedCallback(dmfModule, 
                                                               FALSE);
            break;

        // This is an unexpected and unhandled state.
        //
        default:
            break;
    }

Exit:

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "ConnectedStandby_OnConnectedStandby exited.");

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

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
DMF_ConnectedStandby_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type ConnectedStandby.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ConnectedStandby* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);


    ntStatus = PoRegisterPowerSettingCallback(NULL,
                                              &GUID_CONSOLE_DISPLAY_STATE,
                                              &ConnectedStandby_OnConnectedStandby,
                                              (VOID*)DmfModule,
                                              &moduleContext->PowerSettingHandle);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "PoRegisterPowerSettingCallback fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

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
DMF_ConnectedStandby_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Close an instance of a DMF Module of type ConnectedStandby.
    Clean up po handle if it exists.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ConnectedStandby* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->PowerSettingHandle != NULL)
    {
        ntStatus = PoUnregisterPowerSettingCallback(moduleContext->PowerSettingHandle);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                        "PoUnregisterPowerSettingCallback fails: ntStatus=%!STATUS!", ntStatus);
        }
        moduleContext->PowerSettingHandle = NULL;
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
DMF_ConnectedStandby_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type ConnectedStandby.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_ConnectedStandby;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_ConnectedStandby;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_ConnectedStandby;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_ConnectedStandby);
    dmfCallbacksDmf_ConnectedStandby.DeviceOpen = DMF_ConnectedStandby_Open;
    dmfCallbacksDmf_ConnectedStandby.DeviceClose = DMF_ConnectedStandby_Close;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_ConnectedStandby);

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_ConnectedStandby,
                                            ConnectedStandby,
                                            DMF_CONTEXT_ConnectedStandby,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_ConnectedStandby.CallbacksDmf = &dmfCallbacksDmf_ConnectedStandby;
    dmfModuleDescriptor_ConnectedStandby.CallbacksWdf = &dmfCallbacksWdf_ConnectedStandby;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_ConnectedStandby,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// eof: Dmf_ConnectedStandby.c
//
