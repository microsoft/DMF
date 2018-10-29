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
  // Module will not load if SPB Connection not found.
  //
  BOOLEAN SpbConnectionMandatory;
  // Module will not load if Interrupt not found.
  //
  BOOLEAN InterruptMandatory;
  // SPB Connection index for this instance.
  //
  ULONG SpbConnectionIndex;
  // Interrupt index for this instance.
  //
  ULONG InterruptIndex;
  // Open in Read or Write mode.
  //
  ULONG OpenMode;
  // Share Access.
  //
  ULONG ShareAccess;
  // Passive handling.
  //
  BOOLEAN PassiveHandling;
  // Can SPB wake the device.
  //
  BOOLEAN CanWakeDevice;
  // Optional Callback from ISR (with Interrupt Spin Lock held).
  //
  EVT_DMF_SpbTarget_InterruptIsr* EvtSpbTargetInterruptIsr;
  // Optional Callback at DPC_LEVEL Level.
  //
  EVT_DMF_SpbTarget_InterruptDpc* EvtSpbTargetInterruptDpc;
  // Optional Callback at PASSIVE_LEVEL Level.
  //
  EVT_DMF_SpbTarget_InterruptPassive* EvtSpbTargetInterruptPassive;
} DMF_CONFIG_SpbTarget;
````
Member | Description
----|----
SpbConnectionMandatory | Module must find the SPB connection at SpbConnectionIndex in order to initialize properly.
InterruptMandatory | Module must find the Interrupt at InterruptIndex in order to initialize properly.
SpbConnectionIndex | The index of the SPB line that this Module's instance should access.
InterruptIndex | The index of the Interrupt that this Module's instance should access.
OpenMode | Indicates if this Module's instance will read and/or write from/to the SPB line.
ShareAccess | Indicates if this Module's instance will access the SPB line in exclusive mode.
PassiveHandling | Indicates that interrupts that happen on the SPB line should be dispatched to Client's callback at PASSIVE_LEVEL. Otherwise, the Client's callback is called at DISPATCH_LEVEL.
CanWakeDevice | Indicates if the SPB line should be able wake the system.
EvtSpbTargetInterruptIsr | The callback that is called at DPC_LEVEL when an interrupt happens on the SPB line. The interrupt spin-lock is already acquired while this callback runs.
EvtSpbTargetInterruptDpc | The callback that is called at DISPATCH_LEVEL when an interrupt happens on the SPB line.
EvtSpbTargetInterruptPassive | The callback that is called at PASSIVE_LEVEL when an interrupt happens on the SPB line.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### SpbTarget_QueuedWorkItem_Type
````
typedef enum
{
  // Sentinel for validity checking.
  //
  SpbTarget_QueuedWorkItem_Invalid,
  // ISR/DPC has no additional work to do.
  //
  SpbTarget_QueuedWorkItem_Nothing,
  // ISR has more work to do at DISPATCH_LEVEL.
  //
  SpbTarget_QueuedWorkItem_Dpc,
  // DPC has more work to do at PASSIVE_LEVEL.
  //
  SpbTarget_QueuedWorkItem_WorkItem
} SpbTarget_QueuedWorkItem_Type;
````
Indicates what the Client wants to do after EvtSpbTargetInterruptIsr/EvtSpbTargetInterruptDpc callbacks have executed.

Member | Description
----|----
SpbTarget_QueuedWorkItem_Nothing | Do nothing.
SpbTarget_QueuedWorkItem_Dpc | Enqueue a DPC.
SpbTarget_QueuedWorkItem_WorkItem | Enqueue a workitem.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_SpbTarget_InterruptIsr
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_SpbTarget_InterruptIsr(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG MessageId,
    _Out_ SpbTarget_QueuedWorkItem_Type* QueuedWorkItem
    );
````

Client callback at DISPATCH_LEVEL when an interrupt occurs on the SPB line of this instantiated Module.

##### Returns

TRUE if this driver recognizes the interrupt as its own.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.
MessageId | The MessageId of the interrupt.
QueuedWorkItem | Indicates what post processing the Client wants to after this callback executes.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_SpbTarget_InterruptDpc
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_SpbTarget_InterruptDpc(
    _In_ DMFMODULE DmfModule,
    _Out_ SpbTarget_QueuedWorkItem_Type* QueuedWorkItem
    );
````

Client callback at DISPATCH_LEVEL when an interrupt occurs on the SPB line of this instantiated Module.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.
QueuedWorkItem | Indicates what post processing the Client wants to after this callback executes.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_SpbTarget_InterruptPassive
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_SpbTarget_InterruptPassive(
    _In_ DMFMODULE DmfModule
    );
````

Client callback at PASSIVE_LEVEL when an interrupt occurs on the SPB line of this instantiated Module.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpbTarget Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SpbTarget_BufferFullDuplex

````
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

* Dmf_RequestStream

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

