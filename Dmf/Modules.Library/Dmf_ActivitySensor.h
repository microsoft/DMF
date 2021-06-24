#pragma once
/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ActivitySensor.h

Abstract:

    Companion file to Dmf_ActivitySensor.cpp.

Environment:

    User-mode Driver Framework

--*/

// Only support 19H1 and above because of library size limitations on RS5.
//
#if defined(DMF_USER_MODE) && IS_WIN10_19H1_OR_LATER && defined(__cplusplus)

// Enum copy from WinRT ActivitySensor class enum. 
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.sensors.activitytype?view=winrt-19041.
//
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

typedef struct _ACTIVITY_SENSOR_STATE
{
    BOOLEAN IsSensorValid;
    ActivitySensor_Reading CurrentActivitySensorState;
} ACTIVITY_SENSOR_STATE;

// DMF Module event callback.
//
typedef
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_ActivitySensor_EvtActivitySensorReadingChangedCallback(
    _In_ DMFMODULE DmfModule,
    _In_ ACTIVITY_SENSOR_STATE* ActivitySensorState
    );

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Specific Motion Activity device Id to open. This is optional.
    //
    WCHAR* DeviceId;
    // Callback to inform Parent Module that Motion Activity has new changed reading.
    //
    EVT_DMF_ActivitySensor_EvtActivitySensorReadingChangedCallback* EvtActivitySensorReadingChangeCallback;
} DMF_CONFIG_ActivitySensor;

// This macro declares the following functions:
// DMF_ActivitySensor_ATTRIBUTES_INIT()
// DMF_CONFIG_ActivitySensor_AND_ATTRIBUTES_INIT()
// DMF_ActivitySensor_Create()
//
DECLARE_DMF_MODULE(ActivitySensor)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ActivitySensor_CurrentStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ ACTIVITY_SENSOR_STATE* CurrentState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ActivitySensor_Start(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ActivitySensor_Stop(
    _In_ DMFMODULE DmfModule
    );

#endif // IS_WIN10_19H1_OR_LATER

// eof: Dmf_ActivitySensor.h
//
