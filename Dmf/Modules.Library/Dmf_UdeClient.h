/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_UdeClient.h

Abstract:

    Companion file to Dmf_UdeClient.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

// This Module is only supported in Kernel-mode because UDE only support Kernel-mode.
//
#if !defined(DMF_USER_MODE)

#include <Ude/1.0/Udecx.h>

// Specifies the type of Port on the virtual controller's root hub.
//
typedef enum
{
    UDE_CLIENT_PORT_USB2_0,
    UDE_CLIENT_PORT_USB3_0
} UDE_CLIENT_PLUGIN_PORT_TYPE;

// Allows Client to return the Controllers Capability.
//
typedef
_Function_class_(EVT_DMF_UdeClient_Controller_QueryUsbCapability)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_UdeClient_Controller_QueryUsbCapability(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE UdecxWdfDevice,
    _In_ GUID* CapabilityType,
    _In_ ULONG OutputBufferLength,
    _Out_writes_to_opt_(OutputBufferLength, *ResultLength)  PVOID OutputBuffer,
    _Out_ ULONG* ResultLength
    );

// Allows Client to do reset Controller or the devices attached to it.
//
typedef
_Function_class_(EVT_DMF_UdeClient_Controller_Reset)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_UdeClient_Controller_Reset(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE UdecxWdfDevice
    );

// Allows Client to perform other operations before the UsbDevice is created.
//
typedef
_Function_class_(EVT_DMF_UdeClient_UsbDevice_PreCreate)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_UdeClient_UsbDevice_PreCreate(
    _In_ DMFMODULE DmfModule,
    _In_ PUDECXUSBDEVICE_INIT UsbDeviceInit
    );

// Allows Client to perform other operations after the UsbDevice is created.
//
typedef
_Function_class_(EVT_DMF_UdeClient_UsbDevice_PostCreate)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_UdeClient_UsbDevice_PostCreate(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UsbDevice
    );

// Allows Client to perform IO operations on an endpoint.
//
typedef
_Function_class_(EVT_DMF_UdeClient_Endpoint_DeviceIoControl)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_UdeClient_Endpoint_DeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBENDPOINT Endpoint,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

// Endpoint Reset callback. This is mandatory.
//
typedef
_Function_class_(EVT_DMF_UdeClient_Endpoint_Reset)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_UdeClient_Endpoint_Reset(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBENDPOINT Endpoint,
    _In_ WDFREQUEST Request
    );

// Endpoint Start callback. This is optional.
//
typedef
_Function_class_(EVT_DMF_UdeClient_Endpoint_Start)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_UdeClient_Endpoint_Start(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBENDPOINT Endpoint
    );

// Endpoint Purge callback. This is optional.
//
typedef
_Function_class_(EVT_DMF_UdeClient_Endpoint_Purge)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_UdeClient_Endpoint_Purge(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBENDPOINT Endpoint
    );

typedef struct _UdeClient_CONFIG_Endpoint
{
    // Endpoint Address to used.
    // 
    UCHAR EndpointAddress;
    // Endpoint Queue Dispatch Type.
    //
    WDF_IO_QUEUE_DISPATCH_TYPE QueueDispatchType;
    // DeviceIOControl callback.
    //
    EVT_DMF_UdeClient_Endpoint_DeviceIoControl* EvtEndpointDeviceIoControl;
    // Endpoint Reset callbacks. This is Mandatory.
    //
    EVT_DMF_UdeClient_Endpoint_Reset* EvtEndpointReset;
    // Endpoint Start callbacks. This is Optional.
    //
    EVT_DMF_UdeClient_Endpoint_Start* EvtEndpointStart;
    // Endpoint Purge callbacks. This is Optional.
    //
    EVT_DMF_UdeClient_Endpoint_Purge* EvtEndpointPurge;
} UdeClient_CONFIG_Endpoint;

typedef struct _UdeClient_CONFIG_UsbDevice
{
    // Usb Descriptors.
    //
    UCHAR* UsbDeviceDescriptor;
    USHORT UsbDeviceDescriptorSize;
    UCHAR* UsbBosDescriptor;
    USHORT UsbBosDescriptorSize;
    UCHAR* UsbConfigDescriptor;
    USHORT UsbConfigDescriptorSize;
    USHORT LanguageIdentifier;
    UCHAR* UsbLanguageDescriptor;
    USHORT UsbLanguageDescriptorSize;
    UCHAR UsbLanguageDescriptorIndex;
    WCHAR* UsbManufacturerStringDescriptor;
    USHORT UsbManufacturerStringDescriptorSize;
    UCHAR UsbManufacturerStringDescriptorIndex;
    WCHAR* ProductStringDescriptor;
    USHORT ProductStringDescriptorSize;
    UCHAR ProductStringDescriptorIndex;
    // Usb Device Speed.
    //
    UDECX_USB_DEVICE_SPEED UsbDeviceSpeed;
    // Endpoint type. If Simple EndpointType, the endpoints are created before plugIn.
    //
    UDECX_ENDPOINT_TYPE UsbDeviceEndpointType;
    // Simple Endpoint configuration details.
    // Applicable only if UsbDeviceEndpointType is UdecxEndpointTypeSimple.
    //
    UdeClient_CONFIG_Endpoint* SimpleEndpointConfigs;
    ULONG SimpleEndpointCount;
} UdeClient_CONFIG_UsbDevice;

typedef struct _UdeClient_CONFIG_Controller
{
    // Number of Usb2.0 Ports.
    //
    USHORT NumberOfUsb20Ports;
    // Number of Usb3.0 Ports.
    //
    USHORT NumberOfUsb30Ports;
    // Type of reset operation supported by the Controller.
    //
    UDECX_WDF_DEVICE_RESET_ACTION ControllerResetAction;
    // Callback for Controller's USB Capability Query. This is Mandatory.
    //
    EVT_DMF_UdeClient_Controller_QueryUsbCapability* EvtControllerQueryUsbCapability;
    // Callback for Controller's Reset.
    //
    EVT_DMF_UdeClient_Controller_Reset* EvtControllerReset;
} UdeClient_CONFIG_Controller;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct _DMF_CONFIG_UdeClient
{
    // Configuration for the Usb Host Controller.
    //
    UdeClient_CONFIG_Controller UsbControllerConfig;
    // Whether the Usb Device need to be Created-and-PluggedIn on Open or not.
    //
    BOOLEAN UsbCreateAndPlugOnOpen;
    // Below two entries are looked at only if the UsbCreateAndPlugOnOpen is TRUE.
    //
    // Port Number and PortType to be used while PlugIn this Usb Device.
    //
    UDE_CLIENT_PLUGIN_PORT_TYPE PlugInPortType;
    ULONG PlugInPortNumber;
    // Configuration Details for the Usb Device.
    //
    UdeClient_CONFIG_UsbDevice UsbDeviceConfig;
    // Callbacks for Pre and Post Create callbacks for any Usb Device Create 
    // This is applicable for both plugged in at Open or later.
    //
    EVT_DMF_UdeClient_UsbDevice_PreCreate* EvtUsbDevicePreCreate;
    EVT_DMF_UdeClient_UsbDevice_PostCreate* EvtUsbDevicePostCreate;
} DMF_CONFIG_UdeClient;

// This macro declares the following functions:
// DMF_UdeClient_ATTRIBUTES_INIT()
// DMF_CONFIG_UdeClient_AND_ATTRIBUTES_INIT()
// DMF_UdeClient_Create()
//
DECLARE_DMF_MODULE(UdeClient)

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
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_DeviceEndpointCreate(
    _In_ DMFMODULE DmfModule,
    _In_ PUDECXUSBENDPOINT_INIT EndpointInit,
    _In_ UdeClient_CONFIG_Endpoint* ConfigEndpoint,
    _Out_ UDECXUSBENDPOINT* Endpoint
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_DevicePlugOutAndDelete(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UdeClient_DeviceSignalFunctionWake(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice,
    _In_ ULONG Interface
    );

// Static Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_Static_DeviceInitInitialize(
    _In_ PWDFDEVICE_INIT DeviceInit
    );

#endif // !defined(DMF_USER_MODE)

// eof: Dmf_UdeClient.h
//
