## DMF_Doorbell

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

Allows the Client to enqueue multiple requests to a callback such that
only a single workitem is equeued regardless of how many times the 
enqueue Method is called. If several enqueues occur prior to the
corresponding callback being called, the callback is only called one time.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_Doorbell
````
typedef struct
{
    // Callback called when the doorbell is rung.
    //
    EVT_DMF_Doorbell_ClientCallback* WorkItemCallBack;
} DMF_CONFIG_Doorbell;
````
Member | Description
----|----
WorkItemCallBack | Callback called when the doorbell is rung.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

typedef
_Function_class_(EVT_DMF_Doorbell_ClientCallback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Doorbell_ClientCallback(
    _In_ DMFMODULE DmfModule
    );

Callback called when doorbell is rung. It is only called a single time regardless of the number
of times the doorbell has been rung.

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.

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

Rings the doorbell and the work Item is enqueued if it is not already.

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.

##### Remarks

Module method to be called by Client whenever the registered callback needs to be invoked.

##### DMF_Doorbell_Flush

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Doorbell_Flush(
    _In_ DMFMODULE DmfModule
    );
````

Client calls this Method to flush and wait for pending callbacks to complete.

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.

##### Remarks

Module Method to be called by Client whenever the rung doorbell needs to be flushed.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* PostureViaSsh DMF Module

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Task Execution

-----------------------------------------------------------------------------------------------------------------------------------
