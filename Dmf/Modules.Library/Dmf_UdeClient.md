## DMF_UdeClient

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module implements the functionality of an USB Device Emulation Extension (UDECX) client driver. 

The Module allows the client manage creating/destroying a virtual USB Device and plugIn/plugOut it on virtual USB Host Controller's root port.
The created virtual USB device can have simple or complex endpoints.
The Module allows the client to create the USB device on demand or at the Module is opened. 
If the Module configured for Create & Plug In at Open, the client can pass the configuration details on the virtual USB Device as well as the port to which 
the device needed to plugged in, in the Module's configuration.  

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_UdeClient
````
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
````
Member | Description
----|----
UsbControllerConfig | Configuration for the Virtual Usb Host Controller to be created.  
UsbCreateAndPlugOnOpen | Whether the Virtual Usb Device need to be Created-and-PluggedIn on Open or not. 
PlugInPortType | Port Type (USB 2.0 or USB 3.0) to be used while PlugIn this Usb Device. Set this if the UsbCreateAndPlugOnOpen is set as TRUE. 
PlugInPortNumber | Port Number to be used while PlugIn this Usb Device. Set this if the UsbCreateAndPlugOnOpen is set as TRUE. 
UsbDeviceConfig | Configuration Details for the Usb Device to be created and plugged in. Set this if the UsbCreateAndPlugOnOpen is set as TRUE. 
EvtUsbDevicePreCreate | Optional callback that allows clients perform pre create operation like setting up state change operation callbacks on the Usb Device being created. This callback is called just before the Usb Device is created. Refer UDECX_USB_DEVICE_STATE_CHANGE_CALLBACKS.
EvtUsbDevicePreCreate | Optional callback that allows clients perform post processing tasks. This callback is called just after the Usb Device is successfully created.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

##### UDE_CLIENT_PLUGIN_PORT_TYPE
````
typedef enum
{
    UDE_CLIENT_PORT_USB2_0,
    UDE_CLIENT_PORT_USB3_0
} UDE_CLIENT_PLUGIN_PORT_TYPE
````
Member | Description
----|----
UDE_CLIENT_PORT_USB2_0 | USB 2.0 Port.
UDE_CLIENT_PORT_USB3_0 | USB 3.0 Port. 

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_UdeClient_Controller_QueryUsbCapability
````
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
````
This callback is called to determine the capabilities that are supported by the emulated USB host controller. 

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
UdecxWdfDevice | A handle to a framework device object that represents the controller.
CapabilityType | Pointer to a GUID specifying the requested capability. The possible GUID values are: GUID_USB_CAPABILITY_CHAINED_MDLS GUID_USB_CAPABILITY_SELECTIVE_SUSPEND GUID_USB_CAPABILITY_FUNCTION_SUSPEND GUID_USB_CAPABILITY_DEVICE_CONNECTION_HIGH_SPEED_COMPATIBLE GUID_USB_CAPABILITY_DEVICE_CONNECTION_SUPER_SPEED_COMPATIBLE
OutputBufferLength | The length, in bytes, of the request's output buffer, if an output buffer is available.
OutputBuffer | An optional pointer to a location that receives the buffer's address.
ResultLength | A location that, on return, contains the size, in bytes, of the information that the callback function stored in OutputBuffer.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_UdeClient_Controller_Reset
````
typedef
_Function_class_(EVT_DMF_UdeClient_Controller_Reset)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_UdeClient_Controller_Reset(
    _In_ DMFMODULE DmfModule,  
    _In_ WDFDEVICE UdecxWdfDevice
    );
````
This callback is called to notify the client that it must handle a reset request including resetting all downstream devices attached to the emulated host controller. This call is asynchronous. The client signals completion with status information by calling UdecxWdfDeviceResetComplete. 

NOTE: This is called ONLY if ControllerResetAction is set as UdecxWdfDeviceResetActionResetWdfDevice.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
UdecxWdfDevice | A handle to a framework device object that represents the controller.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_UdeClient_UsbDevice_PreCreate
````
typedef
_Function_class_(EVT_DMF_UdeClient_UsbDevice_PreCreate)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_UdeClient_UsbDevice_PreCreate(
    _In_ DMFMODULE DmfModule,
    _In_ PUDECXUSBDEVICE_INIT UsbDeviceInit
    );
````
Optional callback that allows clients perform pre-create operation. This callback is called just before the Usb Device is created. 
An example of pre-create operation is- setting up state change operation callbacks using UdecxUsbDeviceInitSetStateChangeCallbacks. 

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
UsbDeviceInit | A pointer to an opaque structure that the client can pass into UdecxUsbDeviceInitSetStateChangeCallbacks.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_UdeClient_UsbDevice_PostCreate
````
typedef
_Function_class_(EVT_DMF_UdeClient_UsbDevice_PostCreate)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_UdeClient_UsbDevice_PostCreate(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UsbDevice
    );
````
Optional callback that allows clients perform post-create operation. This callback is called just after the Usb Device is created. 
An example of post-create operation is- setting up context for this newly created device or saving the device for later use. 

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
UsbDevice | A handle to a framework device object that represents the Virtual USB Device.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_UdeClient_Endpoint_DeviceIoControl
````
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
````
Callback that allows clients to handle Requests coming in an Endpoint. This callback is Mandatory. The client completes the request and signals completion with 
status by calling WdfRequestCompleteWithInformation method.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
Endpoint | A handle to a framework device object that represents an Endpoint on a Virtual USB Device.
Queue | The WDFQUEUE associated with Request.
Request | A handle to a framework Request object that represent the request.
OutputBufferLength | Length of the request's output buffer, if an output buffer is available.
InputBufferLength | Length of the request's input buffer, if an input buffer is available.
IoControlCode | The driver-defined or system-defined I/O control code (IOCTL) that is associated with the request.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_UdeClient_Endpoint_Reset
````
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
````
Callback that allows clients to handle Reset an Endpoint. This callback is Mandatory. The client completes the request and signals completion with status by calling WdfRequestCompleteWithInformation method.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
Endpoint | A handle to a framework device object that represents an Endpoint on a Virtual USB Device.
Request | A handle to a framework Request object that represent the request to reset the endpoint. 

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_UdeClient_Endpoint_Start
````
typedef
_Function_class_(EVT_DMF_UdeClient_Endpoint_Start)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_UdeClient_Endpoint_Start(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBENDPOINT Endpoint
    );
````
Callback to indicate to clients that that framework is starting processing I/O request on an Endpoint. This callback is Optional.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
Endpoint | A handle to a framework device object that represents an Endpoint on a Virtual USB Device.
-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_UdeClient_Endpoint_Purge
````
typedef
_Function_class_(EVT_DMF_UdeClient_Endpoint_Purge)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_UdeClient_Endpoint_Purge(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBENDPOINT Endpoint
    );
````
Callback to indicate to clients that framework has stopped queuing I/O requests to the endpoint's queue and cancelled unprocessed requests. This callback is Optional. 
The client is required to ensure all I/O forwarded from the endpoint’s queue has been completed, and that newly forwarded I/O request fail, until a callback to EVT_DMF_UdeClient_Endpoint_Start.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
Endpoint | A handle to a framework device object that represents an Endpoint on a Virtual USB Device.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_UdeClient_DeviceCreateAndPlugIn

````
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
````

This Method is for creating Virtual USB device and then plug it into the desired port on the virtual controller's root hub.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
UsbDeviceConfig | Configuration Details for the Usb Device to be created and plugged in.
PortType | Port Type (USB 2.0 or USB 3.0) to be used while PlugIn this Usb Device.
PortNumber | Port Number to be used while PlugIn this Usb Device.
UdecxUsbDevice | A handle to a framework device object that represents the newly created Virtual USB Device.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_UdeClient_DeviceEndpointAddressGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UdeClient_DeviceEndpointAddressGet(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBENDPOINT Endpoint,
    _Out_ UCHAR* Address
    );
````

Get the address from a given endpoint.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
Endpoint | The given endpoint.
Address | Address of the given endpoint.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_UdeClient_DeviceEndpointCreate

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_DeviceEndpointCreate(
    _In_ DMFMODULE DmfModule,
    _In_ PUDECXUSBENDPOINT_INIT EndpointInit,
    _In_ UdeClient_CONFIG_Endpoint* EndpointConfig,
    _Out_ UDECXUSBENDPOINT* Endpoint
    );
````

This Method is for creating an Endpoint on a Virtual USB device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
EndpointInit | An opaque structure that represent an initialization configuration for Endpoint.
EndpointConfig | Endpoint configuration.
Endpoint | A handle to a framework device object that represents the newly created Endpoint.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_UdeClient_DeviceInformationGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UdeClient_DeviceInformationGet(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice,
    _Out_ UDE_CLIENT_PLUGIN_PORT_TYPE* PortType,
    _Out_ ULONG* PortNumber
    );
````

Gets port and port type information from a give UdecxUsbDevice.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
UdecxUsbDevice | A handle to a framework device object that represents the Virtual USB Device.
PortType | Type of port given UdecxDevice is plugged into.
PortNumber | Port number the given UdecxDevice is plugged into or zero if not plugged in.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_UdeClient_DevicePlugOutAndDelete

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_DevicePlugOutAndDelete(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice
    );
````

This Method is for plugging the virtual USB device out and then deleting it.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
UdecxUsbDevice | A handle to a framework device object that represents the Virtual USB Device.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_UdeClient_DeviceSignalFunctionWake

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UdeClient_DeviceSignalFunctionWake(
    _In_ DMFMODULE DmfModule,
    _In_ UDECXUSBDEVICE UdecxUsbDevice,
    _In_ ULONG Interface
    );
````

This Method is for initiating wake up of the USB 2.0 device or a specified function in a USB 3.0 device from a low power state. 

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_UdeClient Module handle.
UdecxUsbDevice | A handle to a framework device object that represents the Virtual USB Device.
Interface | Specifies the function. This is applicable only for USB 3.0 device.  Pass 0 for USB 2.0 device.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_UdeClient_Static_DeviceInitInitialize

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UdeClient_Static_DeviceInitInitialize(
    _In_ PWDFDEVICE_INIT DeviceInit
    );
````

This Method is for initializing the UDECx. This MUST be called before the WdfDevice is created.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DeviceInit | An opaque structure for Device Initalization.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* Dmf_IoctlHandler

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* SurfaceTypeCoverV7FprUdeDriver

-----------------------------------------------------------------------------------------------------------------------------------

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Pattern

-----------------------------------------------------------------------------------------------------------------------------------

