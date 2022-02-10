/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_VirtualHidKeyboard.h

Abstract:

    Companion file to Dmf_VirtualHidKeyboard.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// See hidusage.h for actual key values.
//

// Indicates how this driver types keystrokes.
//
typedef enum
{
    VirtualHidKeyboardMode_Invalid,
    // This driver types the keystrokes and does not expose this function to other drivers.
    //
    VirtualHidKeyboardMode_Standalone,
    // This driver types the keystrokes and exposes this function to other drivers.
    //
    VirtualHidKeyboardMode_Server,
    // This driver does not type keystrokes directly. It calls another driver to do that.
    //
    VirtualHidKeyboardMode_Client,
} VirtualHidKeyboardModeType;

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
    // Determines how the keyboard is instantiated.
    //
    VirtualHidKeyboardModeType VirtualHidKeyboardMode;
    // Name of callback that exposes Client/Server keyboard mode.
    // Name should have this format: L"\\Callback\\[Name specified by Client of Module]"
    //
    WCHAR* ClientServerCallbackName;
} DMF_CONFIG_VirtualHidKeyboard;

// This macro declares the following functions:
// DMF_VirtualHidKeyboard_ATTRIBUTES_INIT()
// DMF_CONFIG_VirtualHidKeyboard_AND_ATTRIBUTES_INIT()
// DMF_VirtualHidKeyboard_Create()
//
DECLARE_DMF_MODULE(VirtualHidKeyboard)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualHidKeyboard_Type(
    _In_ DMFMODULE DmfModule,
    _In_ PUSHORT const KeysToType,
    _In_ ULONG NumberOfKeys,
    _In_ USHORT UsagePage
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualHidKeyboard_Toggle(
    _In_ DMFMODULE DmfModule,
    _In_ USHORT const KeyToToggle,
    _In_ USHORT UsagePage
    );

// eof: Dmf_VirtualHidKeyboard.h
//
