## DMF_ThreadedBufferQueue

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

Implements a DMF_ThreadedBufferQueue which consists of a DMF_Thread and a DMF_BufferQueue. A Client callback is called after work
has been removed from the DMF_BufferQueue's Consumer list. After the callback completes the buffer with work is returned
to the DMF_BufferQueue's Producer list.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_ThreadedBufferQueue
````
typedef struct
{
  // Contains empty buffers in Producer and work buffers in the Consumer.
  //
  DMF_CONFIG_BufferQueue BufferQueueConfig;
  // Optional callback that does work before looping.
  //
  EVT_DMF_Thread_Function* EvtThreadedBufferQueuePre;
  // Mandatory callback that does work when work is ready.
  //
  EVT_DMF_ThreadedBufferQueue_Callback* EvtThreadedBufferQueueWork;
  // Optional callback that does work after looping but before thread ends.
  //
  EVT_DMF_Thread_Function* EvtThreadedBufferQueuePost;
  // Optional callback allows Client to deallocate / dereference any
  // buffer-attached resources.
  //
  EVT_DMF_ThreadedBufferQueue_ReuseCleanup* EvtThreadedBufferQueueReuseCleanup;
} DMF_CONFIG_ThreadedBufferQueue;
````
Member | Description
----|----
BufferQueueConfig | Client sets up the configuration of the internal DMF_BufferQueue (producer/consumer lists).
EvtThreadedBufferQueuePre | This function performs work on behalf of the Client before this Module's main ThreadedBufferQueue function executes.
EvtThreadedBufferQueueWork | This function performs work on behalf of the Client when this Module determines there is work to be done.
EvtThreadedBufferQueuePost | This function performs work on behalf of the Client after this Module's main ThreadedBufferQueue function executes.
EvtThreadedBufferQueueReuseCleanup | The Client may register this callback to do any cleanup needed before the buffer is being flushed / reused.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

##### ThreadedBufferQueue_BufferDisposition
Enum to specify what the Client wants to do with the retrieved work buffer.

````
typedef enum
{
  ThreadedBufferQueue_BufferDisposition_Invalid,
  // Client no longer owns buffer and it is returned to Producer list.
  //
  ThreadedBufferQueue_BufferDisposition_WorkComplete,
  // Client retains ownership of the buffer and will complete it later.
  //
  ThreadedBufferQueue_BufferDisposition_WorkPending,
  ThreadedBufferQueue_BufferDisposition_Maximum
} ThreadedBufferQueue_BufferDisposition;
````
Member | Description
----|----
ThreadedBufferQueue_BufferDisposition_WorkComplete | Client has completed work. The buffer should be returned and next buffer retrieved.
ThreadedBufferQueue_BufferDisposition_WorkPending | Client retains ownership of buffer and will indicate work is complete later. No additional buffer is retrieved.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_ThreadedBufferQueue_Callback
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
ThreadedBufferQueue_BufferDisposition
EVT_DMF_ThreadedBufferQueue_Callback(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* ClientWorkBuffer,
    _In_ ULONG ClientWorkBufferSize,
    _In_ VOID* ClientWorkBufferContext,
    _Out_ NTSTATUS* NtStatus
    );
````

This callback is called when this Module has detected that the Client has work to do. In addition the Module removes the work
buffer from its DMF_BufferQueue Consumer list and presents it to the Client via this callback.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
ClientWorkBuffer | This buffer contains the work that needs to be done in this callback. This buffer is owned by Client until this function returns.
ClientWorkBufferSize | The size of ClientWorkBuffer for validation purposes.
ClientWorkBufferContext | An optional context associated with ClientWorkBuffer.
NtStatus | The NTSTATUS to return to the function that initially populated the work buffer.

##### EVT_DMF_BufferQueue_ReuseCleanup
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ThreadedBufferQueue_ReuseCleanup(_In_ DMFMODULE DmfModule,
                                         _In_ VOID* ClientBuffer,
                                         _In_ VOID* ClientBufferContext);
````

This callback is called when this Module is about to reuse a work buffer. Before the Module puts the work
buffer back in its DMF_BufferQueue's Producer list for reuse, the Module presents it to the Client via this callback.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
ClientWorkBuffer | This buffer contains the work that needs to be done in this callback. This buffer is owned by Client until this function returns.
ClientWorkBufferContext | An optional context associated with ClientWorkBuffer.

##### Remarks

* Use callback to free or reference count decrement resources associated with buffers.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_ThreadedBufferQueue_Count

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
ULONG
DMF_ThreadedBufferQueue_Count(
  _In_ DMFMODULE DmfModule
  );
````

Given a ThreadedBufferQueue instance handle, return the number of entries in the Consumer list.

##### Returns

The number of entries in the list.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.

##### Remarks

* The Consumer list usually has the pending work to do so the number of entries is useful at times.

##### DMF_ThreadedBufferQueue_Enqueue

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_Enqueue(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* ClientBuffer
  );
````

Adds a given DMF_BufferQueue buffer to an instance of ThreadedBufferQueue's DMF_BufferQueue's Consumer list (at the end). This list is consumed in FIFO order.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
ClientBuffer | The given DMF_BufferQueue buffer to add to the list.

##### Remarks

* ClientBuffer *must* have been previously retrieved from an instance of DMF_ThreadedBufferQueue because the buffer must have the appropriate metadata which is stored with ClientBuffer. Buffers allocated by the Client using ExAllocatePool() or WdfMemoryCreate() may not be added Module's list using this API.

##### DMF_ThreadedBufferQueue_EnqueueAtHead

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_EnqueueAtHead(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* ClientBuffer
  );
````

Adds a given DMF_BufferQueue buffer to an instance of ThreadedBufferQueue's DMF_BufferQueue's Consumer list (at the head). This list is consumed in LIFO order.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
ClientBuffer | The given DMF_BufferQueue buffer to add to the list.

##### Remarks

* ClientBuffer *must* have been previously retrieved from an instance of DMF_ThreadedBufferQueue because the buffer must have the appropriate metadata which is stored with ClientBuffer. Buffers allocated by the Client using ExAllocatePool() or WdfMemoryCreate() may not be added Module's list using this API.

##### DMF_ThreadedBufferQueue_EnqueueAndWait

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ThreadedBufferQueue_EnqueueAndWait(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );
````

Adds a given DMF_BufferQueue buffer to an instance of ThreadedBufferQueue's DMF_BufferQueue's Consumer list (at the end). Then, this
function waits until the work enqueued by the call to this Method to finish execution. In this way, it is possible for the
calling thread to receive an NTSTATUS indicating the success or failure of the deferred work.

##### Returns

NTSTATUS of the deferred work.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
ClientBuffer | The given DMF_BufferQueue buffer to add to the list.

##### Remarks

* ClientBuffer *must* have been previously retrieved from an instance of DMF_ThreadedBufferQueue because the buffer must have the appropriate metadata which is stored with ClientBuffer. Buffers allocated by the Client using ExAllocatePool() or WdfMemoryCreate() may not be added Module's list using this API.

##### DMF_ThreadedBufferQueue_EnqueueAtHeadAndWait

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ThreadedBufferQueue_EnqueueAtHeadAndWait(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );
````

Adds a given DMF_BufferQueue buffer to an instance of ThreadedBufferQueue's DMF_BufferQueue's Consumer list (at the head). Then, this
function waits until the work enqueued by the call to this Method to finish execution. In this way, it is possible for the
calling thread to receive an NTSTATUS indicating the success or failure of the deferred work.

##### Returns

NTSTATUS of the deferred work.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
ClientBuffer | The given DMF_BufferQueue buffer to add to the list.

##### Remarks

* ClientBuffer *must* have been previously retrieved from an instance of DMF_ThreadedBufferQueue because the buffer must have the appropriate metadata which is stored with ClientBuffer. Buffers allocated by the Client using ExAllocatePool() or WdfMemoryCreate() may not be added Module's list using this API.

##### DMF_ThreadedBufferQueue_Fetch

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ThreadedBufferQueue_Fetch(
  _In_ DMFMODULE DmfModule,
  _Out_ VOID** ClientBuffer,
  _Outopt_ VOID** ClientBufferContext
  );
````

Remove and return the first buffer from an instance of ThreadedBufferQueue's DMF_BufferQueue's Producer list in FIFO order.

##### Returns

NTSTATUS. Fails if there is no buffer in the list.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
ClientBuffer | The address of the retrieved Client Buffer. The Client may access the buffer at this address.
ClientBufferContext | The address of the Client Buffer Context associated with the retrieved ClientBuffer.

##### Remarks

* Clients use this Method when they need to retrieve a buffer from the list.
* The Client knows the size of the buffer and buffer context because the Client has specified that information when creating the DMF_BufferQueue Module.

##### DMF_ThreadedBufferQueue_Flush

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_Flush(
  _In_ DMFMODULE DmfModule
  );
````

This Method flushes any pending work in this Module's DMF_BufferQueue Consumer list.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.

##### Remarks

* Use this Method to stop processing pending work that has not started. Work that has already started will continue until it has finished processing.

##### DMF_ThreadedBufferQueue_Reuse

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ThreadedBufferQueue_Reuse(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* ClientBuffer
  );
````

Adds a given DMF_BufferQueue buffer to an instance of ThreadedBufferQueue's DMF_BufferQueue's producer list (at the end).

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
ClientBuffer | The given DMF_BufferQueue buffer to add to the list.

##### Remarks

* ClientBuffer *must* have been previously retrieved from an instance of DMF_ThreadedBufferQueue because the buffer must have the appropriate metadata which is stored with ClientBuffer. Buffers allocated by the Client using ExAllocatePool() or WdfMemoryCreate() may not be added Module's list using this API.
* Clients may use this Method if a buffer has been fetched but it is determined that it cannot be enqueued for some reason and must be returned to the free pool of buffers.

##### DMF_ThreadedBufferQueue_Start

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ThreadedBufferQueue_Start(
  _In_ DMFMODULE DmfModule
  );
````

This Method starts the internal Thread.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.

##### Remarks

* Starts the underlying thread that will processes enqueued work.

##### DMF_ThreadedBufferQueue_Stop

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ThreadedBufferQueue_Stop(
  _In_ DMFMODULE DmfModule
  );
````

This Method stops the internal Thread.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.

##### Remarks

* Stops the underlying thread that processes enqueued work.

##### DMF_ThreadedBufferQueue_WorkCompleted

````
VOID
DMF_ThreadedBufferQueue_WorkCompleted(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* ClientBuffer,
  _In_ NTSTATUS NtStatus
  )
````
Allows Client to complete work that was previously pended.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
ClientBuffer | Buffer that contains the work that was pended.
NtStatus | Status indicating result of work.

##### Remarks

* This Method is used when the Client callback has returned ThreadedBufferQueue_BufferDisposition_WorkPending when the work is completed later.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE.
* This Module is useful in cases where multiple threads receive requests to perform work but the work must be done in a synchronous manner (when writing to hardware).
* The Client just needs to supply a callback that does Client specific work.
* This Module does the work of removing work from the Consumer list and replacing the work buffer back into the Producer list.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* This Module creates a DMF_Thread and an associated DMF_BufferQueue. This is a common programming pattern.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Buffers

-----------------------------------------------------------------------------------------------------------------------------------

