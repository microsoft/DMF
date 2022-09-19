## DMF_Time

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a time keeper module which gets current cpu ticks and also calculates the time elapsed in milliseconds and nanoseconds.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
* None
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

##### DMF_Time_ElapsedTimeMillisecondsGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Time_ElapsedTimeMillisecondsGet(
    _In_ DMFMODULE DmfModule,
    _In_ LONGLONG StartTime,
    _Out_ LONGLONG* ElapsedTimeInMilliSeconds
    );
````

Calculate elapsed time in milliseconds.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Time Module handle.
StartTime | StartTime in tick count from which the elapsed time has to be calculated.
ElapsedTimeInMilliSeconds | Return the elapsed time in milliseconds.

##### Remarks

*None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Time_ElapsedTimeNanosecondsGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Time_ElapsedTimeNanosecondsGet(
    _In_ DMFMODULE DmfModule,
    _In_ LONGLONG StartTick,
    _Out_ LONGLONG* ElapsedTimeInNanoSeconds
    );
````

Calculate elapsed time in nanoseconds.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Time Module handle.
StartTime | StartTime in tick count from which the elapsed time has to be calculated.
ElapsedTimeInNanoSeconds | Return the elapsed time in nanoseconds.

##### Remarks

*None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Time_LocalTimeGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Time_LocalTimeGet(
    _In_ DMFMODULE DmfModule,
    _Out_ DMF_TIME_FIELDS* DmfTimeFields
    );
````

This function returns local time in DMF_TIME_FIELDS structure.

##### Returns

*None

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.
DmfTimeFields | Represents time in Year, Month, Day, Hour, Minute, Seconds, Milliseconds, Weekday.

##### Remarks

*None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Time_SystemTimeGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Time_SystemTimeGet(
    _In_ DMFMODULE DmfModule,
    _Out_ LARGE_INTEGER* CurrentSystemTime
    );
````

This function fetches the current system time in UTC..

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.
CurrentSystemTime | Pointer to store current system time.

##### Remarks

*None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Time_TickCountGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
LONGLONG
DMF_Time_TickCountGet(
    _In_ DMFMODULE DmfModule
    );
````

Queries current tick count and returns it to the caller.

##### Returns

Tick count.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Time Module handle.

##### Remarks

*None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

* None

-----------------------------------------------------------------------------------------------------------------------------------

