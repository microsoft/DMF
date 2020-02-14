#pragma once
/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SimpleOrientation.h

Abstract:

    Companion file to Dmf_SimpleOrientation.cpp.

Environment:

    User-mode Driver Framework

--*/

// This Module uses C++/WinRT so it needs RS5+ support. 
// This code will not be compiled in RS4 and below.
//
#if defined(DMF_USER_MODE) && IS_WIN10_RS5_OR_LATER && defined(__cplusplus)

#include "winrt/Windows.Devices.Sensors.h"

typedef struct _SIMPLE_ORIENTATION_SENSOR_STATE
{
    BOOLEAN IsSensorValid;
    winrt::Windows::Devices::Sensors::SimpleOrientation CurrentSimpleOrientation;
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
    winrt::hstring DeviceId;
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

#endif // defined(DMF_USER_MODE) && defined(IS_WIN10_RS5_OR_LATER) && defined(__cplusplus)

// eof: Dmf_SimpleOrientation.h
//
