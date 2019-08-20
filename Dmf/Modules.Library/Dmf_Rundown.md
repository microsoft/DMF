## DMF_Rundown

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This is used for rundown management when an object is being unregistered but its Methods may still 
be called or running. It allows DMF to make sure the resource is available while Methods that are 
already running continue running, but disallows new Methods from starting to run.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Rundown_Dereference

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Rundown_Dereference(
    _In_ DMFMODULE DmfModule
    );
````

Releases the Module acquired in DMF_Rundown_Reference.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Rundown Module handle.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_Rundown_EndAndWait

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Rundown_EndAndWait(
    _In_ DMFMODULE DmfModule
    );
````

Waits for the Module's reference count to reach zero. This is used for rundown management 
when an object is being unregistered but its Methods may still be called or running. 
It allows DMF to make sure the resource is available while Methods that are 
already running continue running, but disallows new Methods from starting to run.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Rundown Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Rundown_Reference

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Rundown_Reference(
    _In_ DMFMODULE DmfModule
    );
````

Can be wrapped around a resource to make sure it exists until Rundown_Dereference method is called.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Rundown Module handle.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_Rundown_Start

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Rundown_Start(
    _In_ DMFMODULE DmfModule
    );
````

Used to set initial reference count at the start of the Rundown lifetime to 1.
Needs to be called by the Client before the Rundown Reference and Dereference use. 

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Rundown Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

To use this Module the Start method needs to be invoked at the beginning of the resource creation/registration and the
EndAndWait method needs to be invoked at the end of the resource use (deletion/unregistration).

During the lifetime of the resource if any other methods need access to it the Reference and Dereference Methods can be 
nested around those accesses which respectively increment and decrement the reference count of the resource.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Task Execution

-----------------------------------------------------------------------------------------------------------------------------------

