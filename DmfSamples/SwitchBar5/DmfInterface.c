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

