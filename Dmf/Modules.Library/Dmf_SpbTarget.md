## DMF_SpbTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module gives the Client access to devices connected to Small Peripheral Bus (SBP).

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_SpbTarget
````
typedef struct
{
    // Module will not load if Spb Connection not found.
    //
    BOOLEAN SpbConnectionMandatory;
    // GPIO Connection index for this instance.
    //
    ULONG SpbConnectionIndex;
    // Open in Read or Write mode.
    //
    ULONG OpenMode;
    // Share Access.
    //
    ULONG ShareAccess;
    // Interrupt Resource
    //
    DMF_CONFIG_InterruptResource InterruptResource;
} DMF_CONFIG_SpbTarget;
````
Member | Description
----|----
SpbConnectionMandatory | Module must find the SPB connection at SpbConnectionIndex in order to initialize properly.
SpbConnectionIndex | The index of the SPB line that this Module's instance should access.
OpenMode | Indicates if this Module's instance will read and/or write from/to the SPB line.
ShareAccess | Indicates if this Module's instance will access the SPB line in exclusive mode.
InteruptResource | Allows Client to specify an interrupt resource associated with the SPB resource.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

* None

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_BufferFullDuplex

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_BufferFullDuplex(
    _In_ DMFMODULE DmfModule,
    _In_reads_(InputBufferLength) UCHAR* InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_(OutputBufferLength) UCHAR* OutputBuffer,
    _In_ ULONG OutputBufferLength
    );
````
Performs a full duplex SPB transfer.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.
InputBuffer | The buffer to write to the device.
InputBufferLength | Size in bytes of InputBuffer.
OutputBuffer | The buffer to read from the device.
OutputBufferLength | Size in bytes of OutputBuffer.

##### Remarks
* See MSDN SPB documentation.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_BufferWrite

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_BufferWrite(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfBytesToWrite) UCHAR* BufferToWrite,
    _In_ ULONG NumberOfBytesToWrite
    );
````
Performs a write to an SPB device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.
BytesToWrite | The buffer to write to the device.
NumberOfBytesToWrite | Size in bytes of BytesToWrite.

##### Remarks
* See MSDN SPB documentation.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_BufferWriteRead

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_BufferFullDuplex(
    _In_ DMFMODULE DmfModule,
    _In_reads_(InputBufferLength) UCHAR* InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_(OutputBufferLength) UCHAR* OutputBuffer,
    _In_ ULONG OutputBufferLength
    );
````
Performs reads a buffer from a given address from an SPB device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.
InputBuffer | The buffer to write to the device (address).
InputBufferLength | Size in bytes of InputBuffer (address length).
OutputBuffer | The buffer to read from the device.
OutputBufferLength | Size in bytes of OutputBuffer.

##### Remarks
* See MSDN SPB documentation.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_ConnectionLock

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_ConnectionLock(
    _In_ DMFMODULE DmfModule
    );
````
Locks an SPB connection.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.

##### Remarks
* See MSDN SPB documentation.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_ConnectionUnlock

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_ConnectionUnlock(
    _In_ DMFMODULE DmfModule
    );
````
Unlocks an SPB connection.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.

##### Remarks
* See MSDN SPB documentation.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_ControllerLock

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_ControllerLock(
    _In_ DMFMODULE DmfModule
    );
````
Locks an SPB controller.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.

##### Remarks
* See MSDN SPB documentation.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_ControllerUnlock

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_ControllerUnlock(
    _In_ DMFMODULE DmfModule
    );
````
Unlocks an SPB controller.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.

##### Remarks
* See MSDN SPB documentation.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_InterruptAcquireLock

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SpbTarget_InterruptAcquireLock(
    _In_ DMFMODULE DmfModule
    );
````

Acquires the interrupt spin-lock associated with the SPB resource configured by the Client.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.

#### Remarks
* See WDF documentation to understand how interrupt spin-locks work at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_InterruptReleaseLock

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SpbTarget_InterruptReleaseLock(
    _In_ DMFMODULE DmfModule
    );
````

Releases the interrupt spin-lock associated with the SPB resource configured by the Client.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.

#### Remarks
* See WDF documentation to understand how interrupt spin-locks work at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

_Must_inspect_result_

##### DMF_SpbTarget_InterruptTryToAcquireLock

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_SpbTarget_InterruptTryToAcquireLock(
    _In_ DMFMODULE DmfModule
    );
````

Acquires the interrupt spin-lock associated with the SPB resource configured by the Client.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.

#### Remarks
* See WDF documentation to understand how interrupt spin-locks work at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_IsResourceAssigned

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_SpbTarget_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* SpbConnectionAssigned,
    _Out_opt_ BOOLEAN* InterruptAssigned
    );
````

Indicates if the SPB resources set by the Client have been found.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.
NumberOfSpbLinesAssigned | The given event index.
NumberOfSpbInterruptsAssigned | The number of SPB interrupts this Module's instance assigned.

#### Remarks
* Use this Method to determine if the resources needed are found. This is for the cases where the Client driver runs regardless of whether or not GPIO lines are available.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module accesses a single SPB resource.
* A Client must instantiate one instance of this Module for every SPB resource the Client needs to access.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* Dmf_RequestTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Targets

-----------------------------------------------------------------------------------------------------------------------------------

