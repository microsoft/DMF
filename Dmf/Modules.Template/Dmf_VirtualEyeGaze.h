/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualEyeGaze.h

Abstract:

    Companion file to Dmf_VirtualEyeGaze.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#include <pshpack1.h>

#define HID_USAGE_TRACKING_DATA                     (0x10)        // CP

typedef struct _POINT2D
{
    LONG    X;
    LONG    Y;
} POINT2D, *PPOINT2D;

typedef struct _POINT3D
{
    LONG    X;
    LONG    Y;
    LONG    Z;
} POINT3D, *PPOINT3D;

typedef struct _GAZE_REPORT
{
    UCHAR     ReportId;
    UCHAR     Reserved[3];
    ULONGLONG    TimeStamp;
    POINT2D     GazePoint;
    POINT3D     LeftEyePosition;
    POINT3D     RightEyePosition;
} GAZE_REPORT, *PGAZE_REPORT;

#include <poppack.h>

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    ULONG ReadFromRegistry;
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
DMF_VirtualEyeGaze_GazeReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ GAZE_REPORT* GazeReport
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualEyeGaze_TrackerStatusReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR TrackerStatus
    );

// eof: Dmf_VirtualEyeGaze.h
//
