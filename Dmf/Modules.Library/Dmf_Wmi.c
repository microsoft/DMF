/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Wmi.c

Abstract:

    Creates a Windows Management Instrumentation (WMI) Instance.
    TODO: Add more WMI support.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_Wmi.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This Module has no Context.
//
DMF_MODULE_DECLARE_NO_CONTEXT(Wmi)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(Wmi)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Wmi_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Wmi.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_Wmi* moduleConfig;
    WDF_WMI_PROVIDER_CONFIG providerConfig;
    WDF_WMI_INSTANCE_CONFIG instanceConfig;
    WDFDEVICE device;
    UNICODE_STRING busResourceName;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    RtlInitUnicodeString(&busResourceName,
                         moduleConfig->ResourceName);

    // Register WMI classes.
    // First specify the resource name which contain the binary MOF resource.
    //
    ntStatus = WdfDeviceAssignMofResourceName(device,
                                              &busResourceName);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceAssignMofResourceName fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_WMI_PROVIDER_CONFIG_INIT(&providerConfig,
                                 &moduleConfig->WmiDataGuid);
    providerConfig.MinInstanceBufferSize = sizeof(moduleConfig->WmiDataSize);

    // Create a WDFWMIPROVIDER handle separately if you are
    // going to dynamically create instances on the provider.  Since one instance
    // is statically created, there is no need to create the provider handle.
    //
    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instanceConfig,
                                                 &providerConfig);

    // By setting Register to TRUE, tell the framework to create a provider
    // as part of the Instance creation call. This eliminates the need to
    // call WdfWmiProviderRegister.
    //
    instanceConfig.Register = TRUE;
    instanceConfig.EvtWmiInstanceQueryInstance = moduleConfig->EvtWmiInstanceQueryInstance;
    instanceConfig.EvtWmiInstanceSetInstance = moduleConfig->EvtWmiInstanceSetInstance;
    instanceConfig.EvtWmiInstanceSetItem = moduleConfig->EvtWmiInstanceSetItem;
    instanceConfig.EvtWmiInstanceExecuteMethod = moduleConfig->EvtWmiInstanceExecuteMethod;

    ntStatus = WdfWmiInstanceCreate(device,
                                    &instanceConfig,
                                    WDF_NO_OBJECT_ATTRIBUTES,
                                    WDF_NO_HANDLE);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfWmiInstanceCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Wmi_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Wmi.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Wmi;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Wmi;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Wmi);
    dmfCallbacksDmf_Wmi.DeviceOpen = DMF_Wmi_Open;

    DMF_MODULE_DESCRIPTOR_INIT(dmfModuleDescriptor_Wmi,
                               Wmi,
                               DMF_MODULE_OPTIONS_PASSIVE,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Wmi,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

// Module Methods
//

// eof: Dmf_Wmi.c
//
