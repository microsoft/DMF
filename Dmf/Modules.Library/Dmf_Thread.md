## DMF_Thread

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a thread as well as an optional event that allows the Client to easily do work in a separate thread.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_Thread
````
typedef struct
{
  // Indicates what callbacks the Client Driver will receive.
  //
  ThreadControlType ThreadControlType;
  union
  {
    // In this mode, the Client Driver is responsible for looping
    // and waiting using Client Driver's structures.
    //
    struct
    {
      // Thread work callback.
      //
      EVT_DMF_Thread_Function* EvtThreadFunction;
    } ClientControl;
    // In this mode, the Client Driver must use the object's Module Methods
    // to set and stop the thread.
    //
    struct
    {
      // Optional callback that does work before looping.
      //
      EVT_DMF_Thread_Function* EvtThreadPre;
      // Mandatory callback that does work when work is ready.
      //
      EVT_DMF_Thread_Function* EvtThreadWork;
      // Optional callback that does work after looping but before thread ends.
      //
      EVT_DMF_Thread_Function* EvtThreadPost;
    } DmfControl;
  } ThreadControl;
} DMF_CONFIG_Thread;
````
Member | Description
----|----
ThreadControlType | Indicates the type of Callback function the Client will write.
EvtThreadFunction | Callback function set when ThreadControlType is set to ThreadControlType_ClientControl. In this case, this callback function does all the work including determining when to wait for work to happen.
EvtThreadPre | Optional callback set when ThreadControlType is set to ThreadControlType_DmfControl. This function performs work on behalf of the Client before this Module's main thread function executes.
EvtThreadWork | Callback set when ThreadControlType is set to ThreadControlType_DmfControl. This callback performs work on behalf of the Client when this Module determines there is work to be done.
EvtThreadPost | Optional callback set when ThreadControlType is set to ThreadControlType_DmfControl. This function performs work on behalf of the Client after this Module's main thread function executes.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### ThreadControlType
````
typedef enum
{
  ThreadControlType_Invalid,
  // The Client will have complete control of thread callback.
  //
  ThreadControlType_ClientControl,
  // The Client will be called when work is available for the Client
  // to perform, but the Client Driver will not control looping.
  //
  ThreadControlType_DmfControl
} ThreadControlType;
````
Member | Description
----|----
ThreadControlType_ClientControl | Indicates that the Client will write the code for the entire thread callback. This setting is used for cases where the thread must wait for multiple events, possible synchronously.
ThreadControlType_DmfControl | Indicates that this Module will provide the entire thread callback. The Client's callback is called only when the Module has work that the Client needs to perform.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Thread_Function
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Thread_Function(
    _In_ DMFMODULE DmfModule
    );
````

This callback is called when this Module has detected that the Client has work to do.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Thread_Start

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Thread_Start(
  _In_ DMFMODULE DmfModule
  );
````

This Method causes the underlying thread to begin execution.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Thread_Stop

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Thread_Stop(
  _In_ DMFMODULE DmfModule
  );
````

##### DMF_Thread_IsStopPending

````
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Thread_IsStopPending(
  _In_ DMFMODULE DmfModule
  );
````

This Method tells the instance of this Module that its Client has work to do.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

##### Remarks

* A call to this Method causes the Client's callback to be called.
* Use this Method only if when ThreadControlType is set to ThreadControlType_DmfControl.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_Thread_WorkReady
````
VOID
DMF_Thread_WorkReady(
  _In_ DMFMODULE DmfModule
  );
````

This Method tells the instance of this Module that its Client has work to do.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

##### Remarks

* A call to this Method causes the Client's callback to be called.
* Use this Method only if when ThreadControlType is set to ThreadControlType_DmfControl.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module makes it easy to create and destroy a simple thread.
* The Client just needs to supply a callback that does Client specific work.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* This Module creates a System Thread and two events that are used to indicate when work is available and when the thread should stop running.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Task Execution

-----------------------------------------------------------------------------------------------------------------------------------

