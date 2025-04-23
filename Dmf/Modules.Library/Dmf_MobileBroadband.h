#pragma once
/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_MobileBroadband.h

Abstract:

    Companion file to Dmf_Mobilebroadband.cpp.

Environment:

    User-mode Driver Framework

--*/

typedef struct _MOBILEBROADBAND_WIRELESS_STATE
{
    // Indicates whether modem is valid or not.
    //
    BOOLEAN IsModemValid;
    // Indicates whether device is connected to a MobileBroadband network.
    //
    BOOLEAN IsNetworkConnected;
    // Indicates whether device is transmitting data or not.
    //
    BOOLEAN IsTransmitting;
} MobileBroadband_WIRELESS_STATE;

typedef
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_MobileBroadband_WirelessStateChangeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ MobileBroadband_WIRELESS_STATE* WirelessState
    );

typedef
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_MobileBroadband_SimReadyChangeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN SimReady
    );

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Callback called when a TransmissionStateChanged event is received.
    //
    EVT_DMF_MobileBroadband_WirelessStateChangeCallback* EvtMobileBroadbandWirelessStateChangeCallback;
    // Callback called when a SimReadyChange event is received.
    //
    EVT_DMF_MobileBroadband_SimReadyChangeCallback* EvtMobileBroadbandSimReadyChangeCallback;
} DMF_CONFIG_MobileBroadband;

// This macro declares the following functions:
// DMF_MobileBroadband_ATTRIBUTES_INIT()
// DMF_CONFIG_MobileBroadband_AND_ATTRIBUTES_INIT()
// DMF_MobileBroadband_Create()
//
DECLARE_DMF_MODULE(MobileBroadband)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_AntennaBackOffTableIndexGet(
    _In_ DMFMODULE DmfModule,
    _In_ INT32 AntennaIndex,
    _Out_ INT32* AntennaBackOffTableIndex
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_AntennaBackOffTableIndexSet(
    _In_ DMFMODULE DmfModule,
    _In_reads_(AntennaCount) INT32* AntennaIndex,
    _In_reads_(AntennaCount) INT32* AntennaBackOffTableIndex,
    _In_ INT32 AntennaCount,
    _In_ BOOLEAN AbsoluteAntennaIndexMode
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_MccMncGet(
    _In_ DMFMODULE DmfModule,
    _Out_ DWORD* MobileCountryCode,
    _Out_ DWORD* MobileNetworkCode
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_SarBackOffDisable(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_SarBackOffEnable(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_WirelessStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ MobileBroadband_WIRELESS_STATE* MobileBroadbandWirelessState
    );

// eof: Dmf_MobileBroadband.h
//
