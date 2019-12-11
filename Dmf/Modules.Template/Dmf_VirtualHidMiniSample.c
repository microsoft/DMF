/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidMiniSample.c

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

#include "Dmf_VirtualHidMiniSample.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Input from device to system.
//
typedef struct _HIDMINI_INPUT_REPORT
{
    // Report Id.
    //
    UCHAR ReportId;
    // Data in the Read Report.
    //
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
    DMFMODULE DmfModuleVirtualHidMini;
    // Private data for this device.
    //
    BYTE DeviceData;
    UCHAR OutputReport;
    HID_DEVICE_ATTRIBUTES HidDeviceAttributes;
    HID_DESCRIPTOR HidDescriptor;
    HIDMINI_INPUT_REPORT ReadReport;
    WDFTIMER Timer;
} DMF_CONTEXT_VirtualHidMiniSample;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(VirtualHidMiniSample)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(VirtualHidMiniSample)

// MemoryTag.
//
#define MemoryTag 'mDHV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// These are the device attributes returned by the mini driver in response
// to IOCTL_HID_GET_DEVICE_ATTRIBUTES.
//
#define HIDMINI_PID             0xFEED
#define HIDMINI_VID             0xDEED
#define HIDMINI_VERSION         0x0101

// Custom control codes are defined here. They are to be used for sideband 
// communication with the hid minidriver. These control codes are sent to 
// the hid minidriver using Hid_SetFeature() API to a custom collection 
// defined especially to handle such requests.
//
#define  HIDMINI_CONTROL_CODE_SET_ATTRIBUTES              0x00
#define  HIDMINI_CONTROL_CODE_DUMMY1                      0x01
#define  HIDMINI_CONTROL_CODE_DUMMY2                      0x02

// This is the report id of the collection to which the control codes are sent.
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
g_VirtualHidMiniSample_DefaultReportDescriptor[] = 
{
    0x06,0x00, 0xFF,                    // USAGE_PAGE (Vendor Defined Usage Page)
    0x09,0x01,                          // USAGE (Vendor Usage 0x01)
    0xA1,0x01,                          // COLLECTION (HID_FLAGS_COLLECTION_Application)
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
g_VirtualHidMiniSample_DefaultHidDescriptor = 
{
    0x09,   // length of HID descriptor
    0x21,   // descriptor type == HID  0x21
    0x0100, // hid spec release
    0x00,   // country code == Not Specified
    0x01,   // number of HID class descriptors
    {                                       //DescriptorList[0]
        0x22,                               //report descriptor type 0x22
        sizeof(g_VirtualHidMiniSample_DefaultReportDescriptor)   //total length of report descriptor
    }
};

EVT_WDF_TIMER VirtualHidMiniSample_EvtTimerHandler;

NTSTATUS
VirtualHidMiniSample_WriteReport(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
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

    UNREFERENCED_PARAMETER(Request);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // Return error for unknown collection
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidMiniSample_WriteReport: unknown report id %d", Packet->reportId);
        goto Exit;
    }

    // Before touching buffer make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_OUTPUT_REPORT);

    if (Packet->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidMiniSample_WriteReport: invalid input buffer. size %d, expect %d", Packet->reportBufferLen, reportSize);
        goto Exit;
    }

    outputReport = (HIDMINI_OUTPUT_REPORT*)Packet->reportBuffer;

    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;

    // Store the device data in the Module Context.
    //
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);
    moduleContext->DeviceData = outputReport->Data;

    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

NTSTATUS
VirtualHidMiniSample_GetFeature(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ HID_XFER_PACKET* Packet,
    _Out_ ULONG* ReportSize
    )
/*++

Routine Description:

    Handles IOCTL_HID_GET_FEATURE for all the collection.

Arguments:

    DmfModule - Child (DMF_VirtualHidMini) Module's handle.
    Packet - Contains the target buffer.
    ReportSize - Indicates how much data is written to target buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG reportSize;
    MY_DEVICE_ATTRIBUTES* myAttributes;
    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;
    DMFMODULE dmfModuleParent;

    UNREFERENCED_PARAMETER(Request);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    PHID_DEVICE_ATTRIBUTES  hidAttributes = &moduleContext->HidDeviceAttributes;

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidMiniSample_GetFeature fails: invalid report id %d", Packet->reportId);
        goto Exit;
    }

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualHidMiniSample_GetFeature fails: output buffer too small. Size %d, expect %d",
                    Packet->reportBufferLen,
                    reportSize);
        goto Exit;
    }

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
VirtualHidMiniSample_SetFeature(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ HID_XFER_PACKET* Packet,
    _Out_ ULONG* ReportSize
    )
/*++

Routine Description:

    Handles IOCTL_HID_SET_FEATURE for all the collection.
    For control collection (custom defined collection) it handles
    the user-defined control codes for sideband communication

Arguments:

    DmfModule - Child (DMF_VirtualHidMini) Module's handle.
    Packet - Contains the source buffer.
    ReportSize - Indicates how much data is read from source buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG reportSize;
    HIDMINI_CONTROL_INFO* controlInfo;
    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;
    DMFMODULE dmfModuleParent;

    UNREFERENCED_PARAMETER(Request);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);
    
    PHID_DEVICE_ATTRIBUTES  hidAttributes = &moduleContext->HidDeviceAttributes;

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidMiniSample_SetFeature fails: invalid report id %d", Packet->reportId);
        goto Exit;
    }

    // Before touching control code make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_CONTROL_INFO);

    if (Packet->reportBufferLen < reportSize) 
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualHidMiniSample_SetFeature fails: invalid input buffer. size %d, expect %d",
                    Packet->reportBufferLen, reportSize);
        goto Exit;
    }

    controlInfo = (HIDMINI_CONTROL_INFO*)Packet->reportBuffer;
    switch (controlInfo->ControlCode)
    {
        case HIDMINI_CONTROL_CODE_SET_ATTRIBUTES:
            // Store the device attributes in device extension.
            //
            hidAttributes->ProductID     = controlInfo->u.Attributes.ProductID;
            hidAttributes->VendorID      = controlInfo->u.Attributes.VendorID;
            hidAttributes->VersionNumber = controlInfo->u.Attributes.VersionNumber;

            // Set ntStatus and information.
            //
            *ReportSize = reportSize;
            ntStatus = STATUS_SUCCESS;
            break;

        case HIDMINI_CONTROL_CODE_DUMMY1:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            break;

        case HIDMINI_CONTROL_CODE_DUMMY2:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            break;

        default:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidMiniSample_SetFeature fails: Unknown control Code 0x%x", controlInfo->ControlCode);
            break;
    }

Exit:

    return ntStatus;
}

NTSTATUS
VirtualHidMiniSample_GetInputReport(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ HID_XFER_PACKET* Packet,
    _Out_ ULONG* ReportSize
    )
/*++

Routine Description:

    Handles IOCTL_HID_GET_INPUT_REPORT for all the collection.

Arguments:

    DmfModule - Child (DMF_VirtualHidMini) Module's handle.
    Packet - Contains the target buffer.
    ReportSize - Indicates how much data is written to target buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG reportSize;
    HIDMINI_INPUT_REPORT* reportBuffer;
    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;
    DMFMODULE dmfModuleParent;

    UNREFERENCED_PARAMETER(Request);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidMiniSample_GetInputReport fails: invalid report id %d", Packet->reportId);
        goto Exit;
    }

    reportSize = sizeof(HIDMINI_INPUT_REPORT);
    if (Packet->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualHidMiniSample_GetInputReport fails: output buffer too small. Size %d, expect %d",
                    Packet->reportBufferLen,
                    reportSize);
        goto Exit;
    }

    reportBuffer = (HIDMINI_INPUT_REPORT*)(Packet->reportBuffer);

    reportBuffer->ReportId = CONTROL_COLLECTION_REPORT_ID;
    reportBuffer->Data     = moduleContext->OutputReport;

    // Report how many bytes were copied.
    //
    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

NTSTATUS
VirtualHidMiniSample_SetOutputReport(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ HID_XFER_PACKET* Packet,
    _Out_ ULONG* ReportSize
    )
/*++

Routine Description:

    Handles IOCTL_HID_SET_OUTPUT_REPORT for all the collection.

Arguments:

    DmfModule - Child (DMF_VirtualHidMini) Module's handle.
    Packet - Contains the source buffer.
    ReportSize - Indicates how much data is read from source buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG reportSize;
    HIDMINI_OUTPUT_REPORT* reportBuffer;
    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;
    DMFMODULE dmfModuleParent;

    UNREFERENCED_PARAMETER(Request);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidMiniSample_SetOutputReport fails: unknown report id %d", Packet->reportId);
        goto Exit;
    }

    // before touching buffer make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_OUTPUT_REPORT);

    if (Packet->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualHidMiniSample_SetOutputReport fails: invalid input buffer. size %d, expect %d",
                    Packet->reportBufferLen,
                    reportSize);
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
VirtualHidMiniSample_RetrieveNextInputReport(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ UCHAR** Buffer,
    _Out_ ULONG* BufferSize
    )
/*++

Routine Description:

    Called by Child to allow Parent to populate an input report.

Arguments:

    DmfModule - Child Module's handle.
    Request - Request containing input report. Client may opt to keep this request and return it later.
    Buffer - Address of buffer with input report data returned buffer to caller.
    BufferSize - Size of data in buffer returned to caller.

Return Value:

    NTSTATUS

--*/
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;
    HIDMINI_INPUT_REPORT* readReport;

    UNREFERENCED_PARAMETER(Request);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    readReport = &moduleContext->ReadReport;

    // Populate data to return to caller.
    //
    readReport->ReportId = CONTROL_FEATURE_REPORT_ID;
    readReport->Data = moduleContext->DeviceData;

    // Return to caller.
    //
    *Buffer = (UCHAR*)readReport;
    *BufferSize = sizeof(HIDMINI_INPUT_REPORT);

    return STATUS_SUCCESS;
}

void
VirtualHidMiniSample_EvtTimerHandler(
    _In_ WDFTIMER Timer
    )
/*++
Routine Description:

    This periodic timer callback routine checks the device's manual queue and
    completes any pending request with data from the device.

Arguments:

    Timer - Handle to a timer object that was obtained from WdfTimerCreate.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;

    dmfModule = (DMFMODULE)WdfTimerGetParentObject(Timer);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Tell Child Module to dequeue next pending request and call this Module's 
    // callback function to populate it.
    //
    ntStatus = DMF_VirtualHidMini_InputReportGenerate(moduleContext->DmfModuleVirtualHidMini,
                                                      VirtualHidMiniSample_RetrieveNextInputReport);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

PWSTR g_VirtualHidMiniSample_Strings[] =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    VHIDMINI_DEVICE_STRING
};

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_VirtualHidMiniSample_ChildModulesAdd(
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
    DMF_CONFIG_VirtualHidMiniSample* moduleConfig;
    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;
    DMF_CONFIG_VirtualHidMini virtualHidDeviceMiniModuleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleConfig->ReadFromRegistry)
    {
        // TODO: Read HID descriptors from Registry.
        //
    }

    // VirtualHidMini
    // --------------
    //
    DMF_CONFIG_VirtualHidMini_AND_ATTRIBUTES_INIT(&virtualHidDeviceMiniModuleConfig,
                                                        &moduleAttributes);

    virtualHidDeviceMiniModuleConfig.VendorId = HIDMINI_VID;
    virtualHidDeviceMiniModuleConfig.ProductId = HIDMINI_PID;
    virtualHidDeviceMiniModuleConfig.VersionNumber = HIDMINI_VERSION;

    virtualHidDeviceMiniModuleConfig.HidDescriptor = &g_VirtualHidMiniSample_DefaultHidDescriptor;
    virtualHidDeviceMiniModuleConfig.HidDescriptorLength = sizeof(g_VirtualHidMiniSample_DefaultHidDescriptor);
    virtualHidDeviceMiniModuleConfig.HidReportDescriptor = g_VirtualHidMiniSample_DefaultReportDescriptor;
    virtualHidDeviceMiniModuleConfig.HidReportDescriptorLength = sizeof(g_VirtualHidMiniSample_DefaultReportDescriptor);

    // Set virtual device attributes.
    //
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.VendorID = HIDMINI_VID;
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.ProductID = HIDMINI_PID;
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.VersionNumber = HIDMINI_VERSION;
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.Size = sizeof(HID_DEVICE_ATTRIBUTES);

    virtualHidDeviceMiniModuleConfig.GetInputReport = VirtualHidMiniSample_GetInputReport;
    virtualHidDeviceMiniModuleConfig.GetFeature = VirtualHidMiniSample_GetFeature;
    virtualHidDeviceMiniModuleConfig.SetFeature = VirtualHidMiniSample_SetFeature;
    virtualHidDeviceMiniModuleConfig.SetOutputReport = VirtualHidMiniSample_SetOutputReport;
    virtualHidDeviceMiniModuleConfig.WriteReport = VirtualHidMiniSample_WriteReport;

    virtualHidDeviceMiniModuleConfig.StringSizeCbManufacturer = sizeof(VHIDMINI_MANUFACTURER_STRING);
    virtualHidDeviceMiniModuleConfig.StringManufacturer = VHIDMINI_MANUFACTURER_STRING;
    virtualHidDeviceMiniModuleConfig.StringSizeCbProduct = sizeof(VHIDMINI_PRODUCT_STRING);
    virtualHidDeviceMiniModuleConfig.StringProduct = VHIDMINI_PRODUCT_STRING;
    virtualHidDeviceMiniModuleConfig.StringSizeCbSerialNumber = sizeof(VHIDMINI_SERIAL_NUMBER_STRING);
    virtualHidDeviceMiniModuleConfig.StringSerialNumber = VHIDMINI_SERIAL_NUMBER_STRING;

    virtualHidDeviceMiniModuleConfig.Strings = g_VirtualHidMiniSample_Strings;
    virtualHidDeviceMiniModuleConfig.NumberOfStrings = ARRAYSIZE(g_VirtualHidMiniSample_Strings);

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleVirtualHidMini);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_VirtualHidMiniSample_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type VirtualHidMiniSample.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualHidMiniSample* moduleConfig;
    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;
    const ULONG timerPeriodInSeconds = 5;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Initialize the device's data.
    //
    moduleContext->DeviceData = 0;
    moduleContext->ReadReport.ReportId = CONTROL_FEATURE_REPORT_ID;
    moduleContext->ReadReport.Data = moduleContext->DeviceData;

    WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig,
                                   VirtualHidMiniSample_EvtTimerHandler,
                                   timerPeriodInSeconds);

    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = DmfModule;
    ntStatus = WdfTimerCreate(&timerConfig,
                              &timerAttributes,
                              &moduleContext->Timer);
    if ( !NT_SUCCESS(ntStatus) )
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Start immediately.
    //
    WdfTimerStart(moduleContext->Timer,
                  WDF_REL_TIMEOUT_IN_MS(0));

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_VirtualHidMiniSample_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type VirtualHidMiniSample.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_VirtualHidMiniSample* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfTimerStop(moduleContext->Timer,
                 TRUE);

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
DMF_VirtualHidMiniSample_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type VirtualHidMiniSample.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_VirtualHidMiniSample;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_VirtualHidMiniSample;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_VirtualHidMiniSample);
    dmfCallbacksDmf_VirtualHidMiniSample.ChildModulesAdd = DMF_VirtualHidMiniSample_ChildModulesAdd;
    dmfCallbacksDmf_VirtualHidMiniSample.DeviceOpen = DMF_VirtualHidMiniSample_Open;
    dmfCallbacksDmf_VirtualHidMiniSample.DeviceClose = DMF_VirtualHidMiniSample_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_VirtualHidMiniSample,
                                            VirtualHidMiniSample,
                                            DMF_CONTEXT_VirtualHidMiniSample,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_VirtualHidMiniSample.CallbacksDmf = &dmfCallbacksDmf_VirtualHidMiniSample;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_VirtualHidMiniSample,
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

// eof: Dmf_VirtualHidMiniSample.c
//
