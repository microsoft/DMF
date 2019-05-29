/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    DmfInterface.c

Abstract:

    LegacyProtocol Sample: Demonstrates how to use the Legacy DMF (Protocol/Transport) Interface to dynamically 
                           attach a Child Module to a Parent Module at runtime instead of compile time
                           as is normally done. This feature allows a driver to implement a generic
                           Protocol. Then, the driver chooses a Transport specific implementation at runtime.
                           For example, a Protocol could implement a Latch that opens and closes. Then, the
                           Transport might allow that Protocol to execute over HID, USB, or PCI.
                           In this example, the registry is used to indicate which Transport should load, 1 or 2.
                           Then, that Transport Module is created and attached to the Protocol Module. In 
                           EvtDeviceD0Entry() this driver displays "Hello, world!" via the Protocol which then
                           calls the attached Transport to do the actual work.

Environment:

    Kernel mode only

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
EVT_WDF_DRIVER_DEVICE_ADD LegacyProtocolTransportEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP LegacyProtocolTransportEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;
EVT_WDF_DEVICE_D0_ENTRY LegacyProtocolTransportEvtDeviceD0Entry;

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        LegacyProtocolTransportEvtDriverContextCleanup,
                        LegacyProtocolTransportEvtDeviceAdd)
#pragma code_seg()

typedef struct
{
    // Identifies which transport to load.
    //
    ULONG TransportId;
    // The Interface's Protocol Module.
    //
    DMFMODULE DmfModuleProtocol;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(LegacyProtocolTransportEvtDriverContextCleanup)

_Use_decl_annotations_
NTSTATUS
LegacyProtocolTransportEvtDeviceAdd(
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
    pnpPowerCallbacks.EvtDeviceD0Entry = LegacyProtocolTransportEvtDeviceD0Entry;

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
ULONG
TransportIdGet(
    _In_ WDFDEVICE WdfDevice
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
    ULONG transportId;
    ULONG valueData;
    DECLARE_CONST_UNICODE_STRING(valueName, L"TransportSelect");

    PAGED_CODE();

    transportId = 1;

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
                                     &valueName,
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
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Invalid Server Id=%u", valueData);
        goto Exit;
    }

    transportId = (ULONG)valueData;
    
Exit:

    return transportId;
}
#pragma code_seg()

#pragma code_seg("PAGED")
VOID
LegacyProtocolTransportModuleAdd(
    _In_ DMFMODULE DmfModuleProtocol,
    _In_ struct _DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PVOID DmfModuleInit
    )
/*++

Routine Description:

    Given a Protocol Module, this callback adds a Child Module that is used by the given
    Protocol Module as a Transport Module. The interface between the Protocol and 
    Transport Module must match.

Arguments:

    DmfModuleProtocol - The given Protocol Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure passed to Dmf_ModuleAdd().

Return Value:

    NTSTATUS indicates if the Transport Module is created.

--*/
{
    DEVICE_CONTEXT* deviceContext;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    WDFDEVICE device;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    device = DMF_ParentDeviceGet(DmfModuleProtocol);
    deviceContext = DeviceContextGet(device);

    if (deviceContext->TransportId == 1)
    {
        // LegacyTransportA
        // ----------------
        //
        DMF_LegacyTransportA_ATTRIBUTES_INIT(&moduleAttributes);
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         NULL);
    }
    else if (deviceContext->TransportId == 2)
    {
        // LegacyTransportB
        // ----------------
        //
        DMF_LegacyTransportB_ATTRIBUTES_INIT(&moduleAttributes);
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         NULL);
    }
    else
    {
        ASSERT(FALSE);
    }
}
#pragma code_seg()

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
    DEVICE_CONTEXT* deviceContext;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    deviceContext->TransportId = TransportIdGet(Device);

    // LegacyProtocol
    // --------------
    //
    DMF_LegacyProtocol_ATTRIBUTES_INIT(&moduleAttributes);
    moduleAttributes.TransportModuleAdd = LegacyProtocolTransportModuleAdd;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &(deviceContext->DmfModuleProtocol));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");
}
#pragma code_seg()

NTSTATUS
LegacyProtocolTransportEvtDeviceD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    When the driver powers up call a Protocol Method that calls the corresponding
    Transport Module's Method. In this case, the Method simply displays a string
    along with an indication of which Transport Module is running.
 
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
    ntStatus = STATUS_SUCCESS;

    // Call the Protocol's Method. The underlying Transport will do the work.
    //
    DMF_LegacyProtocol_StringDisplay(deviceContext->DmfModuleProtocol,
                                     L"Hello, world!");

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}

// eof: DmfInterface.c
//
