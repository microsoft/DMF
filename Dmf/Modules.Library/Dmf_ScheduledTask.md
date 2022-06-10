## DMF_ScheduledTask

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module allows Clients to scheduled work that will completed a SINGLE time, either
for the duration of the Driver's runtime or persistent over reboots.

NOTE: A better name for this Module is "ScheduleTaskOnce".

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_ScheduledTask
````
typedef struct
{
  // The Client Driver function that will perform work one time in a work item.
  //
  EVT_DMF_ScheduledTask_Callback* EvtScheduledTaskCallback;
  // Client context for above callback used by the timer and by
  // DMF_ScheduledTask__ExecuteNowDeferredEx Method.
  //
  VOID* CallbackContext;
  // Indicates if the operation should be done every time driver loads or only a
  // single time (persistent across reboots).
  //
  ScheduledTask_Persistence_Type PersistenceType;
  // Indicates if the callback is executed immediately or is deferred.
  //
  ScheduledTask_ExecutionMode_Type ExecutionMode;
  // Indicates when the callback is executed, D0Entry or PrepareHardware.
  //
  ScheduledTask_ExecuteWhen_Type ExecuteWhen;
  // Retry timeout for ScheduledTask_WorkResult_SuccessButTryAgain.
  //
  ULONG TimerPeriodMsOnSuccess;
  // Retry timeout for ScheduledTask_WorkResult_FailButTryAgain.
  //
  ULONG TimerPeriodMsOnFail;
  // Delay before initial deferred call begins.
  //
  ULONG TimeMsBeforeInitialCall;
} DMF_CONFIG_ScheduledTask;
````
Member | Description
----|----
EvtScheduledTaskCallback | It is the Client callback that is executed.
CallbackContext | it is the Client specific context that is passed to EvtScheduledTaskCallback.
PersistenceType | Indicates whether or not the number of times EvtScheduledTaskCallback has executed is persistent across reboots or not.
ExecutionMode | Indicates whether or not EvtScheduledTaskCallback executes in the caller's thread or a different thread.
ExecuteWhen | Indicates when EvtScheduledTaskCallback should be called.
TimerPeriodMsOnSuccess | The amount of time to wait in milliseconds until the EvtScheduledTaskCallback is called again in the case of a successful call.
TimerPeriodMsOnFail | The amount of time to wait in milliseconds until the EvtScheduledTaskCallback is called again in the case of a failed call.
TimeMsBeforeInitialCall | The amount of time to wait in milliseconds before the initial deferred call occurs. Default is zero milliseconds.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### ScheduledTask_Result_Type
````
typedef enum
{
  ScheduledTask_WorkResult_Invalid,
  ScheduledTask_WorkResult_Success,
  ScheduledTask_WorkResult_Fail,
  // Use in deferred case only.
  //
  ScheduledTask_WorkResult_FailButTryAgain,
  ScheduledTask_WorkResult_SuccessButTryAgain
} ScheduledTask_Result_Type;
````
Member | Description
----|----
ScheduledTask_WorkResult_Success | Indicates that the Client callback succeeded. **IMPORTANT:** The callback will never be called again based on the
the persistence type setting.
ScheduledTask_WorkResult_Fail | Indicates that the Client callback failed.
ScheduledTask_WorkResult_FailButTryAgain | Indicates that the Client callback fails but that another attempt should be made automatically based on fail timer interval.
ScheduledTask_WorkResult_SuccessButTryAgain | Indicates that the Client callback succeeded, but that another attempt should be made based on success timer interval.

-----------------------------------------------------------------------------------------------------------------------------------
##### ScheduledTask_Persistence_Type
````
typedef enum
{
  ScheduledTask_Persistence_Invalid,
  // Only do the work one time ever.
  //
  ScheduledTask_Persistence_PersistentAcrossReboots,
  // Do the work every time the Client starts.
  //
  ScheduledTask_Persistence_NotPersistentAcrossReboots
} ScheduledTask_Persistence_Type;
````
Member | Description
----|----
ScheduledTask_Persistence_PersistentAcrossReboots | It means that the number of times the callback has executed should be written to the registry so that the value is persistent across reboots.
ScheduledTask_Persistence_NotPersistentAcrossReboots | It means that the number of times the callback has executed should not be written to the registry so that the value is not persistent across reboots.

-----------------------------------------------------------------------------------------------------------------------------------
##### ScheduledTask_ExecuteWhen_Type
````
typedef enum
{
  ScheduledTask_ExecuteWhen_Invalid,
  // In PrepareHardware.
  //
  ScheduledTask_ExecuteWhen_PrepareHardware,
  // In D0Entry.
  //
  ScheduledTask_ExecuteWhen_D0Entry,
  // Not applicable for this Module. It means Client uses other Methods
  // do other work.
  //
  ScheduledTask_ExecuteWhen_Other
} ScheduledTask_ExecuteWhen_Type;
````
Member | Description
----|----
ScheduledTask_ExecuteWhen_PrepareHardware | It means that the Client callback will try to execute from EvtDevicePrepareHardware callback. It may or may not actually execute depending on other attributes set during instantiation.
ScheduledTask_ExecuteWhen_D0Entry | It means that the Client callback will try to execute from EvtDeviceD0Entry callback. It may or may not actually execute depending on other attributes set during instantiation.
ScheduledTask_ExecuteWhen_Other | It means that the Client callback will execute neither in during PrepareHardware nor D0Exntry.

-----------------------------------------------------------------------------------------------------------------------------------
##### ScheduledTask_ExecutionMode_Type
````
typedef enum
{
  ScheduledTask_ExecutionMode_Invalid,
  ScheduledTask_ExecutionMode_Immediate,
  ScheduledTask_ExecutionMode_Deferred
} ScheduledTask_ExecutionMode_Type;
````
Member | Description
----|----
ScheduledTask_ExecutionMode_Immediate | Indicates that the Client callback should be called in the thread of the caller. For example, if the caller is in EvtDevicePrepareHardware, this call will happen in the same thread.
ScheduledTask_ExecutionMode_Deferred | Indicates that the Client callback should be called in a different thread than the caller. For example, if the caller is in EvtDevicePrepareHardware, this call will happen in a different thread.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_ScheduledTask_Callback
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
ScheduledTask_Result_Type
EVT_DMF_ScheduledTask_Callback(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* CallbackContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );
````

This is the DMF_ScheduledTask Client callback that performs work on behalf of the Client. Depending on how the Module is
instantiated, this Method may or not be called as necessary. This callback is also used with the Methods that cause
this callback to execute on demand immediately or on demand deferred.

**Important:** It is possible that this callback can occur in two different threads during ReleaseHardware/D0Entry in cases when retries happen.
It may be necessary to synchronize the code inside this callback.

##### Parameters
Parameter | Description
----|----
ClientContext | Client driver specific context buffer. (This is usually a Client specific context which is the same for all calls.)
CallbackContext | Client driver specific context buffer. (This is usually a Client specific context which is different for all calls.)
PreviousState | Used when the callback is executed in D0Entry to tell the Client the previous power state.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_ScheduledTask_Cancel

````
VOID
DMF_ScheduledTask_Cancel(
  _In_ DMFModule DmfModule
  );
````

Cancel any pending ScheduledTask Execution.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ScheduledTask Module handle.

##### Remarks

* Call `DMF_ScheduledTask_Cancel()` before calling `WdfObjectDelete()` when a dynamic instance is used.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_ScheduledTask_ExecuteNow
````
_Must_inspect_result_
ScheduledTask_Result_Type
DMF_ScheduledTask_ExecuteNow(
  _In_ DMFModule DmfModule,
  _In_ VOID* CallbackContext
  );
````

This Method causes the deferred code to execute now. The deferred code will execute before this call completes.

##### Returns

NTSTATUS. Fails if the Request cannot be added to the queue.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ScheduledTask Module handle.
CallbackContext | This is a Client specific context that is passed to the Client's deferred execution callback.

##### Remarks

* Is possible to use this Method from a Module's Close callback.
* In this case, the Client callback is called in the same thread.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_ScheduledTask_ExecuteNowDeferred
````
_Must_inspect_result_
NTSTATUS
DMF_ScheduledTask_ExecuteNowDeferred(
  _In_ DMFModule DmfModule,
  _In_opt_ VOID* CallbackContext
  );
````

**IMPORTANT:** Use `DMF_ScheduledTask_ExecuteNowDeferredEx` instead of this Method.

This Method causes the deferred code to execute one time (at a later time). The deferred code will execute shortly
after this call completes. The callback's return value is **not** honored using this Method (due to a an error in the logic).

##### Returns

NTSTATUS. Fails if the Request cannot be added to the queue.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ScheduledTask Module handle.
CallbackContext | This is a Client specific context that is passed to the Client's deferred execution callback.

##### Remarks

* Is possible to use this Method from a Module's Close callback.
* It is possible to use this Method to execute code in a different thread. Sometimes, due to locking issues, this capability is needed.
* It is best to use the Ex version of this Method as it works intuitively.
* This Method is present to maintain compatibility with legacy Clients.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_ScheduledTask_ExecuteNowDeferredEx
````
_Must_inspect_result_
NTSTATUS
DMF_ScheduledTask_ExecuteNowDeferredEx(
  _In_ DMFModule DmfModule
  );
````

This Method causes the deferred code to execute one time (at a later time). The deferred code will execute shortly
after this call completes. The callback's return value **is** honored using this Method. The callback is passed
the context specified in Module config.

##### Returns

NTSTATUS. Fails if the Request cannot be added to the queue.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ScheduledTask Module handle.

##### Remarks

* Is possible to use this Method from a Module's Close callback.
* It is possible to use this Method to execute code in a different thread. Sometimes, due to locking issues, this capability is needed.
* **IMPORTANT:** When the callback returns `ScheduledTask_WorkResult_Success` it means that the callback will never execute again.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_ScheduledTask_Restart

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ScheduledTask_Restart(
    _In_ DMFMODULE DmfModule
    );
````

This Method allows Client to schedule tasks after `DMF_ScheduledTask_Cancel()` has been called.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ScheduledTask Module handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_ScheduledTask_TimesRunGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ScheduledTask_TimesRunGet(
  _In_ DMFModule DmfModule,
  _Out_ ULONG* TimesRun
  );
````

This Method reads the default registry setting that keeps track of the number of times the DMF_ScheduledTask routine has executed.

##### Returns

NTSTATUS. Fails if the value cannot be read from the registry.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ScheduledTask Module handle.
TimesRun | Returns the number of times the DMF_ScheduledTask routine has executed (the value read from the registry).

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_ScheduledTask_TimesRunSet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ScheduledTask_TimesRunSet(
  _In_ DMFModule DmfModule,
  _In_ ULONG TimesRun
  );
````

This Method writes the default registry setting that keeps track of the number of times the DMF_ScheduledTask routine has executed to a
given value.

##### Returns

NTSTATUS. Fails if the value cannot be written to the registry.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ScheduledTask Module handle.
TimesRun | The given value to write to the registry.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module is useful when it is necessary to execute code in a different thread due to locking constraints.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* This Module implements a WDFTIMER and a WDFWORKITEM and uses either as needed.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DMF_QueuedWorkItem

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

* Add support for a delay in the loop that executes all the deferred calls to prevent the processor from spinning for too long.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Task Execution

-----------------------------------------------------------------------------------------------------------------------------------

