/*++

    Copyright (c) Microsoft Corporation. All Rights Reserved.
    Licensed under the MIT license.

Module Name:

   Dmf_ComponentFirmwareUpdateHidTransport.h

Abstract:

   Companion file to Dmf_ComponentFirmwareUpdateHidTransport.c

Environment:

   User-mode Driver Framework

--*/

#pragma once


// Configuration of the module.
//
typedef struct
{
    // Hid transport protocol.
    //
    UINT16 Protocol;

    // Number of Input reports reads that are simultaneously pended.
    //
    DWORD NumberOfInputReportReadsPended;
    // Optional timeout to be used for transport operations. If not
    // specifically set, it will use the default timeout of HIDDEVICE_RECOMMENDED_WAIT_TIMEOUT_MS.
    //
    ULONG HidDeviceWaitTimeoutMs;

    // Payload buffer fill alignment required.
    //
    UINT PayloadFillAlignment;
} DMF_CONFIG_ComponentFirmwareUpdateHidTransport;

// This macro declares the following functions:
// DMF_ComponentFirmwareUpdateHidTransport_ATTRIBUTES_INIT()
// DMF_CONFIG_ComponentFirmwareUpdateHidTransport_AND_ATTRIBUTES_INIT()
// DMF_ComponentFirmwareUpdateHidTransport_Create()
//
DECLARE_DMF_MODULE(ComponentFirmwareUpdateHidTransport)

// Module Methods
//

// eof: Dmf_ComponentFirmwareUpdateHidTransport.h
//