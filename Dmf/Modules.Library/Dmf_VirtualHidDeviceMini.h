/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidDeviceMini.h

Abstract:

    Companion file to Dmf_VirtualHidDeviceMini.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#include "Dmf_HidDefinitions.h"

// TODO: HACK
//
#if defined(DMF_USER_MODE)

#include <pshpack1.h>
typedef struct _HID_DESCRIPTOR
{
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    USHORT  bcdHID;
    UCHAR   bCountry;
    UCHAR   bNumDescriptors;

    //
    // An array of one OR MORE descriptors.
    //
    struct _HID_DESCRIPTOR_DESC_LIST {
       UCHAR   bReportType;
       USHORT  wReportLength;
    } DescriptorList [1];

} HID_DESCRIPTOR, * PHID_DESCRIPTOR;
#include <poppack.h>

typedef struct _HID_DEVICE_ATTRIBUTES {

    ULONG           Size;
    //
    // sizeof (struct _HID_DEVICE_ATTRIBUTES)
    //

    //
    // Vendor ids of this hid device
    //
    USHORT          VendorID;
    USHORT          ProductID;
    USHORT          VersionNumber;
    USHORT          Reserved[11];

} HID_DEVICE_ATTRIBUTES, * PHID_DEVICE_ATTRIBUTES;

//
// Internal IOCTLs for the class/mini driver interface.
//

#define IOCTL_HID_GET_DEVICE_DESCRIPTOR             HID_CTL_CODE(0)
#define IOCTL_HID_GET_REPORT_DESCRIPTOR             HID_CTL_CODE(1)
#define IOCTL_HID_READ_REPORT                       HID_CTL_CODE(2)
#define IOCTL_HID_WRITE_REPORT                      HID_CTL_CODE(3)
#define IOCTL_HID_GET_STRING                        HID_CTL_CODE(4)
#define IOCTL_HID_ACTIVATE_DEVICE                   HID_CTL_CODE(7)
#define IOCTL_HID_DEACTIVATE_DEVICE                 HID_CTL_CODE(8)
#define IOCTL_HID_GET_DEVICE_ATTRIBUTES             HID_CTL_CODE(9)
#define IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST    HID_CTL_CODE(10)

//
// Internal IOCTLs supported by UMDF HID minidriver.  
//
#define IOCTL_UMDF_HID_SET_FEATURE                   HID_CTL_CODE(20)
#define IOCTL_UMDF_HID_GET_FEATURE                   HID_CTL_CODE(21)
#define IOCTL_UMDF_HID_SET_OUTPUT_REPORT             HID_CTL_CODE(22)
#define IOCTL_UMDF_HID_GET_INPUT_REPORT              HID_CTL_CODE(23)
#define IOCTL_UMDF_GET_PHYSICAL_DESCRIPTOR           HID_CTL_CODE(24)

//
// Codes for HID-specific descriptor types, from HID USB spec.
//
#define HID_HID_DESCRIPTOR_TYPE             0x21
#define HID_REPORT_DESCRIPTOR_TYPE          0x22
#define HID_PHYSICAL_DESCRIPTOR_TYPE        0x23

//
//  These are string IDs for use with IOCTL_HID_GET_STRING
//  They match the string field offsets in Chapter 9 of the USB Spec.
//
#define HID_STRING_ID_IMANUFACTURER     14
#define HID_STRING_ID_IPRODUCT          15
#define HID_STRING_ID_ISERIALNUMBER     16

#endif

typedef UCHAR VirtualHidDeviceMini_HID_REPORT_DESCRIPTOR;

typedef 
NTSTATUS
EVT_VirtualHidDeviceMini_WriteReport(_In_ DMFMODULE,
                                     _In_ HID_XFER_PACKET* Packet,
                                     _Out_ ULONG* ReportSize);

typedef 
NTSTATUS
EVT_VirtualHidDeviceMini_GetFeature(_In_ DMFMODULE,
                                    _In_ HID_XFER_PACKET* Packet,
                                    _Out_ ULONG* ReportSize);

typedef 
NTSTATUS
EVT_VirtualHidDeviceMini_SetFeature(_In_ DMFMODULE,
                                    _In_ HID_XFER_PACKET* Packet,
                                    _Out_ ULONG* ReportSize);

typedef 
NTSTATUS
EVT_VirtualHidDeviceMini_GetInputReport(_In_ DMFMODULE,
                                        _In_ HID_XFER_PACKET* Packet,
                                        _Out_ ULONG* ReportSize);

typedef 
NTSTATUS
EVT_VirtualHidDeviceMini_SetOutputReport(_In_ DMFMODULE,
                                         _In_ HID_XFER_PACKET* Packet,
                                         _Out_ ULONG* ReportSize);

typedef 
NTSTATUS
EVT_VirtualHidDeviceMini_RetrieveNextInputReport(_In_ DMFMODULE,
                                                 _Out_ UCHAR** Buffer,
                                                 _Out_ ULONG* BufferSize);

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

    size_t StringSizeCbManufacturer;
    PWSTR StringManufacturer;
    size_t StringSizeCbProduct;
    PWSTR StringProduct;
    size_t StringSizeCbSerialNumber;
    PWSTR StringSerialNumber;

    PWSTR* Strings;
    ULONG NumberOfStrings;

    // Client callback handlers.
    //
    EVT_VirtualHidDeviceMini_WriteReport* WriteReport;
    EVT_VirtualHidDeviceMini_GetFeature* GetFeature;
    EVT_VirtualHidDeviceMini_SetFeature* SetFeature;
    EVT_VirtualHidDeviceMini_GetInputReport* GetInputReport;
    EVT_VirtualHidDeviceMini_SetOutputReport* SetOutputReport;
    EVT_VirtualHidDeviceMini_RetrieveNextInputReport* RetrieveNextInputReport;
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
