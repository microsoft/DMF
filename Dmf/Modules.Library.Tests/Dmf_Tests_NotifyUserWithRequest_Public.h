/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:

    Dmf_Tests_NotifyUserWithRequest_Public.h

Abstract:

    This Module contains the common declarations shared by driver and user applications.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// {8BF0F3E1-6F78-4A5D-B9C0-3E4A2F1D8C7E}
//
DEFINE_GUID(GUID_DEVINTERFACE_Tests_NotifyUserWithRequest,
    0x8bf0f3e1, 0x6f78, 0x4a5d, 0xb9, 0xc0, 0x3e, 0x4a, 0x2f, 0x1d, 0x8c, 0x7e);

#define IOCTL_Tests_NotifyUserWithRequest_GET_EVENT     CTL_CODE(FILE_DEVICE_UNKNOWN, 4100, METHOD_BUFFERED, FILE_READ_ACCESS)

// IOCTL_Tests_NotifyUserWithRequest_GET_EVENT Output.
//
#pragma pack(push, 1)
typedef struct
{
    // Incrementing counter data returned from the driver.
    //
    LONG DataValue;
} Tests_NotifyUserWithRequest_EventData;
#pragma pack(pop)

// eof: Dmf_Tests_NotifyUserWithRequest_Public.h
//
