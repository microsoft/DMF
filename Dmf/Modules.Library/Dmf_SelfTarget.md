## DMF_SelfTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Provides support so a Client can send a Request to its own stack.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

* None

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

##### DMF_SelfTarget_Get

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SelfTarget_Get(
  _In_ DMFMODULE DmfModule,
  _Out_ WDFIOTARGET* IoTarget
  );
````

This Method gives the caller access to the underlying WDFIOTARGET.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SelfTarget Module handle.
IoTarget | The WDFIOTARGET that accepts Requests that will then be forwarded to the Client driver's stack.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SelfTarget_Send

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SelfTarget_Send(
  _In_ DMFMODULE DmfModule,
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

This Method allows the Caller to send an asynchronous request to the Client driver's stack.

##### Returns

NTSTATUS indicating what the Client drivers' stack returned.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SelfTarget Module handle.
RequestBuffer | The Client buffer sent via the Request to the Client driver's stack.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data via the Request sent to the Client driver's stack.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to the Client driver's stack.
RequestIoctl | The IOCTL code of the Request to send to the Client driver's stack.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
EvtContinuousRequestTargetSingleAsynchronousRequest | A callback called when the asynchronous Request that is sent to the Client driver's stack completes.
SingleAsynchronousRequestClientContext | The Client specific context passed to EvtContinuousRequestTargetSingleAsynchronousRequest.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SelfTarget_SendSynchronously

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SelfTarget_SendSynchronously(
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

This Method allows the Caller to send a synchronous request to the Client driver's stack.

##### Returns

NTSTATUS indicating what the Client drivers' stack returned.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SelfTarget Module handle.
RequestBuffer | The Client buffer sent via the Request to the Client driver's stack.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data via the Request sent to the Client driver's stack.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to the Client driver's stack.
RequestIoctl | The IOCTL code of the Request to send to the Client driver's stack.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
BytesWritten | Contains the number of bytes transferred via the Request sent to the Client driver's stack.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module makes it easy to send a Request to the driver's own stack.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_ContinuousRequestTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* This Module acquires a WDFIOTARGET to its own stack when the Module opens.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Targets

-----------------------------------------------------------------------------------------------------------------------------------

