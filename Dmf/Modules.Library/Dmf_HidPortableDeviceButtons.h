/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_HidPortableDeviceButtons.h

Abstract:

    Companion file to Dmf_HidPortableDeviceButtons.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef enum
{
    HidPortableDeviceButtons_ButtonId_Invalid,
    HidPortableDeviceButtons_ButtonId_Power,
    HidPortableDeviceButtons_ButtonId_VolumePlus,
    HidPortableDeviceButtons_ButtonId_VolumeMinus,
    // These are both not supported.
    //
    HidPortableDeviceButtons_ButtonId_Windows,
    HidPortableDeviceButtons_ButtonId_RotationLock,
    HidPortableDeviceButtons_ButtonId_Maximum
} HidPortableDeviceButtons_ButtonIdType;

typedef enum
{
    HidPortableDeviceButtons_Hotkey_BrightnessUp,
    HidPortableDeviceButtons_Hotkey_BrightnessDown,
} HidPortableDeviceButtons_HotkeyType;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Vendor Id of the virtual keyboard.
    //
    USHORT VendorId;
    // Product Id of the virtual keyboard.
    //
    USHORT ProductId;
    // Version number of the virtual keyboard.
    //
    USHORT VersionNumber;
} DMF_CONFIG_HidPortableDeviceButtons;

// This macro declares the following functions:
// DMF_HidPortableDeviceButtons_ATTRIBUTES_INIT()
// DMF_CONFIG_HidPortableDeviceButtons_AND_ATTRIBUTES_INIT()
// DMF_HidPortableDeviceButtons_Create()
//
DECLARE_DMF_MODULE(HidPortableDeviceButtons)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_HidPortableDeviceButtons_ButtonIsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ HidPortableDeviceButtons_ButtonIdType ButtonId
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_HidPortableDeviceButtons_ButtonStateChange(
    _In_ DMFMODULE DmfModule,
    _In_ HidPortableDeviceButtons_ButtonIdType ButtonId,
    _In_ ULONG ButtonStateDown
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_HidPortableDeviceButtons_HotkeyStateChange(
    _In_ DMFMODULE DmfModule,
    _In_ HidPortableDeviceButtons_ButtonIdType Hotkey,
    _In_ ULONG HotkeyStateDown
);

// eof: Dmf_HidPortableDeviceButtons.h
//
