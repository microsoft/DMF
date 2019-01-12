/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_LegacyProtocol.h

Abstract:

    Companion file to Dmf_LegacyProtocol.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// {B279DB36-54DD-4912-BFC7-1402964A6717}
//
DEFINE_GUID(LegacyProtocol_Interface_Guid, 
    0xb279db36, 0x54dd, 0x4912, 0xbf, 0xc7, 0x14, 0x2, 0x96, 0x4a, 0x67, 0x17);

// LegacyProtocol Protocol-Transport (Interface) Messages.
//
#define LegacyProtocol_TransportMessage_StringPrint     0

// This macro declares the following functions:
// DMF_LegacyProtocol_ATTRIBUTES_INIT()
// DMF_LegacyProtocol_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(LegacyProtocol)

// Module Methods
//

VOID
DMF_LegacyProtocol_StringDisplay(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* String
    );

// eof: Dmf_LegacyProtocol.h
//
