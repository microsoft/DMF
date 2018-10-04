## DMF_GpioTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module gives the Client access to GPIO pins.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_GpioTarget
````
typedef struct
{
  // Module will not load if Gpio Connection not found.
  //
  BOOLEAN GpioConnectionMandatory;
  // Module will not load if Interrupt not found.
  //
  BOOLEAN InterruptMandatory;
  // GPIO Connection index for this instance.
  //
  ULONG GpioConnectionIndex;
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
  // Can GPIO wake the device.
  //
  BOOLEAN CanWakeDevice;
  // Optional Callback from ISR (with Interrupt Spin Lock held).
  //
  EVT_DMF_GpioTarget_InterruptIsr* EvtGpioTargetInterruptIsr;
  // Optional Callback at DPC_LEVEL Level.
  //
  EVT_DMF_GpioTarget_InterruptDpc* EvtGpioTargetInterruptDpc;
  // Optional Callback at PASSIVE_LEVEL Level.
  //
  EVT_DMF_GpioTarget_InterruptPassive* EvtGpioTargetInterruptPassive;
} DMF_CONFIG_GpioTarget;
````
Member | Description
----|----
GpioConnectionMandatory | Module must find the Gpio connection at GpioConnectionIndex in order to initialize properly.
InterruptMandatory | Module must find the Interrupt at InterruptIndex in order to initialize properly.
GpioConnectionIndex | The index of the GPIO line that this Module's instance should access.
InterruptIndex | The index of the Interrupt that this Module's instance should access.
OpenMode | Indicates if this Module's instance will read and/or write from/to the GPIO line.
ShareAccess | Indicates if this Module's instance will access the GPIO line in exclusive mode.
PassiveHandling | Indicates that interrupts that happen on the GPIO line should be dispatched to Client's callback at PASSIVE_LEVEL. Otherwise, the Client's callback is called at DISPATCH_LEVEL.
CanWakeDevice | Indicates if the GPIO line should be able wake the system.
EvtGpioTargetInterruptIsr | The callback that is called at DPC_LEVEL when an interrupt happens on the GPIO line. The interrupt spin-lock is already acquired while this callback runs.
EvtGpioTargetInterruptDpc | The callback that is called at DISPATCH_LEVEL when an interrupt happens on the GPIO line.
EvtGpioTargetInterruptPassive | The callback that is called at PASSIVE_LEVEL when an interrupt happens on the GPIO line.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### GpioTarget_QueuedWorkItem_Type
````
typedef enum
{
  // Sentinel for validity checking.
  //
  GpioTarget_QueuedWorkItem_Invalid,
  // ISR/DPC has no additional work to do.
  //
  GpioTarget_QueuedWorkItem_Nothing,
  // ISR has more work to do at DISPATCH_LEVEL.
  //
  GpioTarget_QueuedWorkItem_Dpc,
  // DPC has more work to do at PASSIVE_LEVEL.
  //
  GpioTarget_QueuedWorkItem_WorkItem
} GpioTarget_QueuedWorkItem_Type;
````
Indicates what the Client wants to do after EvtGpioTargetInterruptIsr/EvtGpioTargetInterruptDpc callbacks have executed.

Member | Description
----|----
GpioTarget_QueuedWorkItem_Nothing | Do nothing.
GpioTarget_QueuedWorkItem_Dpc | Enqueue a DPC.
GpioTarget_QueuedWorkItem_WorkItem | Enqueue a workitem.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_GpioTarget_InterruptIsr
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_GpioTarget_InterruptIsr(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG MessageId,
    _Out_ GpioTarget_QueuedWorkItem_Type* QueuedWorkItem
    );
````

Client callback at DISPATCH_LEVEL when an interrupt occurs on the GPIO line of this instantiated Module.

##### Returns

TRUE if this driver recognizes the interrupt as its own.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_GpioTarget Module handle.
MessageId | The MessageId of the interrupt.
QueuedWorkItem | Indicates what post processing the Client wants to after this callback executes.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_GpioTarget_InterruptDpc
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_GpioTarget_InterruptDpc(
    _In_ DMFMODULE DmfModule,
    _Out_ GpioTarget_QueuedWorkItem_Type* QueuedWorkItem
    );
````

Client callback at DISPATCH_LEVEL when an interrupt occurs on the GPIO line of this instantiated Module.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_GpioTarget Module handle.
QueuedWorkItem | Indicates what post processing the Client wants to after this callback executes.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_GpioTarget_InterruptPassive
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_GpioTarget_InterruptPassive(
    _In_ DMFMODULE DmfModule
    );
````

Client callback at PASSIVE_LEVEL when an interrupt occurs on the GPIO line of this instantiated Module.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_GpioTarget Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_GpioTarget_InterruptAcquireLock

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_GpioTarget_InterruptAcquireLock(
  _In_ DMFMODULE DmfModule
  );
````

Acquires the interrupt spin-lock associated with the GPIO pin configured by the Client.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_GpioTarget Module handle.
Remarks | * See WDF documentation to understand how interrupt spin-locks work at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_GpioTarget_InterruptReleaseLock

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_GpioTarget_InterruptReleaseLock(
  _In_ DMFMODULE DmfModule
  );
````

Releases the interrupt spin-lock associated with the GPIO pin configured by the Client.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_GpioTarget Module handle.
Remarks | * See WDF documentation to understand how interrupt spin-locks work at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

_Must_inspect_result_

##### DMF_GpioTarget_InterruptTryToAcquireLock

````
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_GpioTarget_InterruptTryToAcquireLock(
  _In_ DMFMODULE DmfModule
  );
````

Acquires the interrupt spin-lock associated with the GPIO pin configured by the Client.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_GpioTarget Module handle.
Remarks | * See WDF documentation to understand how interrupt spin-locks work at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_GpioTarget_IsResourceAssigned

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_GpioTarget_IsResourceAssigned(
  _In_ DMFMODULE DmfModule,
  _Out_opt_ BOOLEAN* GpioConnectionAssigned,
  _Out_opt_ BOOLEAN* InterruptAssigned
  );
````

Indicates if the GPIO resources set by the Client have been found.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_GpioTarget Module handle.
NumberOfGpioLinesAssigned | The given event index.
NumberOfGpioInterruptsAssigned | The number of GPIO interrupts this Module's instance assigned.
Remarks | * Use this Method to determine if the resources needed are found. This is for the cases where the Client driver runs regardless of whether or not GPIO lines are available.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_GpioTarget_Read

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_GpioTarget_Read(
  _In_ DMFMODULE DmfModule,
  _Out_ BOOLEAN* PinValue
  );
````

Reads the state of the GPIO line of the instantiated Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_GpioTarget Module handle.
PinValue | The value read from the GPIO line.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_GpioTarget_Write

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_GpioTarget_Write(
  _In_ DMFMODULE DmfModule,
  _In_ BOOLEAN Value
  );
````

Sets the state of the GPIO line to a given state.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_GpioTarget Module handle.
Value | The given state.

##### Remarks

* The name of this Method should be DMF_GpioTarget_LineStateSet.
* This Method does not toggle the pin.
* Value should be called LineState.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module access a single GPIO line.
* A Client must instantiate one instance of this Module for every GPIO line the Client needs to access.

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

Targets

-----------------------------------------------------------------------------------------------------------------------------------

