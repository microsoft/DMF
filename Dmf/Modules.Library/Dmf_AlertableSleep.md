## DMF_AlertableSleep

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a driver pattern that allows the Client create delays (sleeps) that are interruptible.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_AlertableSleep
````
typedef struct
{
  // Several events are created and each works independently. The code protects
  // against a caller sleeping using the same event twice.
  //
  ULONG EventCount;
} DMF_CONFIG_AlertableSleep;
````
Member | Description
----|----
EventCount | The number of distinct events to create. A single event can delay a single thread.

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

##### DMF_AlertableSleep_Abort

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AlertableSleep_Abort(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG EventIndex
  );
````

This Method causes the thread delayed by the given event index to resume execution.

##### Returns

NTSTATUS. Fails if the delay was interrupted.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AlertableSleep Module handle.
EventIndex | The given event index.
Milliseconds | The amount of time to delay in milliseconds.

##### Remarks

* Generally speaking this Method should be used for long delays, perhaps 10 seconds or longer.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_AlertableSleep_ResetForReuse

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AlertableSleep_ResetForReuse(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG EventIndex
  );
````

This Method causes the event of the given event index to reset so it can be used again.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AlertableSleep Module handle.
EventIndex | The given event index.

##### Remarks

* Use this method to delay a subsequent time using the same event index.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_AlertableSleep_Sleep

````
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_AlertableSleep_Sleep(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG EventIndex,
  _In_ ULONG Milliseconds
  );
````

This Method causes the current thread to delay for a given time in milliseconds using the given event index.

##### Returns

NTSTATUS. Fails if the delay was interrupted.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AlertableSleep Module handle.
EventIndex | The given event index.
Milliseconds | The given time to delay in milliseconds.

##### Remarks

* Generally speaking this Method should be used for long delays, perhaps 10 seconds or longer.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* In some cases, it is necessary to delay an operation for a long time. But it may also be necessary to interrupt that delay if some event happens, such as the driver disabling.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* This Module uses Portable Event to make the current thread wait.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Task Execution

-----------------------------------------------------------------------------------------------------------------------------------

