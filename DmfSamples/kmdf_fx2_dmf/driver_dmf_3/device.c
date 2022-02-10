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

#include "device.tmh"

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
    WDF_IO_QUEUE_CONFIG                 ioQueueConfig;
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
    // DMF: DMF always creates a default queue, so is not necessary for the Client driver to create it.
    //      Since this driver will not create a default queue it is not necessary to call 
    //      DMF_DmfDeviceInitHookQueueConfig.
    //

    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchManual);

    //
    // This queue is used for requests that don't directly access the device. The
    // requests in this queue are serviced only when the device is in a fully
    // powered state and sends an interrupt. So we can use a non-power managed
    // queue to park the requests since we don't care whether the device is idle
    // or fully powered up.
    //
    ioQueueConfig.PowerManaged = WdfFalse;

    status = WdfIoQueueCreate(device,
                              &ioQueueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              &pDevContext->InterruptMsgQueue
                              );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
            "WdfIoQueueCreate failed 0x%x\n", status);
        goto Error;
    }

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

    return status;

Error:

    //
    // Log fail to add device to the event log
    //
    EventWriteFailAddDevice(&activity,
                            pDevContext->DeviceName,
                            pDevContext->Location,
                            status);

    return status;
}

//
// DMF: Sometimes Modules require static data passed via their CONFIG. Dmf_IoctlHandler is such a Module.
// This Module requires a table of the IOCTLs that the Client driver handles. Each record contains the
// minimum sizes of the IOCTLs input/output buffers, as well as a callback that handles that IOCTL when
// it is received. Using that information Dmf_IoctlHandler will validate the input/output buffers sizes
// for each IOCTL in the table. If the sizes are correct, the corresponding callback is called.
//
IoctlHandler_IoctlRecord OsrFx2_IoctlHandlerTable[] =
{
    { (LONG)IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE, 0, 0, OsrFxIoDeviceControl, FALSE },
};

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
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;
    DEVICE_CONTEXT* pDevContext;

    PAGED_CODE();

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

    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &pDevContext->DmfModuleOsrFx2);

    // IoctlHandler
    // ------------
    //
    // DMF: Although the DMF_OsrFx2 Module handles most of the IOCTLs, for demonstration purposes and to 
    //      support the OsrFx2InterruptPipeCallback, one IOCTL is handled directly by the Client. Thus,
    //      IOCTLs first go to the Dmf_OsrFx2 Module. If they are not handled there, then they are handled
    //      here by this Client.
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.IoctlRecords = OsrFx2_IoctlHandlerTable;
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(OsrFx2_IoctlHandlerTable);
    moduleConfigIoctlHandler.DeviceInterfaceGuid = GUID_DEVINTERFACE_OSRUSBFX2;
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
    moduleConfigIoctlHandler.CustomCapabilities = L"microsoft.hsaTestCustomCapability_q536wpkpf5cy2\0";
    moduleConfigIoctlHandler.IsRestricted = DEVPROP_TRUE;
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &pDevContext->DmfModuleIoctlHandler);
}

_Use_decl_annotations_
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
