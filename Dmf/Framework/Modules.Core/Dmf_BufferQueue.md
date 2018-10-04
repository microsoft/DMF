## DMF_BufferQueue

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a Producer/Consumer data structure that is composed of two lists. One list is created with buffers (Producer) and the
other list is created with zero buffers (Consumer). The Producer is a Source-mode DMF_BufferPool instance. The Consumer is a
Sink-mode DMF_BufferPool instance.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_BufferQueue
````
typedef struct
{
  // DMF_BufferQueue has a source and sink list.
  // Source is configured by Client using these settings.
  //
  BufferPool_SourceSettings SourceSettings;
  // Sink is configured internally.
  //
} DMF_CONFIG_BufferQueue;
````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_BufferQueue_Count

````
_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DMF_BufferQueue_Count(
  _In_ DMFMODULE DmfModule
  );
````

Given a DMF_BufferQueue instance handle, return the number of entries in the Consumer list.

##### Returns

The number of entries in the list.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_BufferQueue Module handle.

##### Remarks

* The Consumer list usually has the pending work to do so the number of entries is useful at times.
* Use this when entries must be processed from a DPC but the DPC must not execute indefinitely.
* The number of buffers in the DMF_BufferQueue's Consumer list may change immediately after or during this call if other threads are using the same Module instance.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_BufferQueue_Dequeue

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_Dequeue(
  _In_ DMFMODULE DmfModule,
  _Out_ VOID** ClientBuffer,
  _Out_ VOID** ClientBufferContext
  );
````

Remove and retrieve the first buffer from an instance of DMF_BufferQueue's Consumer list in FIFO order.

##### Returns

NTSTATUS. Fails if there is no buffer in the list.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_BufferQueue Module handle.
ClientBuffer | The address of the retrieved Client Buffer. The Client may access the buffer at this address.
ClientBufferContext | The address of the Client Buffer Context associated with the retrieved ClientBuffer.

##### Remarks

* After retrieving a buffer using this Method, the Client usually reads the contents of the buffer and performs processing using that data. Afterward, the Client returns the buffer to the DMF_BufferQueue's Producer.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_BufferQueue_DequeueWithMemoryDescriptor

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_DequeueWithMemoryDescriptor(
  _In_ DMFMODULE DmfModule,
  _Out_ VOID** ClientBuffer,
  _Out_ PWDF_MEMORY_DESCRIPTOR MemoryDescriptor,
  _Out_ VOID** ClientBufferContext
  );
````

Remove and retrieve the first buffer from an instance of DMF_BufferQueue's Consumer in FIFO order along with its corresponding WDF
memory descriptor.

##### Returns

STATUS_SUCCESS if a buffer was retrieved.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_BufferQueue Module handle.
ClientBuffer | The address where the address of the retrieved buffer is written.
MemoryDescriptor | The address where the WDF memory descriptor associated with the retrieved buffer is written.
ClientBufferContext | The address where the Client Context associated with the retrieved buffer is written.

##### Remarks

* Clients use this Method when they need to retrieve a buffer from the list and then call other WDF APIs that perform operations on the buffer using the associated WDF_MEMORY_DESCRIPTOR object. For example, use this API when the buffer will be sent to a target device using a WDF_REQUEST.
* After retrieving a buffer using this Method, the Client usually reads the contents of the buffer and performs processing using that data. Afterward, the Client returns the buffer to the DMF_BufferQueue's Producer.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_BufferQueue_Enqueue

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Enqueue(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* ClientBuffer
  );
````

Adds a given DMF_BufferQueue buffer to an instance of DMF_BufferQueue's Consumer (at the end).

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_BufferQueue Module handle.
ClientBuffer | The given DMF_BufferQueue buffer to add to the list.

##### Remarks

* ClientBuffer *must* have been previously retrieved from an instance of DMF_BufferQueue because the buffer must have the appropriate metadata which is stored with ClientBuffer. Buffers allocated by the Client using ExAllocatePool() or WdfMemoryCreate() may not be added Module's list using this API.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_BufferQueue_Enumerate

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Enumerate(
  _In_ DMFMODULE DmfModule,
  _In_ EVT_DMF_BufferPool_Enumeration EntryEnumerationCallback,
  _In_opt_ VOID* ClientDriverCallbackContext,
  _Out_opt_ VOID** ClientBuffer,
  _Out_opt_ VOID** ClientBufferContext
  );
````

This Method enumerates all the buffers in a DMF_BufferQueue's Consumer list and calls a given callback for each buffer.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_BufferPool Module handle.
EntryEnumerationCallback | The given callback that is called for every buffer in the DMF_BufferQueue instance.
ClientDriverCallbackContext | The Client specific context that is passed to the given callback.
ClientBuffer | ClientBuffer is used to return a DMF_BufferQueue buffer to the Client after the buffer has been removed from the list.
ClientBufferContext | ClientBufferContext is used to return a DMF_BufferQueue buffer's Context-buffer to the Client after the buffer has been removed from the list.

##### Remarks

* Clients use this Method when they need to search or perform actions on all the buffers in a DMF_BufferQueue's Consumer list.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_BufferQueue_Fetch

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_Fetch(
  _In_ DMFMODULE DmfModule,
  _Out_ VOID** ClientBuffer,
  _Out_ VOID** ClientBufferContext
  );
````

Remove and retrieve the first buffer from an instance of DMF_BufferQueue's Producer list in FIFO order.

##### Returns

NTSTATUS. Fails if there is no buffer in the list.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_BufferQueue Module handle.
ClientBuffer | The address of the retrieved Client Buffer. The Client may access the buffer at this address.
ClientBufferContext | The address of the Client Buffer Context associated with the retrieved ClientBuffer.

##### Remarks

* After retrieving a buffer using this Method, the Client usually populates this buffer with data to be used later and then enqueues the buffer into the DMF_BufferQueue's Consumer. That buffer is later dequeued from the DMF_BufferQueue's Consumer and processed in some way. Afterward, the Client returns the buffer to the DMF_BufferQueue's Producer.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_BufferQueue_Flush

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Flush(
  _In_ DMFMODULE DmfModule
  );
````

Removes all the buffers in a DMF_BufferQueue's Consumer list and returns those buffers to the DMF_BufferQueue's Producer list.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_BufferQueue Module handle.

##### Remarks

* Use this Method in cases when the pending work in the Consumer list does not need to be done or cannot be done.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_BufferQueue_Reuse

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Reuse(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* ClientBuffer
  );
````

Adds a given DMF_BufferQueue buffer to an instance of DMF_BufferQueue's Producer (at the end).

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_BufferQueue Module handle.
ClientBuffer | The given DMF_BufferQueue buffer to add to the list.

##### Remarks

* ClientBuffer *must* have been previously retrieved from an instance of DMF_BufferQueue because the buffer must have the appropriate metadata which is stored with ClientBuffer. Buffers allocated by the Client using ExAllocatePool() or WdfMemoryCreate() may not be added Module's list using this API.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. This tells DMF to create PASSIVE_LEVEL locks so that paged pool can be accessed.
* This Module provides a pair of lists, one which is populated with buffers (Producer) and one empty list (Consumer).
* The Producer contains empty buffers that can be retrieved and written to.
* After the buffers from Producer are written to, they are put in the Consumer. The Consumer is, essentially, a list of pending work.
* After buffers are removed from Consumer list and the corresponding work is done, they should be added back to the Producer list.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_BufferPool (2)

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* This Module instantiates two instances of DMF_BufferPool.
* Most Methods from DMF_BufferPool are copied and renamed and routed to either the Producer or Consumer list in this Module so that the Client can access the DMF_BufferPool information.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DMF_QueuedWorkItem
* DMF_ThreadedBufferQueue
* DMF_NotifyUserWithRequest

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Buffers

-----------------------------------------------------------------------------------------------------------------------------------

