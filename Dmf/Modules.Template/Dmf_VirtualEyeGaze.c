/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualEyeGaze.c

Abstract:

    DMF version of Eye Gaze HID sample.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_VirtualEyeGaze.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef unsigned char HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

#include <pshpack1.h>

#pragma once

#define HID_USAGE_PAGE_EYE_HEAD_TRACKER             (0x0012)
#define HID_USAGE_PAGE_NAME_EYE_HEAD_TRACKER        "Eye and Head Trackers"

#define HID_USAGE_UNDEFINED                         (0x0000)        // Type
#define HID_USAGE_EYE_TRACKER                       (0x0001)        // CA
#define HID_USAGE_HEAD_TRACKER                      (0x0002)        // CA
//RESERVED                                          0x0003-0x000F

// HID_REPORT_ID List
#define HID_USAGE_TRACKING_DATA                     (0x10)        // CP
#define HID_USAGE_CAPABILITIES                      (0x11)        // CL
#define HID_USAGE_CONFIGURATION                     (0x12)        // CL
#define HID_USAGE_TRACKER_STATUS                    (0x13)        // CL
#define HID_USAGE_TRACKER_CONTROL                   (0x14)        // CL
//RESERVED                                          0x0015-0x001F

// HID_USAGE_TRACKING_DATA - Input Collection
#define HID_USAGE_TIMESTAMP                         (0x0020)        // DV
#define HID_USAGE_POSITION_X                        (0x0021)        // DV 
#define HID_USAGE_POSITION_Y                        (0x0022)        // DV
#define HID_USAGE_POSITION_Z                        (0x0023)        // DV
#define HID_USAGE_GAZE_LOCATION                     (0x0024)        // CP
#define HID_USAGE_LEFT_EYE_POSITION                 (0x0025)        // CP
#define HID_USAGE_RIGHT_EYE_POSITION                (0x0026)        // CP
#define HID_USAGE_HEAD_POSITION                     (0x0027)        // CP
#define HID_USAGE_ROTATION_ABOUT_X_AXIS             (0x0028)        // DV
#define HID_USAGE_ROTATION_ABOUT_Y_AXIS             (0x0029)        // DV
#define HID_USAGE_ROTATION_ABOUT_Z_AXIS             (0x002A)        // DV
//RESERVED                                          0x002B-0x00FF

// HID_USAGE_CAPABILITIES - Feature Collection 
#define HID_USAGE_TRACKER_QUALITY                   (0x0100)        // SV
#define HID_USAGE_MINIMUM_TRACKING_DISTANCE         (0x0101)        // SV
#define HID_USAGE_OPTIMUM_TRACKING_DISTANCE         (0x0102)        // SV
#define HID_USAGE_MAXIMUM_TRACKING_DISTANCE         (0x0103)        // SV
#define HID_USAGE_MAXIMUM_SCREEN_PLANE_WIDTH        (0x0104)        // SV
#define HID_USAGE_MAXIMUM_SCREEN_PLANE_HEIGHT       (0x0105)        // SV
//RESERVED                                          0x00106-0x01FF

// HID_USAGE_CONFIGURATION - Feature Collection 
#define HID_USAGE_DISPLAY_MANUFACTURER_ID           (0x0200)        // SV
#define HID_USAGE_DISPLAY_PRODUCT_ID                (0x0201)        // SV
#define HID_USAGE_DISPLAY_SERIAL_NUMBER             (0x0202)        // SV
#define HID_USAGE_DISPLAY_MANUFACTURER_DATE         (0x0203)        // SV
#define HID_USAGE_CALIBRATED_SCREEN_WIDTH           (0x0204)        // SV
#define HID_USAGE_CALIBRATED_SCREEN_HEIGHT          (0x0205)        // SV
//RESERVED                                          0x0204-0x02FF

// HID_USAGE_TRACKER_STATUS - Feature Collection 
#define HID_USAGE_SAMPLING_FREQUENCY                (0x0300)        // DV
#define HID_USAGE_CONFIGURATION_STATUS              (0x0301)        // DV
//RESERVED                                          0x0302-0x03FF

// HID_USAGE_TRACKER_CONTROL - Feature Collection 
#define HID_USAGE_MODE_REQUEST                      (0x0400)        // DV

// TODO: API Validator has to be turned off for the driver. This needs to be fixed.

#define TRACKER_QUALITY_RESERVED                    0
#define TRACKER_QUALITY_FINE_GAZE                   1

#define TRACKER_STATUS_RESERVED                     0
#define TRACKER_STATUS_READY                        1
#define TRACKER_STATUS_CONFIGURING                  2
#define TRACKER_STATUS_SCREEN_SETUP_NEEDED          3
#define TRACKER_STATUS_USER_CALIBRATION_NEEDED      4

#define MODE_REQUEST_ENABLE_GAZE_POINT              1
#define MODE_REQUEST_ENABLE_EYE_POSITION            2
#define MODE_REQUEST_ENABLE_HEAD_POSITION           3

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

typedef struct _CAPABILITIES_REPORT
{
    UCHAR         ReportId;
    UCHAR         TrackerQuality;
    ULONG        MinimumTrackingDistance;
    ULONG        OptimumTrackingDistance;
    ULONG        MaximumTrackingDistance;
    ULONG        MaximumScreenPlaneWidth;
    ULONG        MaximumScreenPlaneHeight;
} CAPABILITIES_REPORT, *PCAPABILITIES_REPORT;

typedef struct _CONFIGURATION_REPORT
{
    UCHAR         ReportId;
    UCHAR         Reserved;
    USHORT        DisplayManufacturerId;
    USHORT        DisplayProductId;
    ULONG        DisplaySerialNumber;
    USHORT        DisplayManufacturerDate;
    LONG         CalibratedScreenWidth;
    LONG         CalibratedScreenHeight;
} CONFIGURATION_REPORT, *PCONFIGURATION_REPORT;

typedef struct _TRACKER_STATUS_REPORT
{
    UCHAR         ReportId;
    UCHAR         Reserved;
    UCHAR         ConfigurationStatus;
    USHORT        SamplingFrequency;
} TRACKER_STATUS_REPORT, *PTRACKER_STATUS_REPORT;

typedef struct _TRACKER_CONTROL_REPORT
{
    UCHAR         ReportId;
    UCHAR         ModeRequest;
} TRACKER_CONTROL_REPORT, *PTRACKER_CONTROL_REPORT;

#include <poppack.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_VirtualEyeGaze
{
    // Underlying VHIDMINI2 support.
    //
    DMFMODULE DmfModuleVirtualHidMini;

    CAPABILITIES_REPORT CapabilitiesReport;
    CONFIGURATION_REPORT ConfigurationReport;
    TRACKER_STATUS_REPORT TrackerStatusReport;
    GAZE_REPORT GazeReport;
} DMF_CONTEXT_VirtualEyeGaze;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(VirtualEyeGaze)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(VirtualEyeGaze)

// MemoryTag.
//
#define MemoryTag 'mDHV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

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
#define VHIDMINI_DEVICE_STRING          L"UMDF Virtual hidmini device"  
#define VHIDMINI_DEVICE_STRING_INDEX    5

#define CONTROL_FEATURE_REPORT_ID   0x01

// TODO: Fix PID and VID
//
// These are the device attributes returned by the mini driver in response
// to IOCTL_HID_GET_DEVICE_ATTRIBUTES.
//
#define HIDMINI_PID             0xFEED
#define HIDMINI_VID             0xDEED
#define HIDMINI_VERSION         0x0101

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
g_VirtualEyeGaze_DefaultReportDescriptor[] = 
{
    HID_USAGE_PAGE(HID_USAGE_PAGE_EYE_HEAD_TRACKER),
    HID_USAGE(HID_USAGE_EYE_TRACKER),
    HID_BEGIN_APPLICATION_COLLECTION(),
#pragma region HID_USAGE_TRACKING_DATA
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_TRACKING_DATA),

            HID_LOGICAL_MINIMUM(0x00),
            HID_LOGICAL_MAXIMUM(0xFF),
            HID_REPORT_SIZE_UINT8(),
            HID_REPORT_COUNT(3),
            HID_INPUT_STATIC_VALUE(),

            HID_USAGE(HID_USAGE_TIMESTAMP),
            HID_REPORT_COUNT(8),
            HID_INPUT_DYNAMIC_VALUE(),

            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE_UINT32(),
            HID_LOGICAL_MINIMUM_DWORD(0x80000000),
            HID_LOGICAL_MAXIMUM_DWORD(0x7FFFFFFF),

            HID_USAGE(HID_USAGE_GAZE_LOCATION),
            HID_BEGIN_PHYSICAL_COLLECTION(),
                HID_REPORT_COUNT(2),
                HID_USAGE(HID_USAGE_POSITION_X),
                HID_USAGE(HID_USAGE_POSITION_Y),
                HID_INPUT_DYNAMIC_VALUE(),
            HID_END_COLLECTION_EX(),

            HID_USAGE(HID_USAGE_LEFT_EYE_POSITION),
            HID_BEGIN_PHYSICAL_COLLECTION(),
                HID_REPORT_COUNT(3),
                HID_USAGE(HID_USAGE_POSITION_X),
                HID_USAGE(HID_USAGE_POSITION_Y),
                HID_USAGE(HID_USAGE_POSITION_Z),
                HID_INPUT_DYNAMIC_VALUE(),
            HID_END_COLLECTION_EX(),

            HID_USAGE(HID_USAGE_RIGHT_EYE_POSITION),
            HID_BEGIN_PHYSICAL_COLLECTION(),
                HID_REPORT_COUNT(3),
                HID_USAGE(HID_USAGE_POSITION_X),
                HID_USAGE(HID_USAGE_POSITION_Y),
                HID_USAGE(HID_USAGE_POSITION_Z),
                HID_INPUT_DYNAMIC_VALUE(),
            HID_END_COLLECTION_EX(),
        HID_END_COLLECTION_EX(),
#pragma endregion
#pragma region HID_USAGE_CAPABILITIES
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_CAPABILITIES),

            HID_REPORT_SIZE_UINT8(),
            HID_REPORT_COUNT(1),
            HID_LOGICAL_MINIMUM(0x01),
            HID_LOGICAL_MAXIMUM(0x04),
            HID_USAGE_WORD(HID_USAGE_TRACKER_QUALITY),
            HID_FEATURE_STATIC_VALUE(),

            HID_REPORT_COUNT(5),
            HID_REPORT_SIZE_UINT32(),
            HID_LOGICAL_MINIMUM(0x00),
            HID_LOGICAL_MAXIMUM_DWORD(0x7FFFFFFF),

            HID_USAGE_WORD(HID_USAGE_MINIMUM_TRACKING_DISTANCE),
            HID_USAGE_WORD(HID_USAGE_OPTIMUM_TRACKING_DISTANCE),
            HID_USAGE_WORD(HID_USAGE_MAXIMUM_TRACKING_DISTANCE),
            HID_USAGE_WORD(HID_USAGE_MAXIMUM_SCREEN_PLANE_WIDTH),
            HID_USAGE_WORD(HID_USAGE_MAXIMUM_SCREEN_PLANE_HEIGHT),
            HID_FEATURE_STATIC_VALUE(),

            //HID_REPORT_SIZE_UINT32(),
            //HID_LOGICAL_MINIMUM_WORD(0x00000000),
            //HID_LOGICAL_MAXIMUM_WORD(0x7FFFFFFF),
        HID_END_COLLECTION_EX(),
#pragma endregion
#pragma region HID_USAGE_CONFIGURATION
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_CONFIGURATION),
            HID_REPORT_SIZE_UINT8(),
            HID_REPORT_COUNT(1),

            HID_FEATURE_STATIC_VALUE(),

            HID_REPORT_SIZE_UINT16(),
            HID_LOGICAL_MINIMUM(0x00),
            HID_LOGICAL_MAXIMUM_DWORD(0x0000FFFF),

            HID_USAGE_WORD(HID_USAGE_DISPLAY_MANUFACTURER_ID),
            HID_FEATURE_STATIC_VALUE(),

            HID_USAGE_WORD(HID_USAGE_DISPLAY_PRODUCT_ID),
            HID_FEATURE_STATIC_VALUE(),

            HID_REPORT_SIZE_UINT32(),
            HID_LOGICAL_MINIMUM(0x00),
            HID_LOGICAL_MAXIMUM_DWORD(0x7FFFFFFF),

            HID_USAGE_WORD(HID_USAGE_DISPLAY_SERIAL_NUMBER),
            HID_FEATURE_STATIC_VALUE(),

            HID_REPORT_SIZE_UINT16(),
            HID_LOGICAL_MINIMUM(0x00),
            HID_LOGICAL_MAXIMUM_DWORD(0x0000FFFF),

            HID_USAGE_WORD(HID_USAGE_DISPLAY_MANUFACTURER_DATE),
            HID_FEATURE_STATIC_VALUE(),

            HID_REPORT_SIZE_UINT32(),
            HID_REPORT_COUNT(2),
            HID_LOGICAL_MINIMUM(0x00),
            HID_LOGICAL_MAXIMUM_DWORD(0x7FFFFFFF),

            HID_USAGE_WORD(HID_USAGE_CALIBRATED_SCREEN_WIDTH),
            HID_USAGE_WORD(HID_USAGE_CALIBRATED_SCREEN_HEIGHT),
            HID_FEATURE_STATIC_VALUE(),

        HID_END_COLLECTION_EX(),
#pragma endregion
#pragma region HID_USAGE_TRACKER_STATUS
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_TRACKER_STATUS),

            HID_REPORT_SIZE_UINT8(),
            HID_REPORT_COUNT(1),
            HID_FEATURE_STATIC_VALUE(),

            HID_LOGICAL_MINIMUM(0x00),
            HID_LOGICAL_MAXIMUM(0x04),
            HID_USAGE_WORD(HID_USAGE_CONFIGURATION_STATUS),
            HID_FEATURE_DYNAMIC_VALUE(),

            HID_REPORT_SIZE_UINT16(),
            HID_LOGICAL_MINIMUM(0x00),
            HID_LOGICAL_MAXIMUM_WORD(0x7FFF),

            HID_USAGE_WORD(HID_USAGE_SAMPLING_FREQUENCY),
            HID_FEATURE_DYNAMIC_VALUE(),
        HID_END_COLLECTION_EX(),
#pragma endregion
#pragma region HID_USAGE_TRACKER_CONTROL
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_TRACKER_CONTROL),
            HID_REPORT_SIZE_UINT8(),
            HID_REPORT_COUNT(1),

            HID_REPORT_SIZE_UINT8(),
            HID_LOGICAL_MINIMUM(0x00),
            HID_LOGICAL_MAXIMUM(0x01),

            HID_USAGE_WORD(HID_USAGE_MODE_REQUEST),
            HID_FEATURE_DYNAMIC_VALUE(),
        HID_END_COLLECTION_EX(),
#pragma endregion
    HID_END_COLLECTION_EX()
};

// This is the default HID descriptor returned by the mini driver
// in response to IOCTL_HID_GET_DEVICE_DESCRIPTOR. The size
// of report descriptor is currently the size of g_DefaultReportDescriptor.
//
HID_DESCRIPTOR
g_VirtualEyeGaze_DefaultHidDescriptor = 
{
    0x09,   // length of HID descriptor
    0x21,   // descriptor type == HID  0x21
    0x0100, // hid spec release
    0x00,   // country code == Not Specified
    0x01,   // number of HID class descriptors
    {                                       //DescriptorList[0]
        0x22,                               //report descriptor type 0x22
        sizeof(g_VirtualEyeGaze_DefaultReportDescriptor)   //total length of report descriptor
    }
};

NTSTATUS
VirtualEyeGaze_WriteReport(
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualEyeGaze_WriteReport: unknown report id %d", Packet->reportId);
        goto Exit;
    }

    // Before touching buffer make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_OUTPUT_REPORT);

    if (Packet->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualEyeGaze_WriteReport: invalid input buffer. size %d, expect %d", Packet->reportBufferLen, reportSize);
        goto Exit;
    }

    outputReport = (HIDMINI_OUTPUT_REPORT*)Packet->reportBuffer;

    DMF_CONTEXT_VirtualEyeGaze* moduleContext;

    // Store the device data in the Module Context.
    //
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);
    //moduleContext->DeviceData = outputReport->Data;

    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

NTSTATUS
VirtualEyeGaze_GetFeature(
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
    UCHAR* reportData;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualEyeGaze_GetFeature fails: invalid report id %d", Packet->reportId);
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
                    "VirtualEyeGaze_GetFeature fails: output buffer too small. Size %d, expect %d",
                    Packet->reportBufferLen,
                    reportSize);
        goto Exit;
    }

    switch (Packet->reportId)
    {
        case HID_USAGE_CAPABILITIES:
            reportSize = sizeof(CAPABILITIES_REPORT);
            reportData = (PUCHAR)&moduleContext->CapabilitiesReport;
            break;
        case HID_USAGE_CONFIGURATION:
            reportSize = sizeof(CONFIGURATION_REPORT);
            reportData = (PUCHAR)&moduleContext->ConfigurationReport;
            break;
        case HID_USAGE_TRACKER_STATUS:
            reportSize = sizeof(TRACKER_STATUS_REPORT);
            reportData = (PUCHAR)&moduleContext->TrackerStatusReport;
            break;
        default:
            ntStatus = STATUS_INVALID_PARAMETER;
            KdPrint(("GetFeature: invalid report id %d\n", Packet->reportId));
            goto Exit;
    }

    memcpy(Packet->reportBuffer,
           reportData,
           reportSize);

    // Report how many bytes were written.
    //
    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

NTSTATUS
VirtualEyeGaze_SetFeature(
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
    TRACKER_CONTROL_REPORT* trackerControlReport;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
    DMFMODULE dmfModuleParent;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(ReportSize);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    if (Packet->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualEyeGaze_SetFeature fails: invalid report id %d", Packet->reportId);
        goto Exit;
    }

    // Before touching control code make sure buffer is big enough.
    //
    reportSize = sizeof(TRACKER_CONTROL_REPORT);

    if (Packet->reportBufferLen < reportSize) 
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualEyeGaze_SetFeature fails: invalid input buffer. size %d, expect %d",
                    Packet->reportBufferLen, reportSize);
        goto Exit;
    }

    trackerControlReport = (TRACKER_CONTROL_REPORT*)Packet->reportBuffer;

    // TODO: Handle mode request.
    //

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

NTSTATUS
VirtualEyeGaze_GetInputReport(
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
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualEyeGaze_GetInputReport fails: invalid report id %d", Packet->reportId);
        goto Exit;
    }

    reportSize = sizeof(HIDMINI_INPUT_REPORT);
    if (Packet->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualEyeGaze_GetInputReport fails: output buffer too small. Size %d, expect %d",
                    Packet->reportBufferLen,
                    reportSize);
        goto Exit;
    }

    reportBuffer = (HIDMINI_INPUT_REPORT*)(Packet->reportBuffer);

    reportBuffer->ReportId = CONTROL_COLLECTION_REPORT_ID;
    //reportBuffer->Data     = moduleContext->OutputReport;

    // Report how many bytes were copied.
    //
    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

NTSTATUS
VirtualEyeGaze_SetOutputReport(
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
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualEyeGaze_SetOutputReport fails: unknown report id %d", Packet->reportId);
        goto Exit;
    }

    // before touching buffer make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_OUTPUT_REPORT);

    if (Packet->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualEyeGaze_SetOutputReport fails: invalid input buffer. size %d, expect %d",
                    Packet->reportBufferLen,
                    reportSize);
        goto Exit;
    }

    reportBuffer = (HIDMINI_OUTPUT_REPORT*)Packet->reportBuffer;

    //moduleContext->OutputReport = reportBuffer->Data;

    // Report how many bytes were written.
    //
    *ReportSize = reportSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

NTSTATUS
VirtualEyeGaze_InputReportTrackerStatus(
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
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
    //HIDMINI_INPUT_REPORT* readReport;

    UNREFERENCED_PARAMETER(Request);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    //readReport = &moduleContext->ReadReport;

    // Populate data to return to caller.
    //
    //readReport->ReportId = CONTROL_FEATURE_REPORT_ID;
    //readReport->Data = moduleContext->DeviceData;

    // Return to caller.
    //
    *Buffer = (UCHAR*)&moduleContext->TrackerStatusReport;
    *BufferSize = sizeof(TRACKER_STATUS_REPORT);

    return STATUS_SUCCESS;
}

NTSTATUS
VirtualEyeGaze_InputReportGazeReport(
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
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
    //HIDMINI_INPUT_REPORT* readReport;

    UNREFERENCED_PARAMETER(Request);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    //readReport = &moduleContext->ReadReport;

    // Populate data to return to caller.
    //
    //readReport->ReportId = CONTROL_FEATURE_REPORT_ID;
    //readReport->Data = moduleContext->DeviceData;

    // Return to caller.
    //
    *Buffer = (UCHAR*)&moduleContext->GazeReport;
    *BufferSize = sizeof(GAZE_REPORT);

    return STATUS_SUCCESS;
}

#include <SetupAPI.h>
#include <initguid.h>
#include <cfgmgr32.h>
DEFINE_GUID(GUID_CLASS_MONITOR, 0x4d36e96e, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);

void
VirtualEyeGaze_PrimaryMonitorInfoGet(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
    
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    HDEVINFO devInfo = SetupDiGetClassDevsEx(&GUID_CLASS_MONITOR, NULL, NULL, DIGCF_PRESENT | DIGCF_PROFILE, NULL, NULL, NULL);
    if (NULL == devInfo)
    {
        return;
    }

    for (ULONG i = 0; ERROR_NO_MORE_ITEMS != GetLastError(); ++i)
    {
        SP_DEVINFO_DATA devInfoData;

        memset(&devInfoData, 0, sizeof(devInfoData));
        devInfoData.cbSize = sizeof(devInfoData);

        if (!SetupDiEnumDeviceInfo(devInfo, i, &devInfoData))
        {
            return;
        }
        TCHAR Instance[MAX_DEVICE_ID_LEN];
        if (!SetupDiGetDeviceInstanceId(devInfo, &devInfoData, Instance, MAX_PATH, NULL))
        {
            return;
        }

        HKEY hEDIDRegKey = SetupDiOpenDevRegKey(devInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

        if (!hEDIDRegKey || (hEDIDRegKey == INVALID_HANDLE_VALUE))
        {
            continue;
        }

        BYTE EDIDdata[1024];
        DWORD edidsize = sizeof(EDIDdata);

        if (ERROR_SUCCESS != RegQueryValueEx(hEDIDRegKey, L"EDID", NULL, NULL, EDIDdata, &edidsize))
        {
            RegCloseKey(hEDIDRegKey);
            continue;
        }
        moduleContext->ConfigurationReport.CalibratedScreenWidth = ((EDIDdata[68] & 0xF0) << 4) + EDIDdata[66];
        moduleContext->ConfigurationReport.CalibratedScreenHeight = ((EDIDdata[68] & 0x0F) << 8) + EDIDdata[67];

        RegCloseKey(hEDIDRegKey);

        // this only handles the case of the primary monitor
        break;
    }
    SetupDiDestroyDeviceInfoList(devInfo);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

PWSTR g_VirtualEyeGaze_Strings[] =
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
DMF_VirtualEyeGaze_ChildModulesAdd(
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
    DMF_CONFIG_VirtualEyeGaze* moduleConfig;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
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

    virtualHidDeviceMiniModuleConfig.HidDescriptor = &g_VirtualEyeGaze_DefaultHidDescriptor;
    virtualHidDeviceMiniModuleConfig.HidDescriptorLength = sizeof(g_VirtualEyeGaze_DefaultHidDescriptor);
    virtualHidDeviceMiniModuleConfig.HidReportDescriptor = g_VirtualEyeGaze_DefaultReportDescriptor;
    virtualHidDeviceMiniModuleConfig.HidReportDescriptorLength = sizeof(g_VirtualEyeGaze_DefaultReportDescriptor);

    // Set virtual device attributes.
    //
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.VendorID = HIDMINI_VID;
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.ProductID = HIDMINI_PID;
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.VersionNumber = HIDMINI_VERSION;
    virtualHidDeviceMiniModuleConfig.HidDeviceAttributes.Size = sizeof(HID_DEVICE_ATTRIBUTES);

    virtualHidDeviceMiniModuleConfig.GetInputReport = VirtualEyeGaze_GetInputReport;
    virtualHidDeviceMiniModuleConfig.GetFeature = VirtualEyeGaze_GetFeature;
    virtualHidDeviceMiniModuleConfig.SetFeature = VirtualEyeGaze_SetFeature;
    //virtualHidDeviceMiniModuleConfig.SetOutputReport = VirtualEyeGaze_SetOutputReport;
    virtualHidDeviceMiniModuleConfig.WriteReport = VirtualEyeGaze_WriteReport;

    virtualHidDeviceMiniModuleConfig.StringSizeCbManufacturer = sizeof(VHIDMINI_MANUFACTURER_STRING);
    virtualHidDeviceMiniModuleConfig.StringManufacturer = VHIDMINI_MANUFACTURER_STRING;
    virtualHidDeviceMiniModuleConfig.StringSizeCbProduct = sizeof(VHIDMINI_PRODUCT_STRING);
    virtualHidDeviceMiniModuleConfig.StringProduct = VHIDMINI_PRODUCT_STRING;
    virtualHidDeviceMiniModuleConfig.StringSizeCbSerialNumber = sizeof(VHIDMINI_SERIAL_NUMBER_STRING);
    virtualHidDeviceMiniModuleConfig.StringSerialNumber = VHIDMINI_SERIAL_NUMBER_STRING;

    virtualHidDeviceMiniModuleConfig.Strings = g_VirtualEyeGaze_Strings;
    virtualHidDeviceMiniModuleConfig.NumberOfStrings = ARRAYSIZE(g_VirtualEyeGaze_Strings);

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
DMF_VirtualEyeGaze_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type VirtualEyeGaze.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualEyeGaze* moduleConfig;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Initialize the device's data.
    // TODO: This should come from the Config.
    //
    moduleContext->CapabilitiesReport.ReportId = HID_USAGE_CAPABILITIES;
    moduleContext->CapabilitiesReport.TrackerQuality = TRACKER_QUALITY_FINE_GAZE;
    moduleContext->CapabilitiesReport.MinimumTrackingDistance = 50000;
    moduleContext->CapabilitiesReport.OptimumTrackingDistance = 65000;
    moduleContext->CapabilitiesReport.MaximumTrackingDistance = 90000;

    TRACKER_STATUS_REPORT* trackerStatus = &moduleContext->TrackerStatusReport;
    trackerStatus->ReportId = HID_USAGE_TRACKER_STATUS;
    trackerStatus->ConfigurationStatus = TRACKER_STATUS_RESERVED;

    VirtualEyeGaze_PrimaryMonitorInfoGet(DmfModule);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_VirtualEyeGaze_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type VirtualEyeGaze.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

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
DMF_VirtualEyeGaze_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type VirtualEyeGaze.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_VirtualEyeGaze;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_VirtualEyeGaze;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_VirtualEyeGaze);
    dmfCallbacksDmf_VirtualEyeGaze.ChildModulesAdd = DMF_VirtualEyeGaze_ChildModulesAdd;
    dmfCallbacksDmf_VirtualEyeGaze.DeviceOpen = DMF_VirtualEyeGaze_Open;
    dmfCallbacksDmf_VirtualEyeGaze.DeviceClose = DMF_VirtualEyeGaze_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_VirtualEyeGaze,
                                            VirtualEyeGaze,
                                            DMF_CONTEXT_VirtualEyeGaze,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_VirtualEyeGaze.CallbacksDmf = &dmfCallbacksDmf_VirtualEyeGaze;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_VirtualEyeGaze,
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

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualEyeGaze_GazeReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ GAZE_REPORT* GazeReport
    )
/*++

Routine Description:

    Send a gaze report.

Arguments:

    DmfModule - This Module's handle.
    GazeReport - The report to send.

Return Value:

    STATUS_SUCCESS if the key was toggled.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualEyeGaze* moduleConfig;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 VirtualEyeGaze);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlCopyMemory(&moduleContext->GazeReport,
                  GazeReport,
                  sizeof(GAZE_REPORT));

    // Tell Child Module to dequeue next pending request and call this Module's 
    // callback function to populate it.
    //
    ntStatus = DMF_VirtualHidMini_InputReportGenerate(moduleContext->DmfModuleVirtualHidMini,
                                                      VirtualEyeGaze_InputReportGazeReport);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualEyeGaze_TrackerStatusReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR TrackerStatus
    )
/*++

Routine Description:

    Send a gaze report.

Arguments:

    DmfModule - This Module's handle.
    GazeReport - The report to send.

Return Value:

    STATUS_SUCCESS if the key was toggled.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualEyeGaze* moduleConfig;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 VirtualEyeGaze);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleContext->TrackerStatusReport.ConfigurationStatus = TrackerStatus;

    // Tell Child Module to dequeue next pending request and call this Module's 
    // callback function to populate it.
    //
    ntStatus = DMF_VirtualHidMini_InputReportGenerate(moduleContext->DmfModuleVirtualHidMini,
                                                      VirtualEyeGaze_InputReportTrackerStatus);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_VirtualEyeGaze.c
//
