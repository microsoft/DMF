## DMF_Thread

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a thread and a design pattern where a Client needs to wait for some events in a loop and do some work every time any of the events is set. 

The Module provides the Client two control options : DMF control and client control. In case of DMF control, the Module implements the aforementioned loop and event - making it easy for the Client. In case of client control the Client gets more flexiblity, though it needs to implement the loop and manage the events totally on its own.  

The Client provides callback functions at the time of creating the module. The client is required to Start the Module after it is created (DMF_Thread_Start), and Stop it DMF_Thread_Stop) before it is deleted. 

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_Thread
````
typedef struct
{
  ThreadControlType ThreadControlType;
  union
  {
    struct
    {
      EVT_DMF_Thread_Function* EvtThreadFunction;
    } ClientControl;
    struct
    {
      EVT_DMF_Thread_Function* EvtThreadPre;
      EVT_DMF_Thread_Function* EvtThreadWork;
      EVT_DMF_Thread_Function* EvtThreadPost;
    } DmfControl;
  } ThreadControl;
} DMF_CONFIG_Thread;
````
Member | Description
----|----
ThreadControlType | Indicates the type of control the Client needs: ThreadControlType_ClientControl or ThreadControlType_DmfControl. 
EvtThreadFunction | Callback function set when ThreadControlType is set to ThreadControlType_ClientControl. It is called when the Client calls DMF_Thread_Start method. 
EvtThreadPre | Optional callback set when ThreadControlType is set to ThreadControlType_DmfControl. This callback is called when the Client calls DMF_Thread_Start method. The client may use the callback to perform any necessary initializations.
EvtThreadWork | Callback set when ThreadControlType is set to ThreadControlType_DmfControl. This callback is called when the Client calls the DMF_Thread_WorkReady method. In this callback, the Client process/executes the work that is 'ready'. 
EvtThreadPost | Optional callback set when ThreadControlType is set to ThreadControlType_DmfControl. This callback is called when the Client calls DMF_Thread_Stop method. In this callback the client performs any de-inialization it needs to do before the Module stops. 

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### ThreadControlType
````
typedef enum
{
  ThreadControlType_Invalid,
  ThreadControlType_ClientControl,
  ThreadControlType_DmfControl
} ThreadControlType;
````
Member | Description
----|----
ThreadControlType_ClientControl | The client is expected to implement the looping logic in this control mode in its EvtThreadFunction callback. This setting is typically used for cases where the Client's EvtThreadFunction callback must wait for multiple events, and every time an event is set, the client executes the corresponding work synchronously. When the Client calls the Start method to start the Module, the Module creates a thread and calls the Client's EvtThreadFunction callback. A typical Client loops in that threads waiting for a work-ready event or a termination events that are totally managed by the Client. The Module requires that the Client calls Stop method on the Module prior to deleting it. 
ThreadControlType_DmfControl | DMF simplifies the Client's work by internally implementing the events and the looping logic. The Client calls the Start method to internally start the module, however the Module waits for the Client to call DMF_Thread_Workready method before calling the Client's EvtThreadWork callback. The Client calls the DMF_Thread_WorkReady method everytime work is ready and in response the Module calls the Client's EvtThreadWork callback. A typical Client implementation for EvtThreadWork is to execute all the pending work and exit from the callback, it does not wait for any work-ready events. Instead when the Client needs to do more work, it simply calls DMF_Thread_Workready again, the Module calls the EvtThreadWork callback again. 

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

This callback is called when this Module is started, i.e. the client calls the DMF_Thread_Start method. 

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_Thread_Pre
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Thread_Pre(
	_In_ DMFMODULE DmfModule
	);
````

This is an optional callback that the client may set if the ThreadControlType is set to ThreadControlType_DmfControl.
If this callback is set, it is called when the client calls DMF_Thread_Start. 

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_Thread_Work
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Thread_Work(
	_In_ DMFMODULE DmfModule
	);
````

This is an mandatory callback if the ThreadControlType is set to ThreadControlType_DmfControl. 
This callback is called whenever the Client indicates that work is ready to be processed by calling DMF_Thread_WorkReady method. 

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

##### Remarks
* Multiple calls to DMF_Thread_WorkReady callback may only call the Client's EvtThreadWork once. So from the EvtThreadWork the client must account for all the ready work. However the Module ensures that the client's EvtThreadWork will be called atleast once after 1 or more calls to DMF_Thread_WorkReady (as long as the client doesnt call Dmf_Thread_Stop).
* Thus function is not re-entrant and is sycnrhonized. Even if the client calls DMF_Thread_WorkReady multiple times, the Module ensures that only 1 instance of this callback is running.
* This callback may be called even if there is no work to be processed. This may happen if the prior execution of the callback already processed all the work. The Client's code must account for this case. 
* If there is pending work, and the Client calls DMF_Thread_Stop, this callback may not get called. 

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_Thread_Post
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Thread_Post(
	_In_ DMFMODULE DmfModule
	);
````

This is an optional callback that the client may set if the ThreadControlType is set to ThreadControlType_DmfControl.
If this callback is set, it is called when the client calls DMF_Thread_Stop. 

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

* For ThreadControlType_ClientControl : The Client's EvtThreadFunction callback is called. 
* For ThreadControlType_DmfControl : The Client's EvtThreadPre callback is called, where the client may perform any needed initializations. The EvtThreadWork callback is not called until a future call to DMF_Thread_WorkReady method by the client. 

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Thread_Stop

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Thread_Stop(
  _In_ DMFMODULE DmfModule
  );
````

This Method causes the underlying thread to stop execution. This call blocks until the execution is stopped. 

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

##### Remarks
* Client may only call this method if it has perivously called DMF_Thread_Start and has not called a corresponding DMF_Thread_Stop. 
* Client is allowed to leverage the pattern Start --> Stop --> Start --> Stop
* For ThreadControlType_ClientControl : This call blocks until the Client's EvtThreadFunction callback returns.  
* For ThreadControlType_DmfControl : This call blocks until the Client's EvtThreadWork callback returns. If the Client had called DMF_Thread_WorkReady method, the module does *not* ensrue that the EvtThreadWork would be called before the thread is stopped. 
* For ThreadControlType_DmfControl : The client's optional EvtThreadPost callback is called before this call returns. 

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Thread_IsStopPending

````
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Thread_IsStopPending(
  _In_ DMFMODULE DmfModule
  );
````

????

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

##### Remarks


-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_Thread_WorkReady
````
VOID
DMF_Thread_WorkReady(
  _In_ DMFMODULE DmfModule
  );
````

This Method tells the instance of this Module that its Client has work to do. In response to this call
the Client's EvtThreadWork callback is called. 

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Thread Module handle.

##### Remarks

* Multiple calls to DMF_Thread_WorkReady callback may only call the Client's EvtThreadWork once. So from the EvtThreadWork the client must account for all the ready work. However the Module ensures that teh client's EvtThreadWork will be called atleast once after 1 or more calls to DMF_Thread_WorkReady (as long as the client doesnt call Dmf_Thread_Stop).
* Use this Method only if when ThreadControlType is set to ThreadControlType_DmfControl.
* This Method can be called before the Thread starts as well.

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

