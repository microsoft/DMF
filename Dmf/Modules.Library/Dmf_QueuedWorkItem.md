## DMF_QueuedWorkItem

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

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

-----------------------------------------------------------------------------------------------------------------------------------
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

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
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

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

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

-----------------------------------------------------------------------------------------------------------------------------------

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

This Method causes the DMF_QueuedWorkItem instance's callback to be called one time and it waits for that call to complete.

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
* This Method waits for the callback to finish execution.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE.
* In some cases, it is necessary to execute code in a different thread than the current thread. This Module serves this purpose.
* The Client initializes this Module's DMF_BufferQueue lists with the maximum size of parameters buffer that will be passed to the enqueue function.
* The Client initializes the number of buffers to equal the maximum number of allowed simultaneous calls.
* If the Client requires that the callback not execute synchronously, the Client should create more than one instance of this Module.
* Workitems enqueued begin synchronously but are not guaranteed to finish synchronously. If a Client needs workitems to also finish synchronously, use DMF_ThreadedBufferQueue instead.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_BufferQueue
* DMF_ScheduledTask

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

-----------------------------------------------------------------------------------------------------------------------------------

Task Execution

-----------------------------------------------------------------------------------------------------------------------------------

