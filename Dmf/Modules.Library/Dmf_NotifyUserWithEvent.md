## DMF_NotifyUserWithEvent

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

Implements a driver pattern that allows a Kernel-mode driver to share a named event with a User-mode component.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_NotifyUserWithEvent
````
typedef struct
{
  // This field allows for validation of the index.
  //
  ULONG MaximumEventIndex;
  // The names of each of the events that are to be opened.
  //
  UNICODE_STRING EventNames[NotifyUserWithEvent_MAXIMUM_EVENTS];
} DMF_CONFIG_NotifyUserWithEvent;
````
Member | Description
----|----
MaximumEventCount | The maximum number of events this instantiated Module exposes.
EventNames | The name of each exposed event.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_NotifyUserWithEvent_Notify

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithEvent_Notify(
  _In_ DMFMODULE DmfModule
  );
````

Sets the default event in an instance of this Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithEvent Module handle.

##### Remarks

* Most Clients that use User-mode Events only use one event so this Method is most commonly used.

##### DMF_NotifyUserWithEvent_NotifyByIndex

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithEvent_NotifyByIndex(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG EventIndex
  );
````

Sets the event associated with the given event index in an instance of this Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithEvent Module handle.
EventIndex | The given event index.

##### Remarks

* Most Clients that use User-mode Events only use one event.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module performs the functions necessary to convert the User-mode handles to Kernel-mode handles and reference the handle so that they are valid for the lifetime of the driver.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

User Notification

-----------------------------------------------------------------------------------------------------------------------------------

