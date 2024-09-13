## DMF_ActivitySensor

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module allows the Client to monitor the Activity interface and get current device Activity state information.

https://docs.microsoft.com/en-us/uwp/api/windows.devices.sensors.activitysensor?view=winrt-19041

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_ActivitySensor
````
// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Specific Activity device Id to open. This is optional.
    //
    WCHAR* DeviceId;
    // Callback to inform Parent Module that Activity has new changed reading.
    //
    EVT_DMF_ActivitySensor_EvtActivitySensorReadingChangedCallback* EvtActivitySensorReadingChangeCallback;
} DMF_CONFIG_ActivitySensor;
````
Member | Description
----|----
DeviceId | Specific Activity device Id to open. If Client does not set DeviceId, it opens default Activity sensor if it exists.
EvtActivitySensorReadingChangeCallback | Allows the Client to get the status of Activity state when it changes.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

##### ACTIVITY_SENSOR_STATE

````
typedef enum class _ACTIVITY_READING
{
    Unknown = 0,
    Idle = 1,
    Stationary = 2,
    Fidgeting = 3,
    Walking = 4,
    Running = 5,
    InVehicle = 6,
    Biking = 7
} ActivitySensor_Reading;
````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

##### ACTIVITY_SENSOR_STATE
````
typedef struct _ACTIVITY_SENSOR_STATE
{
    BOOLEAN IsSensorValid;
    ActivitySensor_Reading CurrentActivitySensor;
} ACTIVITY_SENSOR_STATE;
````
Member | Description
----|----
IsSensorValid | Indicates whether Activity interface is valid or not.
CurrentActivitySensor | Current Activity status.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_ActivitySensor_EvtActivitySensorReadingChangedCallback
````
typedef
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_ActivitySensor_EvtActivitySensorReadingChangedCallback(
    _In_ DMFMODULE DmfModule,
    _In_ ACTIVITY_SENSOR_STATE* ActivitySensorState
    );
````

Client specific callback that allows the Client to get status of Activity when it changes.

##### Returns

None

##### Parameters
Member | Description
----|----
DmfModule | An open DMF_ActivitySensor Module handle.
ActivitySensorState | Updated ActivitySensor state information.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_ActivitySensor_CurrentStateGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ActivitySensor_CurrentStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ ACTIVITY_SENSOR_STATE* CurrentState
    );
````

Get the current Activity state from sensor.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ActivitySensor Module handle.
CurrentState | Current Activity state.

##### DMF_ActivitySensor_Start

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ActivitySensor_Start(
    _In_ DMFMODULE DmfModule
    );
````

Allows Client to start monitoring ActivitySensor sensor.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ActivitySensor Module handle.

##### Remarks

* ActivitySensor sensor is started by default.
* Only use this Method, if `DMF_ActivitySensor_Stop` is used.

##### DMF_ActivitySensor_Stop

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ActivitySensor_Stop(
    _In_ DMFMODULE DmfModule
    )
````

Allows Client to stop monitoring ActivitySensor sensor.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ActivitySensor Module handle.

##### Remarks

* ActivitySensor sensor is stopped automatically when Module closes.
* Client calls this Method to manually stop the sensor.
* After calling this Method, Client may call `DMF_ActivitySensor_Start`.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module uses C++/WinRT, so it needs RS5+ support. Module specific code will not be compiled in RS4 and below.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Sensor

-----------------------------------------------------------------------------------------------------------------------------------

