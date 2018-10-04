/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    DmfInterface.c

Abstract:


    SwitchBar1 Sample: Waits for the OSR FX2 driver to load. When it does, reads changes to switch state and sets
                       lightbar on the board to match switch settings. This driver opens the underlying function
                       driver as a remote target using DMF_DeviceInterface Module.

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
    In this case, this driver starts the continuous reader and makes sure that the lightbar is
    set correctly per the state of the switches.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    NTSTATUS ntStatus;

    ntStatus = DMF_DeviceInterfaceTarget_StreamStart(DmfModule);
    if (NT_SUCCESS(ntStatus))
    {
        // Do an initial read and write for the current state of the board before
        // any switches have been changed.
        //
        SwitchBarReadSwitchesAndUpdateLightBar(DmfModule);
    }
    ASSERT(NT_SUCCESS(ntStatus));
}

static
VOID
SwitchBar_OnDeviceRemovalNotification(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Device Removal Notification.
    In this case, this driver stops the continuous reader.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    VOID

--*/
{
    DMF_DeviceInterfaceTarget_StreamStop(DmfModule);
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
    moduleConfigDeviceInterfaceTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    
    // These callbacks tell us when the underlying target is available. When it is available, the continuous reader is started
    // and the lightbar on the board is initialized to the current state of the switches.
    // When it is not available, the continuous reader is stopped. 
    //
    DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(&moduleAttributes,
                                               &moduleEventCallbacks);
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPostOpen = SwitchBar_OnDeviceArrivalNotification;
    moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = SwitchBar_OnDeviceRemovalNotification;

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);
}

