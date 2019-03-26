/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:

    Dmf_NonPnp_Public.h

Abstract:

    This Module contains the common declarations shared by driver and user applications.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework
    User-mode Application

--*/

#pragma once

#define NonPnp_SymbolicLinkName     L"\\DosDevices\\NonPnp"

#define IOCTL_INDEX             0x800
#define FILE_DEVICE_NonPnp      65501U

#define IOCTL_NonPnp_MESSAGE_TRANSFER CTL_CODE(FILE_DEVICE_NonPnp,     \
                                               IOCTL_INDEX,            \
                                               METHOD_BUFFERED,        \
                                               FILE_READ_ACCESS)

#define NonPnp_BufferSize       sizeof(CHAR) * 100

// eof: Dmf_NonPnp_Public.h
//
