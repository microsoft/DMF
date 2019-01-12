LegacyProtocolTransport Sample KMDF/DMF Function Driver
================================================
This sample shows how to choose and attach a Child Module to a Parent Module at runtime. In this case, the Child Module is considered to be the
Transport and the Parent Module is considered to be the Protocol Module.

*NOTE: This sample uses an existing feature in DMF that will be replaced by a more robust feature in the 
near future.*

Most Modules that use Child Modules do so by creating the connection at compile time. In some cases, however, it is useful for a Client driver to
choose the Child Module a Parent Module will use at runtime. For example, consider a Module that exposes an virtual HID Ambient Light Sensor (ALS)
on its upper edge and contains Methods for reading HID Feature Reports that contain ALS parameters such as light sensitivity and sending
Lux information in HID Input Reports. That Module needs to communicate with a physical ALS sensor on its lower edge. Such a sensor may be
attached, for example, using USB, I2C PCI or may just be a virtual sensor. Using DMF Interfaces (as shown in this example), it is possible
for a driver determine how the physical ALS sensor is attached using a registry key or by some other means. Then, based on that information
the Client driver can instantiate an ALS Module and a Module that reads ALS sensor data corresponding to the type of bus the sensor is attached to. 
(If the sensor is connected via I2C, it can communicate using one Module. If the sensor is connected via USB, it can communicate with the sensor
using a different Module.) Then, the Client driver binds the two Modules such that the upper ALS layer communicates with ALS sensor without
knowing how the sensor is connected. In this example, the upper (Parent) Module is considered to be the Protocol Module and the lower (Child)
Module is considered to be the Transport Module. The Protocol Module implements an "ALS protocol". The Transport Module "transports ALS sensor
data" via the bus the sensor is attached to.

The idea of connecting Modules this way is similar to how a function driver can act as a protocol and a lower filter driver can act as a
transport.

Code Tour
=========
Using DMF and the default libraries distributed with DMF the above driver is easy to write. The code is small. All of the code is in the file, DmfInterface.c.
The code for the *entire* driver (minus .rc and .inx file) is listed here:

````
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
````

Annotated Code Tour
===================
In this section, the above code is annotated section by section:

First, DMF is included. Note that most drivers will `#include "DmfModules.Library.h"`. This file includes all of DMF as well as all the Modules distributed with DMF.
When a team creates a new Library, this library will be a subset of the new Library. That team will always include that new Library's include file. In this case,
however, this driver is able to use the Library that is publicly distributed with DMF. Always `#include <initguid.h>` one time before including the DMF Library in one 
file.

```
// The DMF Library and the DMF Template Library Modules this driver uses.
// NOTE: The Template Library automatically includes the DMF Library.
//
#include <initguid.h>
#include "..\Modules.Template\DmfModules.Template.h"
```

These lines are for trace logging:

```
#include "Trace.h"
#include "DmfInterface.tmh"
```

These are the forward declarations:

```
// Forward declarations.
//
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD LegacyProtocolTransportEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP LegacyProtocolTransportEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;
EVT_WDF_DEVICE_D0_ENTRY LegacyProtocolTransportEvtDeviceD0Entry;
```

Next, this driver uses the DMF's version of `DriverEntry()` but because this driver has a Device Context, it
uses its own DeviceAdd callback (`LegacyProtocolTransportEvtDeviceAdd`)instead of the default callback provided by DMF.
```
/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        LegacyProtocolTransportEvtDriverContextCleanup,
                        LegacyProtocolTransportEvtDeviceAdd)
#pragma code_seg()
```
Here is the driver's Device Context. The Transport Id indicates which underlying Transport the Protocol driver
will load.
````
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
````
The driver uses DMF's Driver Context Clean up callback:
````
#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(LegacyProtocolTransportEvtDriverContextCleanup)
````
This is the driver's DeviceAdd function. It is similar to other DMF drivers that have a Device Context. It creates a
WDFDEVICE with a Device Context and then creates DMF Modules.
````
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
````
Next, is a helper function that simply reads the registry to determine which of two Transports (#1 or #2) should be used. 
````
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
````
Next, is the ModulesAdd callback. This callback instantiates a Protocol Module. It also reads the TransportId from the 
registry and saves that value in the Device Context. Later, when DMF callback to the Client driver the Client Driver
creates the Transport Module. That happens in `LegacyProtocolTransportModuleAdd`.
````
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

    // ProtocolV1
    // ----------
    //
    DMF_LegacyProtocol_ATTRIBUTES_INIT(&moduleAttributes);
    moduleAttributes.TransportsCreator = LegacyProtocolTransportModuleAdd;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &(deviceContext->DmfModuleProtocol));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");
}
#pragma code_seg()
````
Next, is the callback that DMF calls after it has created the Protocol Module. This callback creates the 
Transport Module that is indicated by the TransportId. When this callback returns, the Transport Module is a Child
of the Protocol Module and the Protocol Module is able to call the underlying Transport Method as needed.
````
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
````
Next, is the driver EvtDeviceD0Entry callback. In this callback the driver simply calls a Protocol Method. This Method will
execute a corresponding Method in using its attached Transport Module.
````
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
````

Testing the driver
==================

1. Use Traceview to load the driver's .pdb file and turn set logging settings to log "information".
2. Install this sample using this command with Devcon.exe from DDK: `devcon install LegacyProtocolTransport.inf root\Transport1`. This will install the driver such that it uses Transport 1.
3. Using Traceview you can see logging showing that Transport1's Method is called when the driver starts.
4. Uninstall the driver.
4. Install this sample using this command with Devcon.exe from DDK: `devcon install LegacyProtocolTransport.inf root\Transport2`. This will install the driver such that it uses Transport 2.
5. Using Traceview you can see logging showing that Transport2's Method is called when the driver starts.

