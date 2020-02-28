## DMF_DefaultTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a driver pattern that streams IOCTL requests to the default WDFIOTARGET (the next driver in the stack). This Module
automatically creates buffers and WDFREQUESTS for both input and output data performs all the necessary operations to attach
those buffers to WDFREQUESTS.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_DefaultTarget
````
typedef struct
{
  // Module Config for Child Module.
  //
  DMF_CONFIG_ContinuousRequestTarget ContinuousRequestTargetModuleConfig;
} DMF_CONFIG_DefaultTarget;
````
Member | Description
----|----
ContinuousRequestTargetModuleConfig | Contains the settings for the underlying RequesetStream.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DefaultTarget_BufferPut

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DefaultTarget_BufferPut(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* ClientBuffer
  );
````

This Method adds a given DMF_BufferPool buffer back into this Module's Output DMF_BufferPool.

##### Returns

NTSTATUS. Fails if the given DMF_BufferPool buffer cannot be added to the list.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DefaultTarget Module handle.
ClientBuffer | The given DMF_BufferPool buffer.

##### Remarks

* NOTE: ClientBuffer must be a properly formed DMF_BufferPool buffer.
-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DefaultTarget_Cancel

````
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_DefaultTarget_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequest DmfRequest
    );
````

This Method cancels the underlying WDFREQUEST associated with a given DmfRequest.

##### Returns

TRUE if the underlying WDFREQUEST has been canceled.
FALSE if the underlying WDFREQUEST could not be canceled because it has been completed or is being completed.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DefaultTarget Module handle.
DmfRequest | A handle to a WDFREQUEST that is returned by `DMF_DefaultTarget_SendEx()`.

##### Remarks
* **Caller must use DMF_DefaultTarget_Cancel() to cancel the DmfRequest returned by `DMF_DefaultTarget_SendEx()`. Caller may not use WdfRequestCancel() because 
the Module may asynchronously process, complete and delete the underlying WDFREQUEST at any time.**
** 

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DefaultTarget_Get

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_DefaultTarget_Get(
  _In_ DMFMODULE DmfModule,
  _Out_ WDFIOTARGET* IoTarget
  );
````

Gives the Client direct access to the WDFIOTARGET handle.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DefaultTarget Module handle.
IoTarget | The address where the Client reads the WDFIOTARGET handle after this Method completes.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DefaultTarget_Send

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DefaultTarget_Send(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ DefaultTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _In_opt_ DefaultTarget_CallbackType_SingleAsynchronousBufferOutput EvtContinuousRequestTargetSingleAsynchronousRequest,
  _In_opt_ VOID* SingleAsynchronousRequestClientContext
  );
````

This Method uses the given parameters to create a Request and send it asynchronously to the Module's underlying WDFIOTARGET.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DefaultTarget Module handle.
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

##### DMF_DefaultTarget_SendEx

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DefaultTarget_SendEx(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ DefaultTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _In_ ContinuousRequestTarget_CompletionOptions CompletionOption,
  _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
  _In_opt_ VOID* SingleAsynchronousRequestClientContext,
  _Out_opt_ RequestTarget_DmfRequest* DmfRequest
  );
````

This Method uses the given parameters to create a Request and send it asynchronously to the Module's underlying WDFIOTARGET.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DefaultTarget Module handle.
RequestBuffer | The Client buffer that is sent to this Module's underlying WDFIOTARGET.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data from this Module's underlying WDFIOTARGET.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to this Module's underlying WDFIOTARGET.
RequestIoctl | The IOCTL that tells the Module's underlying WDFIOTARGET the purpose of the associated Request that is sent.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
CompletionOption | Indicates if asynchronous callback should happen at PASSIVE_LEVEL or DISPATCH_LEVEL.
EvtContinuousRequestTargetSingleAsynchronousRequest | The Client callback that is called when this Module's underlying WDFIOTARGET completes the request.
SingleAsynchronousRequestClientContext | The Client specific context that is sent to EvtContinuousRequestTargetSingleAsynchronousRequest.
DmfRequest | Optional address of the handle to the handle to the WDFREQUEST that has been sent.

##### Remarks
* Caller passes `DmfRequest` when it is possible that the caller may want to cancel the WDFREQUEST that was created and
sent to the underlying WDFIOTARGET.
* **Caller must use `DMF_DefaultTarget_Cancel()` to cancel the WDFREQUEST associated with DmfRequest. Caller may not use WdfRequestCancel() because 
the Module may asynchronously process, complete and delete the underlying WDFREQUEST at any time.**
** 
* **Caller must not use value returned in DmfRequest for any purpose except to pass it `DMF_DefaultTarget_Cancel()`.** For example, do not assign a context to the handle.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DefaultTarget_SendSynchronously

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DefaultTarget_SendSynchronously(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ DefaultTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _Out_opt_ size_t* BytesWritten
  );
````

This Method uses the given parameters to create a Request and send it synchronously to the Module's underlying WDFIOTARGET.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DefaultTarget Module handle.
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

##### DMF_DefaultTarget_StreamStart

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DefaultTarget_StreamStart(
  _In_ DMFMODULE DmfModule
  );
````

Starts streaming (causes the created Requests and associated buffers to be sent to the underlying WDFIOTARGET).

##### Returns

NTSTATUS. Fails if Requests cannot be sent to the underlying WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DefaultTarget Module handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DefaultTarget_StreamStop

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_DefaultTarget_StreamStop(
  _In_ DMFMODULE DmfModule
  );
````

Stops streaming. The pending requests are canceled and no new Requests will be sent to the underlying WDFIOTARGET.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_DefaultTarget Module handle.

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

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Targets

-----------------------------------------------------------------------------------------------------------------------------------

