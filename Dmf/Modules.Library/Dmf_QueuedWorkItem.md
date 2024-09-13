## DMF_QueuedWorkItem

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

Implements a driver pattern that allows the Client to cause a callback to execute in a different thread at a
later time. This is very similar to a WDFWORKITEM. But there are some important differences:
  1. The callback will execute exactly the number of times the Client attempts to enqueue it.
  2. The callback can receive call specific information.
  3. All enqueued workitems in a specific instance of this Module begin to execute synchronously in the order in which they
    are enqueued. (They may not finish synchronously, however.) A Method is provided that allows the caller that enqueued
    the workitem to wait until that particular workitem has finished execution and, optionally, retrieve a result (NTSTATUS)
    of the enqueued operation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_QueuedWorkItem
````
typedef struct
{
  // Deferred call callback to be implemented by Client.
  //
  EVT_DMF_QueuedWorkItem_Callback* EvtQueuedWorkitemFunction;
  // Producer list holds empty pre-allocated buffers ready for use.
  // Consumer list holds buffers that have pending work.
  //
  DMF_CONFIG_BufferQueue BufferQueueConfig;
} DMF_CONFIG_QueuedWorkItem;
````
Member | Description
----|----
EvtQueuedWorkitemFunction | The Client's callback that will execute in a different thread.
ClientContext | Client specific context passed in the callback.
BufferQueueConfig | Contains parameters for initializing the child DMF_BufferQueue Module. The Client sets up buffers that are big enough to hold the maximum data that will be sent to the callback.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_QueuedWorkItem_Callback
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ScheduledTask_Result_Type
EVT_DMF_QueuedWorkItem_Callback(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext
    );
````

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_QueuedWorkItem handle.
ClientBuffer | Contains the parameters for this call.
ClientBufferContext | Client specific context passed by Client when the deferred call was enqueued.

##### Remarks

* ClientBuffer is accessible only while this callback executes.
* Use `DMF_QueuedWorkItem_StatusSet()` to set the NTSTATUS for callers of `DMF_QueuedWorkItem_EnqueueAndWait()`.
* Always return ScheduledTask_Result_Success from this callback. The return value is only present for legacy reasons.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_QueuedWorkItem_Enqueue

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_QueuedWorkItem_Enqueue(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(ContextBufferSize) VOID* ContextBuffer,
  _In_ ULONG ContextBufferSize
  );
````

This Method causes the DMF_QueuedWorkItem instance's callback to be called one time.

##### Returns

NTSTATUS. Fails if the value cannot be read from the registry.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_QueuedWorkItem Module handle.
ContextBuffer | A Client specific buffer that contains parameter that are used during the callback's execution.
ContextBufferSize | The size in bytes of ContextBuffer.

##### Remarks

* Regardless of whether or not the callback is already running, calling this method causes the callback to be called exactly one more time.
* This Method does not wait for the callback to finish execution.

##### DMF_QueuedWorkItem_EnqueueAndWait

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_QueuedWorkItem_EnqueueAndWait(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(ContextBufferSize) VOID* ContextBuffer,
  _In_ ULONG ContextBufferSize
  );
````

This Method causes the DMF_QueuedWorkItem instance's callback to be called one time and it waits for that call to complete. The caller
can also access the NTSTATUS set by the callback.

##### Returns

NTSTATUS. (This status is set by the Module instance callback.)

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_QueuedWorkItem Module handle.
ContextBuffer | A Client specific buffer that contains the deferred work to be done.
ContextBufferSize | The size in bytes of ContextBuffer.

##### Remarks

* Regardless of whether or not the callback is already running, calling this method causes the callback to be called exactly one more time.
* This Method waits for the callback to finish execution.
* The callback must use `DMF_QueuedWorkItem_StatusSet()` to set the NTSTATUS returned by this Method.

##### DMF_QueuedWorkItem_Flush

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_QueuedWorkItem_Flush(
    _In_ DMFMODULE DmfModule
    );
````
Flushes any pending work item. If its callback has not yet started executing, it will execute before
this Method returns. If its callback has started executing it will finish executing before this
Method returns.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_QueuedWorkItem Module handle.

##### Remarks

* Use this Method to prevent the callback from executing before releasing resource used by the callback.

##### DMF_QueuedWorkItem_StatusSet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_QueuedWorkItem_StatusSet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ NTSTATUS NtStatus
    );
````
Allows the callback to set the NTSTATUS of the operation submitted to it via the
given Client Buffer.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_QueuedWorkItem Module handle.
ClientBuffer | Buffer that contains the work that was pended.
NtStatus | Status indicating result of work done by the callback.

##### Remarks

* This Method to populate the NTSTATUS that is returned by `DMF_QueuedWorkItem_EnqueueAndWait()`.
* If the Client does not use `DMF_QueuedWorkItem_EnqueueAndWait()` then it is not necessary to call this Method, but it is
still fine to do so.
* The default NTSTATUS returned by the Module is STATUS_SUCCESS. It is only necessary to call this Method if a different STATUS should 
be returned to the caller of `DMF_QueuedWorkItem_EnqueueAndWait()`.
* *This Method must only be called from the callback while ClientBuffer is owned by the callback.*

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE.
* In some cases, it is necessary to execute code in a different thread than the current thread. This Module serves this purpose.
* The Client initializes this Module's DMF_BufferQueue lists with the maximum size of parameters buffer that will be passed to the enqueue function.
* The Client initializes the number of buffers to equal the maximum number of allowed simultaneous calls.
* If the Client requires that the callback not execute synchronously, the Client should create more than one instance of this Module.
* Workitems enqueued begin synchronously but are not guaranteed to finish synchronously. If a Client needs workitems to also finish synchronously, use DMF_ThreadedBufferQueue instead.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

* Fix DMF so that the Module State remains open as long as its children remain open (while they are closing).
* Fix this name: QueuedWorkItem_CallbackType_StreamAsynchronousBufferOutput


-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Task Execution

-----------------------------------------------------------------------------------------------------------------------------------

