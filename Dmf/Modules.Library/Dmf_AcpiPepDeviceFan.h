/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AcpiPepDeviceFan.h

Abstract:

    Companion file to Dmf_AcpiPepDeviceFan.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

#if !defined(DMF_USER_MODE)

// pepfx.h is not compatible with pep_x.h. Clients that use pep_x.h must:
// 
// #define DMF_DONT_INCLUDE_PEPFX
// #include <DmfModules.Library.h>
//
#if !defined(DMF_DONT_INCLUDE_PEPFX)

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Enumerator specifying fan range indexes.
//
typedef enum _AcpiPepDeviceFan_FAN_RANGE_INDEX
{
    AcpiPepDeviceFan_FanRangeIndex0 = 0,
    AcpiPepDeviceFan_FanRangeIndex1,
    AcpiPepDeviceFan_FanRangeIndex2,
    AcpiPepDeviceFan_FanRangeIndex3,
    AcpiPepDeviceFan_NumberOfFanRanges,
} AcpiPepDeviceFan_FAN_RANGE_INDEX;

// Structure to specify trip points for fan operation.
//
#pragma pack(push, 1)
typedef struct
{
    UINT16 Low;
    UINT16 High;
} AcpiPepDeviceFan_TRIP_POINT;
#pragma pack(pop)

// Fan calls into Client supplied function that fetches fan information from hardware.
//
typedef
_Function_class_(EVT_DMF_AcpiPepDeviceFan_FanSpeedGet)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_AcpiPepDeviceFan_FanSpeedGet(_In_ DMFMODULE DmfModule,
                                     _In_ ULONG FanInstanceIndex,
                                     _Out_ UINT16* Data,
                                     _In_ size_t DataSize);

// Fan calls into Client supplied function that supplies trip point to hardware.
//
typedef
_Function_class_(EVT_DMF_AcpiPepDeviceFan_FanTripPointsSet)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_AcpiPepDeviceFan_FanTripPointsSet(_In_ DMFMODULE DmfModule,
                                          _In_ ULONG FanInstanceIndex,
                                          _In_ AcpiPepDeviceFan_TRIP_POINT TripPoint);


// Fan calls into Client supplied function that supplies fan range information from hardware.
//
typedef
_Function_class_(EVT_DMF_AcpiPepDeviceFan_DsmFanRangeGet)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_AcpiPepDeviceFan_DsmFanRangeGet(_In_ DMFMODULE DmfModule,
                                        _Out_ ULONG DsmFanRange[AcpiPepDeviceFan_NumberOfFanRanges]);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Instance index used by hardware to identify this fan.
    //
    ULONG FanInstanceIndex;
    // GUID defined for FAN DSMs.
    //
    GUID FanDsmGuid;
    // Unique string assigned to this fan. Needs to be global.
    //
    CHAR* FanInstanceName;
    // Fan's name as WCHAR string as specified in ACPI. Needs to be global.
    //
    WCHAR* FanInstanceNameWchar;
    // ULONG containing Fan name as specified in ACPI.
    //
    ULONG FanInstanceRealName;
    // HWID of the fan corresponding to the one in ACPI. Needs to be global.
    //
    CHAR* FanInstanceHardwareId;
    // Callback invoked upon _FST function call.
    //
    EVT_DMF_AcpiPepDeviceFan_FanSpeedGet* FanSpeedGet;
    // Callback invoked upon _DSM function call with
    // function index FAN_DSM_TRIPPOINT_FUNCTION_INDEX.
    //
    EVT_DMF_AcpiPepDeviceFan_FanTripPointsSet* FanTripPointsSet;
    // Callback invoked upon _DSM function call with
    // function index FAN_DSM_RANGE_FUNCTION_INDEX.
    //
    EVT_DMF_AcpiPepDeviceFan_DsmFanRangeGet* DsmFanRangeGet;
    // Client can specify Fan resolution to be returned as a result of _DSM call.
    // This allows the OS to know the resolution of the trip points it provides.
    //
    ULONG DsmFanCapabilityResolution;
    // Client can specify Fan support index to be returned as a result of _DSM call.
    // This allows the OS to know all the DSMs supported by this fan.
    //
    UCHAR DsmFunctionSupportIndex;
    // AcpiPepDevice can register an arrival callback invoked in Module Open.
    //
    EVT_DMF_MODULE_OnDeviceNotificationPostOpen* ArrivalCallback;
} DMF_CONFIG_AcpiPepDeviceFan;

// This macro declares the following functions:
// DMF_AcpiPepDeviceFan_ATTRIBUTES_INIT()
// DMF_CONFIG_AcpiPepDeviceFan_AND_ATTRIBUTES_INIT()
// DMF_AcpiPepDeviceFan_Create()
//
DECLARE_DMF_MODULE(AcpiPepDeviceFan)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_AcpiPepDeviceFan_AcpiDeviceTableGet(
    _In_ DMFMODULE DmfModule,
    _Out_ PEP_ACPI_REGISTRATION_TABLES* PepAcpiRegistrationTables
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_AcpiPepDeviceFan_FanInitializedFlagGet(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AcpiPepDeviceFan_NotifyRequestSchedule(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG NotifyCode
    );

#endif // !defined(DMF_DONT_INCLUDE_PEPFX)

#endif // defined(DMF_USER_MODE)

// eof: Dmf_AcpiPepDeviceFan.h
//
