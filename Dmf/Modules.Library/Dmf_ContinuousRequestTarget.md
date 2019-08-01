## DMF_ContinuousRequestTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a driver pattern that streams IOCTL requests to a WDFIOTARGET. This Module automatically creates buffers and WDFREQUESTS
for both input and output data performs all the necessary operations to attach those buffers to WDFREQUESTS.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_ContinuousRequestTarget
````
typedef struct
{
  // Number of Asynchronous requests.
  //
  ULONG ContinuousRequestCount;
  // Number of Input Buffers.
  //
  ULONG BufferCountInput;
  // Number of Output Buffers.
  //
  ULONG BufferCountOutput;
  // Request type.
  //
  ContinuousRequestTarget_RequestType RequestType;
  // Size of input buffer for each request.
  //
  ULONG BufferInputSize;
  // Size of Client Driver Input Buffer Context.
  //
  ULONG BufferContextInputSize;
  // Size of output buffer for each request.
  //
  ULONG BufferOutputSize;
  // Size of Client Driver Output Buffer Context.
  //
  ULONG BufferContextOutputSize;
  // Indicates if a look aside list should be used for output buffer
  // This is not required for input buffer since input is not cached.
  //
  ULONG EnableLookAsideOutput;
  // Pool Type for Input Buffer.
  //
  POOL_TYPE PoolTypeInput;
  // Pool Type for Output Buffer.
  //
  POOL_TYPE PoolTypeOutput;
  // Input buffer callback.
  //
  EVT_DMF_ContinuousRequestTarget_BufferInput* EvtContinuousRequestTargetBufferInput;
  // Output buffer callback.
  //
  EVT_DMF_ContinuousRequestTarget_BufferOutput* EvtContinuousRequestTargetBufferOutput;
  // IOCTL to send for ContinuousRequestTarget_RequestType_Ioctl or ContinuousRequestTarget_RequestType_InternalIoctl.
  //
  ULONG ContinuousRequestTargetIoctl;
  // Flag to indicate whether to Purge target in D0Exit and Start in D0Entry
  // This flag should be set to TRUE, if IO target needs to process all the requests
  // before entering low power.
  //
  BOOLEAN PurgeAndStartTargetInD0Callbacks;
  // Indicates the mode of ContinuousRequestTarget.
  //
  ContinuousRequestTarget_ModeType ContinuousRequestTargetMode;
} DMF_CONFIG_ContinuousRequestTarget;
````
Member | Description
----|----
ContinuousRequestCount | Indicates how many simultaneous asynchronous Requests are created and sent to the underlying WDFIOTARGET.
BufferCountInput | The number of input buffers to create and enqueue.
BufferCountOutput | The number of output buffers to create and enqueue.
ContinuousRequestTarget_RequestType RequestType | The type of Requests to create for this instance of this Module.
BufferInputSize | Size of each input DMF_BufferPool buffer.
BufferContextInputSize | Size of each input DMF_BufferPool's buffer's context.
BufferOutputSize | Size of each output DMF_BufferPool buffer.
BufferContextOutputSize | Size of each output DMF_BufferPool's buffer's context.
EnableLookAsideOutput | Indicates whether or not the underlying DMF_BufferPool’s are opened in Infinite Mode.
PoolTypeInput | Type if memory for input buffers.
PoolTypeOutput | Type of memory for output buffers.
EvtContinuousRequestTargetBufferInput | The callback that allows the Client to populate input buffers before they are sent to the underlying target.
EvtContinuousRequestTargetBufferOutput | The callback that allows the Client to read data from output buffers that have been returned by the underlying target.
ClientContext | Client specific context that is sent to the Client callback functions.
ContinuousRequestTargetIoctl | The IOCTL that is set in the Requests that are sent to the underlying target.
PurgeAndStartTargetInD0Callbacks | Indicates that streaming should be stopped in D3 and started in D0.
ContinuousRequestTargetMode | Indicates the mode of ContinuousRequestTarget.

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
ContinuousRequestTarget_RequestType_InternalIoctl | The Requests will be sent to Internal Device Io Control handler of the underlying WDFIOTARGET.
ContinuousRequestTarget_RequestType_Ioctl | The Requests will be sent to Device Io Control handler of the underlying WDFIOTARGET.
ContinuousRequestTarget_RequestType_Read | The Requests will be sent to Read handler of the underlying WDFIOTARGET.
ContinuousRequestTarget_RequestType_Write | The Requests will be sent to Write handler of the underlying WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------
##### ContinuousRequestTarget_BufferDisposition
Enum to specify who own's the output buffer and whether or not to continue streaming the request or stop

````
typedef enum
{
  ContinuousRequestTarget_BufferDisposition_Invalid,
  ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming,
  ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming,
  ContinuousRequestTarget_BufferDisposition_ClientAndContinueStreaming,
  ContinuousRequestTarget_BufferDisposition_ClientAndStopStreaming,
  ContinuousRequestTarget_BufferDisposition_Maximum
} ContinuousRequestTarget_BufferDisposition;
````
Member | Description
----|----
ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming | DMF_ContinuousRequestTarget will own the buffer and streaming will continue. Client must not access the buffer after the callback returns.
ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming | DMF_ContinuousRequestTarget will own the buffer and streaming will stop. Client must not access the buffer after the callback returns.
ContinuousRequestTarget_BufferDisposition_ClientAndContinueStreaming | Client will own the buffer and streaming will continue. Client may access the buffer after the callback returns. Client must eventually return the buffer to DMF_ContinuousRequestTarget.
ContinuousRequestTarget_BufferDisposition_ClientAndStopStreaming | Client will own the buffer and streaming will stop. Client may access the buffer after the callback returns. Client must eventually return the buffer to DMF_ContinuousRequestTarget.

-----------------------------------------------------------------------------------------------------------------------------------
##### ContinuousRequestTarget_ModeType
These definitions indicate the mode of ContinuousRequestTarget.
Indicates how and when the Requests start and stop streaming.

````
typedef enum
{
  ContinuousRequestTarget_Mode_Invalid = 0,
  ContinuousRequestTarget_Mode_Automatic,
  ContinuousRequestTarget_Mode_Manual,
  ContinuousRequestTarget_Mode_Maximum,
} ContinuousRequestTarget_ModeType;
````
Member | Description
----|----
ContinuousRequestTarget_Mode_Automatic | DMF_ContinuousRequestTarget_Start invoked automatically on DMF_ContinuousRequestTarget_IoTargetSet and DMF_ContinuousRequestTarget_Stop invoked automatically on DMF_ContinuousRequestTarget_IoTargetClear.
ContinuousRequestTarget_Mode_Manual | DMF_ContinuousRequestTarget_Start and DMF_ContinuousRequestTarget_Stop must be called explicitly by the Client when needed.

-----------------------------------------------------------------------------------------------------------------------------------
##### ContinuousRequestTarget_CompletionOptions
These definitions indicate the completion options associated with Completion routine.
Indicates how and when the completion routine should be called.

````
typedef enum
{
    ContinuousRequestTarget_CompletionOptions_Default = 0,
    ContinuousRequestTarget_CompletionOptions_Passive,
    ContinuousRequestTarget_CompletionOptions_Maximum,
} ContinuousRequestTarget_CompletionOptions;
````
Member | Description
----|----
ContinuousRequestTarget_CompletionOptions_Default | EVT_DMF_ContinuousRequestTarget_SendCompletion will be called at dispatch level.
ContinuousRequestTarget_Mode_Manual | EVT_DMF_ContinuousRequestTarget_SendCompletion will be called at passive level.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_ContinuousRequestTarget_BufferInput
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ContinuousRequestTarget_BufferInput(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(*InputBufferSize) VOID* InputBuffer,
    _Out_ size_t* InputBufferSize,
    _In_ VOID* ClientBuferContextInput
    );
````

Allows the Client to populate the Input buffers after they are created and before they are sent to the underlying WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.
InputBuffer | The input buffer that the Client will populate.
InputBufferSize | The size of InputBuffer in bytes.
ClientBuferContextInput | The DMF_BufferPool buffer's Client specific context that corresponds with InputBuffer.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_ContinuousRequestTarget_BufferOutput
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ContinuousRequestTarget_BufferDisposition
EVT_DMF_ContinuousRequestTarget_BufferOutput(
    _In_ DMFMODULE DmfModule,
    _In_reads_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _In_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    );
````

Allows the Client to access the output buffers that are returned by the underlying WDFIOTARGET.

##### Returns

Tells how DMF_ContinuousRequestTarget will deal with the output buffer after calling the Client's callback. Indicates who owns the
buffer, Client or DMF_ContinuousRequestTarget.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.
OutputBuffer | The output buffer that contains the data returned by the underlying WDFIOTARGET.
OutputBufferSize | The size of OutputBuffer.
ClientBuferContextOutput | The DMF_BufferPool buffer's Client specific context associated with OutputBuffer.
CompletionStatus | The return NTSTATUS in the associated Request returned by the underlying WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_ContinuousRequestTarget_SendCompletion
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ContinuousRequestTarget_SendCompletion(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    );
````

Allows the Client to access the output buffers that are returned by the underlying WDFIOTARGET (for a single asynchronous call).

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.
ClientRequestContext | A Client specific context. Usually it is a WDFREQUEST.
InputBuffer | The input buffer that contains the data sent to the underlying WDFIOTARGET.
InputBufferBytesWritten | Number of bytes written. For IOCTL this will be actual size of the Input Buffer. 
OutputBuffer | The output buffer that contains the data returned by the underlying WDFIOTARGET.
OutputBufferBytesRead | Number of bytes read.
CompletionStatus | The return NTSTATUS in the associated Request returned by the underlying WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_ContinuousRequestTarget_BufferPut

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ContinuousRequestTarget_BufferPut(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* ClientBuffer
  );
````

This Method adds a given DMF_BufferPool buffer back into this Module's Output DMF_BufferPool.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.
ClientBuffer | The given DMF_BufferPool buffer.

##### Remarks

* NOTE: The given DMF_BufferPool buffer must be a properly formed DMF_BufferPool buffer.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_ContinuousRequestTarget_IoTargetClear

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ContinuousRequestTarget_IoTargetClear(
  _In_ DMFMODULE DmfModule
  );
````

Clears the WDFIOTARGET that receives Requests from an instance of this Module.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_ContinuousRequestTarget_IoTargetSet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_IoTargetSet(
  _In_ DMFMODULE DmfModule,
  _In_ WDFIOTARGET IoTarget
  );
````

Sets the internal WDFIOTARGET to a given WDFIOTARGET that receives Requests from an instance of this Module.
If ContinuousRequestTargetMode is set to ContinuousRequestTarget_Mode_Automatic then DMF_ContinuousRequestTarget_Start
will be invoked.

##### Returns

NTSTATUS. Fails if Requests cannot be sent in ContinuousRequestTarget_Mode_Automatic mode.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.
IoTarget | The given WDFIOTARGET.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_ContinuousRequestTarget_Send

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_Send(
  _In_ DMFMODULE DmfModule,
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

This Method uses the given parameters to create a Request and send it asynchronously to the Module's underlying WDFIOTARGET.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.
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

##### DMF_ContinuousRequestTarget_SendEx

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_SendEx(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ ContinuousRequestTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _In_ ContinuousRequestTarget_CompletionOptions CompletionOption,
  _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
  _In_opt_ VOID* SingleAsynchronousRequestClientContext
  );
````

This Method uses the given parameters to create a Request and send it asynchronously to the Module's underlying WDFIOTARGET.
Ex version of DMF_RequestTarget_Send, allows the clients to specify ContinuousRequestTarget_CompletionOptions, which controls how completion routine will be called. 

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.
RequestBuffer | The Client buffer that is sent to this Module's underlying WDFIOTARGET.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data from this Module's underlying WDFIOTARGET.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to this Module's underlying WDFIOTARGET.
RequestIoctl | The IOCTL that tells the Module's underlying WDFIOTARGET the purpose of the associated Request that is sent.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
CompletionOption | Completion option associated with the completion routine.
EvtContinuousRequestTargetSingleAsynchronousRequest | The Client callback that is called when this Module's underlying WDFIOTARGET completes the request.
SingleAsynchronousRequestClientContext | The Client specific context that is sent to EvtContinuousRequestTargetSingleAsynchronousRequest.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_ContinuousRequestTarget_SendSynchronously

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_SendSynchronously(
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

This Method uses the given parameters to create a Request and send it synchronously to the Module's underlying WDFIOTARGET.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.
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

##### DMF_ContinuousRequestTarget_Start

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ContinuousRequestTarget_Start(
  _In_ DMFMODULE DmfModule
  );
````

Starts streaming (causes the created Requests and associated buffers to be sent to the underlying WDFIOTARGET).

##### Returns

NTSTATUS. Fails if Requests cannot be sent to the underlying WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_ContinuousRequestTarget_Stop

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ContinuousRequestTarget_Stop(
  _In_ DMFMODULE DmfModule
  );
````

Stops streaming. The pending requests are canceled and no new Requests will be sent to the underlying WDFIOTARGET.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_ContinuousRequestTarget_StopAndWait

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContinuousRequestTarget_StopAndWait(
  _In_ DMFMODULE DmfModule
  );
````

Stops streaming. The pending requests are canceled and no new Requests will be sent to the underlying WDFIOTARGET.
This Method will block until all the pending requests have been returned.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ContinuousRequestTarget Module handle.

##### Remarks
* Clients should use this Method prior to the Close of a Parent Module or when the Client Driver will be disabled.
-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. Also, it causes the completion routine executed during streaming to run at PASSIVE_LEVEL.
* This Module does all the work of allocating the buffers and Requests as specified by the Client.
* This Module stops and start streaming automatically during power transition.
* This Module is similar to the USB Continuous Reader in WDF but for any WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_BufferPool (3)
* DMF_QueuedWorkitem (2)

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DMF_DeviceInterfaceTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Targets

-----------------------------------------------------------------------------------------------------------------------------------

