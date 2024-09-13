## DMF_RequestTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

Implements a driver pattern that sends IOCTL requests to a WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

##### ContinuousRequestTarget_RequestType
Enum to specify the type of request

````
typedef enum
{
  ContinuousRequestTarget_RequestType_Invalid,
  ContinuousRequestTarget_RequestType_InternalIoctl,
  ContinuousRequestTarget_RequestType_Ioctl,
  ContinuousRequestTarget_RequestType_Read,
  ContinuousRequestTarget_RequestType_Write
} ContinuousRequestTarget_RequestType;
````
Member | Description
----|----
ContinuousRequestTarget_RequestType_InternalIoctl | Indicates that the request is sent to the Internal Device I/O Control handler in the underlying WDFIOTARGET.
ContinuousRequestTarget_RequestType_Ioctl | Indicates that the request is sent to the Device I/O Control handler in the underlying WDFIOTARGET.
ContinuousRequestTarget_RequestType_Read | Indicates that the request is sent to the File Read handler in the underlying WDFIOTARGET.
ContinuousRequestTarget_RequestType_Write | Indicates that the request is sent to the File Write handler in the underlying WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_RequestTarget_SendCompletion
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_RequestTarget_SendCompletion(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    );
````

Callback to the Client when a request is completed by the target WDFIOTARGET.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RequestTarget Module handle.
ClientRequestContext | The Client specific context the Client sent when sending the request. Usually, this is the corresponding WDFREQUEST.
InputBuffer | Contains the corresponding WDFREQUEST input buffer.
InputBufferBytesWritten | Number of bytes written. For IOCTL this will be actual size of the Input Buffer. 
OutputBuffer | Contains the corresponding WDFREQUEST output buffer.
OutputBufferBytesRead | Number of bytes read.
CompletionStatus | The completion status sent by the underlying WDFIOTARGET.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_RequestTarget_Cancel

````
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_RequestTarget_Cancel(
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
DmfModule | An open DMF_RequestTarget Module handle.
DmfRequestIdCancel | The unique request id returned by `DMF_RequestTarget_SendEx()`.

##### Remarks
* **Caller must use DMF_RequestTarget_Cancel() to cancel the DmfRequestIdCancel returned by `DMF_RequestTarget_SendEx()`. Caller may not use WdfRequestCancel() because 
the Module may asynchronously process, complete and delete the underlying WDFREQUEST at any time.**
** 

##### DMF_RequestTarget_IoTargetClear

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestTarget_IoTargetClear(
  _In_ DMFMODULE DmfModule
  );
````

Clears the underlying WDFIOTARGET to the given WDFIOTARGET.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RequestTarget Module handle.

##### Remarks

##### DMF_RequestTarget_IoTargetSet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestTarget_IoTargetSet(
  _In_ DMFMODULE DmfModule,
  _In_ WDFIOTARGET IoTarget
  );
````

Sets the underlying WDFIOTARGET to the given WDFIOTARGET.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RequestTarget Module handle.
IoTarget | The given WDFIOTARGET. It must already be created and opened.

##### Remarks

##### DMF_RequestTarget_ReuseCreate

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RequestTarget_ReuseCreate(
    _In_ DMFMODULE DmfModule,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    );
````

Creates a WDFREQUEST that will be reused one or more times with the "Reuse" Methods.

##### Returns

NTSTATUS. Fails if a WDFREQUEST cannot be created.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RequestTarget Module handle.
DmfRequestIdReuse | Address where the created WDFREQUEST's cookie is returned. Use this cookie with the other "Reuse" Methods.

##### Remarks

* The driver must have a corresponding call to `DMF_RequestTarget_ReuseDelete` in order to free memory associated with a call to this Method.

##### DMF_RequestTarget_ReuseDelete

````
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_RequestTarget_ReuseDelete(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    );
````

Deletes a WDFREQUEST that was previously created using `DMF_RequestTarget_ReuseCreate`.

##### Returns

    TRUE if the WDFREQUEST was found and deleted.
    FALSE if the WDFREQUEST was not found.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RequestTarget Module handle.
DmfRequestIdReuse | Associated cookie of the WDFREQUEST to delete.

##### Remarks

##### DMF_RequestTarget_ReuseSend

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RequestTarget_ReuseSend(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ RequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    );
````

Reuses a given WDFREQUEST created by `DMF_RequestTarget_ReuseCreate` Method. Attaches buffers, prepares it to be sent to WDFIOTARGET and sends it.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Module's internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RequestTarget Module handle.
DmfRequestIdReuse | Associated cookie of the WDFREQUEST to send.
RequestBuffer | The Client buffer that is sent to this Module's underlying WDFIOTARGET.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data from this Module's underlying WDFIOTARGET.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to this Module's underlying WDFIOTARGET.
RequestIoctl | The IOCTL that tells the Module's underlying WDFIOTARGET the purpose of the associated Request that is sent.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
EvtRequestTargetSingleAsynchronousRequest | The Client callback that is called when this Module's underlying WDFIOTARGET completes the request.
SingleAsynchronousRequestClientContext | The Client specific context that is sent to EvtRequestTargetSingleAsynchronousRequest.
DmfRequestIdCancel | Returns a unique id associated with the underlying WDFREQUEST. Client may use this id to cancel the asynchronous transaction.

##### Remarks

* Caller passes `DmfRequestIdCancel` when it is possible that the caller may want to cancel the WDFREQUEST that was created and
sent to the underlying WDFIOTARGET.
* **Caller must use `DMF_RequestTarget_Cancel()` to cancel the WDFREQUEST associated with DmfRequestIdCancel. Caller may not use WdfRequestCancel() because 
the Module may asynchronously process, complete and delete the underlying WDFREQUEST at any time.**
** 
* **Caller must not use value returned in DmfRequestIdCancel for any purpose except to pass it `DMF_RequestTarget_Cancel()`.** For example, do not assign a context to the handle.

##### DMF_RequestTarget_Send

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RequestTarget_Send(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ ContinuousRequestTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
  _In_opt_ VOID* SingleAsynchronousRequestClientContext
  );
````

Creates a WDFREQUEST and asynchronously sends it to the DMF_RequestTarget Module instance's WDFIOTARGET.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RequestTarget Module handle.
RequestBuffer | The input buffer.
RequestLength | The size in bytes of the input buffer.
ResponseBuffer | The output buffer.
ResponseLength | The size in bytes of the output buffer.
RequestType | Indicates the type of request to send to the underlying WDFIOTARGET.
RequestIoctl | The IOCTL code to send to the underlying WDFIOTARGET.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
EvtRequestTargetSingleAsynchronousRequest | The Client callback that is called when request completes.
SingleAsynchronousRequestClientContext | A Client specific context that is sent to the Client callback that is called when the request completes.

##### Remarks

##### DMF_RequestTarget_SendEx

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RequestTarget_SendEx(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ ContinuousRequestTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
  _In_opt_ VOID* SingleAsynchronousRequestClientContext,
  _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
  );
````

Creates a WDFREQUEST and asynchronously sends it to the DMF_RequestTarget Module instance's WDFIOTARGET.
Ex version of DMF_RequestTarget_Send, allows the clients to specify ContinuousRequestTarget_CompletionOptions, which controls how completion routine will be called. 

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RequestTarget Module handle.
RequestBuffer | The input buffer.
RequestLength | The size in bytes of the input buffer.
ResponseBuffer | The output buffer.
ResponseLength | The size in bytes of the output buffer.
RequestType | Indicates the type of request to send to the underlying WDFIOTARGET.
RequestIoctl | The IOCTL code to send to the underlying WDFIOTARGET.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
EvtRequestTargetSingleAsynchronousRequest | The Client callback that is called when request completes.
SingleAsynchronousRequestClientContext | A Client specific context that is sent to the Client callback that is called when the request completes.
DmfRequestIdCancel | Returns a unique id associated with the underlying WDFREQUEST. Client may use this id to cancel the asynchronous transaction.

##### Remarks

* Caller passes `DmfRequestIdCancel` when it is possible that the caller may want to cancel the WDFREQUEST that was created and
sent to the underlying WDFIOTARGET.
* **Caller must use `DMF_RequestTarget_Cancel()` to cancel the WDFREQUEST associated with DmfRequestIdCancel. Caller may not use WdfRequestCancel() because 
the Module may asynchronously process, complete and delete the underlying WDFREQUEST at any time.**
** 
* **Caller must not use value returned in DmfRequestIdCancel for any purpose except to pass it `DMF_RequestTarget_Cancel()`.** For example, do not assign a context to the handle.

##### DMF_RequestTarget_SendSynchronously

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RequestTarget_SendSynchronously(
  _In_ DMFMODULE DmfModule,
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

Creates a WDFREQUEST and sends it to the DMF_RequestTarget Module instance's WDFIOTARGET and waits for that request to be completed.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RequestTarget Module handle.
RequestBuffer | The input buffer.
RequestLength | The size in bytes of the input buffer.
ResponseBuffer | The output buffer.
ResponseLength | The size in bytes of the output buffer.
RequestType | Indicates the type of request to send to the underlying WDFIOTARGET.
RequestIoctl | The IOCTL code to send to the underlying WDFIOTARGET.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
BytesWritten | Indicates how many bytes were written to the output buffer by the underlying WDFIOTARGET.

##### Remarks

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

* DMF_ContinuousRequestTarget
* DMF_DeviceInterfaceTarget
* DMF_SelfTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Targets

-----------------------------------------------------------------------------------------------------------------------------------

