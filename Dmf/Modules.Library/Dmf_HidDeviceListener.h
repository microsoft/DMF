/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_HidDeviceListener.h

Abstract:

    Companion file to Dmf_HidDeviceListener.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// The maximum number of devices of supported device product Ids that are
// searched in the Module Config.
//
#define DMF_HidDeviceListener_Maximum_Pid_Count 8

// Client callback for matching HID device arrival.
//
typedef
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
EVT_DMF_HidDeviceListener_DeviceArrivalCallback(_In_ DMFMODULE DmfModule,
                                                _In_ PUNICODE_STRING SymbolicLinkName,
                                                _In_ WDFIOTARGET IoTarget,
                                                _In_ PHIDP_PREPARSED_DATA PreparsedHidData,
                                                _In_ HID_COLLECTION_INFORMATION* HidCollectionInformation);

// Client callback for matching HID device removal.
//
typedef
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
EVT_DMF_HidDeviceListener_DeviceRemovalCallback(_In_ DMFMODULE DmfModule,
                                                _In_ PUNICODE_STRING SymbolicLinkName);

typedef struct
{
    // The Vendor Id of the HID device(s).
    //
    UINT16 VendorId;
    // List of HID Product Ids.
    //
    UINT16 ProductIds[DMF_HidDeviceListener_Maximum_Pid_Count];
    // Number of entries in the above array.
    //
    ULONG ProductIdsCount;
    // Information needed to idenitify the right HID device(s).
    //
    UINT16 Usage;
    UINT16 UsagePage;
    // Client callback for matching HID device arrival.
    //
    EVT_DMF_HidDeviceListener_DeviceArrivalCallback* EvtHidTargetDeviceArrivalCallback;

    // Client callback for matching HID device removal.
    //
    EVT_DMF_HidDeviceListener_DeviceRemovalCallback* EvtHidTargetDeviceRemovalCallback;
} DMF_CONFIG_HidDeviceListener;

// This macro declares the following functions:
// DMF_HidDeviceListener_ATTRIBUTES_INIT()
// DMF_CONFIG_HidDeviceListener_AND_ATTRIBUTES_INIT()
// DMF_HidDeviceListener_Create()
//
DECLARE_DMF_MODULE(HidDeviceListener)

// eof: Dmf_HidDeviceListener.h
//
