/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:

    Dmf_CrashDump_Public.h

Abstract:

    This Module contains the common declarations shared by driver and user applications.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// {F4A58486-FD91-4BF9-96BC-DDA5CF571EDF}
//
DEFINE_GUID(GUID_DEVINTERFACE_CrashDump,
            0xf4a58486, 0xfd91, 0x4bf9, 0x96, 0xbc, 0xdd, 0xa5, 0xcf, 0x57, 0x1e, 0xdf);

//-[Crash Dump Data Source]-----------------------------------------------------------------
//

#define IOCTL_DATA_SOURCE_CREATE    CTL_CODE(FILE_DEVICE_UNKNOWN, 2048, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_DATA_SOURCE_DESTROY   CTL_CODE(FILE_DEVICE_UNKNOWN, 2049, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_DATA_SOURCE_WRITE     CTL_CODE(FILE_DEVICE_UNKNOWN, 2050, METHOD_BUFFERED, FILE_WRITE_ACCESS)
// NOTE: It is only available in DEBUG build.
//
#define IOCTL_CRASH_DRIVER          CTL_CODE(FILE_DEVICE_UNKNOWN, 2051, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_DATA_SOURCE_READ      CTL_CODE(FILE_DEVICE_UNKNOWN, 2052, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DATA_SOURCE_OPEN      CTL_CODE(FILE_DEVICE_UNKNOWN, 2053, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DATA_SOURCE_CAPTURE   CTL_CODE(FILE_DEVICE_UNKNOWN, 2054, METHOD_BUFFERED, FILE_READ_ACCESS)

// IOCTL_DATA_SOURCE_CREATE Parameters.
//
#pragma pack(push, 1)
typedef struct
{
    // Number of entries in RingBuffer.
    //
    ULONG EntriesCount;
    // Size of each entry in RingBuffer.
    //
    ULONG EntrySize;
    // GUID to display in crash dump data.
    //
    GUID Guid;
} DATA_SOURCE_CREATE;
#pragma pack(pop)

// IOCTL_DATA_SOURCE_OPEN Parameters.
//
#pragma pack(push, 1)
typedef struct
{
    // Number of entries in RingBuffer. This is populated by the driver.
    //
    ULONG EntriesCount;
    // Size of each entry in RingBuffer. This is populated by the driver.
    //
    ULONG EntrySize;
    // GUID to display in crash dump data.
    //
    GUID Guid;
} DATA_SOURCE_OPEN;
#pragma pack(pop)

// IOCTL_DATA_SOURCE_OPEN ReadOrWrite values.
// DATA_SOURCE_EITHER is used for Destroy functions. Not for open.
//
typedef enum
{
    DataSourceModeInvalid = 0,
    DataSourceModeWrite = 1,
    DataSourceModeRead = 2,
    DataSourceModeMaximum
} DataSourceModeType;

//------------------------------------------------------------------------------------------
//

// eof: Dmf_CrashDump_Public.h
//
