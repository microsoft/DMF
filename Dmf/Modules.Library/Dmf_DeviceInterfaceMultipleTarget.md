## DMF_DeviceInterfaceTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a driver pattern that streams IOCTL requests to multiple WDFIOTARGETs that dynamically appears/disappears. This Module
automatically creates buffers and WDFREQUESTS for both input and output data performs all the necessary operations to attach
those buffers to WDFREQUESTS.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_DeviceInterfaceTarget
````
typedef struct
{
    // Module's Open option.
    //
    DeviceInterfaceMultipleTarget_PnpRegisterWhen_Type ModuleOpenOption;
    // Target Device Interface GUID.
    //
    GUID DeviceInterfaceMultipleTargetGuid;
    // Open in Read or Write mode.
    //
    ULONG OpenMode;
    // Share Access.
    //
    ULONG ShareAccess;
    // Module Config for Child Module.
    //
    DMF_CONFIG_ContinuousRequestTarget ContinuousRequestTargetModuleConfig;
    // Callback to specify IoTarget State.
    //
    EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChange* EvtDeviceInterfaceMultipleTargetOnStateChange;
    // Callback to notify Interface arrival.
    //
    EVT_DMF_DeviceInterfaceMultipleTarget_OnPnpNotification* EvtDeviceInterfaceMultipleTargetOnPnpNotification;
} DMF_CONFIG_DeviceInterfaceMultipleTarget;
````
Member | Description
----|----
ModuleOpenOption | This option defines when the Module does the PNP interface arrival/removal registration.
DeviceInterfaceTargetGuid | The Device Interface GUID of the underlying WDFIOTARGET to open.
OpenMode | The Open Mode used when opening the underlying WDFIOTARGET.
ShareAccess | The Share Access mask used when opening the underlying WDFIOTARGET.
ContinuousRequestTargetModuleConfig | Contains the settings for the underlying RequesetStream.
EvtDeviceInterfaceTargetOnStateChange | Callback to the Client that indicates the state of the target has changed.
EvtDeviceInterfaceTargetOnPnpNotification | Callback to the Client that allows the Client to indicate if the target should open.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### DeviceInterfaceTarget_StateType
Enum to specify IO Target State.

````
typedef enum
{
  DeviceInterfaceTarget_StateType_Invalid,
  DeviceInterfaceTarget_StateType_Open,
  DeviceInterfaceTarget_StateType_QueryRemove,
  DeviceInterfaceTarget_StateType_QueryRemoveCancelled,
  DeviceInterfaceTarget_StateType_QueryRemoveComplete,
  DeviceInterfaceTarget_StateType_Close,
  DeviceInterfaceTarget_StateType_Maximum
} DeviceInterfaceTarget_StateType;
````

##### DeviceInterfaceMultipleTarget_PnpRegisterWhen_Type
Enum to determine when the Module should register for PnpNotifications for the device interface GUID specified in the Module configuration. Module will register for existing interfaces, so arrival callbacks can happen immediately after registration.

````
typedef enum
{
    // Module is opened in PrepareHardware and closed in ReleaseHardware.
    //
    DeviceInterfaceMultipleTarget_PnpRegisterWhen_PrepareHardware = 0,
    // Module is opened in D0Entry and closed in D0Exit.
    //
    DeviceInterfaceMultipleTarget_PnpRegisterWhen_D0Entry,
    // Module is opened in when module is created.
    //
    DeviceInterfaceMultipleTarget_PnpRegisterWhen_Create
} DeviceInterfaceMultipleTarget_PnpRegisterWhen_Type;
````
-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_DeviceInterfaceTarget_OnStateChange
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_DeviceInterfaceTarget_OnStateChange(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target,
    _In_ DeviceInterfaceMultipleTarget_StateType IoTargetState
    );
````

Callback to the Client that indicates the state of the target has changed.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceTarget Module handle.
Target | Target instance associated with the callback.
IoTargetState | The new state the WDFIOTARGET is transitioning to.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_DeviceInterfaceTarget_OnPnpNotification
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_DeviceInterfaceTarget_OnPnpNotification(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName,
    _Out_ BOOLEAN* IoTargetOpen
    );
````

Callback to the Client that allows the Client to indicate if the target should open.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceTarget Module handle.
SymbolicLinkName | Symbolic link name of the WDFIOTARGET.
IoTargetOpen | The address where the Client writes TRUE to indicate that the WDFIOTARGET should open or FALSE if the WDFIOTARGET should not open.

##### Remarks

* See DMF_ContinuousRequestTarget.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DeviceInterfaceTarget_BufferPut

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_BufferPut(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target,
    _In_ VOID* ClientBuffer
    );
````

This Method adds a given DMF_BufferPool buffer back into a given DMFIOTARGET's Output DMF_BufferPool.

##### Returns

NTSTATUS. Fails if the given DMF_BufferPool buffer cannot be added to the list.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceTarget Module handle.
Target | The given DMFIOTARGET handle.
ClientBuffer | The given DMF_BufferPool buffer.

##### Remarks

* NOTE: ClientBuffer must be a properly formed DMF_BufferPool buffer.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DeviceInterfaceTarget_Get

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_Get(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target,
    _Out_ WDFIOTARGET* IoTarget
    );
````

Gives the Client direct access to the WDFIOTARGET handle associated with the given DMFIOTARGET.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceTarget Module handle.
Target | The given DMFIOTARGET handle.
IoTarget | The address where the Client reads the WDFIOTARGET handle after this Method completes.

##### Remarks

* Clients should call DMF_ModuleReference() before using the returned handle in order to synchronize with possible asynchronous removal.
* Clients should call DMF_ModuleDereferece() after using the returned handle in order to synchronize with possible asynchronous removal.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DeviceInterfaceMultipleTarget_GuidGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_GuidGet(
    _In_ DMFMODULE DmfModule,
    _Out_ GUID* Guid
    );
````

The device interface GUID associated with this Module's WDFIOTARGET.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Guid | The device interface GUID associated with this Module's WDFIOTARGET.

##### Remarks

* Clients use this Method when the same callback is used for multiple device interfaces.
* The callback can use this Method to determine which device interface has called the Client.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DeviceInterfaceTarget_Send

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    );
````

This Method uses the given parameters to create a Request and send it asynchronously to the given DMFIOTARGET.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceTarget Module handle.
Target | The given DMFIOTARGET handle.
RequestBuffer | The Client buffer that is sent to this Module's underlying WDFIOTARGET.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data from this Module's underlying WDFIOTARGET.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to this Module's underlying WDFIOTARGET.
RequestIoctl | The IOCTL that tells the Module's underlying WDFIOTARGET the purpose of the associated Request that is sent.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
EvtContinuousRequestTargetSingleAsynchronousRequest | The Client callback that is called when this Module's underlying WDFIOTARGET completes the request.
SingleAsynchronousRequestClientContext | The Client specific context that is sent to EvtContinuousRequestTargetSingleAsynchronousRequest.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DeviceInterfaceTarget_SendSynchronously

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _Out_opt_ size_t* BytesWritten
    );
````

This Method uses the given parameters to create a Request and send it synchronously to the given DMFIOTARGET.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceTarget Module handle.
Target | The given DMFIOTARGET handle.
RequestBuffer | The Client buffer that is sent to this Module's underlying WDFIOTARGET.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data from this Module's underlying WDFIOTARGET.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to this Module's underlying WDFIOTARGET.
RequestIoctl | The IOCTL that tells the Module's underlying WDFIOTARGET the purpose of the associated Request that is sent.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
BytesWritten | The number of bytes transferred to/from the underlying WDFIOTARGET.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DeviceInterfaceTarget_StreamStart

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_StreamStart(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target
    );
````

Starts streaming (causes the created Requests and associated buffers to be sent to the given DMFIOTARGET).

##### Returns

NTSTATUS. Fails if Requests cannot be sent to the underlying WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceTarget Module handle.
Target | The given DMFIOTARGET handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DeviceInterfaceTarget_StreamStop

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_DeviceInterfaceMultipleTarget_StreamStop(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target
    );
````

Stops streaming. The pending requests are canceled and no new Requests will be sent to the given DMFIOTARGET.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceTarget Module handle.
Target | The given DMFIOTARGET handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE.
* This Module does all the work of allocating the buffers and Requests as specified by the Client.
* This Module stops and start streaming automatically during power transition.
* This Module is similar to the USB Continuous Reader in WDF but for any WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_RequestTarget
* DMF_ContinousRequestTarget_

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Add User-mode support.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Targets

-----------------------------------------------------------------------------------------------------------------------------------

