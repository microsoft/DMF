<!---
    name: SwitchBar2 Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 6)
    platform: KMDF/DMF
    language: c
    category: USB
    description: Demonstrates a DMF Container driver and use of <Dmf_DeviceInterfaceTarget>.
    samplefwlink: 
--->

SwitchBar2 Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 6)
========================================================================
This sample shows how to perform a common task in device drivers: Wait for a device interface to appear. When it appears, send/receive synchronous/asynchronous IOCTLs from that device interface.
Gracefully deal with arrival/removal of the device interface. 

This sample does the same as the previous sample except that streaming of IOCTLs happens automatically. Thus, it is not necessary to explicitly start and stop the continuous
target.

This sample is a function driver that does the following:

1. Register for a PnP Notification that the OSR FX2 driver is running.
2. When the OSR FX2 driver's device interface appears, this sample then sends the OSR FX2 driver four IOCTLS (`IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE`). (Every time one of the switches is pressed
the underlying OSR FX2 driver will return one of those IOCTLs indicating that a switch setting has changed.)
3. When this sample receives back an IOCTL indicating that a switch setting has changed, this driver then sends an IOCTL (`IOCTL_OSRUSBFX2_READ_SWITCHES`) synchronously to read the state
of the switches.
4. Then, this sample converts the bit mask received into a setting that will cause the corresponding LEDs on the light bar to set or clear to match the
settings of the switches.
5. Then, the sample sends an IOCTL (`IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY`) with that bit mask to set the light bar.
6. Finally, the IOCTL (`IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE`) is reused and sent again to the OSR Fx2 driver.
7. When the underlying OSR FX2 device and driver are removed, this sample stops streaming. It then waits again for the device to appear.

This sample shows several ideas:

1. Dmf Container Driver: A Dmf Container driver is a driver that only instantiates one or more Modules. Such a driver does not have a device context. In this case,
the driver has instantiated a single Module, `Dmf_DeviceInterfaceTarget`. That Module, along with it is callback function into the Client driver, does all the work 
listed above. Note how the sample has only the "business logic" in the callback function. The rest of the code, probably 90% of the driver, is contained in side of the DMF Library.

2. Macros that define default implementations `DriverEntry()`, `AddDevice()` and `DriverContextCleanup()`: It is not mandatory to use any or all of these macros. In most cases,
the DriverEntry macro is suitable. The AddDevice macro is suitable for drivers that do not need a device context (because they use Modules to do all their work).

2. `Dmf_DeviceInterfaceTarget`: This Module is does most of the work of the driver and is frequenly used in DMF drivers. This Module waits for a specific device interface specified by the Client to appear.
When the target appears, a sequence of `WDFREQUEST`s are sent to the target. These `WDFREQUEST`S contains an IOCTL and associated input/output buffers. When the `WDFREQUEST`s are completed by the underlying target a 
callback function in the Client is called. That callback function receives the data returned by the underlying target. It is here that the Client can perform the "busines logic" of the driver. Essentially, 
this Module is a "continuous reader" for any type of `WDFIOTARGET`. This sample shows how to do both synchronous and asynchronous calls to `WDFIOTARGET`s. Note that frequenly `Dmf_DeviceInterfaceTarget` is a Child
Module of a Module that performs specific actions onto a specific `WDFIOTARGET`.

3. How to use WDF primitives with DMF: In this case, a work item created and its context has an instance of `Dmf_DeviceInterfaceTarget`. This technique is often used to
access DMF Modules from WDF callback functions.

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

    Waits for the OSR FX2 driver to load. When it does, reads changes to switch state and sets
    lightbar on the board to match switch settings.

Environment:

    Kernel mode only

--*/

// The Dmf Library and the Dmf Library Modules this driver uses.
// In this sample, the driver uses the default library, unlike the earlier sample drivers that
// use the Template library.
//
#include <initguid.h>
#include "DmfModules.Library.h"

// This file contains all the OSR FX2 specific definitions.
//
#include "..\Modules.Template\Dmf_OsrFx2_Public.h"

#include "Trace.h"
#include "DmfInterface.tmh"

// DMF: These lines provide default DriverEntry/AddDevice/DriverCleanup functions.
//
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD SwitchBarEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP SwitchBarEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        SwitchBarEvtDriverContextCleanup,
                        SwitchBarEvtDeviceAdd)
#pragma code_seg()

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(SwitchBarEvtDriverContextCleanup)
DMF_DEFAULT_DEVICEADD(SwitchBarEvtDeviceAdd,
                      DmfDeviceModulesAdd)
#pragma code_seg()

// Rotates an 8-bit mask a given number of bits.
//
UCHAR 
RotateUCHAR(
    _In_ UCHAR BitMask, 
    _In_ UCHAR RotateByBits
    )
/*++

Routine Description:

    Shifts a given bit mask by a given number of bits.

Arguments:

    BitMask - The given bit mask.
    RotateByBits - The given number of bits to shift.

Return Value:

    The result of the rotate operation.

--*/
{
    UCHAR returnValue;

    ASSERT(RotateByBits <= 8);
    returnValue = (BitMask << RotateByBits) | ( BitMask >> (8 - RotateByBits));
    return returnValue;
}

#pragma code_seg("PAGE")
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
SwitchBarReadSwitchesAndUpdateLightBar(
    _In_ DMFMODULE DmfModuleDeviceInterfaceTarget
    )
/*++

Routine Description:

    Reads current state of switches from the board and then sets the light bar
    in a corresponding manner.

Arguments:

    DmfModuleInterfaceTarget - Allows access to the OSR FX2 driver.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    SWITCH_STATE switchData;

    PAGED_CODE();

    // Switches have changed. Read them. (Wait until the switch is read.)
    //
    ntStatus = DMF_DeviceInterfaceTarget_SendSynchronously(DmfModuleDeviceInterfaceTarget,
                                                           NULL,
                                                           0,
                                                           (VOID*)&switchData,
                                                           sizeof(SWITCH_STATE),
                                                           ContinuousRequestTarget_RequestType_Ioctl,
                                                           IOCTL_OSRUSBFX2_READ_SWITCHES,
                                                           0,
                                                           NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Convert the switch data to corresponding light bar data.
    //
    switchData.SwitchesAsUChar = RotateUCHAR(switchData.SwitchesAsUChar, 
                                             5);
    switchData.SwitchesAsUChar = ~(switchData.SwitchesAsUChar & 0xFF);

    // Set the light bar...no need to wait.
    //
    ntStatus = DMF_DeviceInterfaceTarget_Send(DmfModuleDeviceInterfaceTarget,
                                              &switchData.SwitchesAsUChar,
                                              sizeof(UCHAR),
                                              NULL,
                                              0,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY,
                                              0,
                                              NULL,
                                              NULL);

Exit:
    ;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_WDF_WORKITEM)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
SwitchBarWorkitemHandler(
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
    DMFMODULE* dmfModuleAddressDeviceInterfaceTarget;

    PAGED_CODE();

    // Get the address where the DMFMODULE is located.
    //
    dmfModuleAddressDeviceInterfaceTarget = WdfObjectGet_DMFMODULE(Workitem);

    // Read switches and set lights.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(*dmfModuleAddressDeviceInterfaceTarget);

    WdfObjectDelete(Workitem);
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ContinuousRequestTarget_BufferDisposition
SwitchBarSwitchChangedCallback(
    _In_ DMFMODULE DmfModule,
    _In_reads_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _In_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Continous reader has received a buffer from the udnerlying target (OSR FX2) driver.
    This runs in DISPATCH_LEVEL. Since this driver must syncrhonously read the state
    of the switches, this function just spawns a workitem that runs a PASSIVE_LEVEL.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    OutputBuffer - It is the data switch data returned from OSR FX2 board.
    OutputBufferSize - Size of switch data returned from OSR FX2 board (UCHAR).
    ClientBufferContextOutput - Not used.
    CompletionStatus - OSR FX2 driver's result code for that IOCTL.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    WDFWORKITEM workitem;
    WDF_WORKITEM_CONFIG workitemConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMFMODULE* dmfModuleAddress;
    ContinuousRequestTarget_BufferDisposition returnValue;

    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(ClientBufferContextOutput);

    if (!NT_SUCCESS(CompletionStatus))
    {
        // This will happen when OSR FX2 board is unplugged.
        //
        returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        goto Exit;
    }

    // Create a WDFWORKITEM and enqueue it. The workitem's function will delete the workitem.
    //
    WDF_WORKITEM_CONFIG_INIT(&workitemConfig, 
                             SwitchBarWorkitemHandler);
    workitemConfig.AutomaticSerialization = WdfFalse;

    // It is not possible to get the WDFWORKITEM's parent, so create space for the DMFMODULE
    // in the workitem's context.
    ///
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                           DMFMODULE);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfWorkItemCreate(&workitemConfig,
                                 &objectAttributes,
                                 &workitem);

    dmfModuleAddress = WdfObjectGet_DMFMODULE(workitem);
    *dmfModuleAddress = DmfModule;

    WdfWorkItemEnqueue(workitem);

    // Continue streaming this IOCTL.
    //
    returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;

Exit:

    return returnValue;
}

static
VOID
SwitchBar_OnDeviceArrivalNotification(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Arrival Notification.
    In this case, this driver starts the continuous reader.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    // Dmf_ContinousRequestTarget has been set to start automatically, so it is not started here.
    // Also, the PreClose callback is not necessary.
    //

    // Do an initial read and write for the current state of the board before
    // any switches have been changed.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(DmfModule);
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

    Add all the Dmf Modules used by this driver.

Arguments:

    Device - WDFDEVICE handle.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    NTSTATUS

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_DeviceInterfaceTarget moduleConfigDeviceInterfaceTarget;
    DMF_MODULE_EVENT_CALLBACKS moduleEventCallbacks;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    // DeviceInterfaceTarget
    // ---------------------
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_OSRUSBFX2;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = 4;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(SWITCH_STATE);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 4;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = SwitchBarSwitchChangedCallback;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;

    // These callbacks tell us when the underlying target is available. When it is available, the lightbar on the board is initialized
    // to the current state of the switches.
    //
    DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(&moduleAttributes,
                                               &moduleEventCallbacks);
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = SwitchBar_OnDeviceArrivalNotification;

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);
}

```

Annotated Code Tour
===================
In this section, the above code is annotated section by section:

First, DMF is included. Note that most drivers will `#include "DmfModules.Library.h"`. This file includes all of DMF as well as all the Modules distributed with DMF.
When a team creates a new Library, this library will be a subset of the new Library. That team will always include that new Library's include file. In this case,
however, this driver is able to use the Library that is publicly distributed with DMF. Always `#include <initguid.h>` one time before including the DMF Library in one 
file.

```
// The Dmf Library and the Dmf Library Modules this driver uses.
// In this sample, the driver uses the default library, unlike the earlier sample drivers that
// use the Template library.
//
#include <initguid.h>
#include "DmfModules.Library.h"
```

This line just includes the definitions that allow this driver to talk to the OSR FX2 driver:

```
// This file contains all the OSR FX2 specific definitions.
//
#include "..\Modules.Template\Dmf_OsrFx2_Public.h"
```

These lines are for trace logging:

```
#include "Trace.h"
#include "DmfInterface.tmh"
```

These are the forward declarations of the functions for DriverEntry, AddDevice, DriverContextCleanup and DmfDeviceModulesAdd:

```
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD SwitchBarEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP SwitchBarEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;
```

Next, this driver uses the DMF's version of `DriverEntry()`, `AddDevice()` and `DriverContextCleanup()`. Note the use of #pragma to set the code in the "INIT" and "PAGED" segments. 
When these macros are used, the first place where Client specific code executes is in the function `DmfDeviceModulesAdd()`.
```
/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        SwitchBarEvtDriverContextCleanup,
                        SwitchBarEvtDeviceAdd)
#pragma code_seg()

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(SwitchBarEvtDriverContextCleanup)
DMF_DEFAULT_DEVICEADD(SwitchBarEvtDeviceAdd,
                      DmfDeviceModulesAdd)
#pragma code_seg()
```

This is a helper function that rotates bits. It is necesary to light the lightbar's LEDs to match the switches in an intuitive manner:

```
// Rotates an 8-bit mask a given number of bits.
//
UCHAR 
RotateUCHAR(
    _In_ UCHAR BitMask, 
    _In_ UCHAR RotateByBits
    )
/*++

Routine Description:

    Shifts a given bit mask by a given number of bits.

Arguments:

    BitMask - The given bit mask.
    RotateByBits - The given number of bits to shift.

Return Value:

    The result of the rotate operation.

--*/
{
    UCHAR returnValue;

    ASSERT(RotateByBits <= 8);
    returnValue = (BitMask << RotateByBits) | ( BitMask >> (8 - RotateByBits));
    return returnValue;
}
```

This is a function that synchronously reads the state of the switches, then asynchronously sends the desired state of the lightbar.
This function receives a `DMFMODULE` as a parameter. This `DMFMODULE` corresponds to the device interface exposed by the OSR FX2 function driver. This Module has created,
opened and stored a `WDFIOTARGET` that corresponds to the device interface exposed by the OSR FX2 driver. This Module has several Methods that allow the Client to
send and receive data from that `WDFIOTARGET`. 

In this case, `DMF_DeviceInterfaceTarget_SendSynchronously()` is used to read the state of the switches. Note that the
Client only needs to send input/output buffers. The Module will create the corresponding WDFEREQUEST using those buffers and IOCTL. When, the Client calls this 
Method, the return value is store in the buffer, switchData.

Next, the data is converted to a value that can be sent to the lightbar.

Next, the Module Method, `DMF_DeviceInterfaceTarget_Send()`, is used to asynchronously send that value to the lightbar.

Note that using DMF, it is possible to organize drivers so that the business logic is separate from the glue performs the actual work. In this case, the Client driver
does not need to deal with `WDFREQUEST` at all, just simple buffers. In general, Modules are designed that way.

```
#pragma code_seg("PAGE")
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
SwitchBarReadSwitchesAndUpdateLightBar(
    _In_ DMFMODULE DmfModuleDeviceInterfaceTarget
    )
/*++

Routine Description:

    Reads current state of switches from the board and then sets the light bar
    in a corresponding manner.

Arguments:

    DmfModuleInterfaceTarget - Allows access to the OSR FX2 driver.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    SWITCH_STATE switchData;

    PAGED_CODE();

    // Switches have changed. Read them. (Wait until the switch is read.)
    //
    ntStatus = DMF_DeviceInterfaceTarget_SendSynchronously(DmfModuleDeviceInterfaceTarget,
                                                           NULL,
                                                           0,
                                                           (VOID*)&switchData,
                                                           sizeof(SWITCH_STATE),
                                                           ContinuousRequestTarget_RequestType_Ioctl,
                                                           IOCTL_OSRUSBFX2_READ_SWITCHES,
                                                           0,
                                                           NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Convert the switch data to corresponding light bar data.
    //
    switchData.SwitchesAsUChar = RotateUCHAR(switchData.SwitchesAsUChar, 
                                             5);
    switchData.SwitchesAsUChar = ~(switchData.SwitchesAsUChar & 0xFF);

    // Set the light bar...no need to wait.
    //
    ntStatus = DMF_DeviceInterfaceTarget_Send(DmfModuleDeviceInterfaceTarget,
                                              &switchData.SwitchesAsUChar,
                                              sizeof(UCHAR),
                                              NULL,
                                              0,
                                              ContinuousRequestTarget_RequestType_Ioctl,
                                              IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY,
                                              0,
                                              NULL,
                                              NULL);

Exit:
    ;
}
#pragma code_seg()
```

This code is the WDFWORKITEM handler. It is called after being enqueued from the DISPATCH_LEVEL callback received earlier. This callback simply calls the above
function to read switches and set the lightbar, then it deletes the associated WDFWORKITEM.

```
#pragma code_seg("PAGE")
_Function_class_(EVT_WDF_WORKITEM)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
SwitchBarWorkitemHandler(
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
    DMFMODULE* dmfModuleAddressDeviceInterfaceTarget;

    PAGED_CODE();

    // Get the address where the DMFMODULE is located.
    //
    dmfModuleAddressDeviceInterfaceTarget = WdfObjectGet_DMFMODULE(Workitem);

    // Read switches and set lights.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(*dmfModuleAddressDeviceInterfaceTarget);

    WdfObjectDelete(Workitem);
}
#pragma code_seg()
```

This function is a callback received from the `Dmf_DeviceInterfaceTarget` Module. It is received whenever the `IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE` has returned
from the OSR FX2 driver indicating that the switches on the board have changed. The Module has already extracted the buffers from the underlying `WDFREQUEST` it sent.
In this case, the data returned by the IOCTL is in OutputBuffer. However, this function runs at DISPATCH_LEVEL. In order to perform the above function, it is 
necessary to execute at PASSIVE_LEVEL because it needs to perform a synchrnonous read. Thus, this function does not actually use OutputBuffer. Instead, it simply
creates and spawns a WDFWORKITEM. 

The WDFWORKITEM callback function needs the `Dmf_DeviceInterface Module` handle. Although the WDFWORKITEM's parent is the `DMFMODULE`, it is not possible to 
retrieve the WDFWORKITEM's parent, so the `DMFMODULE` must be stored in the workitem's context. Thus, this function allocates space in the workitem's 
context for the `DMFMODULE` handle and then writes that handle to the context after the workitem has been created. Finally, it enqueues the WORKITEM. Note the use of
the predefined `WdfObjectGet_DMFMODULE()`.

Finally, note the return value of this function. It tells the caller (`Dmf_DeviceInterfaceTarget Module`) who owns the OutputBuffer and whether or not to 
continue streaming. When an error is received (CompletionStatus) it means the OSR FX2 driver is unloading. So, in that case this callback tells
the caller to stop streaming IOCTLs. Otherwise, the callback tells the Client to continue streaming.

```
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ContinuousRequestTarget_BufferDisposition
SwitchBarSwitchChangedCallback(
    _In_ DMFMODULE DmfModule,
    _In_reads_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _In_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Continous reader has received a buffer from the udnerlying target (OSR FX2) driver.
    This runs in DISPATCH_LEVEL. Since this driver must syncrhonously read the state
    of the switches, this function just spawns a workitem that runs a PASSIVE_LEVEL.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    OutputBuffer - It is the data switch data returned from OSR FX2 board.
    OutputBufferSize - Size of switch data returned from OSR FX2 board (UCHAR).
    ClientBufferContextOutput - Not used.
    CompletionStatus - OSR FX2 driver's result code for that IOCTL.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    WDFWORKITEM workitem;
    WDF_WORKITEM_CONFIG workitemConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMFMODULE* dmfModuleAddress;
    ContinuousRequestTarget_BufferDisposition returnValue;

    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(ClientBufferContextOutput);

    if (!NT_SUCCESS(CompletionStatus))
    {
        // This will happen when OSR FX2 board is unplugged.
        //
        returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        goto Exit;
    }

    // Create a WDFWORKITEM and enqueue it. The workitem's function will delete the workitem.
    //
    WDF_WORKITEM_CONFIG_INIT(&workitemConfig, 
                             SwitchBarWorkitemHandler);
    workitemConfig.AutomaticSerialization = WdfFalse;

    // It is not possible to get the WDFWORKITEM's parent, so create space for the DMFMODULE
    // in the workitem's context.
    ///
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                           DMFMODULE);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfWorkItemCreate(&workitemConfig,
                                 &objectAttributes,
                                 &workitem);

    dmfModuleAddress = WdfObjectGet_DMFMODULE(workitem);
    *dmfModuleAddress = DmfModule;

    WdfWorkItemEnqueue(workitem);

    // Continue streaming this IOCTL.
    //
    returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;

Exit:

    return returnValue;
}
```

This callback is called when the `Dmf_DeviceInterfaceTarget` Module has opened the underlying device interface requested by the Client. It means the User has plugged in
the OSR FX2 board and its driver has started. In this sample, it is not necessary to start streaming because the Dmf_ContinuousRequestTarget (Child Module of 
Dmf_DeviceInterfaceTarget) has been set to start automatically using this line: 
`moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;`
Note further that this eliminates the need for the PreClose callback.

```

static
VOID
SwitchBar_OnDeviceArrivalNotification(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Arrival Notification.
    In this case, this driver starts the continuous reader.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    // Dmf_ContinousRequestTarget has been set to start automatically, so it is not started here.
    // Also, the PreClose callback is not necessary.
    //

    // Do an initial read and write for the current state of the board before
    // any switches have been changed.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(DmfModule);
}

```

This is the last function in the driver and the first Client function that is called. This is the Modules Add function. In this case, a single Module, `Dmf_DeviceInterfaceTarget`
is instantiated. It receives the following information:
1. What device interface to open (`GUID_DEVINTERFACE_OSRUSBFX2`).
2. The type of `WDFREQUEST` that will be sent to the target. In this case, the driver will send IOCTLs.
2. The IOCTL to send to the target.
3. The number of simultaneous IOCTLs to send to the target.
4. The size, number and pool type of input buffers to create. This corresponds to the values required by the IOCTL. In this case, the IOCTL has no input buffer.
5. The size, number and pool type of output buffers to create. This corresponds to the values required by the IOCTL. In this case, the IOCTL requires a buffer 
of size `sizeof(SWTICH_STATE)` of pool type `NonPagedPoolNx`. `Dmf_DeviceInterfaceTarget` will create all the buffers as needed. (As an aside, bounds checking is automatically performed on the
buffers when they are used. If buffer overrun occurs is detected, an assert will happen.)
6. The callback function that is called by the Module when the `WDFREQUEST` with IOCTL has completed. In this case, the function is: `SwitchBarSwitchChangedCallback`.
7. Finally, the names of callback functions that are called when the `WDFIOTARGET` (`GUID_DEVINTERFACE_OSRUSBFX2`) appears: `SwitchBar_OnDeviceArrivalNotification`.

```
#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DmfDeviceModulesAdd(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Add all the Dmf Modules used by this driver.

Arguments:

    Device - WDFDEVICE handle.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    NTSTATUS

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_DeviceInterfaceTarget moduleConfigDeviceInterfaceTarget;
    DMF_MODULE_EVENT_CALLBACKS moduleEventCallbacks;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    // DeviceInterfaceTarget
    // ---------------------
    //
    DMF_CONFIG_DeviceInterfaceTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceTarget,
                                                         &moduleAttributes);
    moduleConfigDeviceInterfaceTarget.DeviceInterfaceTargetGuid = GUID_DEVINTERFACE_OSRUSBFX2;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = 4;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(SWITCH_STATE);
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 4;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = SwitchBarSwitchChangedCallback;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;

    // These callbacks tell us when the underlying target is available. When it is available, the lightbar on the board is initialized
    // to the current state of the switches.
    //
    DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(&moduleAttributes,
                                               &moduleEventCallbacks);
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = SwitchBar_OnDeviceArrivalNotification;

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);
}

```

Testing the driver
==================

1. Plug in the OSR FX2 board.
2. **IMPORTANT: Install the Sample3 (Sample DMF driver 3)** as the driver that controls the OSR FX2 board. *This is necessary so that the OSR FX2 function driver can accept
IOCTLs at PASSIVE_LEVEL. (The original driver is unable to do so due to the fact that IOCTL handling path is all paged.)*
3. Install this sample.
4. When you run the sample, the light bar should match with the switches. As you change the switches, the light bar should change accordingly.
5. IMPORTANT: If the display on the board shows 5 or S, it means the board is in Idle state (sleep). Press the button to wake it up.


