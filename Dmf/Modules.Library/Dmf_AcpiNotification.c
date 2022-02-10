/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AcpiNotification.c

Abstract:

    Provides ACPI Notification Facilities.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_AcpiNotification.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_AcpiNotification
{
    // ACPI Interface functions.
    //
    ACPI_INTERFACE_STANDARD2 AcpiInterface;
    // Workitem for PASSIVE_LEVEL work.
    //
    WDFWORKITEM Workitem;
    // Tracks if registration is enabled.
    //
    BOOLEAN Registered;
} DMF_CONTEXT_AcpiNotification;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(AcpiNotification)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(AcpiNotification)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

VOID
AcpiNotification_Callback(
    _In_ VOID* Context,
    _In_ ULONG NotifyCode
    )
/*++

Routine Description:

    Root notification callback that is the glue for further callbacks as needed. This function
    calls the Client's callback which does the Client specific work for the given notification.

Arguments:

    Context - This Module's handle.
    NotifyCode - Data from ACPI.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_AcpiNotification* moduleContext;
    DMF_CONFIG_AcpiNotification* moduleConfig;
    BOOLEAN callPassiveLevelCallback;

    FuncEntry(DMF_TRACE);

    dmfModule = DMFMODULEVOID_TO_MODULE(Context);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    if (moduleConfig->DispatchCallback != NULL)
    {
        callPassiveLevelCallback = moduleConfig->DispatchCallback(dmfModule,
                                                                  NotifyCode);
        DmfAssert((! callPassiveLevelCallback) ||
                  (moduleConfig->PassiveCallback != NULL));
    }
    else
    {
        // Dispatch Level callback is optional. Client did not specify it.
        // In this case assume Passive Level Callback is required.
        //
        callPassiveLevelCallback = TRUE;
    }

    if (callPassiveLevelCallback)
    {
        if (moduleConfig->PassiveCallback != NULL)
        {
            DmfAssert(moduleContext->Workitem != NULL);
            WdfWorkItemEnqueue(moduleContext->Workitem);
        }
    }
}

EVT_WDF_WORKITEM AcpiNotificationWorkitemHandler;

#pragma code_seg("PAGE")
_Function_class_(EVT_WDF_WORKITEM)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
AcpiNotificationWorkitemHandler(
    _In_ WDFWORKITEM Workitem
    )
/*++

Routine Description:

    Workitem handler for this Module.

Arguments:

    Workitem - WDFORKITEM which gives access to necessary context including this
               Module's DMF Module.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModule;
    DMF_CONFIG_AcpiNotification* moduleConfig;

    PAGED_CODE();

    dmfModule = (DMFMODULE)WdfWorkItemGetParentObject(Workitem);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    DmfAssert(moduleConfig->PassiveCallback != NULL);
    moduleConfig->PassiveCallback(dmfModule);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Check_return_
NTSTATUS
AcpiNotification_AcpiInterfacesAcquire(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register for ACPI device notifications based on the Client's parameters.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AcpiNotification* moduleContext;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = WdfFdoQueryForInterface(device,
                                       &GUID_ACPI_INTERFACE_STANDARD2,
                                       (PINTERFACE)&moduleContext->AcpiInterface,
                                       sizeof(ACPI_INTERFACE_STANDARD2),
                                       1,
                                       NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfFdoQueryForInterface ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = moduleContext->AcpiInterface.RegisterForDeviceNotifications(moduleContext->AcpiInterface.Context,
                                                                           AcpiNotification_Callback,
                                                                           DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "AcpiInterfaces->RegisterForDeviceNotifications() ntStatus=%!STATUS!", ntStatus);

        if (moduleContext->AcpiInterface.InterfaceDereference != NULL)
        {
            moduleContext->AcpiInterface.InterfaceDereference(moduleContext->AcpiInterface.Context);
        }

        goto Exit;
    }

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID
AcpiNotification_AcpiInterfacesRelease(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Unregister for ACPI device notifications that were previously registered.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_AcpiNotification* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Unregister from the ACPI Notification.
    //
    moduleContext->AcpiInterface.UnregisterForDeviceNotifications(moduleContext->AcpiInterface.Context);

    // Dereference the interface.
    //
    DmfAssert(moduleContext->AcpiInterface.InterfaceDereference != NULL);
    moduleContext->AcpiInterface.InterfaceDereference(moduleContext->AcpiInterface.Context);
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
DMF_AcpiNotification_ModulePrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    If Module is not instantiated in manual mode, enable notifications from ACPI.

Arguments:

    DmfModule - This Module's handle.
    ResourcesRaw: Same as passed to EvtPrepareHardware.
    ResourcesTranslated: Same as passed to EvtPrepareHardware.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_AcpiNotification* moduleConfig;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (! moduleConfig->ManualMode)
    {
        ntStatus = DMF_AcpiNotification_EnableDisable(DmfModule,
                                                      TRUE);
    }
    else
    {
        ntStatus = STATUS_SUCCESS;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleReleaseHardware)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_AcpiNotification_ModuleReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    If Module is not instantiated in manual mode, disable notifications from ACPI.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_AcpiNotification* moduleConfig;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (! moduleConfig->ManualMode)
    {
        ntStatus = DMF_AcpiNotification_EnableDisable(DmfModule,
                                                      FALSE);
    }
    else
    {
        ntStatus = STATUS_SUCCESS;
    }

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
DMF_AcpiNotification_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type AcpiNotification.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AcpiNotification* moduleContext;
    WDF_WORKITEM_CONFIG workItemConfiguration;
    WDF_OBJECT_ATTRIBUTES workItemAttributes;
    WDFDEVICE device;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    device = DMF_ParentDeviceGet(DmfModule);

    // Create the Passive Level Workitem if necessary.
    //
    WDF_WORKITEM_CONFIG_INIT(&workItemConfiguration,
                             AcpiNotificationWorkitemHandler);
    workItemConfiguration.AutomaticSerialization = WdfFalse;

    WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
    workItemAttributes.ParentObject = DmfModule;

    ntStatus = WdfWorkItemCreate(&workItemConfiguration,
                                 &workItemAttributes,
                                 &moduleContext->Workitem);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfWorkItemCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // NOTE: Do not create the notification here because notifications can begin immediately afterward
    //       and the Client may not be ready.
    //

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_AcpiNotification_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type AcpiNotification.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_AcpiNotification* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Disable notifications in case Client failed to do so.
    // Do this before work item is deleted in case a notification is 
    // in process. Any remaining workitem will wait to complete afterward.
    //
    DMF_AcpiNotification_EnableDisable(DmfModule,
                                       FALSE);

    // Release the Passive Level Workitem if it exists. Make sure it finishes 
    // processing any pending work (including work in progress).
    //
    if (moduleContext->Workitem != NULL)
    {
        // Wait for pending work to finish.
        //
        WdfWorkItemFlush(moduleContext->Workitem);
        WdfObjectDelete(moduleContext->Workitem);
        moduleContext->Workitem = NULL;
    }
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
DMF_AcpiNotification_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type AcpiNotification.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_AcpiNotification;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_AcpiNotification;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_AcpiNotification;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_AcpiNotification);
    dmfCallbacksDmf_AcpiNotification.DeviceOpen = DMF_AcpiNotification_Open;
    dmfCallbacksDmf_AcpiNotification.DeviceClose = DMF_AcpiNotification_Close;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_AcpiNotification);
    dmfCallbacksWdf_AcpiNotification.ModulePrepareHardware = DMF_AcpiNotification_ModulePrepareHardware;
    dmfCallbacksWdf_AcpiNotification.ModuleReleaseHardware = DMF_AcpiNotification_ModuleReleaseHardware;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_AcpiNotification,
                                            AcpiNotification,
                                            DMF_CONTEXT_AcpiNotification,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_AcpiNotification.CallbacksDmf = &dmfCallbacksDmf_AcpiNotification;
    dmfModuleDescriptor_AcpiNotification.CallbacksWdf = &dmfCallbacksWdf_AcpiNotification;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_AcpiNotification,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_AcpiNotification_EnableDisable(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EnableNotifications
    )
/*++

Routine Description:

    Allows Client to enable/disable notifications.

Arguments:

    DmfModule - This Module's handle.
    EnableNotifications - TRUE to enable notifications. FALSE to disable notifications.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AcpiNotification* moduleContext;
    BOOLEAN registered;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(DmfModule,
                                            AcpiNotification);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = STATUS_UNSUCCESSFUL;

    if (EnableNotifications)
    {
        // Registration will be enabled.
        //
        DMF_ModuleLock(DmfModule);
        registered = moduleContext->Registered;
        moduleContext->Registered = TRUE;
        DMF_ModuleUnlock(DmfModule);

        if (registered)
        {
            // It has already been registered.
            //
            ntStatus = STATUS_SUCCESS;
        }
        else
        {
            // Create the ACPI Notification.
            //
            ntStatus = AcpiNotification_AcpiInterfacesAcquire(DmfModule);
            if (! NT_SUCCESS(ntStatus))
            {
                // Registration failed. Reset flag.
                //
                DMF_ModuleLock(DmfModule);
                moduleContext->Registered = FALSE;
                DMF_ModuleUnlock(DmfModule);

                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "AcpiInterfacesAcquire fails: ntStatus=%!STATUS!", ntStatus);
            }
        }
    }
    else
    {
        // Registration will be disabled.
        //
        DMF_ModuleLock(DmfModule);
        registered = moduleContext->Registered;
        moduleContext->Registered = FALSE;
        DMF_ModuleUnlock(DmfModule);

        if (! registered)
        {
            // It was already unregistered. Do nothing.
            //
        }
        else
        {
            // Stop getting notifications from ACPI.
            //
            AcpiNotification_AcpiInterfacesRelease(DmfModule);
        }
        ntStatus = STATUS_SUCCESS;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_AcpiNotification.c
//
