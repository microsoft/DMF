/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidDeviceMiniSample.c

Abstract:

    This Module provides the functionality exposed by the original VHIDMINI2 sample. It creates
    a sample device with some HID features, input and output reports. It is a template upon which
    other virtual HID devices for Kernel/User-mode can be built. Kernel-mode only devices should
    use DMF_VirtualHidDeviceVhf Module instead.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#include "Dmf_VirtualHidDeviceMiniSample.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Input from device to system.
//
typedef struct _HIDMINI_INPUT_REPORT
{
    UCHAR ReportId;

    UCHAR Data; 
} HIDMINI_INPUT_REPORT;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Underlying VHIDMINI2 support.
    //
    DMFMODULE DmfModuleVirtualHidDeviceMini;
    // Private data for this device.
    //
    BYTE DeviceData;
    UCHAR OutputReport;
    HID_DEVICE_ATTRIBUTES HidDeviceAttributes;
    HID_DESCRIPTOR HidDescriptor;
    HIDMINI_INPUT_REPORT ReadReport;
} DMF_CONTEXT_VirtualHidDeviceMiniSample;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(VirtualHidDeviceMiniSample)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(VirtualHidDeviceMiniSample)

// MemoryTag.
//
#define MemoryTag 'mDHV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

//
// These are the device attributes returned by the mini driver in response
// to IOCTL_HID_GET_DEVICE_ATTRIBUTES.
//
#define HIDMINI_PID             0xFEED
#define HIDMINI_VID             0xDEED
#define HIDMINI_VERSION         0x0101

//
// Custom control codes are defined here. They are to be used for sideband 
// communication with the hid minidriver. These control codes are sent to 
// the hid minidriver using Hid_SetFeature() API to a custom collection 
// defined especially to handle such requests.
//
#define  HIDMINI_CONTROL_CODE_SET_ATTRIBUTES              0x00
#define  HIDMINI_CONTROL_CODE_DUMMY1                      0x01
#define  HIDMINI_CONTROL_CODE_DUMMY2                      0x02

//
// This is the report id of the collection to which the control codes are sent
//
#define CONTROL_COLLECTION_REPORT_ID                      0x01
#define TEST_COLLECTION_REPORT_ID                         0x02

#define MAXIMUM_STRING_LENGTH           (126 * sizeof(WCHAR))
#define VHIDMINI_DEVICE_STRING          L"UMDF Virtual hidmini device"  
#define VHIDMINI_MANUFACTURER_STRING    L"UMDF Virtual hidmini device Manufacturer string"  
#define VHIDMINI_PRODUCT_STRING         L"UMDF Virtual hidmini device Product string"  
#define VHIDMINI_SERIAL_NUMBER_STRING   L"UMDF Virtual hidmini device Serial Number string"  
#define VHIDMINI_DEVICE_STRING_INDEX    5

#include <pshpack1.h>

typedef struct _MY_DEVICE_ATTRIBUTES
{
    USHORT VendorID;
    USHORT ProductID;
    USHORT VersionNumber;
} MY_DEVICE_ATTRIBUTES;

typedef struct _HIDMINI_CONTROL_INFO
{
    // Report ID of the collection to which the control request is sent
    //
    UCHAR ReportId;

    // One byte control code (user-defined) for communication with hid 
    // mini driver.
    //
    UCHAR ControlCode;

    //
    // This union contains input data for the control request.
    //
    union 
    {
        MY_DEVICE_ATTRIBUTES Attributes;
        struct 
        {
            ULONG Dummy1;
            ULONG Dummy2;
        } Dummy;
    } u;
} HIDMINI_CONTROL_INFO;

// Output to device from system.
//
typedef struct _HIDMINI_OUTPUT_REPORT
{
    UCHAR ReportId;

    UCHAR Data; 

    USHORT Pad1;

    ULONG Pad2;
} HIDMINI_OUTPUT_REPORT;

#include <poppack.h>

// SetFeature request requires that the feature report buffer size be exactly 
// same as the size of report described in the hid report descriptor (
// excluding the report ID). Since HIDMINI_CONTROL_INFO includes report ID,
// we subtract one from the size.
//
#define FEATURE_REPORT_SIZE_CB      ((USHORT)(sizeof(HIDMINI_CONTROL_INFO) - 1))
#define INPUT_REPORT_SIZE_CB        ((USHORT)(sizeof(HIDMINI_INPUT_REPORT) - 1))
#define OUTPUT_REPORT_SIZE_CB       ((USHORT)(sizeof(HIDMINI_OUTPUT_REPORT) - 1))

#define CONTROL_FEATURE_REPORT_ID   0x01

typedef UCHAR HID_REPORT_DESCRIPTOR;

// This is the default report descriptor for the virtual Hid device returned
// by the mini driver in response to IOCTL_HID_GET_REPORT_DESCRIPTOR.
//
HID_REPORT_DESCRIPTOR
g_VirtualHidDeviceMiniSample_DefaultReportDescriptor[] = 
{
    0x06,0x00, 0xFF,                    // USAGE_PAGE (Vendor Defined Usage Page)
    0x09,0x01,                          // USAGE (Vendor Usage 0x01)
    0xA1,0x01,                          // COLLECTION (Application)
    0x85,CONTROL_FEATURE_REPORT_ID,     // REPORT_ID (1)
    0x09,0x01,                         // USAGE (Vendor Usage 0x01)
    0x15,0x00,                         // LOGICAL_MINIMUM(0)
    0x26,0xff, 0x00,                   // LOGICAL_MAXIMUM(255)
    0x75,0x08,                         // REPORT_SIZE (0x08)
    0x96,(FEATURE_REPORT_SIZE_CB & 0xff), (FEATURE_REPORT_SIZE_CB >> 8), // REPORT_COUNT
    0xB1,0x00,                         // FEATURE (Data,Ary,Abs)
    0x09,0x01,                         // USAGE (Vendor Usage 0x01)
    0x75,0x08,                         // REPORT_SIZE (0x08)
    0x96,(INPUT_REPORT_SIZE_CB & 0xff), (INPUT_REPORT_SIZE_CB >> 8), // REPORT_COUNT
    0x81,0x00,                         // INPUT (Data,Ary,Abs)
    0x09,0x01,                         // USAGE (Vendor Usage 0x01)
    0x75,0x08,                         // REPORT_SIZE (0x08)
    0x96,(OUTPUT_REPORT_SIZE_CB & 0xff), (OUTPUT_REPORT_SIZE_CB >> 8), // REPORT_COUNT
    0x91,0x00,                         // OUTPUT (Data,Ary,Abs)
    0xC0,                           // END_COLLECTION
};

// This is the default HID descriptor returned by the mini driver
// in response to IOCTL_HID_GET_DEVICE_DESCRIPTOR. The size
// of report descriptor is currently the size of G_DefaultReportDescriptor.
//
HID_DESCRIPTOR
g_VirtualHidDeviceMiniSample_DefaultHidDescriptor = 
{
    0x09,   // length of HID descriptor
    0x21,   // descriptor type == HID  0x21
    0x0100, // hid spec release
    0x00,   // country code == Not Specified
    0x01,   // number of HID class descriptors
    {                                       //DescriptorList[0]
        0x22,                               //report descriptor type 0x22
        sizeof(g_VirtualHidDeviceMiniSample_DefaultReportDescriptor)   //total length of report descriptor
    }
};

NTSTATUS
VirtualHidDeviceMiniSample_WriteReport(
    _In_ DMFMODULE DmfModule,
    _In_ HID_XFER_PACKET* Packet,
    _Out_ ULONG* ReportSize
    )
/*++

Routine Description:

    Callback function that allows this Module to support "WriteReport".

Arguments:

    DmfModule - Child Module that makes calls this callback.
    Packet - Extracted HID packet.
    ReportSize - Size of the Report Buffer read.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS ntStatus;
    ULONG reportSize;
    HIDMINI_OUTPUT_REPORT* outputReport;
    DMFMODULE dmfModuleParent;

    KdPrint(("WriteReport\n"));

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // Return error for unknown collection
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        KdPrint(("WriteReport: unknown report id %d\n", Packet->reportId));
        goto Exit;
    }

    // Before touching buffer make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_OUTPUT_REPORT);

    if (Packet->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        KdPrint(("WriteReport: invalid input buffer. size %d, expect %d\n",
                            Packet->reportBufferLen, reportSize));
        goto Exit;
    }

    outputReport = (HIDMINI_OUTPUT_REPORT*)Packet->reportBuffer;

    DMF_CONTEXT_VirtualHidDeviceMiniSample* moduleContext;

    // Store the device data in the Module Context.
    //
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);
    moduleContext->DeviceData = outputReport->Data;

    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

HRESULT
VirtualHidDeviceMiniSample_GetFeature(
    _In_ DMFMODULE DmfModule,
    _In_ HID_XFER_PACKET* Packet,
    _Out_ ULONG* ReportSize
    )
/*++

Routine Description:

    Handles IOCTL_HID_GET_FEATURE for all the collection.

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to  Request Packet.

Return Value:

    NT ntStatus code.

--*/
{
    NTSTATUS ntStatus;
    ULONG reportSize;
    MY_DEVICE_ATTRIBUTES* myAttributes;
    DMF_CONTEXT_VirtualHidDeviceMiniSample* moduleContext;
    DMFMODULE dmfModuleParent;

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    PHID_DEVICE_ATTRIBUTES  hidAttributes = &moduleContext->HidDeviceAttributes;

    KdPrint(("GetFeature\n"));

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        KdPrint(("GetFeature: invalid report id %d\n", Packet->reportId));
        goto Exit;
    }

    //
    // Since output buffer is for write only (no read allowed by UMDF in output
    // buffer), any read from output buffer would be reading garbage), so don't
    // let app embed custom control code in output buffer. The minidriver can
    // support multiple features using separate report ID instead of using
    // custom control code. Since this is targeted at report ID 1, we know it
    // is a request for getting attributes.
    //
    // While KMDF does not enforce the rule (disallow read from output buffer),
    // it is good practice to not do so.
    //

    reportSize = sizeof(MY_DEVICE_ATTRIBUTES) + sizeof(Packet->reportId);
    if (Packet->reportBufferLen < reportSize) 
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        KdPrint(("GetFeature: output buffer too small. Size %d, expect %d\n",
                            Packet->reportBufferLen, reportSize));
        goto Exit;
    }

    //
    // Since this device has one report ID, hidclass would pass on the report
    // ID in the buffer (it wouldn't if report descriptor did not have any report
    // ID). However, since UMDF allows only writes to an output buffer, we can't
    // "read" the report ID from "output" buffer. There is no need to read the
    // report ID since we get it other way as shown above, however this is
    // something to keep in mind.
    //
    myAttributes = (MY_DEVICE_ATTRIBUTES*)(Packet->reportBuffer + sizeof(Packet->reportId));
    myAttributes->ProductID     = hidAttributes->ProductID;
    myAttributes->VendorID      = hidAttributes->VendorID;
    myAttributes->VersionNumber = hidAttributes->VersionNumber;

    // Report how many bytes were written.
    //
    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}


NTSTATUS
VirtualHidDeviceMiniSample_SetFeature(
    _In_ DMFMODULE DmfModule,
    _In_ HID_XFER_PACKET* Packet,
    _Out_ ULONG* ReportSize
    )
/*++

Routine Description:

    Handles IOCTL_HID_SET_FEATURE for all the collection.
    For control collection (custom defined collection) it handles
    the user-defined control codes for sideband communication

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to Request Packet.

Return Value:

    NT ntStatus code.

--*/
{
    NTSTATUS ntStatus;
    ULONG reportSize;
    HIDMINI_CONTROL_INFO* controlInfo;
    DMF_CONTEXT_VirtualHidDeviceMiniSample* moduleContext;
    DMFMODULE dmfModuleParent;

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);
    
    PHID_DEVICE_ATTRIBUTES  hidAttributes = &moduleContext->HidDeviceAttributes;

    KdPrint(("SetFeature\n"));

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        KdPrint(("SetFeature: invalid report id %d\n", Packet->reportId));
        goto Exit;
    }

    // before touching control code make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_CONTROL_INFO);

    if (Packet->reportBufferLen < reportSize) 
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        KdPrint(("SetFeature: invalid input buffer. size %d, expect %d\n",
                            Packet->reportBufferLen, reportSize));
        goto Exit;
    }

    controlInfo = (HIDMINI_CONTROL_INFO*)Packet->reportBuffer;
    switch (controlInfo->ControlCode)
    {
        case HIDMINI_CONTROL_CODE_SET_ATTRIBUTES:
            //
            // Store the device attributes in device extension
            //
            hidAttributes->ProductID     = controlInfo->u.Attributes.ProductID;
            hidAttributes->VendorID      = controlInfo->u.Attributes.VendorID;
            hidAttributes->VersionNumber = controlInfo->u.Attributes.VersionNumber;

            //
            // set ntStatus and information
            //
            *ReportSize = reportSize;
            ntStatus = STATUS_SUCCESS;
            break;

        case HIDMINI_CONTROL_CODE_DUMMY1:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            KdPrint(("SetFeature: HIDMINI_CONTROL_CODE_DUMMY1\n"));
            break;

        case HIDMINI_CONTROL_CODE_DUMMY2:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            KdPrint(("SetFeature: HIDMINI_CONTROL_CODE_DUMMY2\n"));
            break;

        default:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            KdPrint(("SetFeature: Unknown control Code 0x%x\n",
                                controlInfo->ControlCode));
            break;
    }

Exit:

    return ntStatus;
}

NTSTATUS
VirtualHidDeviceMiniSample_GetInputReport(
    _In_ DMFMODULE DmfModule,
    _In_ HID_XFER_PACKET* Packet,
    _Out_ ULONG* ReportSize
    )
/*++

Routine Description:

    Handles IOCTL_HID_GET_INPUT_REPORT for all the collection.

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to Request Packet.

Return Value:

    NT ntStatus code.

--*/
{
    NTSTATUS                ntStatus;
    ULONG                   reportSize;
    HIDMINI_INPUT_REPORT*   reportBuffer;
    DMF_CONTEXT_VirtualHidDeviceMiniSample* moduleContext;
    DMFMODULE dmfModuleParent;

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    KdPrint(("GetInputReport\n"));

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        KdPrint(("GetInputReport: invalid report id %d\n", Packet->reportId));
        goto Exit;
    }

    reportSize = sizeof(HIDMINI_INPUT_REPORT);
    if (Packet->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        KdPrint(("GetInputReport: output buffer too small. Size %d, expect %d\n",
                            Packet->reportBufferLen, reportSize));
        goto Exit;
    }

    reportBuffer = (HIDMINI_INPUT_REPORT*)(Packet->reportBuffer);

    reportBuffer->ReportId = CONTROL_COLLECTION_REPORT_ID;
    reportBuffer->Data     = moduleContext->OutputReport;

    // Report how many bytes were copied
    //
    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}


NTSTATUS
VirtualHidDeviceMiniSample_SetOutputReport(
    _In_ DMFMODULE DmfModule,
    _In_ HID_XFER_PACKET* Packet,
    _Out_ ULONG* ReportSize
    )
/*++

Routine Description:

    Handles IOCTL_HID_SET_OUTPUT_REPORT for all the collection.

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to Request Packet.

Return Value:

    NT ntStatus code.

--*/
{
    NTSTATUS ntStatus;
    ULONG reportSize;
    HIDMINI_OUTPUT_REPORT* reportBuffer;
    DMF_CONTEXT_VirtualHidDeviceMiniSample* moduleContext;
    DMFMODULE dmfModuleParent;

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    KdPrint(("SetOutputReport\n"));

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        KdPrint(("SetOutputReport: unknown report id %d\n", Packet->reportId));
        goto Exit;
    }

    // before touching buffer make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_OUTPUT_REPORT);

    if (Packet->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        KdPrint(("SetOutputReport: invalid input buffer. size %d, expect %d\n",
                            Packet->reportBufferLen, reportSize));
        goto Exit;
    }

    reportBuffer = (HIDMINI_OUTPUT_REPORT*)Packet->reportBuffer;

    moduleContext->OutputReport = reportBuffer->Data;

    // Report how many bytes were written.
    //
    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

NTSTATUS
VirtualHidDeviceMiniSample_RetrieveNextInputReport(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR** Buffer,
    _Out_ ULONG* BufferSize
    )
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_VirtualHidDeviceMiniSample* moduleContext;

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    *Buffer = (UCHAR*)&moduleContext->ReadReport;
    *BufferSize = sizeof(HIDMINI_INPUT_REPORT);

    return STATUS_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

PWSTR g_VirtualHidDeviceMiniSample_Strings[] =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    VHIDMINI_DEVICE_STRING
};

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_VirtualHidDeviceMiniSample_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_VirtualHidDeviceMiniSample* moduleConfig;
    DMF_CONTEXT_VirtualHidDeviceMiniSample* moduleContext;
    DMF_CONFIG_VirtualHidDeviceMini virtualHidDeviceMiniModuleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleConfig->ReadFromRegistry)
    {
        // TODO: Read HID descirptors from Registry.
        //
    }

    // VirtualHidDeviceMini
    // --------------------
    //
    DMF_CONFIG_VirtualHidDeviceMini_AND_ATTRIBUTES_INIT(&virtualHidDeviceMiniModuleConfig,
                                                        &moduleAttributes);

    virtualHidDeviceMiniModuleConfig.VendorId = HIDMINI_VID;
    virtualHidDeviceMiniModuleConfig.ProductId = HIDMINI_PID;
    virtualHidDeviceMiniModuleConfig.VersionNumber = HIDMINI_VERSION;

    virtualHidDeviceMiniModuleConfig.HidDescriptor = &g_VirtualHidDeviceMiniSample_DefaultHidDescriptor;
    virtualHidDeviceMiniModuleConfig.HidDescriptorLength = sizeof(g_VirtualHidDeviceMiniSample_DefaultHidDescriptor);
    virtualHidDeviceMiniModuleConfig.HidReportDescriptor = g_VirtualHidDeviceMiniSample_DefaultReportDescriptor;
    virtualHidDeviceMiniModuleConfig.HidReportDescriptorLength = sizeof(g_VirtualHidDeviceMiniSample_DefaultReportDescriptor);

    // Set virtual device attributes.
    //
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.VendorID = HIDMINI_VID;
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.ProductID = HIDMINI_PID;
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.VersionNumber = HIDMINI_VERSION;
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.Size = sizeof(HID_DEVICE_ATTRIBUTES);

    virtualHidDeviceMiniModuleConfig.GetInputReport = VirtualHidDeviceMiniSample_GetInputReport;
    virtualHidDeviceMiniModuleConfig.GetFeature = VirtualHidDeviceMiniSample_GetFeature;
    virtualHidDeviceMiniModuleConfig.SetFeature = VirtualHidDeviceMiniSample_SetFeature;
    virtualHidDeviceMiniModuleConfig.SetOutputReport = VirtualHidDeviceMiniSample_SetOutputReport;
    virtualHidDeviceMiniModuleConfig.WriteReport = VirtualHidDeviceMiniSample_WriteReport;
    virtualHidDeviceMiniModuleConfig.RetrieveNextInputReport = VirtualHidDeviceMiniSample_RetrieveNextInputReport;

    virtualHidDeviceMiniModuleConfig.StringSizeCbManufacturer = sizeof(VHIDMINI_MANUFACTURER_STRING);
    virtualHidDeviceMiniModuleConfig.StringManufacturer = VHIDMINI_MANUFACTURER_STRING;
    virtualHidDeviceMiniModuleConfig.StringSizeCbProduct = sizeof(VHIDMINI_PRODUCT_STRING);
    virtualHidDeviceMiniModuleConfig.StringProduct = VHIDMINI_PRODUCT_STRING;
    virtualHidDeviceMiniModuleConfig.StringSizeCbSerialNumber = sizeof(VHIDMINI_SERIAL_NUMBER_STRING);
    virtualHidDeviceMiniModuleConfig.StringSerialNumber = VHIDMINI_SERIAL_NUMBER_STRING;

    virtualHidDeviceMiniModuleConfig.Strings = g_VirtualHidDeviceMiniSample_Strings;
    virtualHidDeviceMiniModuleConfig.NumberOfStrings = ARRAYSIZE(g_VirtualHidDeviceMiniSample_Strings);

    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleVirtualHidDeviceMini);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_VirtualHidDeviceMiniSample_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type VirtualHidDeviceMiniSample.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualHidDeviceMiniSample* moduleConfig;
    DMF_CONTEXT_VirtualHidDeviceMiniSample* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext->DeviceData = 0;

    moduleContext->ReadReport.ReportId = CONTROL_FEATURE_REPORT_ID;
    moduleContext->ReadReport.Data = moduleContext->DeviceData;

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_VirtualHidDeviceMiniSample_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type VirtualHidDeviceMiniSample.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualHidDeviceMiniSample_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type VirtualHidDeviceMiniSample.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_VirtualHidDeviceMiniSample;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_VirtualHidDeviceMiniSample;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_VirtualHidDeviceMiniSample);
    dmfCallbacksDmf_VirtualHidDeviceMiniSample.ChildModulesAdd = DMF_VirtualHidDeviceMiniSample_ChildModulesAdd;
    dmfCallbacksDmf_VirtualHidDeviceMiniSample.DeviceOpen = DMF_VirtualHidDeviceMiniSample_Open;
    dmfCallbacksDmf_VirtualHidDeviceMiniSample.DeviceClose = DMF_VirtualHidDeviceMiniSample_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_VirtualHidDeviceMiniSample,
                                            VirtualHidDeviceMiniSample,
                                            DMF_CONTEXT_VirtualHidDeviceMiniSample,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_VirtualHidDeviceMiniSample.CallbacksDmf = &dmfCallbacksDmf_VirtualHidDeviceMiniSample;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_VirtualHidDeviceMiniSample,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_VirtualHidDeviceMiniSample.c
//
