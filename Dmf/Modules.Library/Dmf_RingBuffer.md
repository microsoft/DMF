## DMF_RingBuffer

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a classic ring buffer as a circular list with read/write pointers.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_RingBuffer
````
typedef struct
{
  // Maximum number of items to store.
  //
  ULONG ItemCount;
  // The size of each entry.
  //
  ULONG ItemSize;
  // Indicates the mode of the ring buffer.
  //
  RingBuffer_ModeType Mode;
} DMF_CONFIG_RingBuffer;
````
Member | Description
----|----
ItemCount | Indicates how many items the ring buffer contains.
ItemSize | Indicates the size of each entry in the ring buffer.
Mode | If set to RingBuffer_Mode_DeleteOldestIfFullOnWrite, indicates that the ring buffer never runs out of space. Instead, when the buffer is full and new entry is written to the ring buffer, the oldest entry is discarded to make room for the new entry. If set to RingBuffer_Mode_FailIfFullOnWrite, when the ring buffer is full, new data cannot be written to the ring buffer unless data is read from the ring buffer first.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### RingBuffer_ModeType
These definitions indicate the mode of ring buffer.

````
typedef enum
{
  RingBuffer_Mode_Invalid = 0,
  // If the ring buffer is full, an error will occur when Client writes to it.
  //
  RingBuffer_Mode_FailIfFullOnWrite,
  // If the ring buffer is full, oldest item will be deleted to make space for new element.
  // Thus, writes never fail.
  //
  RingBuffer_Mode_DeleteOldestIfFullOnWrite,
  RingBuffer_Mode_Maximum,
} RingBuffer_ModeType;
````
Member | Description
----|----
RingBuffer_Mode_FailIfFullOnWrite | In this mode, attempts to write to a full ring buffer will fail and an error is returned to the Client.
RingBuffer_Mode_DeleteOldestIfFullOnWrite | In this mode, attempts to write to a full ring buffer will succeed because the oldest element in the ring buffer will be deleted to make space for the new element.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_RingBuffer_Enumeration
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_RingBuffer_Enumeration(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_opt_ VOID* CallbackContext
    );
````

The callback called by the ring buffer enumeration function.

##### Returns

TRUE if enumeration should continue. FALSE if enumeration should stop.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
Buffer | The address of the buffer that is enumerated.
TargetBufferSize | The size of the buffer that is enumerated.
CallbackContext | A call specific context passed by the Client.

##### Remarks

* The enumerator holds the ring buffer's lock while this callback is called, so the Client should not call any DMF_RingBuffer Methods from this callback.
* The Client may change the contents of the buffer.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RingBuffer_Enumerate

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_Enumerate(
  _In_ DMFMODULE DmfModule,
  _In_ BOOLEAN Lock,
  _In_ EVT_DMF_RingBuffer_Enumeration* RingBufferEntryCallback,
  _In_opt_ VOID* RingBufferEntryCallbackContext
  );
````

This Method calls the client provided callback for all items in the ring buffer.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
Lock | Set to TRUE if this Method should lock the ring buffer while it is reordered. In most cases, this parameter should be set to TRUE.
RingBufferEntryCallback | The client provided callback.
RingBufferEntryCallbackContext | A Client specific context passed to the enumerator's callback.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RingBuffer_EnumerateToFindItem

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_EnumerateToFindItem(
  _In_ DMFMODULE DmfModule,
  _In_ EVT_DMF_RingBuffer_Enumeration* RingBufferEntryCallback,
  _In_opt_ VOID* RingBufferEntryCallbackContext,
  _In_ UCHAR* Item,
  _In_ ULONG ItemSize
  );
````

This Method calls client provided callback for ring buffer items that match the client provided data.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
RingBufferEntryCallback | The client provided callback.
RingBufferEntryCallbackContext | A Client specific context passed to the enumerator's callback.
Item | The Client provided entry.
ItemSize | Size of the entry provided by the client.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RingBuffer_Read

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_Read(
  _In_ DMFMODULE DmfModule,
  _Out_writes_(TargetBufferSize) UCHAR* TargetBuffer,
  _In_ ULONG TargetBufferSize
  );
````

Copies the oldest entry in the ring buffer into a given buffer and removes that same entry from the ring buffer.

##### Returns

NTSTATUS This Method fails if there are no items in the ring buffer to read.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
TargetBuffer | The address of the given buffer where the oldest entry in the ring buffer is copied to.
TargetBufferSize | The size of the given buffer.

##### Remarks

* This Method copies into the given buffer that is owned by the Client.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RingBuffer_ReadAll

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_ReadAll(
  _In_ DMFMODULE DmfModule,
  _Out_writes_(TargetBufferSize) UCHAR* TargetBuffer,
  _In_ ULONG TargetBufferSize,
  _Out_ ULONG* BytesWritten
  );
````

This Method copies the entire ring buffer to a given Client buffer.

##### Returns

NTSTATUS This Method fails if the given Client buffer is not large enough.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
SourceBuffer | The given Client buffer. TODO: Rename to TargetBuffer.
SourceBufferSize | The size of the given Client buffer.
BytesWritten | Indicates the number of bytes written to the given Client buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RingBuffer_Reorder

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_Reorder(
  _In_ DMFMODULE DmfModule,
  _In_ BOOLEAN Lock
  );
````

This Method reorders the ring buffer so that the first entry in the ring buffer that is to be removed is located at the beginning of the ring buffer's
buffer.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
Lock | Set to TRUE if this Method should lock the ring buffer while it is reordered. In most cases, this parameter should be set to TRUE.

##### Remarks

* This Method can be used in cases where the ring buffer is to be written and it is necessary for the target to have the items in order (the oldest entry first).
* This Method is a good example of how to write a Method that affects all the items in the ring buffer.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RingBuffer_SegmentsRead

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_SegmentsRead(
  _In_ DMFMODULE DmfModule,
  _In_reads_(NumberOfSegments) UCHAR** Segments,
  _In_ ULONG* SegmentSizes,
  _In_reads_(NumberOfSegments) ULONG* SegmentOffsets,
  _In_ ULONG NumberOfSegments
  );
````

Copies the oldest entry in the ring buffer into a given buffer that is composed of multiple segments and removes that same entry from the ring buffer.

##### Returns

NTSTATUS This Method fails if there are no items in the ring buffer to read.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
Segments | An array of addresses that tells the Method the start address of each segment of the given target buffer.
SegmentSizes | An array of ULONG that tells the Method the length of each segment of the given target buffer.
SegmentOffset | An array of ULONG that tells the Method the starting offset of each segment in the ring buffer entry.
NumberOfSegments | An array of ULONG that tells the Method the size of each segment in the ring buffer entry.

##### Remarks

* This method is used to read from a single contiguous ring buffer entry into different non-contiguous target addresses owned by the Client.
* Using this method, the Client does not need to allocate a temporary buffer to store the ring buffer entry prior to writing its components to different non-contiguous addresses.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RingBuffer_SegmentsWrite

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_SegmentsWrite(
  _In_ DMFMODULE DmfModule,
  _In_reads_(NumberOfSegments) UCHAR** Segments,
  _In_ ULONG* SegmentSizes,
  _In_reads_(NumberOfSegments) ULONG* SegmentOffsets,
  _In_ ULONG NumberOfSegments
  );
````

Copies an entry into the ring buffer from a given buffer that is composed of multiple segments.

##### Returns

NTSTATUS This Method fails if there are no items in the ring buffer to read.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
Segments | An array of addresses that tells the Method the start address of each segment of the given source buffer.
SegmentSizes | An array of ULONG that tells the Method the length of each segment of the given source buffer.
SegmentOffset | An array of ULONG that tells the Method the starting offset of each segment in the ring buffer entry.
NumberOfSegments | An array of ULONG that tells the Method the size of each segment in the ring buffer entry.

##### Remarks

* This method is used to write to a single contiguous ring buffer entry from different non-contiguous source addresses owned by the Client.
* Using this method, the Client does not need to allocate a temporary buffer to store the ring buffer entry prior to reading its components to different non-contiguous addresses.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RingBuffer_TotalSizeGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_TotalSizeGet(
  _In_ DMFMODULE DmfModule,
  _Out_ ULONG* TotalSize
  );
````

This Method returns the total size of the ring buffer.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
TotalSize | The total size of the ring buffer is written here.

##### Remarks

* Although the Client can calculate the size of the ring buffer, this call makes it easier to do so.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_RingBuffer_Write

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_Write(
  _In_ DMFMODULE DmfModule,
  _In_reads_(SourceBufferSize) UCHAR* SourceBuffer,
  _In_ ULONG SourceBufferSize
  );
````

This Method writes from a given Client buffer into the next available entry of the ring buffer. It becomes the last entry.

##### Returns

NTSTATUS This method can fail if the ring buffer is full or the SourceBufferSize does not match the size of each entry in the ring buffer.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_RingBuffer Module handle.
SourceBuffer | The given Client buffer.
SourceBufferSize | The size in bytes of the given Client buffer. This size should be identical to the size of each entry in the ring buffer.

##### Remarks

* The last entry written is the last entry read.
* Because this Method copies from the Client buffer into the ring buffer, the Client may dispose of the SourceBuffer after this call.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module provides a classic ring buffer that uses read/write pointers. The management of the read/write pointers is done internally in DMF_RingBuffer.
* This Module allows the Client to read/write the ring buffer items as a single operation for simple data.
* This Module also allows the Client to read/write the ring buffer items using a map of addresses and offsets for more complex data. This allows the Client to write into the ring buffer items from different addresses. For example, this option is used for cases where protocol data fields are populated from different, non-contiguous addresses without the Client needing to allocate a temporary buffer to store the ring buffer entry.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* DMF_RingBuffer is a single buffer with read/write pointers.
* Internally DMF_RingBuffer uses callbacks which allow a single algorithm to determine which items will be read/written and a different algorithm that determines how the items are actually read.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DMF_LiveKernelDump
* DMF_CrashDump

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Buffers

-----------------------------------------------------------------------------------------------------------------------------------

