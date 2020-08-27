SwitchBar5 Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 9)
========================================================================
This sample shows how to write a Client driver that **only uses Dynamic Modules**. _Drivers that only use Dynamic Modules do not need to use the DMF hooking
APIs._ This sample is similar to SwitchBar5. It is also a filter driver, but it does not filter the IOCTLs that come through the driver.

Two important definitions:

**Static Module**: A Static Module is a Module that is instantiated by a Client in the `DmfModulesAdd` or `ChildModulesAdd` callback. Static Modules' Parent WDFOBJECT is always either a WDFDEVICE or DMFMODULE. The lifetime of static Modules depends only on the lifetime of its Parent WDFOBJECT. *Static Modules **will** receive WDF callbacks.*

**Dynamic Module**: A Dynamic Module is a Module that is instantiated by the Client at anytime and is attached to any WDFOBJECT. The Client uses `WdfObjectDelete()` to destroy a Dynamic Module or it is automatically deleted when its Parent WDFOBJECT is deleted. *Dynamic Modules **will not** receive WDF callbacks.*

Note some important points:

1. Not all Modules can be instantiated dynamically. Only Modules that do not support WDF callbacks can be instantiated dynamically.
2. It is best to always use the hooking APIs because then it is always possible to add Static Modules later (and you can still use Dynamic Modules).
3. If a Client attempts to instantiate a Module dynamically but the Module does not support dynamic instantiation, DMF will not do so and return an error code.

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

    SwitchBar5 Sample: Loads as a filter driver on OSRFX2 driver. When it does, reads changes to 
                       switch state and sets lightbar on the board to match switch settings. This 
                       sample is the same as SwitchBar4 except that this sample show how a driver
                       can use Modules instantiated as Dynamic Modules. 

    NOTE: This sample does not need to hook DMF because it does not use Static Modules, just
          Dynamic Modules. Most DMF drivers use Static Modules, but as this sample shows it is
          possible to use only Dynamic Modules without hooking DMF.

    IMPORTANT: Not all Modules can be instantiated as Dynamic Modules. Only Modules that do not
               support WDF callbacks can be instantiated as Dynamic Modules. See the Documentation
               and file headers to know which Modules can be instantiated as Dynamic Modules.
               (All Modules can be instantiated as Static Modules, but only a subset of those 
               can be instantiated as Dynamic Modules.)

Environment:

    Kernel mode only

--*/

// The DMF Library and the DMF Library Modules this driver uses.
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
EVT_WDF_DEVICE_D0_ENTRY SwitchBarEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT SwitchBarEvtDeviceD0Exit;

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        SwitchBarEvtDriverContextCleanup,
                        SwitchBarEvtDeviceAdd)
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DmfDeviceDynamicModulesAdd(
    _In_ WDFDEVICE Device
    );

typedef struct
{
    // Allows this driver to send communicate with the USB device both using streaming
    // and by sending individual requests.
    //
    DMFMODULE DmfModuleContinuousRequestTarget;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(SwitchBarEvtDriverContextCleanup)

_Use_decl_annotations_
NTSTATUS
SwitchBarEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    // Tell WDF this callback should be called.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceD0Entry = SwitchBarEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = SwitchBarEvtDeviceD0Exit;

    // It is not necessary to call DMF Hooking APIs since this sample use only Dynamic Modules.
    //

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit,
                                           &pnpPowerCallbacks);

    // Set any device attributes needed.
    //
    WdfDeviceInitSetDeviceType(DeviceInit,
                               FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetExclusive(DeviceInit,
                              FALSE);

    // This is a filter driver that loads on OSRUSBFX2 driver.
    //
    WdfFdoInitSetFilter(DeviceInit);

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

    // It is not necessary to call DMF_ModulesCreate since this driver uses only 
    // Dynamic Modules.
    //
    ntStatus = DmfDeviceDynamicModulesAdd(device);

Exit:

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC! ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// Forward declarations of DMF callbacks.
//
EVT_DMF_ContinuousRequestTarget_BufferOutput SwitchBarSwitchChangedCallback;

#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DmfDeviceDynamicModulesAdd(
    _In_ WDFDEVICE Device
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
    PDEVICE_CONTEXT deviceContext;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMF_CONFIG_ContinuousRequestTarget moduleConfigContinuousRequestTarget;
    WDFIOTARGET nextTargetInStack;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    deviceContext = DeviceContextGet(Device);

    // ContinuousRequestTarget
    // -----------------------
    //
    DMF_CONFIG_ContinuousRequestTarget_AND_ATTRIBUTES_INIT(&moduleConfigContinuousRequestTarget,
                                                           &moduleAttributes);
    moduleConfigContinuousRequestTarget.BufferCountOutput = 1;
    moduleConfigContinuousRequestTarget.BufferOutputSize = sizeof(SWITCH_STATE);
    moduleConfigContinuousRequestTarget.ContinuousRequestCount = 1;
    moduleConfigContinuousRequestTarget.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigContinuousRequestTarget.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigContinuousRequestTarget.ContinuousRequestTargetIoctl = IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE;
    moduleConfigContinuousRequestTarget.EvtContinuousRequestTargetBufferOutput = SwitchBarSwitchChangedCallback;
    moduleConfigContinuousRequestTarget.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    // OSR driver needs to be called at PASSIVE_LEVEL because its IOCTL handling code path is all paged.
    // Modules look at this attribute they need to execute code in PASSIVE_LEVEL. It is up to Modules to 
    // determine how to use this flag. (In this case DMF_ContinuousRequestTarget will resend requests back to
    // OSR driver at PASSIVE_LEVEL.
    //
    moduleAttributes.PassiveLevel = TRUE;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = Device;

    // Create a Dynamic Module (ContinuousRequestTarget).
    // NOTE: It is child of Device so it is automatically deleted when Device is deleted.
    //
    ntStatus = DMF_ContinuousRequestTarget_Create(Device,
                                                  &moduleAttributes,
                                                  &objectAttributes,
                                                  &deviceContext->DmfModuleContinuousRequestTarget);
    if (NT_SUCCESS(ntStatus))
    {
        // Get the next target in the stack. It is the OSR FX2 function driver.
        //
        nextTargetInStack = WdfDeviceGetIoTarget(Device);
        // Tell the ContinuousRequestTarget what its target is. (It will automatically start streaming).
        //
        DMF_ContinuousRequestTarget_IoTargetSet(deviceContext->DmfModuleContinuousRequestTarget,
                                                nextTargetInStack);
    }

    return ntStatus;
}
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

    Rotates (left) a given bit UCHAR mask by a given number of bits.

Arguments:

    BitMask - The given bit mask.
    RotateByBits - The given number of bits to shift.

Return Value:

    The result of the rotate operation.

--*/
{
    UCHAR returnValue;
    const ULONG bitsPerByte = 8;

    RotateByBits %= bitsPerByte;
    returnValue = (BitMask << RotateByBits) | ( BitMask >> (bitsPerByte - RotateByBits));
    return returnValue;
}

#pragma code_seg("PAGE")
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
SwitchBarReadSwitchesAndUpdateLightBar(
    _In_ DMFMODULE DmfModuleContinuousRequestTarget
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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "-->%!FUNC!");
    // Switches have changed. Read them. (Wait until the switch is read.)
    //
    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(DmfModuleContinuousRequestTarget,
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

    // Set the light bar. Need to wait as the buffer is on the stack.
    //
    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(DmfModuleContinuousRequestTarget,
                                                             &switchData.SwitchesAsUChar,
                                                             sizeof(UCHAR),
                                                             NULL,
                                                             0,
                                                             ContinuousRequestTarget_RequestType_Ioctl,
                                                             IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY,
                                                             0,
                                                             NULL);

Exit:
    ;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ContinuousRequestTarget_BufferDisposition
SwitchBarSwitchChangedCallback(
    _In_ DMFMODULE DmfModuleContinuousRequestTarget,
    _In_reads_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _In_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Continuous reader has received a buffer from the underlying target (OSR FX2) driver.
    This function runs at PASSIVE_LEVEL because the Module was configured to do so!

Arguments:

    DmfModuleContinuousRequestTarget - The Child Module from which this callback is called (DMF_ContinuousRequestTarget).
    OutputBuffer - It is the data switch data returned from OSR FX2 board.
    OutputBufferSize - Size of switch data returned from OSR FX2 board (UCHAR).
    ClientBufferContextOutput - Not used.
    CompletionStatus - OSR FX2 driver's result code for that IOCTL.

Return Value:

    Indicates the owner of the OutputBuffer after this function completes and whether or
    not streaming should stop.

--*/
{
    ContinuousRequestTarget_BufferDisposition returnValue;

    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(ClientBufferContextOutput);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "-->%!FUNC! CompletionStatus=%!STATUS!", CompletionStatus);

    if (!NT_SUCCESS(CompletionStatus))
    {
        // This will happen when OSR FX2 board is unplugged: Stop streaming.
        // 
        returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! Streaming: stop");
        goto Exit;
    }

    // Read switches and set lights.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(DmfModuleContinuousRequestTarget);

    // Continue streaming this IOCTL.
    //
    returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! Streaming: continue");

Exit:

    return returnValue;
}

NTSTATUS
SwitchBarEvtDeviceD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    When the device powers up, read the switches and set the lightbar appropriately.
 
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

    // Read the state of switches and initialize lightbar.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(deviceContext->DmfModuleContinuousRequestTarget);

    // Start streaming.
    //
    ntStatus = DMF_ContinuousRequestTarget_Start(deviceContext->DmfModuleContinuousRequestTarget);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}

NTSTATUS
SwitchBarEvtDeviceD0Exit(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    When the device powers down, stop streaming.
 
Arguments:

    Device - Handle to a framework device object.
    TargetState - The power state the device is entering.

Return Value:

    NTSTATUS

--*/
{
    DEVICE_CONTEXT* deviceContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(TargetState);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Stop streaming.
    //
    DMF_ContinuousRequestTarget_StopAndWait(deviceContext->DmfModuleContinuousRequestTarget);

    ntStatus = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
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
// The DMF Library and the DMF Library Modules this driver uses.
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
These are the forward declarations of the functions for DriverEntry, DeviceAdd, DeviceD0Entry, DriverContextCleanup and DmfDeviceModulesAdd:
(Note that DeviceIoControl is not listed because the Client Driver uses DMF_IoctlHandler to do most of the processing of each IOCTL.)
```
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD SwitchBarEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP SwitchBarEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;
EVT_WDF_DEVICE_D0_ENTRY SwitchBarEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT SwitchBarEvtDeviceD0Exit;
```
Next, this driver uses the DMF's version of `DriverEntry()` and `DriverContextCleanup()`. Note the use of #pragma to set the code in the "INIT" and "PAGED" segments. 
When these macros are used, the first place where Client specific code executes is in the function `DmfDeviceModulesAdd()`. This driver does not use DMF's DeviceAdd.
```
/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        SwitchBarEvtDriverContextCleanup,
                        SwitchBarEvtDeviceAdd)
#pragma code_seg()

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(SwitchBarEvtDriverContextCleanup)
```
This driver implements its own DeviceAdd because it wants to execute as a filter driver and create its own device context.
This sample, unlike the other samples, *does not hook DMF* because it only uses Dynamic Modules.
```
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DmfDeviceDynamicModulesAdd(
    _In_ WDFDEVICE Device
    );

typedef struct
{
    // Allows this driver to send communicate with the USB device both using streaming
    // and by sending individual requests.
    //
    DMFMODULE DmfModuleContinuousRequestTarget;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)

_Use_decl_annotations_
NTSTATUS
SwitchBarEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    // Tell WDF this callback should be called.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceD0Entry = SwitchBarEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = SwitchBarEvtDeviceD0Exit;

    // It is not necessary to call DMF Hooking APIs since this sample use only Dynamic Modules.
    //

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit,
                                           &pnpPowerCallbacks);

    // Set any device attributes needed.
    //
    WdfDeviceInitSetDeviceType(DeviceInit,
                               FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetExclusive(DeviceInit,
                              FALSE);

    // This is a filter driver that loads on OSRUSBFX2 driver.
    //
    WdfFdoInitSetFilter(DeviceInit);

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

    // It is not necessary to call DMF_ModulesCreate since this driver uses only 
    // Dynamic Modules.
    //
    ntStatus = DmfDeviceDynamicModulesAdd(device);

Exit:

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC! ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()
```

The next function is not a callback. It is just a function that is called in the above function which creates the Dynamic Module this driver uses. Dynamic Modules can be created at anytime, not just in the Client's DeviceAdd callback.

```
// Forward declarations of DMF callbacks.
//
EVT_DMF_ContinuousRequestTarget_BufferOutput SwitchBarSwitchChangedCallback;

#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DmfDeviceDynamicModulesAdd(
    _In_ WDFDEVICE Device
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
    PDEVICE_CONTEXT deviceContext;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMF_CONFIG_ContinuousRequestTarget moduleConfigContinuousRequestTarget;
    WDFIOTARGET nextTargetInStack;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    deviceContext = DeviceContextGet(Device);

    // ContinuousRequestTarget
    // -----------------------
    //
    DMF_CONFIG_ContinuousRequestTarget_AND_ATTRIBUTES_INIT(&moduleConfigContinuousRequestTarget,
                                                           &moduleAttributes);
    moduleConfigContinuousRequestTarget.BufferCountOutput = 1;
    moduleConfigContinuousRequestTarget.BufferOutputSize = sizeof(SWITCH_STATE);
    moduleConfigContinuousRequestTarget.ContinuousRequestCount = 1;
    moduleConfigContinuousRequestTarget.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigContinuousRequestTarget.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigContinuousRequestTarget.ContinuousRequestTargetIoctl = IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE;
    moduleConfigContinuousRequestTarget.EvtContinuousRequestTargetBufferOutput = SwitchBarSwitchChangedCallback;
    moduleConfigContinuousRequestTarget.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    // OSR driver needs to be called at PASSIVE_LEVEL because its IOCTL handling code path is all paged.
    // Modules look at this attribute they need to execute code in PASSIVE_LEVEL. It is up to Modules to 
    // determine how to use this flag. (In this case DMF_ContinuousRequestTarget will resend requests back to
    // OSR driver at PASSIVE_LEVEL.
    //
    moduleAttributes.PassiveLevel = TRUE;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = Device;

    // Create a Dynamic Module (ContinuousRequestTarget).
    // NOTE: It is child of Device so it is automatically deleted when Device is deleted.
    //
    ntStatus = DMF_ContinuousRequestTarget_Create(Device,
                                                  &moduleAttributes,
                                                  &objectAttributes,
                                                  &deviceContext->DmfModuleContinuousRequestTarget);
    if (NT_SUCCESS(ntStatus))
    {
        // Get the next target in the stack. It is the OSR FX2 function driver.
        //
        nextTargetInStack = WdfDeviceGetIoTarget(Device);
        // Tell the ContinuousRequestTarget what its target is. (It will automatically start streaming).
        //
        DMF_ContinuousRequestTarget_IoTargetSet(deviceContext->DmfModuleContinuousRequestTarget,
                                                nextTargetInStack);
    }

    return ntStatus;
}
#pragma code_seg()
```
This is a helper function that rotates bits. It is necessary to light the lightbar's LEDs to match the switches in an intuitive manner:

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
    const ULONG bitsPerByte = 8;

    RotateByBits %= bitsPerByte;
    returnValue = (BitMask << RotateByBits) | ( BitMask >> (bitsPerByte - RotateByBits));
    return returnValue;
}
```

This is a function that synchronously reads the state of the switches, then asynchronously sends the desired state of the lightbar.
This function receives a `DMFMODULE` as a parameter. This `DMFMODULE` corresponds to the device interface exposed by the OSR FX2 function driver. This Module has created,
opened and stored a `WDFIOTARGET` that corresponds to the device interface exposed by the OSR FX2 driver. This Module has several Methods that allow the Client to
send and receive data from that `WDFIOTARGET`. 

In the previous sample, `DMF_DeviceInterfaceTarget_SendSynchronously()` was used to read the state of the switches. But, in this sample `Dmf_DefaultTarget` is used instead
because the client driver is a filter driver and does not need to specify the device interface GUID of the target device. Instead, this driver sends requests to the next
driver in the stack (which is the target that `DMF_ContinuousRequestTarget` opens). Note that the Client only needs to send input/output buffers. The Module will create the corresponding
WDFEREQUEST using those buffers and IOCTL. When, the Client calls this Method, the return value is store in the buffer, switchData.

Next, the data is converted to a value that can be sent to the lightbar.

Next, the Module Method, `DMF_ContinuousRequestTarget_Send()`, is used to asynchronously send that value to the lightbar.

Note that using DMF, it is possible to organize drivers so that the business logic is separate from the glue performs the actual work. In this case, the Client driver
does not need to deal with `WDFREQUEST` at all, just simple buffers. In general, Modules are designed that way.

```
#pragma code_seg("PAGE")
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
SwitchBarReadSwitchesAndUpdateLightBar(
    _In_ DMFMODULE DmfModuleContinuousRequestTarget
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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "-->%!FUNC!");
    // Switches have changed. Read them. (Wait until the switch is read.)
    //
    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(DmfModuleContinuousRequestTarget,
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

    // Set the light bar. Need to wait as the buffer is on the stack.
    //
    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(DmfModuleContinuousRequestTarget,
                                                             &switchData.SwitchesAsUChar,
                                                             sizeof(UCHAR),
                                                             NULL,
                                                             0,
                                                             ContinuousRequestTarget_RequestType_Ioctl,
                                                             IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY,
                                                             0,
                                                             NULL);

Exit:
    ;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
}
#pragma code_seg()

```
This function is a callback received from the `DMF_ContinuousTarget` Module. It is received whenever the `IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE` has returned
from the OSR FX2 driver indicating that the switches on the board have changed. The Module has already extracted the buffers from the underlying `WDFREQUEST` it sent.
In this case, the data returned by the IOCTL is in OutputBuffer. (Note that this sample does not use the buffer. Instead if reads the switch data directly.)

Ordinarily, completion routines from the USB stack run at `DISPATCH_LEVEL`. The completion routine that executes when the continuous requests from the 
from the `DMF_ContinuousRequestTarget` Module executes at `DISPATCH_LEVEL` by default. However, this sample reads the switch state synchronously, so it must
run at `PASSIVE_LEVEL`. Also, the underlying function driver expects that its IOCTL requests arrive at `PASSIVE_LEVEL`. For these reasons, the 
`DMF_ContinuousRequestTarget` Module is configured to run at `PASSIVE_LEVEL` by simply setting that option when the Module is configured:

```
    moduleAttributes.PassiveLevel = TRUE;
```

Finally, note the return value of this function. It tells the caller (`DMF_ContinuousRequestTarget` Module) who owns the OutputBuffer and whether or not to 
continue streaming. When an error is received (CompletionStatus) it means the OSR FX2 driver is unloading. So, in that case this callback tells
the caller to stop streaming IOCTLs. Otherwise, the callback tells the Client to continue streaming.

```
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ContinuousRequestTarget_BufferDisposition
SwitchBarSwitchChangedCallback(
    _In_ DMFMODULE DmfModuleDefaultTarget,
    _In_reads_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _In_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Continuous reader has received a buffer from the underlying target (OSR FX2) driver.
    This function runs at PASSIVE_LEVEL because the Module was configured to do so!

Arguments:

    DmfModuleDefaultTarget - The Child Module from which this callback is called (DMF_ContinuousRequestTarget).
    OutputBuffer - It is the data switch data returned from OSR FX2 board.
    OutputBufferSize - Size of switch data returned from OSR FX2 board (UCHAR).
    ClientBufferContextOutput - Not used.
    CompletionStatus - OSR FX2 driver's result code for that IOCTL.

Return Value:

    None

--*/
{
    ContinuousRequestTarget_BufferDisposition returnValue;

    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(ClientBufferContextOutput);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! CompletionStatus=%!STATUS!", CompletionStatus);

    if (!NT_SUCCESS(CompletionStatus))
    {
        // This will happen when OSR FX2 board is unplugged.
        //
        returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! Streaming: stop");
        goto Exit;
    }

    // Read switches and set lights.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(DmfModuleDefaultTarget);

    // Continue streaming this IOCTL.
    //
    returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! Streaming: continue");

Exit:

    return returnValue;
}
```
Next, is the Client driver's `EvtDeviceD0Entry()` callback. The Client driver needs to support this callback in order to set the state of the lightbar
correctly when the driver starts (and before the user has changed a switch).

This is an example of how DMF calls the Client driver's callback after it has dispatched this callback to all the instantiated Modules.

Note how the Client driver retrieves the DMF_ContinuousRequestTarget Module instance stored in its device context.

Unlike previous samples, where the Parent Module of `DMF_ContinuousRequestTarget` started and stopped streaming automatically, this sample must start streaming when the device powers up and must stop streaming when the device powers down.
Thus both `EvtDeviceD0Entry` and `EvtDeviceD0Exit` are handled by this driver and are shown here:
```
NTSTATUS
SwitchBarEvtDeviceD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    When the device powers up, read the switches and set the lightbar appropriately.
 
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

    // Read the state of switches and initialize lightbar.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(deviceContext->DmfModuleContinuousRequestTarget);

    // Start streaming.
    //
    ntStatus = DMF_ContinuousRequestTarget_Start(deviceContext->DmfModuleContinuousRequestTarget);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}

NTSTATUS
SwitchBarEvtDeviceD0Exit(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    When the device powers down, stop streaming.
 
Arguments:

    Device - Handle to a framework device object.
    TargetState - The power state the device is entering.

Return Value:

    NTSTATUS

--*/
{
    DEVICE_CONTEXT* deviceContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(TargetState);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Stop streaming.
    //
    DMF_ContinuousRequestTarget_StopAndWait(deviceContext->DmfModuleContinuousRequestTarget);

    ntStatus = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}
```

Testing the driver
==================

1. Plug in the OSR FX2 board.
2. **IMPORTANT: Make sure that file, osrusbfx2dm3.sys, is in the same location as this sample .sys and .inf when you install this sample. (The INF file references that file.)** 
3. Install this sample. (The .inf file will install both the function and filter driver together.)
4. When you install the sample driver, the light bar should match with the switches. As you change the switches, the light bar should change accordingly.
5. When you run the application that controls the board, it will perform all the functions except it will not set the lightbar.
6. IMPORTANT: If the display on the board shows 5 or S, it means the board is in Idle state (sleep). Press the button to wake it up.

