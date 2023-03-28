## DMF_InterruptResource

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module gives the Client access to interrupt resources.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_InterruptResource
````
typedef struct
{
    // Module will not load if Interrupt not found.
    //
    BOOLEAN InterruptMandatory;
    // Interrupt index for this instance.
    //
    ULONG InterruptIndex;
    // Passive handling.
    //
    BOOLEAN PassiveHandling;
    // Can the interrupt resources wake the device.
    //
    BOOLEAN CanWakeDevice;
    // Optional Callback from ISR (with Interrupt Spin Lock held).
    //
    EVT_DMF_InterruptResource_InterruptIsr* EvtInterruptResourceInterruptIsr;
    // Optional Callback at DPC_LEVEL Level.
    //
    EVT_DMF_InterruptResource_InterruptDpc* EvtInterruptResourceInterruptDpc;
    // Optional Callback at PASSIVE_LEVEL Level.
    //
    EVT_DMF_InterruptResource_InterruptPassive* EvtInterruptResourceInterruptPassive;
} DMF_CONFIG_InterruptResource;

````
Member | Description
----|----
InterruptMandatory | Module must find the Interrupt at InterruptIndex in order to initialize properly.
InterruptIndex | The index of the Interrupt that this Module's instance should access.
PassiveHandling | Indicates that interrupts that happen on the specified interrupt resource should be dispatched to Client's callback at PASSIVE_LEVEL. Otherwise, the Client's callback is called at DISPATCH_LEVEL.
CanWakeDevice | Indicates if the specified interrupt resource should be able wake the system.
EvtInterruptResourceInterruptIsr | The callback that is called at DPC_LEVEL when an interrupt happens on the specified interrupt resource. The interrupt spin-lock is already acquired while this callback runs.
EvtInterruptResourceInterruptDpc | The callback that is called at DISPATCH_LEVEL when an interrupt happens on the specified interrupt resource.
EvtInterruptResourceInterruptPassive | The callback that is called at PASSIVE_LEVEL when an interrupt happens on the specified interrupt resource.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### InterruptResource_QueuedWorkItem_Type
````
typedef enum
{
  // Sentinel for validity checking.
  //
  InterruptResource_QueuedWorkItem_Invalid,
  // ISR/DPC has no additional work to do.
  //
  InterruptResource_QueuedWorkItem_Nothing,
  // ISR has more work to do at DISPATCH_LEVEL.
  //
  InterruptResource_QueuedWorkItem_Dpc,
  // DPC has more work to do at PASSIVE_LEVEL.
  //
  InterruptResource_QueuedWorkItem_WorkItem
} InterruptResource_QueuedWorkItem_Type;
````
Indicates what the Client wants to do after EvtInterruptResourceInterruptIsr/EvtInterruptResourceInterruptDpc callbacks have executed.

Member | Description
----|----
InterruptResource_QueuedWorkItem_Nothing | Do nothing.
InterruptResource_QueuedWorkItem_Dpc | Enqueue a DPC.
InterruptResource_QueuedWorkItem_WorkItem | Enqueue a workitem.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_InterruptResource_InterruptIsr
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_InterruptResource_InterruptIsr(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG MessageId,
    _Out_ InterruptResource_QueuedWorkItem_Type* QueuedWorkItem
    );
````

Client callback at DISPATCH_LEVEL when an interrupt occurs on the specified interrupt resource of this instantiated Module.

##### Returns

TRUE if this driver recognizes the interrupt as its own.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_InterruptResource Module handle.
MessageId | The MessageId of the interrupt.
QueuedWorkItem | Indicates what post processing the Client wants to after this callback executes.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_InterruptResource_InterruptDpc
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_InterruptResource_InterruptDpc(
    _In_ DMFMODULE DmfModule,
    _Out_ InterruptResource_QueuedWorkItem_Type* QueuedWorkItem
    );
````

Client callback at DISPATCH_LEVEL when an interrupt occurs on the specified interrupt resource of this instantiated Module.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_InterruptResource Module handle.
QueuedWorkItem | Indicates what post processing the Client wants to after this callback executes.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_InterruptResource_InterruptPassive
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_InterruptResource_InterruptPassive(
    _In_ DMFMODULE DmfModule
    );
````

Client callback at PASSIVE_LEVEL when an interrupt occurs on the specified interrupt resource of this instantiated Module.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_InterruptResource Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_InterruptResource_InterruptAcquireLock

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_InterruptResource_InterruptAcquireLock(
    _In_ DMFMODULE DmfModule
    )
````
Acquires the interrupt's spin lock.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_InterruptResource Module handle.

##### Remarks

* Use this Method to synchronize execution of the ISR's handler with other code in the driver.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_InterruptResource_InterruptDisable

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_InterruptResource_InterruptDisable(
    _In_ DMFMODULE DmfModule
    )
````
Disables the interrupt from firing.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_InterruptResource Module handle.

##### Remarks

* WDF automatically disables the interrupt during D0Exit. However, in some cases it might be necessary to disable the interrupt on demand.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_InterruptResource_InterruptEnable

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_InterruptResource_InterruptEnable(
    _In_ DMFMODULE DmfModule
    )
````
Enables the interrupt.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_InterruptResource Module handle.

##### Remarks

* WDF automatically enables the interrupt during D0Entry so it is normally not necessary to call this Method.
* Some drivers need the interrupt enabled earlier than D0Entry in order to perform operations such as download firmware. Use this Method in such cases.
* It is not necessary to explicitly disable the interrupt if this Method is called because WDF does so automatically.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_InterruptResource_InterruptReleaseLock

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_InterruptResource_InterruptReleaseLock(
    _In_ DMFMODULE DmfModule
    )
````
Releases the interrupt's spin lock.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_InterruptResource Module handle.

##### Remarks

* Use this Method to synchronize execution of the ISR's handler with other code in the driver.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_InterruptResource_InterruptTryToAcquireLock

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_InterruptResource_InterruptTryToAcquireLock(
    _In_ DMFMODULE DmfModule
    )
````
Attempts to acquire the interrupt's spin lock if it is not already held.

##### Returns

TRUE if the the call to this Method is able to acquire the interrupt's spin lock; FALSE, otherwise.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_InterruptResource Module handle.

##### Remarks

* Use this Method to synchronize execution of the ISR's handler with other code in the driver.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_InterruptResource_IsResourceAssigned

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_InterruptResource_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* InterruptAssigned
    )
````
Tells the caller if the requested interrupt resource is available.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_InterruptResource Module handle.
InterruptAssigned | TRUE if the resource is available and assigned.

##### Remarks

* This Method is useful in cases where the interrupt is not mandatory.
* Use this Method in a driver that supports configurations that may or may not use an interrupt.
* The availability of the interrupt is determined by the ASL code that describes the device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* Modules that want to provide interrupt resources for their Clients use this Module to allow for most common possible configurations.

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

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

