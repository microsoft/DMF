/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:

    Dmf_EyeGazeIoctl_Public.h

Abstract:

    This module contains the common declarations shared by driver and user applications.

Environment:

    User and Kernel Modes

--*/

#pragma once

#pragma pack(push, 1)

typedef struct _POINT2D
{
    LONG X;
    LONG Y;
} POINT2D, *PPOINT2D;

typedef struct _POINT3D
{
    LONG X;
    LONG Y;
    LONG Z;
} POINT3D, *PPOINT3D;

typedef struct _GAZE_DATA
{
    UCHAR Reserved[3];
    ULONGLONG TimeStamp;
    POINT2D GazePoint;
    POINT3D LeftEyePosition;
    POINT3D RightEyePosition;
} GAZE_DATA, *PGAZE_DATA;

typedef struct _MONITOR_RESOLUTION
{
    ULONG Height;
    ULONG Width;
} MONITOR_RESOLUTION, *PMONITOR_RESOLUTION;

#pragma pack(pop)

// {2E8BE292-682B-493E-B7D8-73033613E530}
DEFINE_GUID(VirtualEyeGaze_GUID, 
0x2e8be292, 0x682b, 0x493e, 0xb7, 0xd8, 0x73, 0x3, 0x36, 0x13, 0xe5, 0x30);

#define IOCTL_EYEGAZE_GAZE_DATA             CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                     0x100, \
                                                     METHOD_BUFFERED, \
                                                     FILE_WRITE_ACCESS)

#define IOCTL_EYEGAZE_MONITOR_RESOLUTION    CTL_CODE(FILE_DEVICE_UNKNOWN,\
                                                     0x101, \
                                                     METHOD_BUFFERED, \
                                                     FILE_WRITE_ACCESS)

// eof: Dmf_EyeGazeIoctl_Public.h
//
