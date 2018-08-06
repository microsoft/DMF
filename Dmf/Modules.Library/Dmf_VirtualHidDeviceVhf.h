/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidDeviceVhf.h

Abstract:

    Companion file to Dmf_VirtualHidDeviceVhf.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

// This code is only supported in Kernel-mode.
//
#if !defined(DMF_USER_MODE) && defined(NTDDI_WINTHRESHOLD) && (NTDDI_VERSION >= NTDDI_WINTHRESHOLD)

#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <vhf.h>

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Describe HID Device.
    // NOTE: In most cases this data is static memory so a pointer to that data
    //       is maintained. This prevents us from creating arbitrary size buffers.
    //
    USHORT VendorId;
    USHORT ProductId;
    USHORT VersionNumber;
    const HID_DESCRIPTOR* HidDescriptor;
    ULONG HidDescriptorLength;
    const UCHAR* HidReportDescriptor;
    ULONG HidReportDescriptorLength;
    HID_DEVICE_ATTRIBUTES HidDeviceAttributes;
    EVT_VHF_ASYNC_OPERATION* IoctlCallback_IOCTL_HID_SET_FEATURE;
    EVT_VHF_ASYNC_OPERATION* IoctlCallback_IOCTL_HID_GET_FEATURE;
    EVT_VHF_ASYNC_OPERATION* IoctlCallback_IOCTL_HID_GET_INPUT_REPORT;
    EVT_VHF_ASYNC_OPERATION* IoctlCallback_IOCTL_HID_WRITE_REPORT;
    EVT_VHF_READY_FOR_NEXT_READ_REPORT* IoctlCallback_IOCTL_HID_READ_REPORT;
    // This context will be passed by Vhf directly (from Vhf).
    //
    VOID* VhfClientContext;
    // Indicates that Vhf device should start when Module opens.
    //
    BOOLEAN StartOnOpen;
} DMF_CONFIG_VirtualHidDeviceVhf;

// This macro declares the following functions:
// DMF_VirtualHidDeviceVhf_ATTRIBUTES_INIT()
// DMF_CONFIG_VirtualHidDeviceVhf_AND_ATTRIBUTES_INIT()
// DMF_VirtualHidDeviceVhf_Create()
//
DECLARE_DMF_MODULE(VirtualHidDeviceVhf)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(
    _In_ DMFMODULE DmfModule,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ NTSTATUS NtStatus
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_VirtualHidDeviceVhf_ReadReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ HID_XFER_PACKET* HidTransferPacket
    );

#endif // !defined(DMF_USER_MODE) && defined(NTDDI_WINTHRESHOLD) && (NTDDI_VERSION >= NTDDI_WINTHRESHOLD)

// eof: Dmf_VirtualHidDeviceVhf.h
//
