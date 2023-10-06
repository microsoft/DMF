## DMF_Stack

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a Stack which consists of a DMF_BufferQueue. Client can push allocated, filled buffers into Dmf_Stack and when needed,
can pop them into an allocated buffer in LIFO order.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_Stack
````
typedef struct
{
    // Maximum number of entries to store.
    //
    ULONG StackDepth;
    // The size of each entry.
    //
    ULONG StackElementSize;
} DMF_CONFIG_Stack;
````
Member | Description
----|----
StackDepth | Maximum number of entries to store.
StackElementSize | The size of each entry.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
* None
-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Stack_Depth

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
ULONG
DMF_Stack_Depth(
  _In_ DMFMODULE DmfModule
  );
````

Given a Stack instance handle, return the number of entries currently in the stack.

##### Returns

The number of entries in the list.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Stack Module handle.

##### Remarks

* The Consumer list usually has the pending work to do so the number of entries is useful at times.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Stack_Flush

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Stack_Flush(
  _In_ DMFMODULE DmfModule
  );
````

Empties the stack.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Stack Module handle.

##### Remarks

*None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Stack_Pop

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Stack_Pop(
    _In_ DMFMODULE DmfModule,
    _Out_writes_bytes_(ClientBufferSize) VOID* ClientBuffer,
    _In_ size_t ClientBufferSize
    )
````

Pops the next buffer in the list (head of the list) if there is a buffer.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Stack Module handle.
ClientBuffer | The given DMF_BufferQueue buffer to add to the list.
ClientBufferSize | Size of Client buffer for verification.

##### Remarks

* ClientBuffer *must* be a valid buffer same size as declared in the config of this Module.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Stack_Push

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Stack_Push(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
````

Push the Client buffer to the top of the stack.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Stack Module handle.
ClientBuffer | The given buffer to add to the list.

##### Remarks

* ClientBuffer *must* be the allocated, initialized, and of the same size as declared in the config of this Module.

-----------------------------------------------------------------------------------------------------------------------------------



#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_BufferQueue

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* This Module creates a stack data structure.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Buffers

-----------------------------------------------------------------------------------------------------------------------------------

