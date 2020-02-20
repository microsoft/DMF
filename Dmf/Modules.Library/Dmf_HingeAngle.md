## DMF_HingeAngle

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This module allows the Client to monitoring Hinge Angle interface and get current device Hinge Angle state information.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_HingeAngle
````
// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Specific hinge angle device Id to open. This is optional.
    //
    WCHAR* DeviceId;
    // Report threshold in degrees.
    //
    double ReportThresholdInDegrees;
    // Callback to inform Parent Module that hinge angle has new changed reading.
    //
    EVT_DMF_HingeAngle_HingeAngleSensorReadingChangeCallback* EvtHingeAngleReadingChangeCallback;
} DMF_CONFIG_HingeAngle;
````
Member | Description
----|----
DeviceId | Specific Hinge Angle device Id to open. If client does not set device Id config, it opens default Hinge Angle sensor if exist.
ReportThresholdInDegrees | Report threshold in degrees that client needs.
EvtHingeAngleReadingChangeCallback | Allows the client to get new status of Hinge Angle state every time it changes.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

##### HINGE_ANGLE_SENSOR_STATE
````
typedef struct _HINGE_ANGLE_SENSOR_STATE
{
    BOOLEAN IsSensorValid;
    double AngleInDegrees;
} HINGE_ANGLE_SENSOR_STATE;
````
Member | Description
----|----
IsSensorValid | Indicate whether Hinge Angle interface is valid or not.
AngleInDegrees | Current Hinge Angle degrees.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_HingeAngle_HingeAngleSensorReadingChangeCallback
````
typedef
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_HingeAngle_HingeAngleSensorReadingChangeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ HINGE_ANGLE_SENSOR_STATE* HingeAngleSensorState
    );
````

Client specific callback that allows the client to get new status of Hinge Angle every time it has state changes.

##### Returns

None

##### Parameters
Member | Description
----|----
DmfModule | An open DMF_HingeAngle Module handle.
HingeAngleState | Structure of HingeAngle state.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HingeAngle_CurrentStateGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HingeAngle_CurrentStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ HINGE_ANGLE_SENSOR_STATE* CurrentState
    );
````

Get the current Hinge Angle state from sensor.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HingeAngle Module handle.
CurrentState | Current Hinge Angle state to get.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HingeAngle_Start

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HingeAngle_Start(
    _In_ DMFMODULE DmfModule
    );
````

Start the Hinge Angle monitor and events. Module start when open by default, client can also use this function to manually start.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HingeAngle Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HingeAngle_Stop

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_HingeAngle_Stop(
    _In_ DMFMODULE DmfModule
    )
````

Stop the Hinge Angle monitor and events. Module stop when close by default, client can also use this function to manually stop.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HingeAngle Module handle.

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

