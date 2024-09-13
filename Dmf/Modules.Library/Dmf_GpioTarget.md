## DMF_GpioTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module gives the Client access to GPIO pins.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_GpioTarget
````
typedef struct
{
    // Module will not load if Gpio Connection not found.
    //
    BOOLEAN GpioConnectionMandatory;
    // GPIO Connection index for this instance.
    //
    ULONG GpioConnectionIndex;
    // Open in Read or Write mode.
    //
    ACCESS_MASK OpenMode;
    // Share Access.
    //
    ULONG ShareAccess;
    // Interrupt Resource.
    //
    DMF_CONFIG_InterruptResource InterruptResource;
} DMF_CONFIG_GpioTarget;
````
Member | Description
----|----
GpioConnectionMandatory | Module must find the Gpio connection at GpioConnectionIndex in order to initialize properly.
GpioConnectionIndex | The index of the GPIO line that this Module's instance should access.
OpenMode | Indicates if this Module's instance will read and/or write from/to the GPIO line.
ShareAccess | Indicates if this Module's instance will access the GPIO line in exclusive mode.
InteruptResource | Allows Client to specify an interrupt resource associated with the interrupt resource.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

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

#### Remarks
* See WDF documentation to understand how interrupt spin-locks work at PASSIVE_LEVEL.

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

#### Remarks
* See WDF documentation to understand how interrupt spin-locks work at PASSIVE_LEVEL.

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

#### Remarks
* See WDF documentation to understand how interrupt spin-locks work at PASSIVE_LEVEL.

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

#### Remarks
* Use this Method to determine if the resources needed are found. This is for the cases where the Client driver runs regardless of whether or not GPIO lines are available.

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

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module accesses a single GPIO line.
* A Client must instantiate one instance of this Module for every GPIO line the Client needs to access.
* IMPORTANT: If the device only has an interrupt resource defined, then use DMF_InterruptResource Module instead of this Module.
* If the Client needs to read/write from/to the GPIO interrupt, then the Client should instantiate a single instance of this Module setting both the GpioConnectionIndex and the InterruptLineIndex set.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Create DMF_GpioEx Module that will allow Client to connect only to interrupt.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Targets

-----------------------------------------------------------------------------------------------------------------------------------

