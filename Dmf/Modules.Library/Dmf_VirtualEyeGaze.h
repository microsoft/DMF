/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_VirtualEyeGaze.h

Abstract:

    Companion file to Dmf_VirtualEyeGaze.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Vendor Id of the eye gaze device.
    //
    USHORT VendorId;
    // Product Id of the eye gaze device.
    //
    USHORT ProductId;
    // Version number of the eye gaze device.
    //
    USHORT VersionNumber;
} DMF_CONFIG_VirtualEyeGaze;

// This macro declares the following functions:
// DMF_VirtualEyeGaze_ATTRIBUTES_INIT()
// DMF_CONFIG_VirtualEyeGaze_AND_ATTRIBUTES_INIT()
// DMF_VirtualEyeGaze_Create()
//
DECLARE_DMF_MODULE(VirtualEyeGaze)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualEyeGaze_ConfigurationDataSet(
    _In_ DMFMODULE DmfModule,
    _In_ CONFIGURATION_DATA* ConfigurationData
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualEyeGaze_CapabilitiesDataSet(
    _In_ DMFMODULE DmfModule,
    _In_ CAPABILITIES_DATA* CapabilitiesData
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualEyeGaze_GazeReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ GAZE_DATA* GazeData
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualEyeGaze_TrackerControlModeGet(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR* Mode
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualEyeGaze_TrackerStatusReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR TrackerStatus
    );

// eof: Dmf_VirtualEyeGaze.h
//
