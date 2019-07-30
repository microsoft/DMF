## DMF_RequestTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a driver pattern that sends IOCTL requests to a WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
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

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
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

-----------------------------------------------------------------------------------------------------------------------------------

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

-----------------------------------------------------------------------------------------------------------------------------------

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

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RequestTarget_Send

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_Send(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
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

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RequestTarget_SendEx

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_SendEx(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ ContinuousRequestTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _In_ ContinuousRequestTarget_CompletionOptions CompletionOption,
  _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestTargetSingleAsynchronousRequest,
  _In_opt_ VOID* SingleAsynchronousRequestClientContext
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
CompletionOption | Indicates the completion option associated with the completion routine.
EvtRequestTargetSingleAsynchronousRequest | The Client callback that is called when request completes.
SingleAsynchronousRequestClientContext | A Client specific context that is sent to the Client callback that is called when the request completes.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_RequestTarget_SendSynchronously

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_RequestTarget_SendSynchronously(
  _In_ DMFMODULE DmfModule,
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

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE.
* This Module does all the work of allocating the buffers and Requests as specified by the Client.
* This Module stops and start streaming automatically during power transition.
* This Module is similar to the USB Continuous Reader in WDF but for any WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_BufferPool (3)

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

-----------------------------------------------------------------------------------------------------------------------------------

Targets

-----------------------------------------------------------------------------------------------------------------------------------

