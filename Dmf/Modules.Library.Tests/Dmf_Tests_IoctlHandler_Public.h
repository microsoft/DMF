/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:

    Dmf_Tests_IoctlHandler_Public.h

Abstract:

    This Module contains the common declarations shared by driver and user applications.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// {FD9FF0B7-029F-4D1E-94DA-8D8CC2BD40CF}
//
DEFINE_GUID(GUID_DEVINTERFACE_Tests_IoctlHandler,
    0xfd9ff0b7, 0x29f, 0x4d1e, 0x94, 0xda, 0x8d, 0x8c, 0xc2, 0xbd, 0x40, 0xcf);

#define IOCTL_Tests_IoctlHandler_SLEEP          CTL_CODE(FILE_DEVICE_UNKNOWN, 4000, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_Tests_IoctlHandler_ZEROBUFFER     CTL_CODE(FILE_DEVICE_UNKNOWN, 4001, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// IOCTL_DATA_SOURCE_CREATE Parameters.
//
#pragma pack(push, 1)
typedef struct
{
    // Wait this long and then complete the request.
    //
    LONG TimeToSleepMilliseconds;
} Tests_IoctlHandler_Sleep;
#pragma pack(pop)

// eof: Dmf_Tests_IoctlHandler_Public.h
//
