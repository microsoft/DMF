InterfaceSample1 Sample KMDF/DMF Function Driver
================================================
This sample shows how to choose and attach a Child Module to a Parent Module at runtime. In this case, the Child Module is considered to be the
Transport and the Parent Module is considered to be the Protocol Module.

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

The "Code Tour" below discusses hows how the Client Driver connects the various components at run-time. However, it is important to
understand each of the components and the specific role each one plays. The individual components are:

##### Dmf_Interface_SampleInterface.h
This is the first file that one writes when designing a DMF Protocol-Transport Interface. It is the map that determines the organization
and contents of all the other components. Specifically, this file defines following:
1. Context related to the binding of the Protocol used by the Protocol: `DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA`.
2. Context related to the binding of the Transport used by the Transport: `DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA`.
3. Definitions of optional Callbacks (zero or more) into the Protocol that allow the Transport to callback into the Protocol: `EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1`.
4. The definitions of a structure that connects DMF specific information to the Client defined Protocol: `DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA`.
5. The declaration of the function that initializes the Protocol interface which populates the above structure with the mandatory Bind (DMF defined) callbacks as well as any optional Protocol callbacks: `DMF_INTERFACE_PROTOCOL_SampleInterface_DESCRIPTOR_INIT`.
6. The definition of a callback that binds instances of the a Protocol and Transport data defined above: `DMF_INTERFACE_SampleInterface_TransportBind`. This information is stored in a construct defined by DMF (`DMFINTERFACE`).
7. The declaration of a callback that unbinds a previously bound Protocol-Transport pair: `DMF_INTERFACE_SampleInterface_TransportUnbind`.
8. Declarations of optional Methods (zero or more) defined by the Transport Module: `DMF_INTERFACE_SampleInterface_TransportMethod1`.
9. Similar to #4 above, definition of a structure that connects DMF specific information to the Client defined Transport: `DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA`.
10. Similar to #5 above, the declaration of the function that initializes the Transport interface which populates the above structure with the mandatory Bind (DMF defined) callbacks as well as any optional Methods: `DMF_INTERFACE_TRANSPORT_SampleInterface_DESCRIPTOR_INIT`.
11. Prototypes of Methods exposed to the Protocol that Clients that instantiate the Protocol can call.
12. Prototypes of Callbacks exposed to the transport that Transport can call.
13. The Macro that internally defines several constructs used by both DMF and the Client.
14. This file should be included in the Module Library Include file in the "Interfaces" section.

Once the above items are properly defined, it serves as a map for how to write the code in the rest of the related files. The Client
also uses this file to know how to instantiate and use the Protocol-Transport interface.

When creating a new interface, it is best practice to copy this file as a new file giving it a new name for the new interface.
For example, if the new interface is called "MyInterface", one would copy Dmf_Interface_SampleInterface.h as Dmf_Interface_MyInterface.h
and then rename all the "SampleInterface" in the file as "MyInterface". Then populate all the above 13 items above as needed
by "MyInterface".

##### Dmf_Interface_SampleInterface.c
This file contains the definitions of the functions declared in its companion .h file. The purpose of this file is to perform the transition
between calls to the abstract interface to the instantiated Protocol and Transport code:

1. First, make sure to include the same DMF Includes used by Modules.
2. Define the function that initializes the Protocol interface which populates the above structure with the mandatory Bind (DMF defined) callbacks as well as any optional Protocol callbacks: `DMF_INTERFACE_PROTOCOL_SampleInterface_DESCRIPTOR_INIT`. This function simply calls `DMF_INTERFACE_PROTOCOL_DESCRIPTOR_INIT` to  initialize the descriptor with mandatory fields. Then, sets the optional callbacks into the Protocol.
3. Define the function that initializes the Transport interface which populates the above structure with the mandatory Bind (DMF defined) callbacks as well as any optional Methods: `DMF_INTERFACE_TRANSPORT_SampleInterface_DESCRIPTOR_INIT`. This function simply calls `DMF_INTERFACE_TRANSPORT_DESCRIPTOR_INIT` to  initialize the descriptor with mandatory fields. Then, sets the optional Bind/Unbind functions as well as any optional Methods that are declared in the companion .h file. When the Client calls those Methods, the call comes to this file. This file will then route that call to the proper instantiated Transport.
4. Define the function that performs the bind between the DMFINTERFACE, Protocol data and Transport data: `DMF_SampleInterface_TransportBind`. This function will route the Bind call to the transport appropriate Bind call.
5. Define the function that performs the unbind of the DMFINTERFACE and its associated Protocol/Transport: `DMF_SampleInterface_TransportUnbind`. This function will route the Unbind call to the transport appropriate Unbind call.
6. Define the Callback that is exposed by the Protocol to the Transport: `EVT_SampleInterface_ProtocolCallback1`.

Once this file is written, the abstract Protocol-Transport layer is finished. Clients will call the Methods exposed by these two files and, in turn, the
instantiated versions of the Protocol-Transport are called on behalf of the Client.

Once the companion .h file is written, the code in this file is not difficult to write as it is simply "glue". Again, the best practice it copy the
template version an use it as a basis for new interfaces.

##### Dmf_SampleInterfaceProtocol1.h
Now that the abstract versions of these files are written, the implemented versions can be written. Note that the
organization of this file is the same as any other Module .h file however it is also based on the "map" written above.

1. The Module's Config is defined: `DMF_CONFIG_SampleInterfaceProtocol1`.
2. The Module is declared using the Module Macro `DECLARE_DMF_MODULE(SampleInterfaceProtocol1)`.
3. The Module's Method's, if any, are declared: `DMF_SampleInterfaceProtocol1_TestMethod`.

##### Dmf_SampleInterfaceProtocol1.c
This file contains the definitions of the declarations in the companion .h file above. The basic structure of this file
is the same as that of other Modules. However, to satisfy the requirements of the DMF Protocol Interface a
few other definitions are included.

1. Define the Module's Context as well as the Macros that enable access to the Module's Config and Context.
2. Define the Module's Create function as well as all the necessary DMF/WDF callbacks.
3. Define the Module's Methods.
4. In addition, an optional Protocol Interface can be defined: `DMF_INTERFACE_PROTOCOL1_CONTEXT` as well as the macro
that allows access to it.
5. The Protocol's Bind/Unbind/PostBind/PreUnbind callbacks are defined:<br>
   `DMF_SampleInterfaceProtocol1_Bind`<br>
   `DMF_SampleInterfaceProtocol1_Unbind`<br>
   `DMF_SampleInterfaceProtocol1_PostBind`<br>
   `DMF_SampleInterfaceProtocol1_PreUnbind`<br>
6. The Protocol's callbacks (called by the Transport) are defined: `DMF_SampleInterfaceProtocol1_Callback1`.
5. Next, the Module's Create function must contain a call to the Protocol's Descriptor initialization function:
    `DMF_INTERFACE_PROTOCOL_SampleInterface_DESCRIPTOR_INIT`. 
6. The optional Protocol Context defined in step 4 is assigned using `DMF_INTERFACE_DESCRIPTOR_SET_CONTEXT_TYPE`.
7. Finally, `DMF_ModuleInterfaceDescriptorAdd` is called using the descriptor populated in step 5.

##### Dmf_SampleInterfaceTransport1.h
Now that the abstract versions of these files are written, the implemented versions can be written. Note that the
organization of this file is the same as any other Module .h file however it is also based on the "map" written above.

1. The Module's Config is defined: `DMF_CONFIG_SampleInterfaceTransport1`.
2. The Module is declared using the Module Macro `DECLARE_DMF_MODULE(SampleInterfaceTransport1)`.
3. Unlike non-Protocol/Transport Modules, this Module does not need to declare the Methods used by the Protocol-Transport interface
because those Methods are internally used by the Module.
4. The Module may optionally define Methods that the Client that instantiates the Module can use. (This sample does not do so.)

Of course, there can be multiple implementations of the Transport defined by the Interface. Each implementation has its own
Module .h file.

##### Dmf_SampleInterfaceTransport1.c
This file contains the definitions of the declarations in the companion .h file above. The basic structure of this file
is the same as that of other Modules. However, to satisfy the requirements of the DMF Transport Interface a
few other definitions are included.

1. Define the Module's Context as well as the Macros that enable access to the Module's Config and Context.
2. Define the Module's Create function as well as all the necessary DMF/WDF callbacks.
3. Define the Module's Methods.
4. In addition, an optional Protocol Interface can be defined: `DMF_INTERFACE_TRANSPORT1_CONTEXT` as well as the macro
that allows access to it.
5. The Protocol's Bind/Unbind/PostBind/PreUnbind callbacks are defined:<br>
   `DMF_SampleInterfaceTransport1_Bind`<br>
   `DMF_SampleInterfaceTransport1_Unbind`<br>
   `DMF_SampleInterfaceTransport1_PostBind`<br>
   `DMF_SampleInterfaceTransport1_PreUnbind`<br>
6. The Transport's Methods (called by the Protocol) are defined: `DMF_SampleInterfaceTransport1_Method1`.
7. Next, the Module's Create function must contain a call to the Protocol's Descriptor initialization function:
    `DMF_INTERFACE_TRANSPORT_SampleInterface_DESCRIPTOR_INIT`. 
8. The optional Transport Context defined in step 4 is assigned using `DMF_INTERFACE_DESCRIPTOR_SET_CONTEXT_TYPE1`.
9. Finally, `DMF_ModuleInterfaceDescriptorAdd` is called using the descriptor populated in step 5.

Of course, there can be multiple implementations of the Transport defined by the Interface. Each implementation has its own
Module .c file.

##### Summary

While there are many steps to creating a proper DMF Protocol-Transport interface, many of the steps are mechanical and relatively
easy. Once the Protocol-Transport declaration and definition exists, it is easy to create new Transports. The benefit is that
the new Transports can be easily "plugged-in" at run-time and the programmer has confidence that the Protocol's code remains
unchanged.

Code Tour
=========
Using DMF and the default libraries distributed with DMF the above driver is easy to write. The code is small. All of the code is in the file, DmfInterface.c.
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

    InterfaceClientTransport1 Sample: Demonstrates how to use the DMF (Protocol/Transport) Interface to dynamically 
                                      bind (attach) a Child Module to a Parent Module at runtime instead of compile time
                                      as is normally done. This feature allows a driver to implement a generic
                                      Protocol. Then, the driver chooses an Transport specific implementation at runtime.
                                      For example, a Protocol could implement a Latch that opens and closes. Then, the
                                      Transport might allow that Protocol to execute over HID, USB, or PCI.

Environment:

    Kernel mode only

--*/

// The DMF Library and the DMF Template Library Modules this driver uses.
// NOTE: The Template Library automatically includes the DMF Library.
//
#include <initguid.h>
#include "..\Modules.Template\DmfModules.Template.h"

#include "Trace.h"
#include "DmfInterface.tmh"

// DMF: These lines provide default DriverEntry/AddDevice/DriverCleanup functions.
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
    // The Client Interface Module.
    //
    DMFMODULE DmfModuleProtocol;
    // The Server Interface Module.
    //
    DMFMODULE DmfModuleTransport;
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
ULONG
InterfaceTransportIdGet(
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
    DMF_CONFIG_SampleInterfaceProtocol1 moduleConfigInterfaceProtocol1;
    DMF_CONFIG_SampleInterfaceTransport1 moduleConfigInterfaceTransport1;
    DMF_CONFIG_SampleInterfaceTransport2 moduleConfigInterfaceTransport2;
    DEVICE_CONTEXT* deviceContext;
    ULONG transportId;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    transportId = InterfaceTransportIdGet(Device);

    // SampleInterfaceProtocol1
    // ------------------------
    //
    DMF_CONFIG_SampleInterfaceProtocol1_AND_ATTRIBUTES_INIT(&moduleConfigInterfaceProtocol1,
                                                            &moduleAttributes);
    moduleConfigInterfaceProtocol1.ModuleId = 1;
    moduleConfigInterfaceProtocol1.ModuleName = "SampleInterfaceProtocol1";
 
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &(deviceContext->DmfModuleProtocol));

    if (transportId == 1)
    {
        // SampleInterfaceTransport1
        // -------------------------
        //
        DMF_CONFIG_SampleInterfaceTransport1_AND_ATTRIBUTES_INIT(&moduleConfigInterfaceTransport1,
                                                                 &moduleAttributes);
        moduleConfigInterfaceTransport1.ModuleId = 1;
        moduleConfigInterfaceTransport1.ModuleName = "SampleInterfaceTransport1";

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransport));
    }

    else if (transportId == 2)
    {
        // SampleInterfaceTransport2
        // -------------------------
        //
        DMF_CONFIG_SampleInterfaceTransport2_AND_ATTRIBUTES_INIT(&moduleConfigInterfaceTransport2,
                                                              &moduleAttributes);
        moduleConfigInterfaceTransport2.ModuleId = 2;
        moduleConfigInterfaceTransport2.ModuleName = "SampleInterfaceTransport2";

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransport));
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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Bind the Modules using SampleInterface Interface.
    // The decision about which Transport to bind has already been made and the
    // Transport Module has already been created.
    //
    ntStatus = DMF_INTERFACE_BIND(deviceContext->DmfModuleProtocol,
                                  deviceContext->DmfModuleTransport,
                                  SampleInterface);
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
    ntStatus = DMF_SampleInterfaceProtocol1_TestMethod(deviceContext->DmfModuleProtocol);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMF_SampleInterfaceProtocol1_TestMethod fails: ntStatus=%!STATUS!", ntStatus);
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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    ntStatus = STATUS_SUCCESS;

    // Unbind the Modules using SampleInterface Interface.
    //
    DMF_INTERFACE_UNBIND(deviceContext->DmfModuleProtocol,
                         deviceContext->DmfModuleTransport,
                         SampleInterface);

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
Here is the driver's Device Context. It just contains the two Modules (Protocol and Transport).
````
typedef struct
{
    // The Client Interface Module.
    //
    DMFMODULE DmfModuleProtocol;
    // The Server Interface Module.
    //
    DMFMODULE DmfModuleTransport;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)
````
The driver uses DMF's Driver Context Clean up callback:
````
#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(InterfaceClientServerEvtDriverContextCleanup)
````
This is the driver's DeviceAdd function. It is similar to other DMF drivers that have a Device Context. It creates a
WDFDEVICE with a Device Context and then creates DMF Modules.
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
    pnpPowerCallbacks.EvtDeviceD0Exit = InterfaceClientServerEvtDeviceD0Exit;
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
Next, is a helper function that simply reads the registry to determine which of two Transports (#1 or #2) should be used. 
````
#pragma code_seg("PAGED")
_Check_return_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
static
ULONG
InterfaceTransportIdGet(
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
Next, is the ModulesAdd callback. This callback instantiates a Protocol Module and then, based on the a value read from
the registry, instantiates a corresponding Transport Module. 
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
    DMF_CONFIG_SampleInterfaceProtocol1 moduleConfigInterfaceProtocol1;
    DMF_CONFIG_SampleInterfaceTransport1 moduleConfigInterfaceTransport1;
    DMF_CONFIG_SampleInterfaceTransport2 moduleConfigInterfaceTransport2;
    DEVICE_CONTEXT* deviceContext;
    ULONG transportId;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    transportId = InterfaceTransportIdGet(Device);

    // SampleInterfaceProtocol1
    // ------------------------
    //
    DMF_CONFIG_SampleInterfaceProtocol1_AND_ATTRIBUTES_INIT(&moduleConfigInterfaceProtocol1,
                                                            &moduleAttributes);
    moduleConfigInterfaceProtocol1.ModuleId = 1;
    moduleConfigInterfaceProtocol1.ModuleName = "SampleInterfaceProtocol1";
 
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &(deviceContext->DmfModuleProtocol));

    if (transportId == 1)
    {
        // SampleInterfaceTransport1
        // -------------------------
        //
        DMF_CONFIG_SampleInterfaceTransport1_AND_ATTRIBUTES_INIT(&moduleConfigInterfaceTransport1,
                                                                 &moduleAttributes);
        moduleConfigInterfaceTransport1.ModuleId = 1;
        moduleConfigInterfaceTransport1.ModuleName = "SampleInterfaceTransport1";

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransport));
    }

    else if (transportId == 2)
    {
        // SampleInterfaceTransport2
        // -------------------------
        //
        DMF_CONFIG_SampleInterfaceTransport2_AND_ATTRIBUTES_INIT(&moduleConfigInterfaceTransport2,
                                                              &moduleAttributes);
        moduleConfigInterfaceTransport2.ModuleId = 2;
        moduleConfigInterfaceTransport2.ModuleName = "SampleInterfaceTransport2";

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransport));
    }
    else
    {
        ASSERT(0);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");
}
#pragma code_seg()
````
Next, the EvtDevicePrepareHardware callback is shown. In this driver, this is where binding of the Protocol and Transport
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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Bind the Modules using SampleInterface Interface.
    // The decision about which Transport to bind has already been made and the
    // Transport Module has already been created.
    //
    ntStatus = DMF_INTERFACE_BIND(deviceContext->DmfModuleProtocol,
                                  deviceContext->DmfModuleTransport,
                                  SampleInterface);
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
    ntStatus = DMF_SampleInterfaceProtocol1_TestMethod(deviceContext->DmfModuleProtocol);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMF_SampleInterfaceProtocol1_TestMethod fails: ntStatus=%!STATUS!", ntStatus);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}
````
Next, the EvtDeviceReleaseHardware callback is shown. In this driver, this is where unbinding of the Protocol and Transport
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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    ntStatus = STATUS_SUCCESS;

    // Unbind the Modules using SampleInterface Interface.
    //
    DMF_INTERFACE_UNBIND(deviceContext->DmfModuleProtocol,
                         deviceContext->DmfModuleTransport,
                         SampleInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}
#pragma code_seg()
````

Testing the driver
==================

1. Use Traceview to load the driver's .pdb file and turn set logging settings to log "information".
2. Install this sample using this command with Devcon.exe from DDK: `devcon install InterfaceSample1.inf root\Transport1`. This will install the driver such that it uses Transport 1.
3. Using Traceview you can see logging showing that Transport1's Method is called when the driver starts.
5. Uninstall the driver.
6. Install this sample using this command with Devcon.exe from DDK: `devcon install InterfaceSample1.inf root\Transport2`. This will install the driver such that it uses Transport 2.
7. Using Traceview you can see logging showing that Transport2's Method is called when the driver starts.
8. Uninstall the driver.
