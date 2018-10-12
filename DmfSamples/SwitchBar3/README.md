SwitchBar3 Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 6)
========================================================================
This sample shows how to write a filter driver using DMF. This sample is similar to SwitchBar2. However,
in this sample the driver is loaded as part of the stack so it is not necessary for the sample to wait
for the arrival of the underlying driver.

Because this sample is a filter, it is able to see all the IOCTLs from the application. In this case,
since the sample is setting the lightbar itself based on the status of the switches, it will allow
the application to perform all its functions, except it will prevent the application from changing
the lightbar. It does so by allowing all the IOCTLs to pass down the stack, except for the IOCTL 
that sets the lightbar.

This sample shows how the Client driver can implement its own "DeviceAdd" function to create a
device context, and importantly, indicate that the Client driver is filter driver to both WDF and DMF.

This sample shows two new ideas:

1. A Client driver that does not use DMF's DeviceAdd function. Instead it implements its own function so that it can do three things:
    a. Receive the EvtDeviceD0Entry callback from WDF. It needs this callback to initialize the lightbar when the driver starts.
    b. Receive the EvtDeviceIoControl callback from WDF. It needs this callback to intercept the IOCTLS as they come from the application so that the application is prevented from setting the state of the lightbar.
    c. Set itself up as a filter driver. In this case it is not necessary for this driver to wait for the underlying function driver to appear.

3. This driver has its own Device Context (`DEVICE_CONTEXT`). There it stores a handle to the DMFMODULE that allows the driver to talk to the underlying function driver.

2. Use of `DMF_DefaultTarget`. Since this sample does not need to create a Remote Target using a GUID it uses the `DMF_DefaultTarget Module` to talk to the underlying function driver. This handle is stored in the Client drivers' Device Context.

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

    SwitchBar3 Sample: Loads as a filter driver on OSRFX2 driver. When it does, reads changes to 
                       switch state and sets lightbar on the board to match switch settings. This 
                       sample sets up a default WDFIOQUEUE so it can examine all the IOCTLs from
                       the application.

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
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SwitchBarModuleDeviceIoControl;

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        SwitchBarEvtDriverContextCleanup,
                        SwitchBarEvtDeviceAdd)
#pragma code_seg()

typedef struct
{
    // Handle to DMF_DefaultTarget:
    // It supports communication to the next driver down the stack.
    //_
    DMFMODULE DmfModuleDefaultTarget;
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
    PDMFDEVICE_INIT dmfDeviceInit;
    DMF_EVENT_CALLBACKS dmfCallbacks;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE queue;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

    // Tell WDF this callback should be called.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceD0Entry = SwitchBarEvtDeviceD0Entry;

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

    // This is a filter driver that loads on OSRUSBFX2 driver.
    //
    WdfFdoInitSetFilter(DeviceInit);
    // DMF Client drivers that are filter drivers must also make this call.
    //
    DMF_DmfFdoSetFilter(dmfDeviceInit);

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

    // This driver will filter IOCTLS, so set up a default queue.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
                                           WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = SwitchBarModuleDeviceIoControl;

    // When a DMF Client driver creates a default queue, it must also make this call.
    // This call is not needed if the Client driver does not create a default queue.
    //
    DMF_DmfDeviceInitHookQueueConfig(dmfDeviceInit,
                                     &queueConfig);

    ntStatus = WdfIoQueueCreate(device,
                                &queueConfig,
                                &objectAttributes,
                                &queue);
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
    _In_ DMFMODULE DmfModuleDefaultTarget
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
    ntStatus = DMF_DefaultTarget_SendSynchronously(DmfModuleDefaultTarget,
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
    ntStatus = DMF_DefaultTarget_Send(DmfModuleDefaultTarget,
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
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
}
#pragma code_seg()

#pragma code_seg("PAGE")
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
    DMFMODULE* dmfModuleAddressDefaultTarget;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");

    // Get the address where the DMFMODULE is located.
    //
    dmfModuleAddressDefaultTarget = WdfObjectGet_DMFMODULE(Workitem);

    // Read switches and set lights.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(*dmfModuleAddressDefaultTarget);

    WdfObjectDelete(Workitem);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
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

    Continuous reader has received a buffer from the underlying target (OSR FX2) driver.
    This runs in DISPATCH_LEVEL. Since this driver must synchronously read the state
    of the switches, this function just spawns a workitem that runs a PASSIVE_LEVEL.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    OutputBuffer - It is the data switch data returned from OSR FX2 board.
    OutputBufferSize - Size of switch data returned from OSR FX2 board (UCHAR).
    ClientBufferContextOutput - Not used.
    CompletionStatus - OSR FX2 driver's result code for that IOCTL.

Return Value:

    Indicates the owner of the OutputBuffer after this function completes and whether or
    not streaming should stop.

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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "-->%!FUNC! CompletionStatus=%!STATUS!", CompletionStatus);

    if (!NT_SUCCESS(CompletionStatus))
    {
        // This will happen when OSR FX2 board is unplugged: Stop streaming.
        // 
        returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! Streaming: stop");
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
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! Streaming: continue");

Exit:

    return returnValue;
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
    PDEVICE_CONTEXT deviceContext;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_DefaultTarget moduleConfigDefaultTarget;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Dmf_DefaultTarget allows the driver to talk to the next driver in the stack. In this case, it is the driver that
    // is filtered by this filter driver. (SwitchBar2 opened a Remote Target on another stack.)
    //

    // DefaultTarget
    // -------------
    //
    DMF_CONFIG_DefaultTarget_AND_ATTRIBUTES_INIT(&moduleConfigDefaultTarget,
                                                 &moduleAttributes);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(SWITCH_STATE);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = SwitchBarSwitchChangedCallback;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    // In this example, the driver tells the Module to stream automatically so that the driver does not need to 
    // explicitly start/stop streaming.
    //
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
    // OSR driver needs to be called at PASSIVE_LEVEL because its IOCTL handling code path is all paged.
    // Modules look at this attribute they need to execute code in PASSIVE_LEVEL. It is up to Modules to 
    // determine how to use this flag. (In this case DMF_ContinuousRequestTarget will resend requests back to
    // OSR driver at PASSIVE_LEVEL.
    //
    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleDefaultTarget);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
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
    SwitchBarReadSwitchesAndUpdateLightBar(deviceContext->DmfModuleDefaultTarget);

    ntStatus = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
SwitchBarModuleDeviceIoControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Filters IOCTLs that come through the stack. All IOCTLs are passed through except for the IOCTL
    that sets the lightbar.

Arguments:

    DmfModule - This Module's handle.
    Queue - WDF file object to where the Request is sent.
    Request - The Request that contains the IOCTL parameters.
    OutputBufferLength - Output buffer length in the Request.
    InputBufferLength - Input buffer length in the Request.
    IoControlCode - IOCTL of the Request.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "-->%!FUNC!");

    switch (IoControlCode)
    {
        case IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY:
        {
            // Filter out setting the Bar Graph Display.
            // Don't let application know or the application will end.
            //
            WdfRequestComplete(Request,
                               STATUS_SUCCESS);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY filtered out");
            break;
        }
        case IOCTL_OSRUSBFX2_GET_CONFIG_DESCRIPTOR:
        case IOCTL_OSRUSBFX2_RESET_DEVICE:
        case IOCTL_OSRUSBFX2_REENUMERATE_DEVICE:
        case IOCTL_OSRUSBFX2_GET_BAR_GRAPH_DISPLAY:
        case IOCTL_OSRUSBFX2_GET_7_SEGMENT_DISPLAY:
        case IOCTL_OSRUSBFX2_SET_7_SEGMENT_DISPLAY:
        case IOCTL_OSRUSBFX2_READ_SWITCHES:
        case IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE:
        default:
        {
            WDFDEVICE device;
            WDFIOTARGET ioTarget;
            WDF_REQUEST_SEND_OPTIONS sendOptions; 

            device = WdfIoQueueGetDevice(Queue);
            ioTarget = WdfDeviceGetIoTarget(device);

            WdfRequestFormatRequestUsingCurrentType(Request);
            WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                          WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
            if (! WdfRequestSend(Request,
                                 ioTarget,
                                 &sendOptions))
            {
                // This is an error that generally should not happen.
                // It could not be passed down, so just complete it with an error.
                //
                WdfRequestComplete(Request,
                                   STATUS_INVALID_DEVICE_STATE);
            }
            else
            {
                // Request will be completed by the target.
                //
            }
            break;
        }
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
}
#pragma code_seg()
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
These are the forward declarations of the functions for DriverEntry, DeviceAdd, DeviceD0Entry, DeviceIoControl, DriverContextCleanup and DmfDeviceModulesAdd:
```
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD SwitchBarEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP SwitchBarEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;
EVT_WDF_DEVICE_D0_ENTRY SwitchBarEvtDeviceD0Entry;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SwitchBarModuleDeviceIoControl;
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
```
_Use_decl_annotations_
NTSTATUS
SwitchBarEvtDeviceAdd(
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
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE queue;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

    // Tell WDF this callback should be called.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceD0Entry = SwitchBarEvtDeviceD0Entry;

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

    // This is a filter driver that loads on OSRUSBFX2 driver.
    //
    WdfFdoInitSetFilter(DeviceInit);
    // DMF Client drivers that are filter drivers must also make this call.
    //
    DMF_DmfFdoSetFilter(dmfDeviceInit);

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

    // This driver will filter IOCTLS, so set up a default queue.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
                                           WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = SwitchBarModuleDeviceIoControl;

    // When a DMF Client driver creates a default queue, it must also make this call.
    // This call is not needed if the Client driver does not create a default queue.
    //
    DMF_DmfDeviceInitHookQueueConfig(dmfDeviceInit,
                                     &queueConfig);

    ntStatus = WdfIoQueueCreate(device,
                                &queueConfig,
                                &objectAttributes,
                                &queue);
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
```
In the above function note these important calls:
```
    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,
                                                &pnpPowerCallbacks);
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit,
                                           &pnpPowerCallbacks);

    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit,
                                          NULL);

    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit,
                                                   NULL);
```
The above calls are mandatory in all Client drivers that use DMF. These calls allow DMF to know what calls the Client driver wants to receive. After DMF has dispatched WDF callbacks to the instantiated Modules, DMF will then dispatch the calls to the Client driver.
NOTE: Even if the Client driver does not support a set of callbacks, all three functions must be called. (This reduces the chance that the Client driver will forget to include this call later when new callbacks might be supported.)
In this sample, the Client driver supports PnP Power Event callbacks but not File Object or Power Policy callbacks.

Next, note this call:
```
    DMF_DmfDeviceInitHookQueueConfig(dmfDeviceInit,
                                     &queueConfig);
```
Similar to the three calls above, this call allows DMF to know what `WDFIOQUEUE` callbacks the Client wants to receive. Unlike, the above three calls however, this call is not mandatory if the Client driver does not support `WDFIOQUEUE` callbacks.

Note the call to:
```
    DMF_DmfFdoSetFilter(dmfDeviceInit);
```
If the Client driver is a filter driver, it must tell DMF that is a filter driver using this call. Doing so, will tell DMF to automatically forward all unsupported WDFREQUESTS to the next driver in the stack.

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
driver in the stack (which is the target that `DMF_DefaultTarget` opens). Note that the Client only needs to send input/output buffers. The Module will create the corresponding
WDFEREQUEST using those buffers and IOCTL. When, the Client calls this Method, the return value is store in the buffer, switchData.

Next, the data is converted to a value that can be sent to the lightbar.

Next, the Module Method, `DMF_DefaultTarget_Send()`, is used to asynchronously send that value to the lightbar.

Note that using DMF, it is possible to organize drivers so that the business logic is separate from the glue performs the actual work. In this case, the Client driver
does not need to deal with `WDFREQUEST` at all, just simple buffers. In general, Modules are designed that way.

```
#pragma code_seg("PAGE")
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
SwitchBarReadSwitchesAndUpdateLightBar(
    _In_ DMFMODULE DmfModuleDefaultTarget
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
    ntStatus = DMF_DefaultTarget_SendSynchronously(DmfModuleDefaultTarget,
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
    ntStatus = DMF_DefaultTarget_Send(DmfModuleDefaultTarget,
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
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
}
#pragma code_seg()
```

This code is the WDFWORKITEM handler. It is called after being enqueued from the DISPATCH_LEVEL callback received earlier. This callback simply calls the above
function to read switches and set the lightbar, then it deletes the associated WDFWORKITEM.

```
#pragma code_seg("PAGE")
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
    DMFMODULE* dmfModuleAddressDefaultTarget;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");

    // Get the address where the DMFMODULE is located.
    //
    dmfModuleAddressDefaultTarget = WdfObjectGet_DMFMODULE(Workitem);

    // Read switches and set lights.
    //
    SwitchBarReadSwitchesAndUpdateLightBar(*dmfModuleAddressDefaultTarget);

    WdfObjectDelete(Workitem);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
}
#pragma code_seg()
```

This function is a callback received from the `Dmf_DefaultTarget` Module. It is received whenever the `IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE` has returned
from the OSR FX2 driver indicating that the switches on the board have changed. The Module has already extracted the buffers from the underlying `WDFREQUEST` it sent.
In this case, the data returned by the IOCTL is in OutputBuffer. However, this function runs at DISPATCH_LEVEL. In order to perform the above function, it is 
necessary to execute at PASSIVE_LEVEL because it needs to perform a synchronous read. Thus, this function does not actually use OutputBuffer. Instead, it simply
creates and spawns a WDFWORKITEM. 

The WDFWORKITEM callback function needs the `Dmf_DefaultTarget` Module handle. Although the WDFWORKITEM's parent is the `DMFMODULE`, it is not possible to 
retrieve the WDFWORKITEM's parent, so the `DMFMODULE` must be stored in the workitem's context. Thus, this function allocates space in the workitem's 
context for the `DMFMODULE` handle and then writes that handle to the context after the workitem has been created. Finally, it enqueues the WORKITEM. Note the use of
the predefined `WdfObjectGet_DMFMODULE()`.

Finally, note the return value of this function. It tells the caller (`Dmf_DeviceInterfaceTarget` Module) who owns the OutputBuffer and whether or not to 
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

    Continuous reader has received a buffer from the underlying target (OSR FX2) driver.
    This runs in DISPATCH_LEVEL. Since this driver must synchronously read the state
    of the switches, this function just spawns a workitem that runs a PASSIVE_LEVEL.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    OutputBuffer - It is the data switch data returned from OSR FX2 board.
    OutputBufferSize - Size of switch data returned from OSR FX2 board (UCHAR).
    ClientBufferContextOutput - Not used.
    CompletionStatus - OSR FX2 driver's result code for that IOCTL.

Return Value:

    Indicates the owner of the OutputBuffer after this function completes and whether or
    not streaming should stop.

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

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "-->%!FUNC! CompletionStatus=%!STATUS!", CompletionStatus);

    if (!NT_SUCCESS(CompletionStatus))
    {
        // This will happen when OSR FX2 board is unplugged: Stop streaming.
        // 
        returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! Streaming: stop");
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
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! Streaming: continue");

Exit:

    return returnValue;
}
```
The previous sample receives notification that the underlying driver is available because it uses a remote target.
This sample does not need such notification because the driver is in the same stack. Instead the lightbar on the device is initialized during the Client driver's `DeviceD0Entry()` callback.

This is the Modules Add function. In this case, a single Module, `Dmf_DefaultTarget` is instantiated. It receives the following information:

1. The type of `WDFREQUEST` that will be sent to the target. In this case, the driver will send IOCTLs.
2. The IOCTL to send to the target.
3. The number of simultaneous IOCTLs to send to the target.
4. The size, number and pool type of input buffers to create. This corresponds to the values required by the IOCTL. In this case, the IOCTL has no input buffer.
5. The size, number and pool type of output buffers to create. This corresponds to the values required by the IOCTL. In this case, the IOCTL requires a buffer 
of size `sizeof(SWTICH_STATE)` of pool type `NonPagedPoolNx`. `Dmf_DefaultTarget` will create all the buffers as needed. (As an aside, bounds checking is automatically performed on the
buffers when they are used. If buffer overrun occurs is detected, an assert will happen.)
6. The callback function that is called by the Module when the `WDFREQUEST` with IOCTL (from the interrupt pipe) has completed. In this case, the function is: `SwitchBarSwitchChangedCallback`.

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
    DMF_CONFIG_DefaultTarget moduleConfigDefaultTarget;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Dmf_DefaultTarget allows the driver to talk to the next driver in the stack. In this case, it is the driver that
    // is filtered by this filter driver. (SwitchBar2 opened a Remote Target on another stack.)
    //

    // DefaultTarget
    // -------------
    //
    DMF_CONFIG_DefaultTarget_AND_ATTRIBUTES_INIT(&moduleConfigDefaultTarget,
                                                 &moduleAttributes);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferCountOutput = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.BufferOutputSize = sizeof(SWITCH_STATE);
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = SwitchBarSwitchChangedCallback;
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    // In this example, the driver tells the Module to stream automatically so that the driver does not need to 
    // explicitly start/stop streaming.
    //
    moduleConfigDefaultTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
    // OSR driver needs to be called at PASSIVE_LEVEL because its IOCTL handling code path is all paged.
    // Modules look at this attribute they need to execute code in PASSIVE_LEVEL. It is up to Modules to 
    // determine how to use this flag. (In this case DMF_ContinuousRequestTarget will resend requests back to
    // OSR driver at PASSIVE_LEVEL.
    //
    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleDefaultTarget);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
}
```
Next, is the Client driver's `EvtDeviceD0Entry()` callback. The Client driver needs to support this callback in order to set the state of the lightbar
correctly when the driver starts (and before the user has changed a switch).

This is an example of how DMF calls the Client driver's callback after it has dispatched this callback to all the instantiated Modules.

Note how the Client driver retrieves the DMF_DefaultTarget Module instance stored in its device context.
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
    SwitchBarReadSwitchesAndUpdateLightBar(deviceContext->DmfModuleDefaultTarget);

    ntStatus = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}
```
The final function in this driver is the function that filters IOCTLs. All the IOCTLs are passed through except
for the IOCTL that sets the lightbar (because the lightbar should only be set based on the state of the switches).

```
#pragma code_seg("PAGE")
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
SwitchBarModuleDeviceIoControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Filters IOCTLs that come through the stack. All IOCTLs are passed through except for the IOCTL
    that sets the lightbar.

Arguments:

    DmfModule - This Module's handle.
    Queue - WDF file object to where the Request is sent.
    Request - The Request that contains the IOCTL parameters.
    OutputBufferLength - Output buffer length in the Request.
    InputBufferLength - Input buffer length in the Request.
    IoControlCode - IOCTL of the Request.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "-->%!FUNC!");

    switch (IoControlCode)
    {
        case IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY:
        {
            // Filter out setting the Bar Graph Display.
            // Don't let application know or the application will end.
            //
            WdfRequestComplete(Request,
                               STATUS_SUCCESS);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY filtered out");
            break;
        }
        case IOCTL_OSRUSBFX2_GET_CONFIG_DESCRIPTOR:
        case IOCTL_OSRUSBFX2_RESET_DEVICE:
        case IOCTL_OSRUSBFX2_REENUMERATE_DEVICE:
        case IOCTL_OSRUSBFX2_GET_BAR_GRAPH_DISPLAY:
        case IOCTL_OSRUSBFX2_GET_7_SEGMENT_DISPLAY:
        case IOCTL_OSRUSBFX2_SET_7_SEGMENT_DISPLAY:
        case IOCTL_OSRUSBFX2_READ_SWITCHES:
        case IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE:
        default:
        {
            WDFDEVICE device;
            WDFIOTARGET ioTarget;
            WDF_REQUEST_SEND_OPTIONS sendOptions; 

            device = WdfIoQueueGetDevice(Queue);
            ioTarget = WdfDeviceGetIoTarget(device);

            WdfRequestFormatRequestUsingCurrentType(Request);
            WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                          WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
            if (! WdfRequestSend(Request,
                                 ioTarget,
                                 &sendOptions))
            {
                // This is an error that generally should not happen.
                // It could not be passed down, so just complete it with an error.
                //
                WdfRequestComplete(Request,
                                   STATUS_INVALID_DEVICE_STATE);
            }
            else
            {
                // Request will be completed by the target.
                //
            }
            break;
        }
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC!");
}
#pragma code_seg()
```

Testing the driver
==================

1. Plug in the OSR FX2 board.
2. **IMPORTANT: Make sure that file, osrusbfx2dm3.sys, is in the same location as this sample .sys and .inf when you install this sample.** 
3. Install this sample. (The .inf file will install both the function and filter driver together.)
4. When you install the sample driver, the light bar should match with the switches. As you change the switches, the light bar should change accordingly.
5. When you run the application that controls the board, it will perform all the functions except it will not set the lightbar.
6. IMPORTANT: If the display on the board shows 5 or S, it means the board is in Idle state (sleep). Press the button to wake it up.


