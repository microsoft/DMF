## DMF_SimpleOrientation

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module allows the Client to monitor the Simple Orientation interface and get current device Simple Orientation state information.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_SimpleOrientation
````
// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Specific Simple Orientation device Id to open. This is optional.
    //
    winrt::hstring DeviceId;
    // Callback to inform Parent Module that Simple Orientation has new changed reading.
    //
    EVT_DMF_SimpleOrientation_SimpleOrientationSensorReadingChangeCallback* EvtSimpleOrientationReadingChangeCallback;
} DMF_CONFIG_SimpleOrientation;
````
Member | Description
----|----
DeviceId | Specific Simple Orientation device Id to open. If Client does not set DeviceId, it opens default Simple Orientation sensor if it exists.
EvtSimpleOrientationReadingChangeCallback | Allows the Client to get the status of Simple Orientation state when it changes.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

##### SIMPLE_ORIENTATION_SENSOR_STATE
````
typedef struct _SIMPLE_ORIENTATION_SENSOR_STATE
{
    BOOLEAN IsSensorValid;
    winrt::Windows::Devices::Sensors::SimpleOrientation CurrentSimpleOrientation;
} SIMPLE_ORIENTATION_SENSOR_STATE;
````
Member | Description
----|----
IsSensorValid | Indicates whether Simple Orientation interface is valid or not.
CurrentSimpleOrientation | Current Simple Orientation status.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_SimpleOrientation_SimpleOrientationSensorReadingChangeCallback
````
typedef
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_SimpleOrientation_SimpleOrientationSensorReadingChangeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ SIMPLE_ORIENTATION_SENSOR_STATE* SimpleOrientationSensorState
    );
````

Client specific callback that allows the Client to get status of Simple Orientation when it changes.

##### Returns

None

##### Parameters
Member | Description
----|----
DmfModule | An open DMF_SimpleOrientation Module handle.
SimpleOrientationState | Updated SimpleOrientation state information.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SimpleOrientation_CurrentStateGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SimpleOrientation_CurrentStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ SIMPLE_ORIENTATION_SENSOR_STATE* CurrentState
    );
````

Get the current Simple Orientation state from sensor.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SimpleOrientation Module handle.
CurrentState | Current Simple Orientation state.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SimpleOrientation_Start

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SimpleOrientation_Start(
    _In_ DMFMODULE DmfModule
    );
````

Allows Client to start monitoring SimpleOrientation sensor.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SimpleOrientation Module handle.

##### Remarks

* SimpleOrientation sensor is started by default.
* Only use this Method, if `DMF_SimpleOrientation_Stop` is used.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SimpleOrientation_Stop

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SimpleOrientation_Stop(
    _In_ DMFMODULE DmfModule
    )
````

Allows Client to stop monitoring SimpleOrientation sensor.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SimpleOrientation Module handle.

##### Remarks

* SimpleOrientation sensor is stopped by default.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module uses C++/WinRT, so it needs RS5+ support.
  Module specific code will not be compiled in RS4 and below.

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

Sensor

-----------------------------------------------------------------------------------------------------------------------------------

