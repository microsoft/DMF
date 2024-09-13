## DMF_NotifyUserWithRequest

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

Implements a driver pattern that accepts Requests (via IOCTLs) from a caller and enqueues them. When data is available to complete
the Requests, the enqueued requests are dequeued and Client specific callbacks are called that populate the Requests. Then, the
Requests are completed.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_NotifyUserWithRequest
````
typedef struct
{
    // Maximum number of pending events allowed. This is to prevent a caller from
    // submitting millions of events.
    //
    LONG MaximumNumberOfPendingRequests;
    // Maximum number of User-mode data entries stored.
    //
    LONG MaximumNumberOfPendingDataBuffers;
    // Size of User-mode data entry.
    //
    LONG SizeOfDataBuffer;
    // This Handler is optionally called when a Request is canceled.
    //
    EVT_DMF_NotifyUserWithRequest_Function* EvtPendingRequestsCancel;
    // Client Driver's Event Log Provider name to enable
    // event logging from the DMF_NotifyUserWithRequest Module.
    //
    PWSTR ClientDriverProviderName;
    // Optional callback for Client to process data before it is flushed.
    //
    EVT_DMF_BufferQueue_ReuseCleanup* EvtDataCleanup;
    // Use automatic time stamping.
    //
    BOOLEAN TimeStamping;
} DMF_CONFIG_NotifyUserWithRequest;
````
Member | Description
----|----
MaximumNumberOfPendingRequests | The maximum number of events the Client wants to enqueue. If a caller sends more Requests (via IOCTLS) simultaneously, the Module will automatically reject the call with an error.
MaximumNumberOfPendingDataBuffers | The maximum number of entries that contain data that will be returned via the Requests. If more data is available, the oldest entry in the queue is discarded.
SizeOfDataBuffer | Size of context data that is passed to the Client's callback.
EvtPendingRequestsCancel | The callback that is called Requests are canceled.
ClientDriverProviderName | Used for Event Logging purposes if the Client has this capability.
EvtDataCleanup | Callback to process queued data before it is flushed.
TimeStamping | If TRUE, this Module timestamps enqueued requests and data buffers.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_NotifyUserWithRequeset_Complete
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_NotifyUserWithRequeset_Complete(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_opt_ ULONG_PTR Context,
    _In_ NTSTATUS NtStatus
    );
````

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequest Module handle.
Request | The WDFREQUEST that has been dequeued and is ready to be returned to the Caller.
Context | Client specific context buffer. (This is usually a Client specific context which is the same for all calls.)
NtStatus | The NTSTATUS to return in the Request to the caller.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_NotifyUserWithRequest_DataBufferTimestampGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
LONGLONG
DMF_NotifyUserWithRequest_DataBufferTimestampGet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* DataBuffer
    );
````

Given a data buffer passed to Client callback, this Method returns its timestamp in clock ticks.

##### Returns

The clock tick count when the data buffer was enqueued.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequest Module handle.
DataBuffer | The given data buffer passed in the callback.

##### Remarks

* Time stamp is zero unless time stamping is enabled when the Module is instantiated using its Config.
* **Only call this Method from the callback using the supplied formal parameter.**

##### DMF_NotifyUserWithRequest_DataProcess

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_DataProcess(
  _In_ DMFMODULE DmfModule,
  _In_opt_ EVT_DMF_NotifyUserWithRequest_Function* EventCallbackFunction,
  _In_opt_ VOID* EventCallbackContext,
  _In_ NTSTATUS NtStatus
  );
````

The Client calls this Method to add data to the queue in DMF_NotifyUserWithRequest instance that holds data that will eventually
be returned to the Caller. Then, this Method tries to return the next pending Request (if any) to the Caller.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequest Module handle.
EventCallbackFunction | The callback that the Module calls which allows the Client to populate the Request that will be returned.
EventCallbackContext | A Client Specific context that is passed to the given callback.
NtStatus | The NTSTATUS to set in the Request that is to be returned.

##### Remarks

* This Method does not extract data from the queue to return. The data to return is passed to the Method.
* This Method allows pending requests to be completed when data they consume is available.
* This Method should not be called if MaximumNumberOfPendingDataBuffers in the Module's config is set to zero.

##### DMF_NotifyUserWithRequest_EventRequestAdd

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequest_EventRequestAdd(
  _In_ DMFMODULE DmfModule,
  _In_ WDFREQUEST Request
  );
````

This Method adds a given Request from a caller into a DMF_NotifyUserWithRequest instance's queue. No attempt is made to complete
the Request from this Method.

##### Returns

NTSTATUS. Fails if the Request cannot be added to the queue.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequest Module handle.
Request | The given Request.

##### Remarks

* Do not use this Method when the DMF_NotifyUserWithRequest instance enqueues data to be returned to the Caller.

##### DMF_NotifyUserWithRequest_RequestProcess

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequest_RequestProcess(
  _In_ DMFMODULE DmfModule,
  _In_ WDFREQUEST Request
  );
````

This Method adds a given Request from a caller into a DMF_NotifyUserWithRequest instance's queue. Then, an attempt is made to immediately
populate and return the Request.

##### Returns

NTSTATUS. Fails if the Request cannot be added to the queue.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequest Module handle.
Request | The given Request.

##### Remarks

* Use this Method when the DMF_NotifyUserWithRequest instance enqueues data to be returned to the Caller.
* This Method should not be called if MaximumNumberOfPendingDataBuffers in the Module's config is set to zero.

##### DMF_NotifyUserWithRequest_RequestReturn

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_RequestReturn(
  _In_ DMFMODULE DmfModule,
  _In_opt_ EVT_DMF_NotifyUserWithRequest_Function* EventCallbackFunction,
  _In_opt_ VOID* EventCallbackContext,
  _In_ NTSTATUS NtStatus
  );
````

The Client calls this Method to dequeue a single Request, populate it and return it to the Caller. The Client populates the request
using a given callback.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequest Module handle.
EventCallbackFunction | The callback that the Module calls which allows the Client to populate the Request that will be returned.
EventCallbackContext | A Client Specific context that is passed to the given callback.
NtStatus | The NTSTATUS to set in the Request that is to be returned.

##### Remarks

* This Method does not extract data from the queue to return. The data to return is passed to the Method.

##### DMF_NotifyUserWithRequest_RequestReturnAll

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_RequestReturnAll(
  _In_ DMFMODULE DmfModule,
  _In_opt_ EVT_DMF_NotifyUserWithRequest_Function* EventCallbackFunction,
  _In_opt_ VOID* EventCallbackContext,
  _In_ NTSTATUS NtStatus
  );
````

The Client calls this Method to dequeue all pending Requests, populate them with the same data and return them to the Caller.
The Client populates the Requests using a given callback.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequest Module handle.
EventCallbackFunction | The callback that the Module calls which allows the Client to populate the Request that will be returned.
EventCallbackContext | A Client Specific context that is passed to the given callback.
NtStatus | The NTSTATUS to set in the Request that is to be returned.

##### Remarks

* This Method does not extract data from the queue to return. The data to return is passed to the Method.
* This Method returns the same data to all pending Requests.

##### DMF_NotifyUserWithRequest_RequestReturnEx

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
VOID
DMF_NotifyUserWithRequest_RequestReturnEx(
  _In_ DMFMODULE DmfModule,
  _In_opt_ EVT_DMF_NotifyUserWithRequest_Function* EventCallbackFunction,
  _In_opt_ VOID* EventCallbackContext,
  _In_ NTSTATUS NtStatus
  );
````

The Client calls this Method to dequeue a single Request, populate it and return it to the Caller. The Client populates the request
using a given callback. The return NTSTATUS indicates if any Request was available to return.

##### Returns

STATUS_SUCCESS - A request was completed normally.
STATUS_UNSUCCESSFUL - There was no request in the queue.None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequest Module handle.
EventCallbackFunction | The callback that the Module calls which allows the Client to populate the Request that will be returned.
EventCallbackContext | A Client Specific context that is passed to the given callback.
NtStatus | The NTSTATUS to set in the Request that is to be returned.

##### Remarks

* This Method does not extract data from the queue to return. The data to return is passed to the Method.

##### DMF_NotifyUserWithRequest_RequestTimestampGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
LONGLONG
DMF_NotifyUserWithRequest_RequestTimestampGet(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    );
````

Given a WDFREQUEST passed to Client callback, this Method returns its time stamp in clock ticks.

##### Returns

The clock tick count when the WDFREQUEST was enqueued.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequest Module handle.
Request | The given request passed in the callback.

##### Remarks

* Time stamp is zero unless time stamping is enabled when the Module is instantiated using its Config.
* **Only call this Method from the callback using the supplied formal parameter.**

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module provides a pair of lists, one which is populated with buffers (Producer) and one empty list (Consumer).
* The Producer contains empty buffers that can be retrieved and written to.
* After the buffers from Producer are written to, they are put in the Consumer. The Consumer is, essentially, a list of pending work.
* After buffers are removed from Producer and the corresponding work is done, they are added back to the Producer.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* Most Methods from DMF_BufferPool are copied and renamed and routed to either the Producer or Consumer list in this Module so that the Client can access the DMF_BufferPool information.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Add this capability to DMF_NotifyUserWithRequestMultiple.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

