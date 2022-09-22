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
#define IOCTL_Tests_IoctlHandler_ZEROSIZE       CTL_CODE(FILE_DEVICE_UNKNOWN, 4002, METHOD_BUFFERED, FILE_WRITE_ACCESS)

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

#if defined(DMF_KERNEL_MODE)

// {6775E8C4-78EE-4269-8FF9-19DC127772F0}
//
DEFINE_GUID(GUID_Tests_IoctlHandler_INTERFACE_STANDARD, 
    0x6775e8c4, 0x78ee, 0x4269, 0x8f, 0xf9, 0x19, 0xdc, 0x12, 0x77, 0x72, 0xf0);

typedef
BOOLEAN
(*TestsIoctlHandler_ValueGet)(_In_ VOID* DmfModuleVoid,
                              _Out_ UCHAR* Value);

typedef
BOOLEAN
(*TestsIoctlHandler_ValueSet)(_In_ VOID* DmfModuleVoid,
                              _In_ UCHAR Value);

typedef struct _Tests_IoctlHandler_INTERFACE_STANDARD
{
    INTERFACE InterfaceHeader;
    TestsIoctlHandler_ValueGet InterfaceValueGet;
    TestsIoctlHandler_ValueSet InterfaceValueSet;
} Tests_IoctlHandler_INTERFACE_STANDARD;

#endif

// eof: Dmf_Tests_IoctlHandler_Public.h
//
