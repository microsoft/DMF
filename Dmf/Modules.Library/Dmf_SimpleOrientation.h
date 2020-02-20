#pragma once
/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SimpleOrientation.h

Abstract:

    Companion file to Dmf_SimpleOrientation.cpp.

Environment:

    User-mode Driver Framework

--*/

// Only support 19H1 and above because of library size limitations on RS5.
//
#if defined(DMF_USER_MODE) && IS_WIN10_19H1_OR_LATER && defined(__cplusplus)

// Enum copy from WinRT SimpleOrientation class enum.
//
typedef enum class _SIMPLE_ORIENTATION_STATE
{
    NotRotated = 0,
    Rotated90DegreesCounterclockwise = 1,
    Rotated180DegreesCounterclockwise = 2,
    Rotated270DegreesCounterclockwise = 3,
    Faceup = 4,
    Facedown = 5,
} SimpleOrientation_State;

typedef struct _SIMPLE_ORIENTATION_SENSOR_STATE
{
    BOOLEAN IsSensorValid;
    SimpleOrientation_State CurrentSimpleOrientation;
} SIMPLE_ORIENTATION_SENSOR_STATE;

// DMF Module event callback.
//
typedef
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_SimpleOrientation_SimpleOrientationSensorReadingChangeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ SIMPLE_ORIENTATION_SENSOR_STATE* SimpleOrientationSensorState
    );

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Specific simple orientation device Id to open. This is optional.
    //
    WCHAR* DeviceId;
    // Callback to inform Parent Module that simple orientation has new changed reading.
    //
    EVT_DMF_SimpleOrientation_SimpleOrientationSensorReadingChangeCallback* EvtSimpleOrientationReadingChangeCallback;
} DMF_CONFIG_SimpleOrientation;

// This macro declares the following functions:
// DMF_SimpleOrientation_ATTRIBUTES_INIT()
// DMF_CONFIG_SimpleOrientation_AND_ATTRIBUTES_INIT()
// DMF_SimpleOrientation_Create()
//
DECLARE_DMF_MODULE(SimpleOrientation)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SimpleOrientation_CurrentStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ SIMPLE_ORIENTATION_SENSOR_STATE* CurrentState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SimpleOrientation_Start(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SimpleOrientation_Stop(
    _In_ DMFMODULE DmfModule
    );

#endif // IS_WIN10_19H1_OR_LATER

// eof: Dmf_SimpleOrientation.h
//
