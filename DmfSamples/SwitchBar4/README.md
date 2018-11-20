SwitchBar4 Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 7)
========================================================================
This sample shows how to write a filter driver using DMF. This sample is similar to SwitchBar3. This sample,
however, uses the DMF_IoctlHandler Module to filter IOCTLs.

Because this sample is a filter, it is able to see all the IOCTLs from the application. In this case,
since the sample is setting the lightbar itself based on the status of the switches, it will allow
the application to perform all its functions, except it will prevent the application from changing
the lightbar. It does so by allowing all the IOCTLs to pass down the stack, except for the IOCTL 
that sets the lightbar.

This sample shows how the Client driver can implement its own "DeviceAdd" function to create a
device context, and importantly, indicate that the Client driver is filter driver to both WDF and DMF.

This sample shows new ideas:

1. When a Client Driver is a filter driver, DMF will automatically pass down any `WDFREQUEST` that is
sent to the Client Driver when the `WDFREQUEST` is not handled by any Module or the Client driver.
2. The use of `DMF_IoctlHandler` Module to easily pass down any `WDFREQUEST` that the driver is not interested in
affecting.
3. The use of `DMF_IoctlHandler` to easily modify the behavior of a selected `WDFREQUEST`.
4. Since the Client Driver uses `Dmf_IoctlHandler` to process IOCTLs from the application, it does not need to
create a default `WDFIOQUEUE`. It does not need to support the `EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL` callback. 
Instead, it just tells `DMF_IoctlHandler` Module what callbacks to call for each supported IOCTL.
5. This sample also shows how the Client Driver creates a second Module. Note how only a single `DMF_MODULE_ATTRIBUTES`
structure is used to instantiate all the Modules. Each Module may have a specific CONFIG, but there is always
a single instance of `DMF_MODULE_ATTRIBUTES` in the `DmfModulesAdd()` callback.
6. This sample also shows how to allow access to IOCTLs exposed by a function driver to processes running "As Administrator". This is done by setting a flag in the `DMF_IoctlHandler` Config._

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

    SwitchBar4 Sample: Loads as a filter driver on OSRFX2 driver. When it does, reads changes to 
                       switch state and sets lightbar on the board to match switch settings. This 
                       sample uses DMF_IoctlHandler instead of directly handling the IOCTLs itself 
                       as SwitchBar3 did.

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

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        SwitchBarEvtDriverContextCleanup,
                        SwitchBarEvtDeviceAdd)
#pragma code_seg()

typedef struct
{
    // Allows this driver to send requests to the next driver down the stack.
    //
    DMFMODULE DmfModuleDefaultTarget;
    // Automatically forwards all unhandled IOCTLs down the stack.
    //
    DMFMODULE DmfModuleIoctlHandler;
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

// Forward declarations of DMF callbacks.
//
EVT_DMF_IoctlHandler_Callback SwitchBarDeviceControl_IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY;
EVT_DMF_ContinuousRequestTarget_BufferOutput SwitchBarSwitchChangedCallback;

// All IOCTLs are automatically forwarded down the stack except for those in this table.
//
IoctlHandler_IoctlRecord SwitchBar_IoctlHandlerTable[] =
{
    { (LONG)IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY, sizeof(BAR_GRAPH_STATE), 0, SwitchBarDeviceControl_IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY, FALSE },
};

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
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

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

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.IoctlRecords = SwitchBar_IoctlHandlerTable;
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(SwitchBar_IoctlHandlerTable);
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeFilterAdministratorOnly;
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &deviceContext->DmfModuleIoctlHandler);
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
    ntStatus = DMF_DefaultTarget_SendSynchronously(DmfModuleDefaultTarget,
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

    DmfModuleDefaultTarget - The Child Module from which this callback is called (DMF_DefaultTarget).
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
    SwitchBarReadSwitchesAndUpdateLightBar(DmfModuleDefaultTarget);

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
    SwitchBarReadSwitchesAndUpdateLightBar(deviceContext->DmfModuleDefaultTarget);

    ntStatus = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS
SwitchBarDeviceControl_IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    When an application sends IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY, don't send it down the stack so that
    the application is prevented from setting the state of the lightbar. Just tell DMF to complete the
    request with STATUS_SUCCESS.

Arguments:

    DmfModule - DMF_IoctlHandler Module handle.
    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.
    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.
    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    STATUS_PENDING - DMF does not complete the WDFREQUEST and Client Driver owns the WDFREQUEST.
    Other - DMF completes the WDFREQUEST with that NTSTATUS.

--*/
{
    
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(IoControlCode);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    // Tell application this driver read the input buffer.
    //
    *BytesReturned = InputBufferSize;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY filtered out");

    // Causes DMF to complete the WDFREQUEST.
    //
    return STATUS_SUCCESS;
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
These are the forward declarations of the functions for DriverEntry, DeviceAdd, DeviceD0Entry, DriverContextCleanup and DmfDeviceModulesAdd:
(Note that DeviceIoControl is not listed because the Client Driver uses DMF_IoctlHandler to do most of the processing of each IOCTL.)
```
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD SwitchBarEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP SwitchBarEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;
EVT_WDF_DEVICE_D0_ENTRY SwitchBarEvtDeviceD0Entry;
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
(Note that this driver does not create a default WDFIOQUEUE. Again, since this driver does not directly receive IOCTLS, such a queue is
not necessary. DMF creates that WDFIOQUEUE on behalf of the Client Driver so that DMF_IoctlHandler will work.)
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

Note the call to:
```
    DMF_DmfFdoSetFilter(dmfDeviceInit);
```
If the Client driver is a filter driver, it must tell DMF that is a filter driver using this call. Doing so, will tell DMF to automatically forward all unsupported WDFREQUESTS to the next driver in the stack.

Next is the DMF callback that instantiates Modules. In this driver, as in SwitchBar3, the `DMF_DefaultTarget` Module is instantiated. Its
continuous request target is configured so that the driver will receive an "interrupt" every time the switch on the board is changed. There
is no change from SwtichBar3 with regard to that. (See SwitchBar3 sample's documentation for more information about how to configure the
continuous request target.)

However, a second Module, `DMF_IoctlHandler`, is instantiated. In this case, the Client Driver is only interested in affecting the behavior
of a single IOCTL that will be sent by the application, `IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY`. The Client Driver tells DMF_IoctlHandler
each IOCTL it is interested in receiving using the specified table. When `DMF_IoctlHandler` sees that IOCTL it will call the Client Driver's
callback, `SwitchBarDeviceControl_IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY'. Since the Client Driver is a filter driver, DMF
will automatically pass down all other IOCTLs to the next driver in the stack.

One other note: This driver uses `IoctlHandler_AccessModeFilterAdministratorOnly` to ensure that only processes running "As Administrator" can open the device and send IOCTL requests.
It is also possible to do so on a per-IOCTL basis. This is a feature of `DMF_IoctlHandler`, not DMF itself.
_
**Important: Multiple Modules are instantiated. However there is only a single instance of DMF_MODULE_ATTRIBUTES that must be reused for every 
Module that is instantiated. It contains the list of all the Modules that DMF needs to create.**
```
// Forward declarations of DMF callbacks.
//
EVT_DMF_IoctlHandler_Callback SwitchBarDeviceControl_IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY;
EVT_DMF_ContinuousRequestTarget_BufferOutput SwitchBarSwitchChangedCallback;

// All IOCTLs are automatically forwarded down the stack except for those in this table.
//
IoctlHandler_IoctlRecord SwitchBar_IoctlHandlerTable[] =
{
    { (LONG)IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY, sizeof(BAR_GRAPH_STATE), 0, SwitchBarDeviceControl_IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY, FALSE },
};

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
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

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

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.IoctlRecords = SwitchBar_IoctlHandlerTable;
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(SwitchBar_IoctlHandlerTable);
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeFilterAdministratorOnly;
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &deviceContext->DmfModuleIoctlHandler);
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
    ntStatus = DMF_DefaultTarget_SendSynchronously(DmfModuleDefaultTarget,
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
This function is a callback received from the `DMF_DefaultTarget` Module. It is received whenever the `IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE` has returned
from the OSR FX2 driver indicating that the switches on the board have changed. The Module has already extracted the buffers from the underlying `WDFREQUEST` it sent.
In this case, the data returned by the IOCTL is in OutputBuffer. (Note that this sample does not use the buffer. Instead if reads the switch data directly.)

Ordinarily, completion routines from the USB stack run at `DISPATCH_LEVEL`. The completion routine that executes when the continuous requests from the 
from the `DMF_DefaultTarget` Module executes at `DISPATCH_LEVEL` by default. However, this sample reads the switch state synchronously, so it must
run at `PASSIVE_LEVEL`. Also, the underlying function driver expects that its IOCTL requests arrive at `PASSIVE_LEVEL`. For these reasons, the 
`DMF_DefaultTarget` Module is configured to run at `PASSIVE_LEVEL` by simply setting that option when the Module is configured:

```
    moduleAttributes.PassiveLevel = TRUE;
```

Finally, note the return value of this function. It tells the caller (`DMF_DefaultTarget` Module) who owns the OutputBuffer and whether or not to 
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

    DmfModuleDefaultTarget - The Child Module from which this callback is called (DMF_DefaultTarget).
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
The final function in this driver is the function that prevents an application from sending `IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY`
to the underlying function driver. The Client Driver filters out that IOCTL by simply completing its associated request with `STATUS_SUCCESS`.
Any other IOCTL that comes through this filter driver is passed down to the next driver in the stack automatically by DMF.
Compared to the previous sample, this code is simpler and smaller.
```
#pragma code_seg("PAGE")
NTSTATUS
SwitchBarDeviceControl_IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    When an application sends IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY, don't send it down the stack so that
    the application is prevented from setting the state of the lightbar. Just tell DMF to complete the
    request with STATUS_SUCCESS.

Arguments:

    DmfModule - DMF_IoctlHandler Module handle.
    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.
    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.
    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    STATUS_PENDING - DMF does not complete the WDFREQUEST and Client Driver owns the WDFREQUEST.
    Other - DMF completes the WDFREQUEST with that NTSTATUS.

--*/
{
    
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(IoControlCode);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    // Tell application this driver read the input buffer.
    //
    *BytesReturned = InputBufferSize;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "%!FUNC! IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY filtered out");

    // Causes DMF to complete the WDFREQUEST.
    //
    return STATUS_SUCCESS;
}
#pragma code_seg()
```

Testing the driver
==================

1. Plug in the OSR FX2 board.
2. **IMPORTANT: Make sure that file, osrusbfx2dm3.sys, is in the same location as this sample .sys and .inf when you install this sample. (The INF file references that file.)** 
3. Install this sample. (The .inf file will install both the function and filter driver together.)
4. When you install the sample driver, the light bar should match with the switches. As you change the switches, the light bar should change accordingly.
5. When you run the application that controls the board, it will perform all the functions except it will not set the lightbar.
6. IMPORTANT: If the display on the board shows 5 or S, it means the board is in Idle state (sleep). Press the button to wake it up.
7. NOTE: The Application will only run with this filter driver installed if it is running "As Administrator".

