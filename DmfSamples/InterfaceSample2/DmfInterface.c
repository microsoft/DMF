/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    DmfInterface.c

Abstract:

    InterfaceSample2: Demonstrates how a Client Driver can using a 2-layer Protocol/Transport 
                      architecture.

Environment:

    Kernel mode
    User-mode

--*/

// The DMF Library and the DMF Library Modules this driver uses.
// NOTE: The Template Library automatically includes the DMF Library.
//
#include <initguid.h>
#include "..\Modules.Template\DmfModules.Template.h"

#include "Trace.h"
#include "DmfInterface.tmh"

// Forward declarations.
//
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD InterfaceClientServerEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP InterfaceClientServerEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;
EVT_WDF_DEVICE_D0_ENTRY InterfaceClientServerEvtDeviceD0Entry;
EVT_WDF_DEVICE_PREPARE_HARDWARE InterfaceClientServerEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE InterfaceClientServerEvtDeviceReleaseHardware;

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        InterfaceClientServerEvtDriverContextCleanup,
                        InterfaceClientServerEvtDeviceAdd)
#pragma code_seg()

typedef struct
{
    // The Interface's Protocol Module.
    //
    DMFMODULE DmfModuleProtocol;
    // The Interface's Transport Module (Upper).
    //
    DMFMODULE DmfModuleTransportUpper;
    // The Interface's Transport Module (Lower).
    //
    DMFMODULE DmfModuleTransportLower;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(InterfaceClientServerEvtDriverContextCleanup)

_Use_decl_annotations_
NTSTATUS
InterfaceClientServerEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    PDMFDEVICE_INIT dmfDeviceInit;
    DMF_EVENT_CALLBACKS dmfCallbacks;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

    // Tell WDF this callback should be called.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceD0Entry = InterfaceClientServerEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDevicePrepareHardware = InterfaceClientServerEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = InterfaceClientServerEvtDeviceReleaseHardware;

    // All DMF drivers must call this function even if they do not support PnP Power callbacks.
    // (In this case, this driver does support a PnP Power callback.)
    //
    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,
                                                &pnpPowerCallbacks);
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit,
                                           &pnpPowerCallbacks);

    // All DMF drivers must call this function even if they do not support File Object callbacks.
    //
    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit,
                                          NULL);

    // All DMF drivers must call this function even if they do not support Power Policy callbacks.
    //
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit,
                                                   NULL);

    // Set any device attributes needed.
    //
    WdfDeviceInitSetDeviceType(DeviceInit,
                               FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetExclusive(DeviceInit,
                              FALSE);

    // Define a device context type.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes, 
                                            DEVICE_CONTEXT);

    // Create the Client driver's WDFDEVICE.
    //
    ntStatus = WdfDeviceCreate(&DeviceInit,
                               &objectAttributes,
                               &device);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Create the DMF Modules this Client driver will use.
    //
    dmfCallbacks.EvtDmfDeviceModulesAdd = DmfDeviceModulesAdd;
    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,
                                       &dmfCallbacks);

    ntStatus = DMF_ModulesCreate(device,
                                 &dmfDeviceInit);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    if (dmfDeviceInit != NULL)
    {
        DMF_DmfDeviceInitFree(&dmfDeviceInit);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC! ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGED")
_Check_return_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
InterfaceTransportIdGet(
    _In_ WDFDEVICE WdfDevice,
    _Out_ ULONG* TransportIdUpper,
    _Out_ ULONG* TransportIdLower
    )
/*++

Routine Description:

    Determines which Transport Module should be bound to the Protocol Module.

Arguments:

    WdfDevice - Client driver Wdf device.

Return Value:

    Identifier of the Transport Module to bind to the Protocol Module.

--*/
{
    NTSTATUS ntStatus;
    WDFKEY wdfSoftwareKey;
    ULONG valueData;
    DECLARE_CONST_UNICODE_STRING(valueNameUpper, L"TransportSelectUpper");
    DECLARE_CONST_UNICODE_STRING(valueNameLower, L"TransportSelectLower");

    PAGED_CODE();

    *TransportIdUpper = 1;
    *TransportIdLower = 1;

    ntStatus = WdfDeviceOpenRegistryKey(WdfDevice,
                                        PLUGPLAY_REGKEY_DRIVER,
                                        KEY_READ,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &wdfSoftwareKey);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceOpenRegistryKey fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = WdfRegistryQueryValue(wdfSoftwareKey,
                                     &valueNameUpper,
                                     sizeof(valueData),
                                     &valueData,
                                     NULL,
                                     NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfRegistryQueryValue fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if ((valueData > 2) ||
        (valueData < 1))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Invalid TransportUpperId=%u", valueData);
        goto Exit;
    }

    *TransportIdUpper = (ULONG)valueData;
    
    ntStatus = WdfRegistryQueryValue(wdfSoftwareKey,
                                     &valueNameLower,
                                     sizeof(valueData),
                                     &valueData,
                                     NULL,
                                     NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfRegistryQueryValue fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if ((valueData > 2) ||
        (valueData < 1))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Invalid TransportLowerId=%u", valueData);
        goto Exit;
    }

    *TransportIdLower = (ULONG)valueData;
    
Exit:

    return ntStatus;
}
#pragma code_seg()

NTSTATUS
UpperTransportBinding(
    _In_ DMFMODULE DmfModuleProtocolLower,
    _Out_ DMFMODULE* DmfModuleTransport
    )
{
    DEVICE_CONTEXT* deviceContext;
    NTSTATUS ntStatus;
    WDFDEVICE device;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    device = DMF_ParentDeviceGet(DmfModuleProtocolLower);
    deviceContext = DeviceContextGet(device);

    *DmfModuleTransport = deviceContext->DmfModuleTransportLower;
    ntStatus = STATUS_SUCCESS;
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMF_INTERFACE_BIND fails: ntStatus=%!STATUS!", ntStatus);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DMF_INTERFACE_BIND succeeds: ntStatus=%!STATUS!", ntStatus);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}

#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DmfDeviceModulesAdd(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Add all the DMF Modules used by this driver.

Arguments:

    Device - WDFDEVICE handle.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    NTSTATUS

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_SampleInterfaceUpperProtocol moduleConfigSampleInterfaceUpperProtocol;
    DMF_CONFIG_SampleInterfaceUpperTransport1 moduleConfigSampleInterfaceUpperTransport1;
    DMF_CONFIG_SampleInterfaceUpperTransport2 moduleConfigSampleInterfaceUpperTransport2;
    DMF_CONFIG_SampleInterfaceLowerTransport1 moduleConfigSampleInterfaceLowerTransport1;
    DMF_CONFIG_SampleInterfaceLowerTransport2 moduleConfigSampleInterfaceLowerTransport2;
    DEVICE_CONTEXT* deviceContext;
    ULONG transportIdUpper;
    ULONG transportIdLower;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    transportIdUpper = 1;
    transportIdLower = 1;

    InterfaceTransportIdGet(Device,
                            &transportIdUpper,
                            &transportIdLower);

    // SampleInterfaceUpperProtocol
    // ----------------------------
    //
    DMF_CONFIG_SampleInterfaceUpperProtocol_AND_ATTRIBUTES_INIT(&moduleConfigSampleInterfaceUpperProtocol,
                                                                &moduleAttributes);
    moduleConfigSampleInterfaceUpperProtocol.ModuleId = 1;
    moduleConfigSampleInterfaceUpperProtocol.ModuleName = "SampleInterfaceUpperProtocol";
 
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &(deviceContext->DmfModuleProtocol));

    if (transportIdUpper == 1)
    {
        // SampleInterfaceUpperTransport1
        // ------------------------------
        //
        DMF_CONFIG_SampleInterfaceUpperTransport1_AND_ATTRIBUTES_INIT(&moduleConfigSampleInterfaceUpperTransport1,
                                                                 &moduleAttributes);
        moduleConfigSampleInterfaceUpperTransport1.ModuleId = 1;
        moduleConfigSampleInterfaceUpperTransport1.ModuleName = "SampleInterfaceUpperTransport1";
        moduleConfigSampleInterfaceUpperTransport1.TransportBindingCallback = UpperTransportBinding;

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransportUpper));
    }
    else if (transportIdUpper == 2)
    {
        // SampleInterfaceUpperTransport2
        // ------------------------------
        //
        DMF_CONFIG_SampleInterfaceUpperTransport2_AND_ATTRIBUTES_INIT(&moduleConfigSampleInterfaceUpperTransport2,
                                                                      &moduleAttributes);
        moduleConfigSampleInterfaceUpperTransport2.ModuleId = 2;
        moduleConfigSampleInterfaceUpperTransport2.ModuleName = "SampleInterfaceUpperTransport2";
        moduleConfigSampleInterfaceUpperTransport2.TransportBindingCallback = UpperTransportBinding;

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransportUpper));
    }
    else
    {
        ASSERT(0);
    }

    if (transportIdLower == 1)
    {
        // SampleInterfaceLowerTransport1
        // ------------------------------
        //
        DMF_CONFIG_SampleInterfaceLowerTransport1_AND_ATTRIBUTES_INIT(&moduleConfigSampleInterfaceLowerTransport1,
                                                                      &moduleAttributes);
        moduleConfigSampleInterfaceLowerTransport1.ModuleId = 1;
        moduleConfigSampleInterfaceLowerTransport1.ModuleName = "SampleInterfaceLowerTransport1";

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransportLower));
    }
    else if (transportIdLower == 2)
    {
        // SampleInterfaceLowerTransport2
        // ------------------------------
        //
        DMF_CONFIG_SampleInterfaceLowerTransport2_AND_ATTRIBUTES_INIT(&moduleConfigSampleInterfaceLowerTransport2,
                                                                      &moduleAttributes);
        moduleConfigSampleInterfaceLowerTransport2.ModuleId = 2;
        moduleConfigSampleInterfaceLowerTransport2.ModuleName = "SampleInterfaceLowerTransport2";

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransportLower));
    }
    else
    {
        ASSERT(0);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");
}
#pragma code_seg()

#pragma code_seg("PAGED")
_Use_decl_annotations_
NTSTATUS
InterfaceClientServerEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    When the driver starts bind the Client and Server Modules.
 
Arguments:

    Device - Handle to a framework device object.
    PreviousState - Device power state which the device was in most recently.
                    If the device is being newly started, this will be
                    PowerDeviceUnspecified.

Return Value:

    NTSTATUS

--*/
{
    DEVICE_CONTEXT* deviceContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Bind the Modules using SampleInterface Interface.
    // The decision about which Transport to bind has already been made and the
    // Transport Module has already been created.
    //
    ntStatus = DMF_INTERFACE_BIND(deviceContext->DmfModuleProtocol,
                                  deviceContext->DmfModuleTransportUpper,
                                  SampleInterfaceUpper);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMF_INTERFACE_BIND fails: ntStatus=%!STATUS!", ntStatus);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DMF_INTERFACE_BIND succeeds: ntStatus=%!STATUS!", ntStatus);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}
#pragma code_seg()

NTSTATUS
InterfaceClientServerEvtDeviceD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    When the driver powers up call a Client (Protocol) Method that calls 
    the corresponding bound (Server) Transport Method.
 
Arguments:

    Device - Handle to a framework device object.
    PreviousState - Device power state which the device was in most recently.
                    If the device is being newly started, this will be
                    PowerDeviceUnspecified.

Return Value:

    NTSTATUS

--*/
{
    DEVICE_CONTEXT* deviceContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(PreviousState);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Call a Test Method exposed by the Client Module.
    //
    ntStatus = DMF_SampleInterfaceUpperProtocol_TestMethod(deviceContext->DmfModuleProtocol);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMF_SampleInterfaceUpperProtocol_TestMethod fails: ntStatus=%!STATUS!", ntStatus);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}

#pragma code_seg("PAGED")
_Use_decl_annotations_
NTSTATUS
InterfaceClientServerEvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    When the driver stops unbind the Client and Server Modules.
 
Arguments:

    Device - Handle to a framework device object.
    PreviousState - Device power state which the device was in most recently.
                    If the device is being newly started, this will be
                    PowerDeviceUnspecified.

Return Value:

    NTSTATUS

--*/
{
    DEVICE_CONTEXT* deviceContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    ntStatus = STATUS_SUCCESS;

    // Unbind the Modules using SampleInterface Interface.
    //
    DMF_INTERFACE_UNBIND(deviceContext->DmfModuleProtocol,
                         deviceContext->DmfModuleTransportUpper,
                         SampleInterfaceUpper);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}
#pragma code_seg()

// eof: DmfInterface.c
//
