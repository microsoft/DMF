## DMF_AcpiNotification

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module allows the Client to request and receive asynchronous notifications from ACPI.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

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
    // Allows Client to start/stop notifications on demand.
    // Otherwise, notifications start/stop during PrepareHardare/ReleaseHardware.
    //
    BOOLEAN ManualMode;
} DMF_CONFIG_AcpiNotification;
````

Member | Description
----|----
DispatchCallback | Optional Client callback that receives notifications at DISPATCH_LEVEL.
PassiveCallback | Optional Client callback that receives notifications at PASSIVE_LEVEL.
ManualMode | Allows the Client to control when the notifications are enabled using `DMF_AcpiNotification_EnableDisable`. Default is FALSE which causes the notifications to start when Module receives `EVT_DevicePrepareHardware` and stop when Module receives`EVT_DeviceReleaseHardware`.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

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

##### DMF_AcpiNotification_EnableDisable

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiNotification_EnableDisable(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EnableNotifications
    )
````

Allows Client to enable/disable notifications from ACPI on demand.

##### Returns

NTSTATUS

##### Parameters

Parameter | Description
----|----
DmfModule | An open DMF_ThreadedBufferQueue Module handle.
EnableNotifications | TRUE to enable notifications. FALSE to disable notifications.

##### Remarks

* Use this Method when ManualMode = TRUE in the Module Config.
* Even if ManualMode == FALSE, the Client may use this Method.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

