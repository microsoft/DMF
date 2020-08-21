/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_EyeGazeGhost.h

Abstract:

    Companion file to Dmf_EyeGazeGhost.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    ULONG ReadFromRegistry;
} DMF_CONFIG_EyeGazeGhost;

// This macro declares the following functions:
// DMF_EyeGazeGhost_ATTRIBUTES_INIT()
// DMF_CONFIG_EyeGazeGhost_AND_ATTRIBUTES_INIT()
// DMF_EyeGazeGhost_Create()
//
DECLARE_DMF_MODULE(EyeGazeGhost)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_EyeGazeGhost_GazeReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ GAZE_REPORT* GazeReport
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_EyeGazeGhost_TrackerStatusReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR TrackerStatus
    );

// eof: Dmf_EyeGazeGhost.h
//
