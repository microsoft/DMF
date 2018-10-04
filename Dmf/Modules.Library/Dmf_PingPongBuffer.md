## DMF_PingPongBuffer

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a ping-pong buffer that contains two separate buffers termed as Ping and Pong buffers. While new transmitted data
received is written to Ping buffer, previously transmitted data is consumed from Pong buffer. Once the information written into
the Ping buffer is validated it is swapped with Pong buffer for consumption.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_PingPongBuffer
````
typedef struct
{
  // The size in bytes of the ping pong buffers.
  //
  ULONG BufferSize;
  // Pool Type.
  // Note: Pool type can be passive if PassiveLevel in Module Attributes is set to TRUE.
  //
  POOL_TYPE PoolType;
} DMF_CONFIG_PingPongBuffer;
````
Member | Description
----|----
BufferSize | The size in bytes of each buffer.
PoolType | Indicates the type of pool to use when each buffer is allocated.

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

##### DMF_PingPongBuffer_Consume

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
UCHAR*
DMF_PingPongBuffer_Consume(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG StartOffset,
  _In_ ULONG PacketLength
  );
````

This Method is used by Client when the Ping buffer is validated and its data is ready to be consumed. This Method swaps
Ping and Pong buffers and returns the address of the Pong buffer. Data after StartOffset-PacketLength, if any, is copied to
the Pong buffer before swapping.

##### Returns

Address of the Pong buffer.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_PingPongBuffer Module handle.
StartOffset | Ping buffer offset where the validated data begins.
PacketLength | Length of validated data in Ping buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_PingPongBuffer_Get

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
UCHAR*
DMF_PingPongBuffer_Get(
  _In_ DMFMODULE DmfModule,
  _Out_ ULONG* Size
  );
````

This Method returns to the caller the address of the Ping buffer and its size.

##### Returns

Address of Ping Buffer.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_PingPongBuffer Module handle.
Size | Size of the Ping Buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_PingPongBuffer_Reset

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_PingPongBuffer_Reset(
  _In_ DMFMODULE DmfModule
  );
````

This Method invalidates all the data in Ping buffer by resetting the write pointer.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_PingPongBuffer Module handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_PingPongBuffer_Shift

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_PingPongBuffer_Shift(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG StartOffset
  );
````

This Method shifts all data in Ping buffer starting at StartOffset to the beginning of the buffer.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_PingPongBuffer Module handle.
StartOffset | The offset in the Ping buffer that contains the first byte of data to be shifted to the beginning of the Ping buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_PingPongBuffer_Write

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_PingPongBuffer_Write(
  _In_ DMFMODULE DmfModule,
  _In_reads_(NumberOfBytesToWrite) UCHAR* SourceBuffer,
  _In_ ULONG NumberOfBytesToWrite,
  _Out_ ULONG* ResultSize
  );
````

This Method writes from a given Client buffer into the Ping buffer.

##### Returns

STATUS_INSUFFICIENT_RESOURCES if NumberOfBytesToWrite is higher than the number of bytes available to write in Ping buffer.
STATUS_SUCCESS otherwise.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_PingPongBuffer Module handle.
SourceBuffer | The given Client buffer.
NumberOfBytesToWrite | The size in bytes of the given Client buffer.
ResultSize | Total size in bytes of Ping buffer after write.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. This tells DMF to create PASSIVE_LEVEL locks so that paged pool can be accessed.
* Note: The processing time of the Pong buffer must be shorter than the data collection and validation time in Ping buffer.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Buffers

-----------------------------------------------------------------------------------------------------------------------------------

