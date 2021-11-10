/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_UdeClient.c

Abstract:

    UdeClient that is used to create additional DMF Modules from scratch.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_UdeClient.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_UdeClient
{
    // Handles IOCTLs for Host Controller Interface.
    //
    DMFMODULE DmfModuleIoctlHandler;
} DMF_CONTEXT_UdeClient;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(UdeClient)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(UdeClient)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This context associated for UdeCxUsbController.
// This is bound at ModuleOpen.
//
typedef struct _CONTEXT_UdeClient_UsbController
{
    // This Module's handle.
    //
    DMFMODULE DmfModule;
    // UDECX Usb Device handle (created during Modules' Open)
    //
    UDECXUSBDEVICE UdecxUsbDevice;
} CONTEXT_UdeClient_UsbController;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTEXT_UdeClient_UsbController, UdeClientControllerContextGet)

// This context associated for UdecxUsbDevice.
//
typedef struct _CONTEXT_UdeClient_UsbDevice
{
    // This Module's handle.
    //
    DMFMODULE DmfModule;
    // Speed of Usb Device.
    //
    UDECX_USB_DEVICE_SPEED UsbDeviceSpeed;
} CONTEXT_UdeClient_UsbDevice;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTEXT_UdeClient_UsbDevice, UdeClientDeviceContextGet)

// This context associated for UdeCxEndpoint.
//
typedef struct _CONTEXT_UdeClient_Endpoint
{
    // This Module's handle.
    //
    DMFMODULE DmfModule;
    // Configuration for this endpoint.
    //
    UdeClient_CONFIG_Endpoint EndpointConfig;
    // Device endpoint is attached to.
    //
    UDECXUSBDEVICE UdecxUsbDevice;
} CONTEXT_UdeClient_Endpoint;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTEXT_UdeClient_Endpoint, UdeClientEndpointContextGet)

// This context associated for EndpointQueue.
//
typedef struct _CONTEXT_UdeClient_EndpointQyeue
{
    // This Module's handle.
    //
    DMFMODULE DmfModule;
    // UDECX Endpoint handle associated with this queue.
    //
    UDECXUSBENDPOINT Endpoint;
} CONTEXT_UdeClient_EndpointQueue;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTEXT_UdeClient_EndpointQueue, UdeClientEndpointQueueContextGet)

// This context contains device specific information for optional retrieval.
//
typedef struct _CONTEXT_UdeDeviceInformation
{
    // Port type device is plugged into if PlugInPortNumber is not zero.
    //
    UDE_CLIENT_PLUGIN_PORT_TYPE PlugInPortType;
    // Port device is plugged into. Zero if not plugged in.
    //
    ULONG PlugInPortNumber;
} CONTEXT_UdeDeviceInformation;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTEXT_UdeDeviceInformation, UdeClient_UdeDeviceInformation)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
UdeClient_EvtIoControl(
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

    Handle Request coming on GUID_DEVINTERFACE_USB_HOST_CONTROLLER.

Arguments:

    DmfModule - The Child Module (Ioctl Handler) from which this callback is called.

    Queue - The WDFQUEUE associated with Request.

    Request - Request data.

    IoctlCode - IOCTL that has been validated to be supported by this Module.

    InputBuffer - Input data buffer.

    InputBufferSize - Input data buffer size.

    OutputBuffer - Output data buffer.

    OutputBufferSize - Output data buffer size.
    
    BytesReturned - Amount of data to be sent back.

Returns:

    STATUS_PENDING - This Module owns the given Request. It will not be completed by the Child Module. This
                     Module must complete the request eventually.
    Any other NTSTATUS - The given request will be completed with this NTSTATUS.

--*/
{
    NTSTATUS ntStatus;
    BOOLEAN handled;
    WDFDEVICE device;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    UNREFERENCED_PARAMETER(IoControlCode);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(OutputBuffer);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "UdeClient_EvtIoControl Request 0x%p Queue 0x%p", Request, Queue);

    *BytesReturned = 0;
    device = WdfIoQueueGetDevice(Queue);
    handled = UdecxWdfDeviceTryHandleUserIoctl(device,
                                               Request);
    if (!handled) 
    {
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxWdfDeviceTryHandleUserIoctl fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // When UdecxWdfDeviceTryHandleUserIoctl returns TRUE, the request is already completed.
    // Return STATUS_PENDING so that the IOCTL handler would not do anything further with this request.
    //
    ntStatus = STATUS_PENDING;

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
NTSTATUS
UdeClient_EvtDeviceQueryUsbCapability(
    _In_ WDFDEVICE UdecxWdfDevice,
    _In_ GUID* CapabilityType,
    _In_ ULONG OutputBufferLength,
    _Out_writes_to_opt_(OutputBufferLength, *ResultLength) PVOID OutputBuffer,
    _Out_ ULONG* ResultLength
    )
/*++

Routine Description:

    Callback for querying the controller capability.

Arguments:

    UdecxWdfDevice - Controller device.

    CapabilityType - GUID of supported USB capability.

    OutputBufferLength - Length of buffer in bytes.

    OutputBuffer - Buffer pointer.

    ResultLength - Result output buffer length.

Return Value:

    NTSTATUS

--*/
{
    DMFMODULE dmfModule;
    DMF_CONFIG_UdeClient* moduleConfig;
    CONTEXT_UdeClient_UsbController* controllerContext;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "UdeClient_EvtDeviceQueryUsbCapability Controller 0x%p GUID 0x%p", UdecxWdfDevice, CapabilityType);

    controllerContext = UdeClientControllerContextGet(UdecxWdfDevice);
    dmfModule = controllerContext->DmfModule;
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    // EvtControllerQueryUsbCapability is mandatory.
    //
    return moduleConfig->UsbControllerConfig.EvtControllerQueryUsbCapability(dmfModule,
                                                                             UdecxWdfDevice,
                                                                             CapabilityType,
                                                                             OutputBufferLength,
                                                                             OutputBuffer,
                                                                             ResultLength);

}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID
UdeClient_EvtControllerReset(
    _In_ WDFDEVICE UdecxWdfDevice
    )
/*++

Routine Description:

    Callback for reseting the controller.

Arguments:

    UdecxWdfDevice - Controller device.

Return Value:

    NTSTATUS

--*/
{
    DMFMODULE dmfModule;
    DMF_CONFIG_UdeClient* moduleConfig;
    CONTEXT_UdeClient_UsbController* controllerContext;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "UdeClient_EvtControllerReset Controller 0x%p", UdecxWdfDevice);

    controllerContext = UdeClientControllerContextGet(UdecxWdfDevice);
    dmfModule = controllerContext->DmfModule;
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    // EvtControllerReset is mandatory.
    //
    moduleConfig->UsbControllerConfig.EvtControllerReset(dmfModule,
                                                         UdecxWdfDevice);

}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID
UdeClient_EvtEndpointReset(
    _In_ UDECXUSBENDPOINT Endpoint,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Callback for reseting an endpoint on a Usb Device.

Arguments:

    Endpoint - Endpoint to be reset.

    Request - Request that represent the request to reset the endpoint.

Return Value:

    None.

--*/
{
    DMFMODULE dmfModule;
    UdeClient_CONFIG_Endpoint* endpointConfig;
    CONTEXT_UdeClient_Endpoint* endpointContext;

    PAGED_CODE();

    endpointContext = UdeClientEndpointContextGet(Endpoint);
    dmfModule = endpointContext->DmfModule;
    endpointConfig = &endpointContext->EndpointConfig;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "UdeClient_EvtEndpointReset Endpoint 0x%p Request 0x%p", Endpoint, Request);

    // EvtEndpointReset is mandatory.
    //
    endpointConfig->EvtEndpointReset(dmfModule,
                                      Endpoint,
                                      Request);
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID
UdeClient_EvtEndpointDeviceIoControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Callback for DeviceIoControl on an endpoint queue.

Arguments:

    Queue - Handle to the framework queue object that is associated with the I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Length of the request's output buffer, if an output buffer is available.

    InputBufferLength - Length of the request's input buffer, if an input buffer is available.

    IoControlCode - The driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    UdeClient_CONFIG_Endpoint* endpointConfig;
    CONTEXT_UdeClient_EndpointQueue* queueContext;
    CONTEXT_UdeClient_Endpoint* endpointContext;
    UDECXUSBENDPOINT endpoint;

    PAGED_CODE();

    queueContext = UdeClientEndpointQueueContextGet(Queue);
    dmfModule = queueContext->DmfModule;
    endpoint = queueContext->Endpoint;
    endpointContext = UdeClientEndpointContextGet(endpoint);
    endpointConfig = &endpointContext->EndpointConfig;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "UdeClient_EvtEndpointDeviceIoControl Queue 0x%p Endpoint 0x%p", Queue, endpoint);

    endpointConfig->EvtEndpointDeviceIoControl(dmfModule,
                                               endpoint,
                                               Queue,
                                               Request,
                                               OutputBufferLength,
                                               InputBufferLength,
                                               IoControlCode);
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID
UdeClient_EvtEndpointStart(
    _In_ UDECXUSBENDPOINT Endpoint
    )
/*++

Routine Description:

    Callback for starting an endpoint on a Usb Device.

Arguments:

    Endpoint - Endpoint to be started.

Return Value:

    None.

--*/
{
    DMFMODULE dmfModule;
    UdeClient_CONFIG_Endpoint* endpointConfig;
    CONTEXT_UdeClient_Endpoint* endpointContext;

    PAGED_CODE();

    endpointContext = UdeClientEndpointContextGet(Endpoint);
    dmfModule = endpointContext->DmfModule;
    endpointConfig = &endpointContext->EndpointConfig;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "UdeClient_EvtEndpointStart Endpoint 0x%p", Endpoint);

    // NOTE: EvtEndpointStart is optional. 
    // Since a call is being made to this function, it is assured that client has set EvtEndpointStart.
    //
    endpointConfig->EvtEndpointStart(dmfModule,
                                     Endpoint);
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID
UdeClient_EvtEndpointPurge(
    _In_ UDECXUSBENDPOINT Endpoint
    )
/*++

Routine Description:

    Callback for purging an endpoint on a Usb Device.

Arguments:

    Endpoint - Endpoint to be started.

Return Value:

    None.

--*/
{
    DMFMODULE dmfModule;
    UdeClient_CONFIG_Endpoint* endpointConfig;
    CONTEXT_UdeClient_Endpoint* endpointContext;

    PAGED_CODE();

    endpointContext = UdeClientEndpointContextGet(Endpoint);
    dmfModule = endpointContext->DmfModule;
    endpointConfig = &endpointContext->EndpointConfig;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "UdeClient_EvtEndpointPurge Endpoint 0x%p", Endpoint);

    // NOTE: EvtEndpointPurge is optional. 
    // Since a call is being made to this function, it is assured that client has set EvtEndpointPurge.
    //
    endpointConfig->EvtEndpointPurge(dmfModule,
                                     Endpoint);
}
#pragma code_seg()

EVT_WDF_IO_QUEUE_STATE Udeclient_EvtWdfIoQueueState;

_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
Udeclient_EvtWdfIoQueueState(
    _In_ WDFQUEUE Queue,
    _In_ WDFCONTEXT Context
    )
/*++

Routine Description:

    Callback so Client can know when data is available in a given
    manual (dispatch type) endpoint queue.

Arguments:

    Queue - The given endpoint queue.
    Client - Context passed to Client.

Return Value:

    None.

--*/
{
    DMFMODULE dmfModule;
    UdeClient_CONFIG_Endpoint* endpointConfig;
    CONTEXT_UdeClient_EndpointQueue* queueContext;
    CONTEXT_UdeClient_Endpoint* endpointContext;
    UDECXUSBENDPOINT endpoint;

    queueContext = UdeClientEndpointQueueContextGet(Queue);
    dmfModule = queueContext->DmfModule;
    endpoint = queueContext->Endpoint;
    endpointContext = UdeClientEndpointContextGet(endpoint);
    endpointConfig = &endpointContext->EndpointConfig;

    DmfAssert(endpointConfig->EndPointReadyContext == Context);
    endpointConfig->EvtEndpointReady(dmfModule,
                                     Queue,
                                     endpoint,
                                     Context);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Udeclient_EvtWdfIoQueueState Queue 0x%p Endpoint 0x%p", Queue, endpoint);

}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
UdeClient_EndpointCreate(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice,
    _In_ PUDECXUSBENDPOINT_INIT* EndpointInit,
    _In_ UdeClient_CONFIG_Endpoint* EndpointConfig,
    _Out_ UDECXUSBENDPOINT* Endpoint
    )
/*++

Routine Description:

    Create an endpoint for a Usb device.

Arguments:

    DmfModule - This Module's handle.
    EndpointInit - Endpoint Init associated with the Usb device.
    EndpointConfig - Endpoint Configuration.
    Endpoint - The newly created Endpoint.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONFIG_UdeClient* moduleConfig;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE endpointQueue;
    UDECXUSBENDPOINT endpoint;
    UDECX_USB_ENDPOINT_CALLBACKS callbacks;
    WDF_OBJECT_ATTRIBUTES queueAttributes;
    CONTEXT_UdeClient_Endpoint* endpointContext;
    CONTEXT_UdeClient_EndpointQueue* queueContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    endpointQueue = NULL;

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, 
                             EndpointConfig->QueueDispatchType);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueAttributes,
                                            CONTEXT_UdeClient_EndpointQueue);
    if (queueConfig.DispatchType < WdfIoQueueDispatchManual)
    {
        // This callback is only relevant for non-Manual queues otherwise WdfIoQueueCreate
        // fails.
        //
        queueConfig.EvtIoInternalDeviceControl = UdeClient_EvtEndpointDeviceIoControl;
    }
    ntStatus = WdfIoQueueCreate(device,
                                &queueConfig,
                                &queueAttributes,
                                &endpointQueue);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoQueueCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    UdecxUsbEndpointInitSetEndpointAddress(*EndpointInit,
                                           EndpointConfig->EndpointAddress);

    if (queueConfig.DispatchType == WdfIoQueueDispatchManual)
    {
        // Manual queue requires this notification so that Client can extract requests.
        //
        DmfAssert(EndpointConfig->EvtEndpointReady != NULL);
        WdfIoQueueReadyNotify(endpointQueue,
                              Udeclient_EvtWdfIoQueueState,
                              EndpointConfig->EndPointReadyContext);
    }

    // EvtEndpointReset is Mandatory.
    //
    DmfAssert(EndpointConfig->EvtEndpointReset != NULL);
    UDECX_USB_ENDPOINT_CALLBACKS_INIT(&callbacks,
                                      UdeClient_EvtEndpointReset);
    if (EndpointConfig->EvtEndpointStart)
    {
        callbacks.EvtUsbEndpointStart = UdeClient_EvtEndpointStart;
    }

    if (EndpointConfig->EvtEndpointPurge)
    {
        callbacks.EvtUsbEndpointPurge = UdeClient_EvtEndpointPurge;
    }

    UdecxUsbEndpointInitSetCallbacks(*EndpointInit, 
                                     &callbacks);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            CONTEXT_UdeClient_Endpoint);
    ntStatus = UdecxUsbEndpointCreate(EndpointInit,
                                      &attributes,
                                      &endpoint);
    if (! NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbEndpointCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    UdecxUsbEndpointSetWdfIoQueue(endpoint,
                                  endpointQueue);

    // Update the endpoint and its queue contexts.
    //
    endpointContext = UdeClientEndpointContextGet(endpoint);
    endpointContext->EndpointConfig = *EndpointConfig;
    endpointContext->DmfModule = DmfModule;
    // Save for retrieval by Method.
    //
    endpointContext->UdecxUsbDevice = UdecxUsbDevice;

    queueContext = UdeClientEndpointQueueContextGet(endpointQueue);
    queueContext->DmfModule = DmfModule;
    queueContext->Endpoint = endpoint;

    endpointQueue = NULL;

    // Return the created endpoint.
    //
    *Endpoint = endpoint;

Exit:

    if (endpointQueue != NULL)
    {
        WdfObjectDelete(endpointQueue);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
UdeClient_SimpleEndpointCreate(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UsbDevice,
    _In_ UdeClient_CONFIG_Endpoint* EndpointConfig,
    _Out_ UDECXUSBENDPOINT* Endpoint
    )
/*++

Routine Description:

    Create a simple endpoint for a Usb device.

Arguments:

    DmfModule - This Module's handle.
    UsbDevice - The Usb device for which the endpoint is being created.
    EndpointConfig - Endpoint Configuration.
    Endpoint - The newly created Endpoint.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    PUDECXUSBENDPOINT_INIT endpointInit;
    endpointInit = UdecxUsbSimpleEndpointInitAllocate(UsbDevice);
    if (endpointInit == NULL) 
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    ntStatus = UdeClient_EndpointCreate(DmfModule,
                                        UsbDevice,
                                        &endpointInit,
                                        EndpointConfig,
                                        Endpoint);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdeClient_EndpointCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    if (endpointInit != NULL) 
    {
        UdecxUsbEndpointInitFree(endpointInit);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
UdeClient_UsbDeviceConfigValidate(
    _In_ DMFMODULE DmfModule,
    _In_ UdeClient_CONFIG_UsbDevice* UsbDeviceConfig
    )
/*++

Routine Description:

    Validate the given Usb Device Config.

Arguments:

    DmfModule - This Module's handle.
    UsbDeviceConfig - The configuration for this Usb Device.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_UdeClient* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (UsbDeviceConfig->UsbDeviceEndpointType == UdecxEndpointTypeDynamic)
    {
        // For Dynamic Endpoint client need to provide the precreate callback to setup EndpointAdd callbacks.
        //
        if (moduleConfig->EvtUsbDevicePreCreate == NULL)
        {
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }
    }
    else if (UsbDeviceConfig->UsbDeviceEndpointType == UdecxEndpointTypeSimple)
    {
        // At least 1 endpoint configuration is needed for Simple EndpointType.
        //
        if (UsbDeviceConfig->SimpleEndpointCount == 0)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Simple Endpoint Type Required at least 1 endpoint");

             ntStatus = STATUS_INVALID_PARAMETER;
             goto Exit;
        }
        else
        {
            if (UsbDeviceConfig->SimpleEndpointConfigs == NULL)
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Missing Endpoint configuration(s)");

                ntStatus = STATUS_INVALID_PARAMETER;
                goto Exit;
            }

            // Make sure EndpointReset Callback is present in each endpoint config.
            //
            for (ULONG endpointIndex = 0; endpointIndex < UsbDeviceConfig->SimpleEndpointCount; ++endpointIndex)
            {
                UdeClient_CONFIG_Endpoint* endpointConfig = &UsbDeviceConfig->SimpleEndpointConfigs[endpointIndex];

                // EndpointReset Callback is mandatory.
                //
                if (endpointConfig->EvtEndpointReset == NULL)
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No Endpoint Reset Callback configured on Endpoint Configuration[%d]", endpointIndex);

                    ntStatus = STATUS_INVALID_PARAMETER;
                    goto Exit;
                }

                // DeviceIoControl Callback is mandatory for dispatch types
                // WdfIoQueueDispatchSequential and WdfIoQueueDispatchParallel.
                //
                if ((endpointConfig->QueueDispatchType < WdfIoQueueDispatchManual) &&
                    (endpointConfig->EvtEndpointDeviceIoControl == NULL))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No Endpoint DeviceIoControl Callback configured on Endpoint Configuration[%d]", endpointIndex);

                    ntStatus = STATUS_INVALID_PARAMETER;
                    goto Exit; 
                }

                // EvtEndpointReady Callback is mandatory for dispatch type WdfIoQueueDispatchManual.
                //
                if ((endpointConfig->QueueDispatchType == WdfIoQueueDispatchManual) &&
                    (endpointConfig->EvtEndpointReady == NULL))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No Endpoint EvtEndpointReady Callback configured on Endpoint Configuration[%d]", endpointIndex);

                    ntStatus = STATUS_INVALID_PARAMETER;
                    goto Exit; 
                }
            }
        }  
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Endpoint Type %d", UsbDeviceConfig->UsbDeviceEndpointType);

        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
UdeClient_CreateUsbDevice(
    _In_ DMFMODULE DmfModule,
    _In_ UdeClient_CONFIG_UsbDevice* UsbDeviceConfig,
    _Out_ UDECXUSBDEVICE* UdecxUsbDevice
    )
/*++

Routine Description:

    Create a Usb Device.

Arguments:

    DmfModule - This Module's handle.
    UsbDeviceConfig - The configuration for this Usb Device.
    UdecxUsbDevice - The created Usb Device.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONFIG_UdeClient* moduleConfig;
    DMF_CONTEXT_UdeClient* moduleContext;
    PUDECXUSBDEVICE_INIT usbDeviceInit;
    UDECXUSBDEVICE usbDevice;
    CONTEXT_UdeClient_UsbController* controllerContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    controllerContext = UdeClientControllerContextGet(device);

    ntStatus = STATUS_SUCCESS;
    usbDeviceInit = NULL;
    usbDevice =  NULL;

    ntStatus = UdeClient_UsbDeviceConfigValidate(DmfModule,
                                                 UsbDeviceConfig);
    if (! NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdeClient_UsbDeviceConfigValidate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    usbDeviceInit = UdecxUsbDeviceInitAllocate(device);
    if (usbDeviceInit == NULL) 
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDeviceInitAllocate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Set required attributes. Client can change these through the preCreate callback.
    //
    UdecxUsbDeviceInitSetSpeed(usbDeviceInit,
                               UsbDeviceConfig->UsbDeviceSpeed);

    UdecxUsbDeviceInitSetEndpointsType(usbDeviceInit,
                                       UsbDeviceConfig->UsbDeviceEndpointType);

    // Let the client define/override options like State Changed Callbacks.
    //
    if (moduleConfig->EvtUsbDevicePreCreate)
    {
        ntStatus = moduleConfig->EvtUsbDevicePreCreate(DmfModule, 
                                                       usbDeviceInit);
        if (! NT_SUCCESS(ntStatus)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtUsbDevicePreCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Add Device descriptor.
    //
    ntStatus = UdecxUsbDeviceInitAddDescriptor(usbDeviceInit,
                                               UsbDeviceConfig->UsbDeviceDescriptor,
                                               UsbDeviceConfig->UsbDeviceDescriptorSize);
    if (! NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDeviceInitAddDescriptor (Device) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (UsbDeviceConfig->UsbBosDescriptorSize != 0)
    {
        // Add BOS descriptor.
        //
        ntStatus = UdecxUsbDeviceInitAddDescriptor(usbDeviceInit,
                                                   UsbDeviceConfig->UsbBosDescriptor,
                                                   UsbDeviceConfig->UsbBosDescriptorSize);
        if (! NT_SUCCESS(ntStatus)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDeviceInitAddDescriptor (BOS) fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Add Config descriptor.
    //
    ntStatus = UdecxUsbDeviceInitAddDescriptor(usbDeviceInit,
                                               UsbDeviceConfig->UsbConfigDescriptor,
                                               UsbDeviceConfig->UsbConfigDescriptorSize);
    if (! NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDeviceInitAddDescriptor (Config) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Add Language descriptors.
    //
    ntStatus = UdecxUsbDeviceInitAddDescriptorWithIndex(usbDeviceInit,
                                                        UsbDeviceConfig->UsbLanguageDescriptor,
                                                        UsbDeviceConfig->UsbLanguageDescriptorSize,
                                                        UsbDeviceConfig->UsbLanguageDescriptorIndex);
    if (! NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDeviceInitAddDescriptorWithIndex (Language) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Add Manufacturer String descriptor.
    //
    UNICODE_STRING UsbManufacturerStringDescriptor;
    RtlInitUnicodeString(&UsbManufacturerStringDescriptor,
                         UsbDeviceConfig->UsbManufacturerStringDescriptor);

    ntStatus = UdecxUsbDeviceInitAddStringDescriptor(usbDeviceInit,
                                                     &UsbManufacturerStringDescriptor,
                                                     UsbDeviceConfig->UsbManufacturerStringDescriptorIndex,
                                                     UsbDeviceConfig->LanguageIdentifier);
    if (! NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDeviceInitAddStringDescriptor (Manufacturer) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Add Product String descriptor.
    //
    UNICODE_STRING ProductStringDescriptor;
    RtlInitUnicodeString(&ProductStringDescriptor,
                        UsbDeviceConfig->ProductStringDescriptor);
    ntStatus = UdecxUsbDeviceInitAddStringDescriptor(usbDeviceInit,
                                                     &ProductStringDescriptor,
                                                     UsbDeviceConfig->ProductStringDescriptorIndex,
                                                     UsbDeviceConfig->LanguageIdentifier);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDeviceInitAddStringDescriptor (Product) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            CONTEXT_UdeClient_UsbDevice);
    ntStatus = UdecxUsbDeviceCreate(&usbDeviceInit,
                                    &attributes,
                                    &usbDevice);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDeviceCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Create succeeded. Update the contexts.
    //
    CONTEXT_UdeClient_UsbDevice* usbDeviceContext = UdeClientDeviceContextGet(usbDevice);
    usbDeviceContext->DmfModule = DmfModule;
    usbDeviceContext->UsbDeviceSpeed = UsbDeviceConfig->UsbDeviceSpeed;

    if (moduleConfig->EvtUsbDevicePostCreate)
    {
        moduleConfig->EvtUsbDevicePostCreate(DmfModule,
                                             usbDevice);
    }

    *UdecxUsbDevice = usbDevice;
    usbDevice = NULL;

Exit:

    if (usbDevice != NULL)
    {
        WdfObjectDelete(usbDevice);
    }
   
    if (usbDeviceInit != NULL)
    {
        UdecxUsbDeviceInitFree(usbDeviceInit);
    }
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
UdeClient_CreateAndPlugUsbDevice(
    _In_ DMFMODULE DmfModule,
    _In_ UdeClient_CONFIG_UsbDevice* UsbDeviceConfig,
    _In_ UDE_CLIENT_PLUGIN_PORT_TYPE PortType,
    _In_ ULONG PlugInPortNumber,
    _Out_ UDECXUSBDEVICE* UdecxUsbDevice
    )
/*++

Routine Description:

    Create a Usb Device and Plug to UdeCx Controller.

Arguments:

    DmfModule - This Module's handle.
    UsbDeviceConfig - Usb Configuration for this device.
    PortType - USB 2.0 or USB 3.0 port.
    PlugInPortNumber - Port Number to be used while PlugIn this Usb Device.
    UdecxUsbDevice - The created Usb Device.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONFIG_UdeClient* moduleConfig;
    DMF_CONTEXT_UdeClient* moduleContext;
    UDECXUSBDEVICE usbDevice;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    usbDevice = NULL;

    if (PortType != UDE_CLIENT_PORT_USB2_0 && PortType != UDE_CLIENT_PORT_USB3_0)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "PortType %d Invalid.", PortType);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Create a USB Device.
    //
    ntStatus = UdeClient_CreateUsbDevice(DmfModule,
                                         UsbDeviceConfig,
                                         &usbDevice);
    if (! NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdeClient_CreateUsbDevice fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // The Only place to create the Simple EndpointType is just before the PlugIn.
    //
    if (UsbDeviceConfig->UsbDeviceEndpointType == UdecxEndpointTypeSimple)
    {
        // Loop through and create all the required endpoints before PlugIn.
        //
        for (ULONG endpointIndex = 0; endpointIndex < UsbDeviceConfig->SimpleEndpointCount; ++endpointIndex)
        {
            UdeClient_CONFIG_Endpoint* endpointConfig = &UsbDeviceConfig->SimpleEndpointConfigs[endpointIndex];
            UDECXUSBENDPOINT endpointOut;
        
            ntStatus = UdeClient_SimpleEndpointCreate(DmfModule,
                                                      usbDevice,
                                                      endpointConfig,
                                                      &endpointOut);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdeClient_SimpleEndpointCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }
    }

    // Plug the newly created Usb Device.
    //
    UDECX_USB_DEVICE_PLUG_IN_OPTIONS pluginOptions;
    UDECX_USB_DEVICE_PLUG_IN_OPTIONS_INIT(&pluginOptions);
    if (PortType == UDE_CLIENT_PORT_USB2_0)
    {
        pluginOptions.Usb20PortNumber = PlugInPortNumber;
    }
    else
    {
        pluginOptions.Usb30PortNumber = PlugInPortNumber;
    }

    CONTEXT_UdeDeviceInformation* udeDeviceInformation;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    // Create context to store device specific Config information for optional later retrieval.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes,
                                            CONTEXT_UdeDeviceInformation);
    ntStatus = WdfObjectAllocateContext(usbDevice,
                                        &objectAttributes,
                                        (VOID**) &udeDeviceInformation);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "WdfObjectAllocateContext fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    // Save for access via Method.
    //
    udeDeviceInformation->PlugInPortType = PortType;
    udeDeviceInformation->PlugInPortNumber = PlugInPortNumber;

    ntStatus = UdecxUsbDevicePlugIn(usbDevice,
                                    &pluginOptions);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDevicePlugIn fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    *UdecxUsbDevice = usbDevice;
    usbDevice = NULL;

Exit:

    // Even if the Create succeeded, but the PlugIn failed free up the just created Device.
    //
    if (usbDevice != NULL)
    {
        WdfObjectDelete(usbDevice);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// The table of IOCTLS that this Module supports.
//
IoctlHandler_IoctlRecord UdeClient_IoctlHandlerTable[] =
{
    {IOCTL_GET_HCD_DRIVERKEY_NAME,  0,  0, UdeClient_EvtIoControl },
    {IOCTL_USB_GET_ROOT_HUB_NAME,   0,  0, UdeClient_EvtIoControl }
};

_Function_class_(DMF_ModuleD0Entry)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_UdeClient_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    UdeClient callback for ModuleD0Entry for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    // NOP Currently.
    //
    ntStatus = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_UdeClient_ModuleD0Entry ntStatus=%!STATUS!", ntStatus);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Function_class_(DMF_ModuleD0Exit)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_UdeClient_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    UdeClient callback for ModuleD0Exit for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(TargetState);

    FuncEntry(DMF_TRACE);

    // NOP Currently.
    //
    ntStatus = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_UdeClient_ModuleD0Exit ntStatus=%!STATUS!", ntStatus);

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

_Function_class_(DMF_ModuleSurpriseRemoval)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UdeClient_SurpriseRemoval(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    UdeClient callback for ModuleSurpriseRemoval for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    FuncEntry(DMF_TRACE);

    // NOP Currently.
    //

    UNREFERENCED_PARAMETER(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UdeClient_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_CONFIG_UdeClient* moduleConfig;
    DMF_CONTEXT_UdeClient* moduleContext;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.DeviceInterfaceGuid = GUID_DEVINTERFACE_USB_HOST_CONTROLLER;
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(UdeClient_IoctlHandlerTable);
    moduleConfigIoctlHandler.IoctlRecords = UdeClient_IoctlHandlerTable;
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
    moduleConfigIoctlHandler.ReferenceString = L"UDE";

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleIoctlHandler);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_UdeClient_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type UdeClient.
    Initialize the UdeCx Host Controller Emulation and if configured to Create and PlugIn a USB device, 
    Create the device and attached to the designated port.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    UDECX_WDF_DEVICE_CONFIG controllerConfig;
    DMF_CONFIG_UdeClient* moduleConfig;
    CONTEXT_UdeClient_UsbController* controllerContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Ensure we have the required callbacks on the controller.
    // NOTE: Rest of the validation is done at a later stage.
    //
    if (moduleConfig->UsbControllerConfig.EvtControllerQueryUsbCapability == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtControllerQueryUsbCapability is Mandatory");
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Allocate a Context to keep items for the UsbController.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            CONTEXT_UdeClient_UsbController);
    ntStatus = WdfObjectAllocateContext(device,
                                        &attributes,
                                        (VOID**)&controllerContext);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfObjectAllocateContext (UsbController) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Save the DmfModule in the Controller context.
    //
    controllerContext->DmfModule = DmfModule;
    controllerContext->UdecxUsbDevice = NULL;

    UDECX_WDF_DEVICE_CONFIG_INIT(&controllerConfig, 
                                 UdeClient_EvtDeviceQueryUsbCapability);
    controllerConfig.NumberOfUsb20Ports = moduleConfig->UsbControllerConfig.NumberOfUsb20Ports;
    controllerConfig.NumberOfUsb30Ports = moduleConfig->UsbControllerConfig.NumberOfUsb30Ports;
    controllerConfig.ResetAction = moduleConfig->UsbControllerConfig.ControllerResetAction;
    if (moduleConfig->UsbControllerConfig.EvtControllerReset != NULL)
    {
        controllerConfig.EvtUdecxWdfDeviceReset = UdeClient_EvtControllerReset;
    }

    ntStatus = UdecxWdfDeviceAddUsbDeviceEmulation(device,
                                                   &controllerConfig);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxWdfDeviceAddUsbDeviceEmulation fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (moduleConfig->UsbCreateAndPlugOnOpen)
    {
        UDECXUSBDEVICE udecxUsbDevice;
        ntStatus = UdeClient_CreateAndPlugUsbDevice(DmfModule,
                                                    &moduleConfig->UsbDeviceConfig,
                                                    moduleConfig->PlugInPortType,
                                                    moduleConfig->PlugInPortNumber,
                                                    &udecxUsbDevice);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdeClient_CreateAndPlugUsbDevice fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Save this in the controller Context.
        //
        controllerContext->UdecxUsbDevice = udecxUsbDevice;

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "On Open Usb Device 0x%p Plugged In", udecxUsbDevice);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_UdeClient_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type UdeClient.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONFIG_UdeClient* moduleConfig;
    CONTEXT_UdeClient_UsbController* controllerContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    controllerContext = UdeClientControllerContextGet(device);

    if (moduleConfig->UsbCreateAndPlugOnOpen && 
        controllerContext->UdecxUsbDevice != NULL)
    {
        ntStatus = UdecxUsbDevicePlugOutAndDelete(controllerContext->UdecxUsbDevice);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDevicePlugOutAndDelete fails: ntStatus=%!STATUS!", ntStatus);
        }

        controllerContext->UdecxUsbDevice = NULL;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type UdeClient.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_UdeClient;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_UdeClient;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_UdeClient;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_UdeClient);
    dmfCallbacksDmf_UdeClient.ChildModulesAdd = DMF_UdeClient_ChildModulesAdd;
    dmfCallbacksDmf_UdeClient.DeviceOpen = DMF_UdeClient_Open;
    dmfCallbacksDmf_UdeClient.DeviceClose = DMF_UdeClient_Close;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_UdeClient);
    dmfCallbacksWdf_UdeClient.ModuleSurpriseRemoval = DMF_UdeClient_SurpriseRemoval;
    dmfCallbacksWdf_UdeClient.ModuleD0Entry = DMF_UdeClient_ModuleD0Entry;
    dmfCallbacksWdf_UdeClient.ModuleD0Exit = DMF_UdeClient_ModuleD0Exit;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_UdeClient,
                                            UdeClient,
                                            DMF_CONTEXT_UdeClient,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_UdeClient.CallbacksDmf = &dmfCallbacksDmf_UdeClient;
    dmfModuleDescriptor_UdeClient.CallbacksWdf = &dmfCallbacksWdf_UdeClient;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_UdeClient,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_DeviceCreateAndPlugIn(
    _In_ DMFMODULE DmfModule,
    _In_ UdeClient_CONFIG_UsbDevice* UsbDeviceConfig,
    _In_ UDE_CLIENT_PLUGIN_PORT_TYPE PortType,
    _In_ ULONG PortNumber,
    _Out_ UDECXUSBDEVICE* UdecxUsbDevice
    )
/*++

Routine Description:

    Create and Plug In a Usb device.

Arguments:

    DmfModule - This Module's handle.
    UsbDeviceConfig - The Usb Configuration for the device to be created.
    PortType - USB 2.0 or USB 3.0 port.
    PortNumber - The plugIn port number.
    UdecxUsbDevice - Usb device created. 

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 UdeClient);

    ntStatus = UdeClient_CreateAndPlugUsbDevice(DmfModule,
                                                UsbDeviceConfig,
                                                PortType,
                                                PortNumber,
                                                UdecxUsbDevice);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdeClient_CreateAndPlugUsbDevice fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Usb Device 0x%p Plugged In", *UdecxUsbDevice);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UdeClient_DeviceEndpointInformationGet(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBENDPOINT Endpoint,
    _Out_opt_ UDECXUSBDEVICE* UdecxUsbDevice,
    _Out_opt_ UCHAR* Address
    )
/*++

Routine Description:

    Get the address from a given endpoint.

Arguments:

    DmfModule - This Module's handle.
    Endpoint - The given endpoint.
    Address - The given endpoint's assigned address.

Return Value:

    None

--*/
{
    UdeClient_CONFIG_Endpoint* endpointConfig;
    CONTEXT_UdeClient_Endpoint* endpointContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 UdeClient);

    endpointContext = UdeClientEndpointContextGet(Endpoint);
    endpointConfig = &endpointContext->EndpointConfig;

    if (Address != NULL)
    {
        *Address = endpointConfig->EndpointAddress;
    }
    if (UdecxUsbDevice != NULL)
    {
        *UdecxUsbDevice = endpointContext->UdecxUsbDevice;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_DeviceEndpointCreate(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice,
    _In_ PUDECXUSBENDPOINT_INIT EndpointInit,
    _In_ UdeClient_CONFIG_Endpoint* EndpointConfig,
    _Out_ UDECXUSBENDPOINT* Endpoint
    )
/*++

Routine Description:

    Create an endpoint for a Usb device.

Arguments:

    DmfModule - This Module's handle.
    UdecxUsbDevice - The device on which the endpoint is located.
    EndpointInit - Endpoint Init associated with the Usb device.
    EndpointConfig - Endpoint Configuration.
    Endpoint - The newly created Endpoint.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 UdeClient);

    ntStatus = UdeClient_EndpointCreate(DmfModule,
                                        UdecxUsbDevice,
                                        &EndpointInit,
                                        EndpointConfig,
                                        Endpoint);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdeClient_EndpointCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "EndpointInit 0x%p Endpoint 0x%p Created", EndpointInit, *Endpoint);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UdeClient_DeviceInformationGet(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice,
    _Out_ UDE_CLIENT_PLUGIN_PORT_TYPE* PortType,
    _Out_ ULONG* PortNumber
    )
/*++

Routine Description:

    Get port and port type information from a give UdecxUsbDevice.

Arguments:

    DmfModule - This Module's handle.
    UdecxUsbDevice - The given UdedxUsbDevice.
    PortType - Type of port given UdecxDevice is plugged into.
    PortNumber - Port number the given UdecxDevice is plugged into or zero if not plugged in.

Return Value:

    None

--*/
{
    CONTEXT_UdeDeviceInformation* udeDeviceInformation;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 UdeClient);

    udeDeviceInformation = UdeClient_UdeDeviceInformation(UdecxUsbDevice);
    *PortType = udeDeviceInformation->PlugInPortType;
    *PortNumber = udeDeviceInformation->PlugInPortNumber;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_DevicePlugOutAndDelete(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice
    )
/*++

Routine Description:

    Unplug and delete an already plugged In Usb Device.

Arguments:

    DmfModule - This Module's handle.
    UdecxUsbDevice - Usb device to be plugged Out and Delete. 

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 UdeClient);

    ntStatus = UdecxUsbDevicePlugOutAndDelete(UdecxUsbDevice);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UdecxUsbDevicePlugOutAndDelete fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Usb Device 0x%p Plugged Out", UdecxUsbDevice);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UdeClient_DeviceSignalFunctionWake(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice,
    _In_ ULONG Interface
    )
/*++

Routine Description:

    Send Wake Signal to the Usb device.

Arguments:

    DmfModule - This Module's handle.
    UdecxUsbDevice - Usb device to be which the signal is sent.
    Interface -  Interface. This is applicable only for USB 3.0 device.

Return Value:

    None.

--*/
{
    CONTEXT_UdeClient_UsbDevice* usbDeviceContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 UdeClient);

    usbDeviceContext = UdeClientDeviceContextGet(UdecxUsbDevice);
    if (usbDeviceContext->UsbDeviceSpeed == UdecxUsbSuperSpeed)
    {
        UdecxUsbDeviceSignalFunctionWake(UdecxUsbDevice,
                                         Interface);
    }
    else
    {
        UdecxUsbDeviceSignalWake(UdecxUsbDevice);
    }

    FuncExitVoid(DMF_TRACE);

}
#pragma code_seg()

// Module Static Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_Static_DeviceInitInitialize(
    _In_ PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    This static method initialises the UdeCx.
    Client of this Module MUST call this method before the device is created.

Arguments:

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS.

--*/
{
    return UdecxInitializeWdfDeviceInit(DeviceInit);
}

// eof: Dmf_UdeClient.c
//
