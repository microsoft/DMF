/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ThermalCoolingInterface.c

Abstract:

    Implement the Thermal Cooling Interface.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_ThermalCoolingInterface.tmh"

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
DMF_MODULE_DECLARE_NO_CONTEXT(ThermalCoolingInterface)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ThermalCoolingInterface)

// Memory Pool Tag.
//
#define MemoryTag 'MICT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

DEVICE_ACTIVE_COOLING ThermalCoolingInterface_ActiveCooling;

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
ThermalCoolingInterface_ActiveCooling(
    _Inout_opt_ VOID* Context,
    _In_ BOOLEAN Engaged
    )
/*++

Routine Description:

    The ActiveCooling callback routine engages or disengages a device's active-cooling function.

Arguments:

    Context - A pointer to interface-specific context information.
              The caller sets this parameter to the value of the Context member of the
              THERMAL_COOLING_INTERFACE structure that the driver previously supplied to the caller.
    Engaged - Indicates whether to engage or disengage active cooling.
              If TRUE, the driver must engage active cooling (for example, by turning the fan on).
              If FALSE, the driver must disengage active cooling (for example, by turning the fan off).

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONFIG_ThermalCoolingInterface* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModule = (DMFMODULE)Context;
    DmfAssert(dmfModule != NULL);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    if (moduleConfig->CallbackActiveCooling != NULL)
    {
        moduleConfig->CallbackActiveCooling(dmfModule,
                                            Engaged);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

DEVICE_PASSIVE_COOLING ThermalCoolingInterface_PassiveCooling;

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
ThermalCoolingInterface_PassiveCooling(
    _Inout_opt_ VOID* Context,
    _In_ ULONG Percentage
    )
/*++

Routine Description:

    The PassiveCooling callback routine controls the degree to which the device
    must throttle its performance to meet cooling requirements.

Arguments:

    Context - A pointer to interface-specific context information.
              The caller sets this parameter to the value of the Context member of the
              THERMAL_COOLING_INTERFACE structure that the driver previously supplied to the caller.
    Percentage - The percentage of full performance at which the device is permitted to operate.
                 A parameter value of 100 indicates that the device is under no cooling restrictions
                 and can operate at full performance level.
                 A parameter value of zero indicates that the device must operate at its lowest thermal level.
                 A parameter value between 0 and 100 indicates the degree to which the device's performance
                 must be throttled to reduce heat generation.
                 This parameter value is a threshold that the device must not exceed.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONFIG_ThermalCoolingInterface* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModule = (DMFMODULE)Context;
    DmfAssert(dmfModule != NULL);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    if (moduleConfig->CallbackPassiveCooling != NULL)
    {
        moduleConfig->CallbackPassiveCooling(dmfModule,
                                             Percentage);
    }

    FuncExitVoid(DMF_TRACE);
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
DMF_ThermalCoolingInterface_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type ThermalCoolingInterface.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    THERMAL_COOLING_INTERFACE thermalCoolingInterface;
    WDF_QUERY_INTERFACE_CONFIG queryInterfaceConfig;
    DMF_CONFIG_ThermalCoolingInterface* moduleConfig;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    ntStatus = STATUS_SUCCESS;

    device = DMF_ParentDeviceGet(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    RtlZeroMemory(&thermalCoolingInterface, sizeof(thermalCoolingInterface));
    thermalCoolingInterface.Size = sizeof(thermalCoolingInterface);
    thermalCoolingInterface.Version = THERMAL_COOLING_INTERFACE_VERSION;
    thermalCoolingInterface.Context = (VOID*)DmfModule;
    thermalCoolingInterface.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
    thermalCoolingInterface.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;
    if (moduleConfig->CallbackActiveCooling != NULL)
    {
        thermalCoolingInterface.Flags = thermalCoolingInterface.Flags | ThermalDeviceFlagActiveCooling;
        thermalCoolingInterface.ActiveCooling = ThermalCoolingInterface_ActiveCooling;
    }
    if (moduleConfig->CallbackPassiveCooling != NULL)
    {
        thermalCoolingInterface.Flags = thermalCoolingInterface.Flags | ThermalDeviceFlagPassiveCooling;
        thermalCoolingInterface.PassiveCooling = ThermalCoolingInterface_PassiveCooling;
    }
    WDF_QUERY_INTERFACE_CONFIG_INIT(&queryInterfaceConfig,
                                    (PINTERFACE)&thermalCoolingInterface,
                                    &GUID_THERMAL_COOLING_INTERFACE,
                                    NULL);

    // Create a driver-defined interface for thermal cooling
    // that other drivers can query and use.
    //
    ntStatus = WdfDeviceAddQueryInterface(device,
                                          &queryInterfaceConfig);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceAddQueryInterface() fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = WdfDeviceCreateDeviceInterface(device,
                                              &GUID_DEVINTERFACE_THERMAL_COOLING,
                                              &moduleConfig->ReferenceString);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceCreateDeviceInterface() fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
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
DMF_ThermalCoolingInterface_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type ThermalCoolingInterface.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_ThermalCoolingInterface;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_ThermalCoolingInterface;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_ThermalCoolingInterface);
    dmfCallbacksDmf_ThermalCoolingInterface.DeviceOpen = DMF_ThermalCoolingInterface_Open;

    DMF_MODULE_DESCRIPTOR_INIT(dmfModuleDescriptor_ThermalCoolingInterface,
                               ThermalCoolingInterface,
                               DMF_MODULE_OPTIONS_DISPATCH,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_ThermalCoolingInterface.CallbacksDmf = &dmfCallbacksDmf_ThermalCoolingInterface;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_ThermalCoolingInterface,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_ThermalCoolingInterface.c
//
