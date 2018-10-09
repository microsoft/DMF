/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AcpiNotification.c

Abstract:

    Provides ACPI Notification Facilities.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_AcpiNotification.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Specific external includes for this DMF Module.
//
#include <Guiddef.h>
#include <wdmguid.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // ACPI Interface functions.
    //
    ACPI_INTERFACE_STANDARD2 AcpiInterface;
    // Workitem for PASSIVE_LEVEL work.
    //
    WDFWORKITEM Workitem;
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
        ASSERT((! callPassiveLevelCallback) ||
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
            ASSERT(moduleContext->Workitem != NULL);
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

    ASSERT(moduleConfig->PassiveCallback != NULL);
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
    ASSERT(moduleContext->AcpiInterface.InterfaceDereference != NULL);
    moduleContext->AcpiInterface.InterfaceDereference(moduleContext->AcpiInterface.Context);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
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
    WDF_WORKITEM_CONFIG_INIT(&workItemConfiguration, AcpiNotificationWorkitemHandler);
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

    // Create the ACPI Notification.
    //
    ntStatus = AcpiNotification_AcpiInterfacesAcquire(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "AcpiInterfacesAcquire fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
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

    // Stop getting notifications from ACPI.
    //
    AcpiNotification_AcpiInterfacesRelease(DmfModule);

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
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_AcpiNotification;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_AcpiNotification;

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

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_AcpiNotification);
    DmfCallbacksDmf_AcpiNotification.DeviceOpen = DMF_AcpiNotification_Open;
    DmfCallbacksDmf_AcpiNotification.DeviceClose = DMF_AcpiNotification_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_AcpiNotification,
                                            AcpiNotification,
                                            DMF_CONTEXT_AcpiNotification,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_AcpiNotification.CallbacksDmf = &DmfCallbacksDmf_AcpiNotification;
    DmfModuleDescriptor_AcpiNotification.ModuleConfigSize = sizeof(DMF_CONFIG_AcpiNotification);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_AcpiNotification,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// eof: Dmf_AcpiNotification.c
//
