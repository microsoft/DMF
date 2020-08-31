InterfaceSample2 Sample KMDF/DMF Function Driver
================================================
This sample shows how to create a n-layer Protocol-Transport driver using DMF's Protocol-Transport Interface. InterfaceSample1
shows how to create a driver and Modules that use a single Protocol-Transport Interface. This sample shows how to create a 
similar driver with the key difference that the Transport (chosen by the Client Driver) itself uses a different Protocol
for which the underlying Transport is also selected by the Client Driver.

Please see [IntrerfaceSample1](../InterfaceSample1/readme.md) to learn the basics of how to build a Protocol-Transport Driver. This sample will discuss the
differences between that sample and an n-layer Protocol-Transport architecture. Note that this sample shows a 2-layer implementation
but the concept can be easily extended to create an n-layer architecture.

First, let's discuss the benefit of using a 2-layer Protocol-Transport driver by using a real-world example:

Consider a power meter device that exposes an interface defined by the OS which gives the OS information about how power is
consumed by various components. Power consumption information is gathered by a power-meter chip that gathers power consumption
data. Each vendor that provides a power meter chip does so using a proprietary interface and, crucially, exposes that data to
the consumer of the chip using a proprietary interface as well. It is up to the device driver to receive queries from the
OS regarding power consumption information and convert those queries into commands that the power meter chip can respond to.
Then, when the power-meter chip responds, the driver must convert those responses into the format required by the OS.

It is easy to see the value of using a single Protocol-Transport architecture. The driver will instantiate a Protocol that
receives queries from the OS by exposing the interface required by the OS power-meter stack. The code in that Protocol will
will be common for all power-meter chips because it interacts with the OS on its upper-edge. There is no need to rewrite that
code for every chip vendor. At its lower edge, the Client Driver will select the Transport based on the power-meter chip that
is installed on the platform. The lower edge of the Protocol transfers data using a common interface that exposed by all
compatible Transports. One Transport is created for each chip vendor. When the driver starts, it detects the power-meter 
chip that is installed, instantiates the Protocol, instantiates the Transport and binds the two at run time. So far, what
is discussed can be easily accomplished using the techniques shown in InterfaceSample1.

Some power-meter chips can be installed on the platform such that they can be connected using different a bus. For example,
a single power-meter chip may be installed such that its underlying the commands to the chip are sent using I2C (SPB) or
HID. 

Using a 1-layer Protocol-Transport, it would be necessary to write two Transports for such a chip: An I2C version and a HID 
version. The Client Driver would select which of the two based on information it knows about how the chip is connected.
Crucially, the connection between the chip and its bus is hardwired at compile time.

Using a 2-layer Protocol-Transport, it is possible to write a single version of the Transport such that it uses a **second
Protocol** at its lower edge to communicate with in bus-independent manner using a **second Transport** that is chosen at
run-time. In this case, there is only a single version of the code that chip exposes instead of n-versions depending
on n-buses.

Consider and example where two vendors create power-meter chips, vendor V1 and V2. Furthermore, it is possible to connect
either of those chips to the platform using either SPB or HID. Thus, the following combinations are possible:<br>
* V1-SPB<br>
* V1-HID<br>
* V2-SPB<br>
* V2-HID<br>

Using a two-layer DMF Protocol-Transport driver it is possible and relatively easy to create a single driver that selects
the proper combination at run-time. First, the driver determines what chip/bus combination is installed. Then, the 
driver instantiates a Protocol Module that exposes the OS power-meter interface at the top edge. Then, the driver 
instantiates a Transport Module that supports the lower-edge of the power-meter protocol. The driver does *not* instantiate
the second-layer Protocol as that is done by the first-layer Transport. But the driver does instantiate the second-layer
Transport Module. Next, the driver binds the Protocol and two Transports it instantiated at run-time. Crucially, the
first-layer Transport itself instantiates the second-layer Protocol and relies on the Client driver to bind its 
Transport.

The above is a detailed example which shows the motivation behind using a 2-layer (n-layer) architecture. Using the 
principles established in InterfaceSample1 it is possible to architect an n-layer driver using the principles shown
in this second example.

This example uses two new Protocols and Transports defined by these files:

**Upper Protocol-Transport**

* [Dmf_Interface_SampleInterfaceUpper.c](../../Dmf/Modules.Template/Dmf_Interface_SampleInterfaceUpper.c) and [Dmf_Interface_SampleInterfaceUpper.h](../../Dmf/Modules.Template/Dmf_Interface_SampleInterfaceUpper.h)
* [Dmf_SampleInterfaceUpperProtocol.c](../../Dmf/Modules.Template/Dmf_SampleInterfaceUpperProtocol.c) and [Dmf_SampleInterfaceUpperProtocol.h](../../Dmf/Modules.Template/Dmf_SampleInterfaceUpperProtocol.h)
* [Dmf_SampleInterfaceUpperTransport1.c](../../Dmf/Modules.Template/Dmf_SampleInterfaceUpperTransport1.c) and [Dmf_SampleInterfaceUpperTransport1.h](../../Dmf/Modules.Template/Dmf_SampleInterfaceUpperTransport1.h)
* [Dmf_SampleInterfaceUpperTransport2.c](../../Dmf/Modules.Template/Dmf_SampleInterfaceUpperTransport2.c) and [Dmf_SampleInterfaceUpperTransport2.h](../../Dmf/Modules.Template/Dmf_SampleInterfaceUpperTransport2.h)

**Lower Protocol-Transport**

* [Dmf_Interface_SampleInterfaceLower.c](../../Dmf/Modules.Template/Dmf_Interface_SampleInterfaceLower.c) and [Dmf_Interface_SampleInterfaceLower.h](../../Dmf/Modules.Template/Dmf_Interface_SampleInterfaceLower.h)
* [Dmf_SampleInterfaceLowerProtocol.c](../../Dmf/Modules.Template/Dmf_SampleInterfaceLowerProtocol.c) and [Dmf_SampleInterfaceLowerProtocol.h](../../Dmf/Modules.Template/Dmf_SampleInterfaceLowerProtocol.h)
* [Dmf_SampleInterfaceLowerTransport1.c](../../Dmf/Modules.Template/Dmf_SampleInterfaceLowerTransport1.c) and [Dmf_SampleInterfaceLowerTransport1.h](../../Dmf/Modules.Template/Dmf_SampleInterfaceLowerTransport1.h)
* [Dmf_SampleInterfaceLowerTransport2.c](../../Dmf/Modules.Template/Dmf_SampleInterfaceLowerTransport2.c) and [Dmf_SampleInterfaceLowerTransport2.h](../../Dmf/Modules.Template/Dmf_SampleInterfaceLowerTransport2.h)

In the description above:
* "UpperProtocol" corresponds to the "Power-meter Interface To OS".
* "UpperTransport" corresponds to the "Power-meter chip vendor". In this sample, there are two 
versions of such a Transport corresponding two V1 and V2 above.
* "LowerProtocol" corresponds to the bus-independent Protocol instantiated by the "UpperTransport".
* "LowerTransport" corresponds to either SPB or HID. (LowerTransport1 corresponds to HID. LowerTransport2 corresponds to SPB.)

Note the use of the "Interface Map" for each Protocol-Transport. Each layer in the n-layer Protocol-Transport architecture
is no different than single-layer Protocol-Transport interface and must adhere to the same rules explained in the 
InterfaceSample1. That is, each Protocol-Transport Interface does not know or care it is used in an n-layer architecture.

Again, to understand the purpose of each of the above files, please see InterfaceSample1's explanation. This sample will
show the Client Driver instantiates those Modules and binds them.

Code Tour
=========
Using DMF and the default libraries distributed with DMF the above driver is easy to write. The code is small. All of the code is in the file, [DmfInterface.c](DmfInterface.c).
The code for the *entire* driver (minus .rc and .inx file) is listed here:

```
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
```

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
EVT_WDF_DRIVER_DEVICE_ADD InterfaceClientServerEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP InterfaceClientServerEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;
EVT_WDF_DEVICE_D0_ENTRY InterfaceClientServerEvtDeviceD0Entry;
EVT_WDF_DEVICE_PREPARE_HARDWARE InterfaceClientServerEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE InterfaceClientServerEvtDeviceReleaseHardware;
```

Next, this driver uses the DMF's version of `DriverEntry()` but because this driver has a Device Context, it
uses its own DeviceAdd callback (`InterfaceClientServerEvtDeviceAdd`)instead of the default callback provided by DMF.
```
/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        InterfaceClientServerEvtDriverContextCleanup,
                        InterfaceClientServerEvtDeviceAdd)
#pragma code_seg()
```
Here is the driver's Device Context. Note that it instantiates **3 Modules**:
1. Protocol Modules for driver's upper edge.
2. Transport Module used by Protocol instantiated in step 1.
3. Transport Module used by Protocol of the Transport instantiated in step 2.
````
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
````
The driver uses DMF's Driver Context Clean up callback:
````
#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(InterfaceClientServerEvtDriverContextCleanup)
````
This is the driver's DeviceAdd function. It is similar to other DMF drivers that have a Device Context. It creates a
WDFDEVICE with a Device Context and then creates DMF Modules. (There is no difference in this callback between this 
example and InterfaceSample1.)
````
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
````
Next, is a helper function that simply reads the registry to determine which of the four possible combinations it
wants to use:
1. UpperProtocol-UpperTransport1
2. UpperProtocol-UpperTransport2
3. LowerProtocol-LowerTransport1
4. LowerProtocol-LowerTransport2
````
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
````
Next, is the crucial callback defined by the Client Driver. It allows the Upper Transport to query the driver about what
lower Transport it should bind to its Lower Protocol. The Client Driver will have already instantiated the Module and
just needs to set the Module handle. **The Upper Transport will perform the actual binding operation.** Note that is
path is chosen and created by the author of the Upper Transport. Other possible binding architectures are possible, but
best practice is that the binding operation happen at the Upper Transport to prevent the Client Driver from accessing
the Upper Protocol Module.

This callback shows the crucial difference between this example and the previous example.
````
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
````
Next, the ModulesAdd callback is shown. In this example, the Client driver instantiates 3 Modules:

1. Protocol Modules for driver's upper edge.
2. Transport Module used by Protocol instantiated in step 1.
3. Transport Module used by Protocol of the Transport instantiated in step 2.

The choice of which Transport Modules to instantiate is based on the settings read from the registry
previously.
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
````
Next, the EvtDevicePrepareHardware callback is shown. In this driver, this is where binding of the Upper Protocol and Upper Transport
happens. However, it can happen in other callbacks. 
````
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

    When the driver starts, bind the Upper Protocol to the Upper Transport.

Arguments:

    Device - Handle to a framework device object.
    ResourcesRaw - Notused.
    ResourcesTranslated - Not used.

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
````
Next, is the driver EvtDeviceD0Entry callback. In this callback the driver simply calls a Protocol Method. This Method will
execute a corresponding Method in using its attached Transport Module.
````
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
````
Next, the EvtDeviceReleaseHardware callback is shown. In this driver, this is where unbinding of the Upper Protocol and Upper Transport
happens. However, it can happen in other callbacks.
````
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
````

Testing the driver
==================

1. Use Traceview to load the driver's .pdb file and turn set logging settings to log "information".
2. Install this sample using this command with Devcon.exe from DDK: `devcon install InterfaceSample2.inf  root\Transport11`.
This will install the driver such that it uses Upper Transport 1 and Lower Transport 1.
3. Using Traceview you can see logging showing that the Method of **Upper Transport1**  and the Method of **Lower Transport1** are called when the driver starts.
4. Uninstall the driver.
5. Install this sample using this command with Devcon.exe from DDK: `devcon install InterfaceSample2.inf  root\Transport12`.
This will install the driver such that it uses Upper Transport 1 and Lower Transport 2.
3. Using Traceview you can see logging showing that the Method of **Upper Transport1**  and the Method of **Lower Transport2** are called when the driver starts.
7. Uninstall the driver.
5. Install this sample using this command with Devcon.exe from DDK: `devcon install InterfaceSample2.inf  root\Transport21`.
This will install the driver such that it uses Upper Transport 2 and Lower Transport 1.
3. Using Traceview you can see logging showing that the Method of **Upper Transport2**  and the Method of **Lower Transport1** are called when the driver starts.
7. Uninstall the driver.
5. Install this sample using this command with Devcon.exe from DDK: `devcon install InterfaceSample2.inf  root\Transport22`.
This will install the driver such that it uses Upper Transport 2 and Lower Transport 2.
3. Using Traceview you can see logging showing that the Method of **Upper Transport2**  and the Method of **Lower Transport2** are called when the driver starts.
7. Uninstall the driver.
