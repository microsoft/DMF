## DMF_Doorbell

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Supports client to synchronize the registered callback by ensuring
to have one Workitem enqueue/running at a time which invokes the client's
registered callback by tracking the doorbell ring.
It doesn't matter how many times the client rings the doorbell,
this module will invoke the client's registered callbacks once before
it clears the tracking. And double check the tracking for any doorbell ring
before completes its Work Item, to ensure not to miss any doorbell ring.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_Doorbell
````
typedef struct
{
    // Function to call from Work Item once the Doorbell is ringed
    //
    EVT_DMF_Doorbell_ClientCallback* WorkItemCallBack;
} DMF_CONFIG_Doorbell;
````
Member | Description
----|----
WorkItemCallBack | Client callback to be called once the Doorbell is ringed.

##### Remarks

WorkItemCallBack is a clients callback that Module will call when the Doorbell is ringed.
-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

````
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Doorbell_WorkitemHandler(
    _In_ WDFWORKITEM Workitem
    );
````

##### Returns

*None

##### Parameters
Parameter | Description
----|----
Workitem - WDFORKITEM which gives access to necessary context including this
               Module's DMF Module.

##### Remarks

Callback called by Wdf WorkItem when the Work Item enqueued.

----
#### Module Methods
----

##### DMF_Doorbell_Ring

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Doorbell_Ring(
    _In_ DMFMODULE DmfModule
    );
````

Rings the Doorbell and the work Item is Enqueued if it is not already.

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.

##### Remarks

Module method to be called by client whenever the registered callback needs to be invoked.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Doorbell_Flush

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Doorbell_Flush(
    _In_ DMFMODULE DmfModule
    );
````

This is used to wait and flush the Work Item if pending.

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.

##### Remarks

Module method to be called by client whenever the ringed doorbell / Workitem needs to be flushed.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module can be used by any client which needs to get schedule their workitem callback once
before its callback executed even if multiple enuque happened from different thread context.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* PostureViaSsh DMF Module

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Hardware

-----------------------------------------------------------------------------------------------------------------------------------
