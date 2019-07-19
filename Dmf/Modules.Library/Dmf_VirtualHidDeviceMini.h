/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidDeviceMini.h

Abstract:

    Companion file to Dmf_VirtualHidDeviceMini.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

typedef UCHAR VirtualHidDeviceMini_HID_REPORT_DESCRIPTOR;

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

    size_t StringSizeCbManufacturer; // sizeof(VHIDMINI_MANUFACTURER_STRING);
    PWSTR StringManufacturer;       // VHIDMINI_MANUFACTURER_STRING;
    size_t StringSizeCbProduct; // sizeof(VHIDMINI_PRODUCT_STRING);
    PWSTR StringProduct;       // VHIDMINI_PRODUCT_STRING;
    size_t StringSizeCbSerialNumber; // sizeof(VHIDMINI_SERIAL_NUMBER_STRING);
    PWSTR StringSerialNumber;       // VHIDMINI_SERIAL_NUMBER_STRING;

    // VHIDMINI_DEVICE_STRING          L"UMDF Virtual hidmini device"  
    // VHIDMINI_DEVICE_STRING_INDEX    5
    PWSTR* Strings;
    ULONG NumberOfStrings;
} DMF_CONFIG_VirtualHidDeviceMini;

// This macro declares the following functions:
// DMF_VirtualHidDeviceMini_ATTRIBUTES_INIT()
// DMF_CONFIG_VirtualHidDeviceMini_AND_ATTRIBUTES_INIT()
// DMF_VirtualHidDeviceMini_Create()
//
DECLARE_DMF_MODULE(VirtualHidDeviceMini)

// Module Methods
//

// eof: Dmf_VirtualHidDeviceMini.h
//
