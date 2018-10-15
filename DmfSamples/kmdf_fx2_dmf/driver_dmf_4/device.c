/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Device.c

Abstract:

    USB device driver for OSR USB-FX2 Learning Kit (DMF Version)

Environment:

    Kernel mode only


--*/

#include <osrusbfx2.h>

#if defined(EVENT_TRACING)
#include "device.tmh"
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, OsrFxEvtDeviceAdd)
#pragma alloc_text(PAGE, OsrDmfModulesAdd)
#endif


NTSTATUS
OsrFxEvtDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
    )
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of the device. All the software resources
    should be allocated in this callback.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES               attributes;
    NTSTATUS                            status;
    WDFDEVICE                           device;
    WDF_DEVICE_PNP_CAPABILITIES         pnpCaps;
    PDEVICE_CONTEXT                     pDevContext;
    GUID                                activity;
    PDMFDEVICE_INIT                     dmfDeviceInit;
    DMF_EVENT_CALLBACKS                 dmfEventCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,"--> OsrFxEvtDeviceAdd routine\n");

    //
    // DMF: Create the PDMFDEVICE_INIT structure.
    //
    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit, NULL);
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit, NULL);
    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit, NULL);

    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

    //
    // Now specify the size of device extension where we track per device
    // context.DeviceInit is completely initialized. So call the framework
    // to create the device and attach it to the lower stack.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
            "WdfDeviceCreate failed with Status code %!STATUS!\n", status);
        return status;
    }

    //
    // DMF: DMF has Utility functions for common tasks. These do not require Modules.
    // Setup the activity ID so that we can log events using it.
    //
    activity = DMF_Utility_ActivityIdFromDevice(device);

    //
    // Get the DeviceObject context by using accessor function specified in
    // the WDF_DECLARE_CONTEXT_TYPE_WITH_NAME macro for DEVICE_CONTEXT.
    //
    pDevContext = GetDeviceContext(device);

    //
    // DMF: DMF has Utility functions for common tasks. These do not require Modules.
    // Get the device's friendly name and location so that we can use it in
    // error logging.  If this fails then it will setup dummy strings.
    //
    DMF_Utility_EventLoggingNamesGet(device,
                                     &pDevContext->DeviceName,
                                     &pDevContext->Location);

    //
    // Tell the framework to set the SurpriseRemovalOK in the DeviceCaps so
    // that you don't get the popup in usermode when you surprise remove the device.
    //
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
    pnpCaps.SurpriseRemovalOK = WdfTrue;

    WdfDeviceSetPnpCapabilities(device, &pnpCaps);

    //
    // DMF: Initialize the DMF_EVENT_CALLBACKS to set the callback DMF will call
    //      to get the list of Modules to instantiate.
    //
    DMF_EVENT_CALLBACKS_INIT(&dmfEventCallbacks);
    dmfEventCallbacks.EvtDmfDeviceModulesAdd = OsrDmfModulesAdd;

    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,
                                       &dmfEventCallbacks);

    //
    // DMF: Tell DMF to create its data structures and create the tree of Modules the 
    //      Client driver has specified (using the above callback). After this call
    //      succeeds DMF has all the information it needs to dispatch WDF entry points
    //      to the tree of Modules.
    //
    status = DMF_ModulesCreate(device,
                               &dmfDeviceInit);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- OsrFxEvtDeviceAdd\n");

    if (!NT_SUCCESS(status))
    {
        //
        // Log fail to add device to the event log
        //
        EventWriteFailAddDevice(&activity,
                                pDevContext->DeviceName,
                                pDevContext->Location,
                                status);
    }

    return status;
}

//
// DMF: This is the callback function called by DMF that allows this driver (the Client Driver)
//      to set the CONFIG for each DMF Module the driver will use.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
OsrDmfModulesAdd(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++
Routine Description:

    EvtDmfDeviceModulesAdd is called by DMF during the Client driver's 
    AddDevice call from the PnP manager. Here the Client driver declares a
    CONFIG structure for every instance of every Module the Client driver 
    uses. Each CONFIG structure is properly initialized for its specific
    use. Then, each CONFIG structure is added to the list of Modules that
    DMF will instantiate.

Arguments:

    Device - The Client driver's WDFDEVICE.
    DmfModuleInit - An opaque PDMFMODULE_INIT used by DMF.

Return Value:

    None

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_OsrFx2 moduleConfigOsrFx2;
    DMF_CONFIG_Pdo moduleConfigPdo;
    DEVICE_CONTEXT* pDevContext;

    pDevContext = GetDeviceContext(Device);

    // OsrFx2
    // ------
    //
    // DMF: Instantiate the DMF_OsrFx2 Module. This Module does most of the work to support the OSR FX2 board.
    //      However, it allows the Client (this driver) to receive notification when the Read Interrupt Pipe
    //      transfers data. This callback will become useful in the next sample. It shows how the same code
    //      can be shared in both samples 3 and 4 using this callback. How Modules callback into Clients is
    //      up to the implementor of the Module.
    //
    DMF_CONFIG_OsrFx2_AND_ATTRIBUTES_INIT(&moduleConfigOsrFx2,
                                          &moduleAttributes);
    moduleConfigOsrFx2.InterruptPipeCallback = OsrFx2InterruptPipeCallback;
    moduleConfigOsrFx2.EventWriteCallback = OsrFx2_EventWriteCallback;
    moduleConfigOsrFx2.Settings = (OsrFx2_Settings_NoEnterIdle | 
                                   OsrFx2_Settings_NoDeviceInterface);
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &pDevContext->DmfModuleOsrFx2);

    // Pdo
    // ---
    //
    DMF_CONFIG_Pdo_AND_ATTRIBUTES_INIT(&moduleConfigPdo,
                                       &moduleAttributes);
    moduleConfigPdo.InstanceIdFormatString = L"SwitchBit=%d";
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &pDevContext->DmfModulePdo);

    // QueuedWorkitem
    // --------------
    //
    DMF_CONFIG_QueuedWorkItem moduleConfigQueuedWorkitem;
    DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT(&moduleConfigQueuedWorkitem,
                                                  &moduleAttributes);
    moduleConfigQueuedWorkitem.BufferQueueConfig.SourceSettings.BufferCount = 4;
    moduleConfigQueuedWorkitem.BufferQueueConfig.SourceSettings.BufferSize = sizeof(UCHAR);
    moduleConfigQueuedWorkitem.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigQueuedWorkitem.EvtQueuedWorkitemFunction = OsrFx2QueuedWorkitem;
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &pDevContext->DmfModuleQueuedWorkitem);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
OsrFx2_EventWriteCallback(_In_ DMFMODULE DmfModule,
                          _In_ OsrFx2_EventWriteMessage EventWriteMessage,
                          _In_ ULONG_PTR Parameter1,
                          _In_ ULONG_PTR Parameter2,
                          _In_ ULONG_PTR Parameter3,
                          _In_ ULONG_PTR Parameter4,
                          _In_ ULONG_PTR Parameter5)
/*++
Routine Description:

    Logging callback function from Dmf_OsrFx Module.

Arguments:

    DmfModule - OsrFx2 Module handle.
    EventWriteMessage - Indicates what code path to log.
    Parameter1 - Code path specific parameter.
    Parameter2 - Code path specific parameter.
    Parameter3 - Code path specific parameter.
    Parameter4 - Code path specific parameter.
    Parameter5 - Code path specific parameter.

Return Value:

    None

--*/
{
    WDFDEVICE device;
    DEVICE_CONTEXT* pDevContext;
    GUID activityId;

    UNREFERENCED_PARAMETER(Parameter1);

    device = DMF_ParentDeviceGet(DmfModule);
    pDevContext = GetDeviceContext(device);

    switch (EventWriteMessage)
    {
        case OsrFx2_EventWriteMessage_ReadStart:
        {
            WDFREQUEST request = (WDFREQUEST)Parameter2;
            ULONG length = (ULONG)Parameter3;

            activityId = DMF_Utility_ActivityIdFromRequest(request);
            // 
            // Log read start event, using IRP activity ID if available or request
            // handle otherwise.
            //
            EventWriteReadStart(&activityId, device, length);
            break;
        }
        case OsrFx2_EventWriteMessage_ReadFail:
        {
            WDFREQUEST request = (WDFREQUEST)Parameter2;
            NTSTATUS status = (ULONG)Parameter3;

            activityId = DMF_Utility_ActivityIdFromRequest(request);
            EventWriteReadFail(&activityId, device, status);
            break;
        }
        case OsrFx2_EventWriteMessage_ReadStop:
        {
            WDFREQUEST request = (WDFREQUEST)Parameter2;
            NTSTATUS status = (ULONG)Parameter3;
            USBD_STATUS usbdStatus = (USBD_STATUS)Parameter4;
            ULONG bytesRead = (ULONG)Parameter5;

            activityId = DMF_Utility_ActivityIdFromRequest(request);
            EventWriteReadStop(&activityId, 
                               device,
                               bytesRead, 
                               status, 
                               usbdStatus);
            break;
        }
        case OsrFx2_EventWriteMessage_WriteStart:
        {
            WDFREQUEST request = (WDFREQUEST)Parameter2;
            ULONG length = (ULONG)Parameter3;

            activityId = DMF_Utility_ActivityIdFromRequest(request);
            EventWriteWriteStart(&activityId, device, (ULONG)length);
            break;
        }
        case OsrFx2_EventWriteMessage_WriteFail:
        {
            WDFREQUEST request = (WDFREQUEST)Parameter2;
            NTSTATUS status = (ULONG)Parameter3;

            activityId = DMF_Utility_ActivityIdFromRequest(request);
            EventWriteWriteFail(&activityId, device, status);
            break;
        }
        case OsrFx2_EventWriteMessage_WriteStop:
        {
            WDFREQUEST request = (WDFREQUEST)Parameter2;
            NTSTATUS status = (ULONG)Parameter3;
            USBD_STATUS usbdStatus = (USBD_STATUS)Parameter4;
            ULONG bytesRead = (ULONG)Parameter5;

            activityId = DMF_Utility_ActivityIdFromRequest(request);
            EventWriteWriteStop(&activityId, 
                               device,
                               bytesRead, 
                               status, 
                               usbdStatus);
            break;
        }
        case OsrFx2_EventWriteMessage_SelectConfigFailure:
        {
            NTSTATUS status = (ULONG)Parameter3;

            activityId = DMF_Utility_ActivityIdFromDevice(device);
            EventWriteSelectConfigFailure(&activityId,
                                          pDevContext->DeviceName,
                                          pDevContext->Location,
                                          status);
            break;
        }
        case OsrFx2_EventWriteMessage_DeviceReenumerated:
        {
            NTSTATUS status = (ULONG)Parameter3;

            activityId = DMF_Utility_ActivityIdFromDevice(WdfObjectContextGetObject(pDevContext));
            EventWriteDeviceReenumerated(&activityId,
                                         pDevContext->DeviceName,
                                         pDevContext->Location,
                                         status);
            break;
        }
        default:
        {
            ASSERT(FALSE);
            break;
        }
    }
}

