## DMF_DeviceInterfaceMultipleTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

Implements a driver pattern that streams IOCTL requests to multiple WDFIOTARGETs that dynamically appears/disappears. This Module
automatically creates buffers and WDFREQUESTS for both input and output data performs all the necessary operations to attach
those buffers to WDFREQUESTS.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_DeviceInterfaceMultipleTarget
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
    // Use Ex version instead. This version is included for legacy Clients only.
    //
    EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChange* EvtDeviceInterfaceMultipleTargetOnStateChange;
    // Callback to specify IoTarget State.
    // This version allows Client to veto the remove.
    //
    EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChangeEx* EvtDeviceInterfaceMultipleTargetOnStateChangeEx;
    // Callback to notify Interface arrival.
    //
    EVT_DMF_DeviceInterfaceMultipleTarget_OnPnpNotification* EvtDeviceInterfaceMultipleTargetOnPnpNotification;
} DMF_CONFIG_DeviceInterfaceMultipleTarget;
````
Member | Description
----|----
ModuleOpenOption | This option defines when the Module does the PNP interface arrival/removal registration.
DeviceInterfaceMultipleTargetGuid | The Device Interface GUID of the underlying WDFIOTARGET to open.
OpenMode | The Open Mode used when opening the underlying WDFIOTARGET.
ShareAccess | The Share Access mask used when opening the underlying WDFIOTARGET.
ContinuousRequestTargetModuleConfig | Contains the settings for the underlying RequesetStream.
EvtDeviceInterfaceMultipleTargetOnStateChange | Callback to the Client that indicates the state of the target has changed.
EvtDeviceInterfaceMultipleTargetOnPnpNotification | Callback to the Client that allows the Client to indicate if the target should open.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

##### DeviceInterfaceMultipleTarget_StateType
Enum to specify IO Target State.

````
typedef enum
{
    DeviceInterfaceMultipleTarget_StateType_Invalid,
    DeviceInterfaceMultipleTarget_StateType_Open,
    DeviceInterfaceMultipleTarget_StateType_QueryRemove,
    // NOTE: This name is not correct. The correct name is on next line, but old name is kept for backward compatibility.
    //
    DeviceInterfaceMultipleTarget_StateType_QueryRemoveCancelled,
    DeviceInterfaceMultipleTarget_StateType_RemoveCancel = DeviceInterfaceMultipleTarget_StateType_QueryRemoveCancelled,
    // NOTE: This name is not correct. The correct name is on next line, but old name is kept for backward compatibility.
    //
    DeviceInterfaceMultipleTarget_StateType_QueryRemoveComplete,
    DeviceInterfaceMultipleTarget_StateType_RemoveComplete = DeviceInterfaceMultipleTarget_StateType_QueryRemoveComplete,
    DeviceInterfaceMultipleTarget_StateType_Close,
    DeviceInterfaceMultipleTarget_StateType_Maximum
} DeviceInterfaceMultipleTarget_StateType;
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
    // Module is opened in when Module is created.
    //
    DeviceInterfaceMultipleTarget_PnpRegisterWhen_Create
} DeviceInterfaceMultipleTarget_PnpRegisterWhen_Type;
````
-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChange
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChange(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target,
    _In_ DeviceInterfaceMultipleTarget_StateType IoTargetState
    );
````

Callback to the Client that indicates the state of the target has changed.
This version is included for legacy Clients. Use the Ex version instead.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | Target instance associated with the callback.
IoTargetState | The new state the WDFIOTARGET is transitioning to.

##### Remarks
* It is not possible to veto the remove or open using this callback. Use Ex version instead.

##### EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChangeEx
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChangeEx(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target,
    _In_ DeviceInterfaceMultipleTarget_StateType IoTargetState
    );
````

Callback to the Client that indicates the state of the target has changed.
This version allows Client to veto the remove.
This version allows Client to veto the open.

##### Returns

* In QueryRemove case, if NT_SUCCESS(of the return value) is TRUE then remove is allowed. Otherwise, remove is vetoed.
* In Open case, if NT_SUCCESS(of the return value) is TRUE then open is allowed. Otherwise, open is vetoed.
* In all other cases, return STATUS_SUCCESS.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | Target instance associated with the callback.
IoTargetState | The new state the WDFIOTARGET is transitioning to.

##### Remarks

##### EVT_DMF_DeviceInterfaceMultipleTarget_OnPnpNotification
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_DeviceInterfaceMultipleTarget_OnPnpNotification(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName,
    _Out_ BOOLEAN* IoTargetOpen
    );
````

Callback to the Client that allows the Client to indicate if the target should open.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
SymbolicLinkName | Symbolic link name of the WDFIOTARGET.
IoTargetOpen | The address where the Client writes TRUE to indicate that the WDFIOTARGET should open or FALSE if the WDFIOTARGET should not open.

##### Remarks

* See DMF_ContinuousRequestTarget.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_DeviceInterfaceMultipleTarget_BufferPut

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
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | The given DMFIOTARGET handle.
ClientBuffer | The given DMF_BufferPool buffer.

##### Remarks

* NOTE: ClientBuffer must be a properly formed DMF_BufferPool buffer.

##### DMF_DeviceInterfaceMultipleTarget_Cancel

````
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_DeviceInterfaceMultipleTarget_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    );
````

This Method cancels the underlying WDFREQUEST associated with a given DmfRequestIdCancel.

##### Returns

TRUE if the underlying WDFREQUEST has been canceled.
FALSE if the underlying WDFREQUEST could not be canceled because it has been completed or is being completed.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
DmfRequestIdCancel | The unique request id returned by `DMF_DeviceInterfaceMultipleTarget_SendEx()`.

##### Remarks
* **Caller must use DMF_DeviceInterfaceMultipleTarget_Cancel() to cancel the DmfRequestIdCancel returned by `DMF_DeviceInterfaceMultipleTarget_SendEx()`. Caller may not use WdfRequestCancel() because 
the Module may asynchronously process, complete and delete the underlying WDFREQUEST at any time.**
** 

##### DMF_DeviceInterfaceMultipleTarget_Get

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
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | The given DMFIOTARGET handle.
IoTarget | The address where the Client reads the WDFIOTARGET handle after this Method completes.

##### Remarks

* Clients should call DMF_ModuleReference() before using the returned handle in order to synchronize with possible asynchronous removal.
* Clients should call DMF_ModuleDereferece() after using the returned handle in order to synchronize with possible asynchronous removal.

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

##### DMF_DeviceInterfaceMultipleTarget_ReuseCreate

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_ReuseCreate(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _Out_ DeviceInterfaceMultipleTarget_DmfRequestReuse* DmfRequestIdReuse
    );
````

Creates a WDFREQUEST that will be reused one or more times with the "Reuse" Methods.
The WDFREQUEST is associated with a specific given Target instance.

##### Returns

NTSTATUS. Fails if a WDFREQUEST cannot be created.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | The given Target instance.
DmfRequestIdReuse | Address where the created WDFREQUEST's cookie is returned. Use this cookie with the other "Reuse" Methods.

##### Remarks

* The driver must have a corresponding call to `DMF_DeviceInterfaceMultipleTarget_ReuseDelete` in order to free memory associated with a call to this Method.

##### DMF_DeviceInterfaceMultipleTarget_ReuseDelete

````
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_DeviceInterfaceMultipleTarget_ReuseDelete(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ DeviceInterfaceMultipleTarget_DmfRequestReuse DmfRequestIdReuse
    );
````

Deletes a WDFREQUEST that was previously created using `DMF_DeviceInterfaceMultipleTarget_ReuseCreate`.
The WDFREQUEST is associated with a specific given Target instance.

##### Returns

    TRUE if the WDFREQUEST was found and deleted.
    FALSE if the WDFREQUEST was not found.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | The given Target instance.
DmfRequestIdReuse | Associated cookie of the WDFREQUEST to delete.

##### Remarks

##### DMF_DeviceInterfaceMultipleTarget_ReuseSend

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_ReuseSend(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ DeviceInterfaceMultipleTarget_DmfRequestReuse DmfRequestIdReuse,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ DeviceInterfaceMultipleTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_DeviceInterfaceMultipleTarget_SendCompletion* EvtDeviceInterfaceMultipleTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ DeviceInterfaceMultipleTarget_DmfRequestCancel* DmfRequestIdCancel
    );
````

Reuses a given WDFREQUEST created by `DMF_DeviceInterfaceMultipleTarget_ReuseCreate` Method. Attaches buffers, prepares it to be sent to WDFIOTARGET and sends it.
The WDFREQUEST is associated with a specific given Target instance.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Module's internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | The given Target instance.
DmfRequestIdReuse | Associated cookie of the WDFREQUEST to send.
RequestBuffer | The Client buffer that is sent to this Module's underlying WDFIOTARGET.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data from this Module's underlying WDFIOTARGET.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to this Module's underlying WDFIOTARGET.
RequestIoctl | The IOCTL that tells the Module's underlying WDFIOTARGET the purpose of the associated Request that is sent.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
EvtDeviceInterfaceMultipleTargetSingleAsynchronousRequest | The Client callback that is called when this Module's underlying WDFIOTARGET completes the request.
SingleAsynchronousRequestClientContext | The Client specific context that is sent to EvtDeviceInterfaceMultipleTargetSingleAsynchronousRequest.
DmfRequestIdCancel | Returns a unique id associated with the underlying WDFREQUEST. Client may use this id to cancel the asynchronous transaction.

##### Remarks

* Caller passes `DmfRequestIdCancel` when it is possible that the caller may want to cancel the WDFREQUEST that was created and
sent to the underlying WDFIOTARGET.
* **Caller must use `DMF_DeviceInterfaceMultipleTarget_Cancel()` to cancel the WDFREQUEST associated with DmfRequestIdCancel. Caller may not use WdfRequestCancel() because 
the Module may asynchronously process, complete and delete the underlying WDFREQUEST at any time.**
** 
* **Caller must not use value returned in DmfRequestIdCancel for any purpose except to pass it `DMF_DeviceInterfaceMultipleTarget_Cancel()`.** For example, do not assign a context to the handle.

##### DMF_DeviceInterfaceMultipleTarget_Send

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
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
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
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

##### DMF_DeviceInterfaceMultipleTarget_SendEx

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_SendEx(
  _In_ DMFMODULE DmfModule,
  _In_ DMFIOTARGET Target,
  _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ DeviceInterfaceMultipleTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
  _In_opt_ VOID* SingleAsynchronousRequestClientContext,
  _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
  );
````

This Method uses the given parameters to create a Request and send it asynchronously to the given DMFIOTARGET.
Ex version of DMF_DeviceInterfaceMultipleTarget_Send allows the clients to specify ContinuousRequestTarget_CompletionOptions, which controls how completion routine will be called. 

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
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
DmfRequestIdCancel | Returns a unique id associated with the underlying WDFREQUEST. Client may use this id to cancel the asynchronous transaction.

##### Remarks
* Caller passes `DmfRequestIdCancel` when it is possible that the caller may want to cancel the WDFREQUEST that was created and
sent to the underlying WDFIOTARGET.
* **Caller must use `DMF_DeviceInterfaceMultipleTarget_Cancel()` to cancel the WDFREQUEST associated with DmfRequestIdCancel. Caller may not use WdfRequestCancel() because 
the Module may asynchronously process, complete and delete the underlying WDFREQUEST at any time.**
** 
* **Caller must not use value returned in DmfRequestIdCancel for any purpose except to pass it `DMF_DeviceInterfaceMultipleTarget_Cancel()`.** For example, do not assign a context to the handle.

##### DMF_DeviceInterfaceMultipleTarget_SendSynchronously

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_ DMFIOTARGET Target,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
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
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
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

##### DMF_DeviceInterfaceMultipleTarget_StreamStart

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
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | The given DMFIOTARGET handle.

##### Remarks

##### DMF_DeviceInterfaceMultipleTarget_StreamStop

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
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | The given DMFIOTARGET handle.

##### Remarks

##### DMF_DeviceInterfaceMultipleTarget_TargetDereference

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
VOID
DMF_DeviceInterfaceMultipleTarget_TargetDereference(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    );
````

Releases a reference to the WDFIOTARGET associated with the given Target.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | The given DMFIOTARGET handle.

##### Remarks

* Use this Method instead of DMF_ModuleDereference() because there more than one WDFIOTARGET may be open.

##### DMF_DeviceInterfaceMultipleTarget_TargetReference

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_TargetReference(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    );
````

Acquires a reference to the WDFIOTARGET associated with the given Target.

##### Returns

STATUS_SUCCESS indicates that the given Target is valid and will remain available until
`DMF_DeviceInterfaceMultipleTarget_TargetDereference` is called.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DeviceInterfaceMultipleTarget Module handle.
Target | The given DMFIOTARGET handle.

##### Remarks

* Use this Method instead of DMF_ModuleReference() because there more than one WDFIOTARGET may be open.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE.
* This Module does all the work of allocating the buffers and Requests as specified by the Client.
* This Module stops and start streaming automatically during power transition.
* This Module is similar to the USB Continuous Reader in WDF but for any WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Targets

-----------------------------------------------------------------------------------------------------------------------------------

