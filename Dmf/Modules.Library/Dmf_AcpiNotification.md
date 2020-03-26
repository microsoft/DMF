## DMF_AcpiNotification

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module allows the Client to request and receive asynchronous notifications from ACPI.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_AcpiNotification
````
typedef struct
{
  // Client's DISPATCH_LEVEL callback when Acpi Notification happens.
  //
  EVT_DMF_AcpiNotification_Dispatch* DispatchCallback;
  // Client's PASSIVE_LEVEL callback when Acpi Notification happens.
  //
  EVT_DMF_AcpiNotification_Passive* PassiveCallback;
} DMF_CONFIG_AcpiNotification;
````
Member | Description
----|----
DispatchCallback | Optional Client callback that receives notifications at DISPATCH_LEVEL.
PassiveCallback | Optional Client callback that receives notifications at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_AcpiNotification_Dispatch

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_AcpiNotification_Dispatch(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG NotifyCode
    );
````

Client callback that receives notifications from ACPI at DISPATCH_LEVEL.

##### Returns

TRUE if the Client wants the optional PASSIVE_LEVEL callback to be called after this callback returns. FALSE, otherwise.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiNotification Module handle.
NotifyCode | Indicates the reason for the notification from ACPI.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_AcpiNotification_Passive
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_AcpiNotification_Passive(
    _In_ DMFMODULE DmfModule
    );
````

Client callback that receives notifications from ACPI at DISPATCH_LEVEL.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiNotification Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

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

