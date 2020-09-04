/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_VirtualEyeGaze.c

Abstract:

    Support for creating a virtual keyboard device that "types" keys into the host.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_VirtualEyeGaze.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef unsigned char HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

#include <pshpack1.h>

#pragma region Eye Tracker HID Usages
// From HUTRR74 - https://www.usb.org/sites/default/files/hutrr74_-_usage_page_for_head_and_eye_trackers_0.pdf

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
#define HID_USAGE_HEAD_DIRECTION                    (0x0028)        // CP
#define HID_USAGE_ROTATION_ABOUT_X_AXIS             (0x0029)        // DV
#define HID_USAGE_ROTATION_ABOUT_Y_AXIS             (0x002A)        // DV
#define HID_USAGE_ROTATION_ABOUT_Z_AXIS             (0x002B)        // DV
//RESERVED                                          0x002C-0x00FF

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
#pragma endregion Eye Tracker HID Usages

#pragma region Eye Tracker HID Usage Constant Definitions
// TODO: API Validator has to be turned off for the driver. This needs to be fixed.

// Tracker Quality
#define TRACKER_QUALITY_RESERVED                    0
#define TRACKER_QUALITY_FINE_GAZE                   1

// Tracker Status
#define TRACKER_STATUS_RESERVED                     0
#define TRACKER_STATUS_READY                        1
#define TRACKER_STATUS_CONFIGURING                  2
#define TRACKER_STATUS_SCREEN_SETUP_NEEDED          3
#define TRACKER_STATUS_USER_CALIBRATION_NEEDED      4

// Device Mode Request
#define MODE_REQUEST_ENABLE_GAZE_POINT              1
#define MODE_REQUEST_ENABLE_EYE_POSITION            2
#define MODE_REQUEST_ENABLE_HEAD_POSITION           4
#pragma endregion Eye Tracker HID Usage Constant Definitions

typedef struct _GAZE_REPORT
{
    UCHAR ReportId;
    GAZE_DATA GazeData;
} GAZE_REPORT, *PGAZE_REPORT;

typedef struct _CAPABILITIES_REPORT
{
    UCHAR ReportId;
    UCHAR TrackerQuality;
    ULONG MinimumTrackingDistance;
    ULONG OptimumTrackingDistance;
    ULONG MaximumTrackingDistance;
    ULONG MaximumScreenPlaneWidth;
    ULONG MaximumScreenPlaneHeight;
} CAPABILITIES_REPORT, *PCAPABILITIES_REPORT;

typedef struct _CONFIGURATION_REPORT
{
    UCHAR ReportId;
    UCHAR Reserved;
    USHORT DisplayManufacturerId;
    USHORT DisplayProductId;
    ULONG DisplaySerialNumber;
    USHORT DisplayManufacturerDate;
    LONG CalibratedScreenWidth;
    LONG CalibratedScreenHeight;
} CONFIGURATION_REPORT, *PCONFIGURATION_REPORT;

typedef struct _TRACKER_STATUS_REPORT
{
    UCHAR ReportId;
    UCHAR Reserved;
    UCHAR ConfigurationStatus;
    USHORT SamplingFrequency;
} TRACKER_STATUS_REPORT, *PTRACKER_STATUS_REPORT;

typedef struct _TRACKER_CONTROL_REPORT
{
    UCHAR ReportId;
    UCHAR ModeRequest;
} TRACKER_CONTROL_REPORT, *PTRACKER_CONTROL_REPORT;

#include <poppack.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_VirtualEyeGaze
{
    // Virtual Hid Device via Vhf.
    //
    DMFMODULE DmfModuleVirtualHidDeviceVhf;

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
#define MemoryTag 'zgEV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include <pshpack1.h>

typedef struct _MY_DEVICE_ATTRIBUTES
{
    USHORT VendorID;
    USHORT ProductID;
    USHORT VersionNumber;
} MY_DEVICE_ATTRIBUTES;

#include <poppack.h>

typedef UCHAR HID_REPORT_DESCRIPTOR;

// This is the default report descriptor for the virtual Hid device returned
// by the mini driver in response to IOCTL_HID_GET_REPORT_DESCRIPTOR.
//
HID_REPORT_DESCRIPTOR
g_VirtualEyeGaze_HidReportDescriptor[] = 
{
    HID_USAGE_PAGE(HID_USAGE_PAGE_EYE_HEAD_TRACKER),
    HID_USAGE(HID_USAGE_EYE_TRACKER),
    HID_BEGIN_APPLICATION_COLLECTION(),
#pragma region HID_USAGE_TRACKING_DATA
        HID_USAGE(HID_USAGE_TRACKING_DATA),
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_TRACKING_DATA),

            HID_LOGICAL_MINIMUM_BYTE(0x00),
            HID_LOGICAL_MAXIMUM_WORD(0x00FF),
            HID_REPORT_SIZE_UINT8(),
            HID_REPORT_COUNT(7),
            HID_INPUT_STATIC_VALUE(),

            HID_USAGE(HID_USAGE_TIMESTAMP),
            HID_UNIT_WORD(0x1001),                      // SI Linear
            HID_UNIT_EXPONENT_BYTE(0x0A),               // -6
            HID_REPORT_COUNT(8),
            HID_INPUT_DYNAMIC_VALUE(),

            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE_UINT32(),
            HID_UNIT_BYTE(0x11),                        // Centimeter
            HID_UNIT_EXPONENT_BYTE(0x0C),               // -4, micrometers
            HID_LOGICAL_MINIMUM_DWORD(0xFFE17B80),      // -2000000
            HID_LOGICAL_MAXIMUM_DWORD(0x001E8480),      //  2000000

            HID_USAGE(HID_USAGE_GAZE_LOCATION),
            HID_BEGIN_PHYSICAL_COLLECTION(),
                HID_REPORT_COUNT(2),
                HID_USAGE(HID_USAGE_POSITION_X),
                HID_USAGE(HID_USAGE_POSITION_Y),
                HID_INPUT_STATIC_VALUE(),
            HID_END_COLLECTION_EX(),

            HID_USAGE(HID_USAGE_LEFT_EYE_POSITION),
            HID_BEGIN_PHYSICAL_COLLECTION(),
                HID_REPORT_COUNT(3),
                HID_USAGE(HID_USAGE_POSITION_X),
                HID_USAGE(HID_USAGE_POSITION_Y),
                HID_USAGE(HID_USAGE_POSITION_Z),
                HID_INPUT_STATIC_VALUE(),
            HID_END_COLLECTION_EX(),

            HID_USAGE(HID_USAGE_RIGHT_EYE_POSITION),
            HID_BEGIN_PHYSICAL_COLLECTION(),
                //HID_REPORT_COUNT(3),
                HID_USAGE(HID_USAGE_POSITION_X),
                HID_USAGE(HID_USAGE_POSITION_Y),
                HID_USAGE(HID_USAGE_POSITION_Z),
                HID_INPUT_STATIC_VALUE(),
            HID_END_COLLECTION_EX(),
        HID_END_COLLECTION_EX(),
#pragma endregion
#pragma region HID_USAGE_CAPABILITIES
        HID_USAGE(HID_USAGE_CAPABILITIES),
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_CAPABILITIES),

            HID_REPORT_SIZE_UINT8(),
            HID_REPORT_COUNT(1),
            HID_USAGE_WORD(HID_USAGE_TRACKER_QUALITY),
            HID_LOGICAL_MINIMUM_BYTE(0x00),
            HID_LOGICAL_MAXIMUM_BYTE(0x01),
            HID_UNIT_BYTE(0x00),                        // None
            HID_UNIT_EXPONENT_BYTE(0x00),               // 0
            HID_FEATURE_STATIC_VALUE(),

            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE_UINT16(),
            //HID_LOGICAL_MINIMUM_BYTE(0x00),
            HID_LOGICAL_MAXIMUM_DWORD(0x0000FFFF),
            HID_FEATURE_STATIC_VALUE(),

            HID_BEGIN_PHYSICAL_COLLECTION(),
                HID_REPORT_COUNT(5),
                HID_REPORT_SIZE_UINT32(),
                HID_LOGICAL_MINIMUM_DWORD(0xFFE17B80),      // -2000000
                HID_LOGICAL_MAXIMUM_DWORD(0x001E8480),      //  2000000
                HID_UNIT_BYTE(0x11),                        // Centimeter
                HID_UNIT_EXPONENT_BYTE(0x0C),               // -4, micrometers
                HID_USAGE_WORD(HID_USAGE_MINIMUM_TRACKING_DISTANCE),
                HID_USAGE_WORD(HID_USAGE_OPTIMUM_TRACKING_DISTANCE),
                HID_USAGE_WORD(HID_USAGE_MAXIMUM_TRACKING_DISTANCE),
                HID_USAGE_WORD(HID_USAGE_MAXIMUM_SCREEN_PLANE_WIDTH),
                HID_USAGE_WORD(HID_USAGE_MAXIMUM_SCREEN_PLANE_HEIGHT),
                HID_FEATURE_STATIC_VALUE(),
            HID_END_COLLECTION_EX(),
        HID_END_COLLECTION_EX(),
#pragma endregion
#pragma region HID_USAGE_CONFIGURATION
        HID_USAGE(HID_USAGE_CONFIGURATION),
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_CONFIGURATION),

            HID_REPORT_SIZE_UINT8(),
            HID_LOGICAL_MINIMUM_BYTE(0x00),
            HID_LOGICAL_MAXIMUM_WORD(0x00FF),
            HID_REPORT_COUNT(1),
            HID_FEATURE_STATIC_VALUE(),

            HID_REPORT_SIZE_UINT16(),
            HID_LOGICAL_MAXIMUM_DWORD(0x0000FFFF),
            HID_UNIT_BYTE(0x00),                        // None
            HID_UNIT_EXPONENT_BYTE(0x00),               // 0
            HID_USAGE_WORD(HID_USAGE_DISPLAY_MANUFACTURER_ID),
            HID_FEATURE_STATIC_VALUE(),

            HID_USAGE_WORD(HID_USAGE_DISPLAY_PRODUCT_ID),
            HID_FEATURE_STATIC_VALUE(),

            HID_REPORT_SIZE_UINT32(),
            HID_LOGICAL_MAXIMUM_DWORD(0x7FFFFFFF),
            HID_USAGE_WORD(HID_USAGE_DISPLAY_SERIAL_NUMBER),
            HID_FEATURE_STATIC_VALUE(),

            HID_REPORT_SIZE_UINT16(),
            //HID_LOGICAL_MINIMUM_BYTE(0x00),
            HID_LOGICAL_MAXIMUM_DWORD(0x0000FFFF),
            HID_USAGE_WORD(HID_USAGE_DISPLAY_MANUFACTURER_DATE),
            HID_FEATURE_STATIC_VALUE(),
                
            HID_BEGIN_PHYSICAL_COLLECTION(),
                HID_UNIT_BYTE(0x11),                        // Centimeter
                HID_UNIT_EXPONENT_BYTE(0x0C),               // -4, micrometers
                HID_LOGICAL_MAXIMUM_DWORD(0x7FFFFFFF),
                HID_REPORT_SIZE_UINT32(),
                HID_USAGE_WORD(HID_USAGE_CALIBRATED_SCREEN_WIDTH),
                HID_FEATURE_STATIC_VALUE(),

                HID_USAGE_WORD(HID_USAGE_CALIBRATED_SCREEN_HEIGHT),
                HID_FEATURE_STATIC_VALUE(),
            HID_END_COLLECTION_EX(),

        HID_END_COLLECTION_EX(),
#pragma endregion
#pragma region HID_USAGE_TRACKER_STATUS (Feature)
        HID_USAGE(HID_USAGE_TRACKER_STATUS),
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_TRACKER_STATUS),

            HID_REPORT_SIZE_UINT8(),
            HID_UNIT_BYTE(0x00),                        // None
            HID_UNIT_EXPONENT_BYTE(0x00),               // 0
            HID_LOGICAL_MAXIMUM_BYTE(0x04),
            HID_USAGE_WORD(HID_USAGE_CONFIGURATION_STATUS),
            HID_FEATURE_DYNAMIC_VALUE(),

            HID_REPORT_SIZE_UINT16(),
            HID_LOGICAL_MAXIMUM_DWORD(0x0000FFFF),
            HID_UNIT_WORD(0xF001),                      // SI Linear
            HID_UNIT_EXPONENT_BYTE(0x00),               // 0
            HID_USAGE_WORD(HID_USAGE_SAMPLING_FREQUENCY),
            HID_FEATURE_DYNAMIC_VALUE(),
        HID_END_COLLECTION_EX(),
#pragma endregion
#pragma region HID_USAGE_TRACKER_STATUS (Input)
        HID_USAGE(HID_USAGE_TRACKER_STATUS),
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_TRACKER_STATUS),

            HID_REPORT_SIZE_UINT8(),
            //HID_UNIT_BYTE(0x00),                        // None
            //HID_UNIT_EXPONENT_BYTE(0x00),               // 0
            HID_LOGICAL_MAXIMUM_BYTE(0x04),
            HID_USAGE_WORD(HID_USAGE_CONFIGURATION_STATUS),
            HID_INPUT_DYNAMIC_VALUE(),

            HID_REPORT_SIZE_UINT16(),
            HID_LOGICAL_MAXIMUM_DWORD(0x0000FFFF),
            HID_UNIT_WORD(0xF001),                      // SI Linear
            HID_UNIT_EXPONENT_BYTE(0x00),               // 0
            HID_USAGE_WORD(HID_USAGE_SAMPLING_FREQUENCY),
            HID_INPUT_DYNAMIC_VALUE(),
        HID_END_COLLECTION_EX(),
#pragma endregion
#pragma region HID_USAGE_TRACKER_CONTROL
        HID_USAGE(HID_USAGE_TRACKER_CONTROL),
        HID_BEGIN_LOGICAL_COLLECTION(),
            HID_REPORT_ID(HID_USAGE_TRACKER_CONTROL),

            HID_REPORT_SIZE_UINT8(),
            HID_LOGICAL_MAXIMUM_BYTE(0x07),
            HID_UNIT_BYTE(0x00),                        // None
            HID_UNIT_EXPONENT_BYTE(0x00),               // 0
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
g_VirtualEyeGaze_HidDescriptor = 
{
    0x09,   // length of HID descriptor
    0x21,   // descriptor type == HID  0x21
    0x0100, // hid spec release
    0x00,   // country code == Not Specified
    0x01,   // number of HID class descriptors
    {                                       //DescriptorList[0]
        0x22,                               //report descriptor type 0x22
        sizeof(g_VirtualEyeGaze_HidReportDescriptor)   //total length of report descriptor
    }
};

_Function_class_(EVT_VHF_ASYNC_OPERATION)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
VirtualEyeGaze_GET_FEATURE(
    _In_ PVOID VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ PVOID VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
{
    NTSTATUS ntStatus;
    ULONG reportSize;
    UCHAR* reportData;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
    DMFMODULE dmfModule;

    UNREFERENCED_PARAMETER(VhfOperationContext);

    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    dmfModule = DMFMODULEVOID_TO_MODULE(VhfClientContext);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    switch (HidTransferPacket->reportId)
    {
        case HID_USAGE_CAPABILITIES:
            reportSize = sizeof(CAPABILITIES_REPORT);
            reportData = (UCHAR*)&moduleContext->CapabilitiesReport;
            break;
        case HID_USAGE_CONFIGURATION:
            reportSize = sizeof(CONFIGURATION_REPORT);
            reportData = (UCHAR*)&moduleContext->ConfigurationReport;
            break;
        case HID_USAGE_TRACKER_STATUS:
            reportSize = sizeof(TRACKER_STATUS_REPORT);
            reportData = (UCHAR*)&moduleContext->TrackerStatusReport;
            break;
        default:
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
    }

    reportSize = sizeof(MY_DEVICE_ATTRIBUTES) + sizeof(HidTransferPacket->reportId);
    if (HidTransferPacket->reportBufferLen < reportSize) 
    {
        goto Exit;
    }

    if (ntStatus == STATUS_INVALID_PARAMETER)
    {
        goto Exit;
    }

    RtlCopyMemory(HidTransferPacket->reportBuffer,
                  reportData,
                  reportSize);

    ntStatus = STATUS_SUCCESS;

Exit:

    if (ntStatus != STATUS_PENDING)
    {
        DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                              VhfOperationHandle,
                                                              ntStatus);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

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

    DmfModule - This Module's handle.

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
    ntStatus = STATUS_SUCCESS;

    // Set default values that are overwritten by Client if necessary.
    //
    moduleContext->CapabilitiesReport.ReportId = HID_USAGE_CAPABILITIES;
    moduleContext->CapabilitiesReport.TrackerQuality = TRACKER_QUALITY_FINE_GAZE;
    moduleContext->CapabilitiesReport.MinimumTrackingDistance = 50000;
    moduleContext->CapabilitiesReport.OptimumTrackingDistance = 65000;
    moduleContext->CapabilitiesReport.MaximumTrackingDistance = 90000;

    TRACKER_STATUS_REPORT* trackerStatus = &moduleContext->TrackerStatusReport;
    trackerStatus->ReportId = HID_USAGE_TRACKER_STATUS;
    trackerStatus->ConfigurationStatus = TRACKER_STATUS_SCREEN_SETUP_NEEDED;

    ntStatus = DMF_VirtualEyeGaze_TrackerStatusReportSend(DmfModule,
                                                          TRACKER_STATUS_SCREEN_SETUP_NEEDED);

    // TODO: After we use User-mode VHF, get primary monitor information.
    //

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

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
    DMF_CONFIG_VirtualHidDeviceVhf virtualHidDeviceVhfModuleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // VirtualHidDeviceVhf
    // -------------------
    //
    DMF_CONFIG_VirtualHidDeviceVhf_AND_ATTRIBUTES_INIT(&virtualHidDeviceVhfModuleConfig,
                                                        &moduleAttributes);

    virtualHidDeviceVhfModuleConfig.VendorId = moduleConfig->VendorId;
    virtualHidDeviceVhfModuleConfig.ProductId = moduleConfig->ProductId;
    virtualHidDeviceVhfModuleConfig.VersionNumber = 0x0001;

    virtualHidDeviceVhfModuleConfig.HidDescriptor = &g_VirtualEyeGaze_HidDescriptor;
    virtualHidDeviceVhfModuleConfig.HidDescriptorLength = sizeof(g_VirtualEyeGaze_HidDescriptor);
    virtualHidDeviceVhfModuleConfig.HidReportDescriptor = g_VirtualEyeGaze_HidReportDescriptor;
    virtualHidDeviceVhfModuleConfig.HidReportDescriptorLength = sizeof(g_VirtualEyeGaze_HidReportDescriptor);

    // Set virtual device attributes.
    //
    virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.VendorID = moduleConfig->VendorId;
    virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.ProductID = moduleConfig->ProductId;
    virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.VersionNumber = moduleConfig->VersionNumber;
    virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.Size = sizeof(virtualHidDeviceVhfModuleConfig.HidDeviceAttributes);

    virtualHidDeviceVhfModuleConfig.StartOnOpen = TRUE;
    virtualHidDeviceVhfModuleConfig.VhfClientContext = DmfModule;

    virtualHidDeviceVhfModuleConfig.IoctlCallback_IOCTL_HID_GET_FEATURE = VirtualEyeGaze_GET_FEATURE;

    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleVirtualHidDeviceVhf);

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
    dmfCallbacksDmf_VirtualEyeGaze.DeviceOpen = DMF_VirtualEyeGaze_Open;
    dmfCallbacksDmf_VirtualEyeGaze.ChildModulesAdd = DMF_VirtualEyeGaze_ChildModulesAdd;

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
    }

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
    _In_ GAZE_DATA* GazeData
    )
/*++

Routine Description:

    Sends the given gaze data from Client to HID stack.

Arguments:

    DmfModule - This Module's handle.
    GazeData - The given gaze data.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
    HID_XFER_PACKET hidXferPacket;
    GAZE_REPORT inputReport;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 VirtualEyeGaze);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    inputReport.GazeData = *GazeData;
    // TODO: Perhaps pass this value in the Config so that Client can 
    //       configure it?
    //
    inputReport.ReportId = HID_USAGE_TRACKING_DATA;

    hidXferPacket.reportBuffer = (UCHAR*)&inputReport;
    hidXferPacket.reportBufferLen = sizeof(GAZE_REPORT);
    hidXferPacket.reportId = inputReport.ReportId;

    ntStatus = DMF_VirtualHidDeviceVhf_ReadReportSend(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                      &hidXferPacket);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualEyeGaze_PrimaryMonitorSettingsSet(
    _In_ DMFMODULE DmfModule,
    _In_ MONITOR_RESOLUTION* MonitorResolution
    )
/*++

Routine Description:

    Sets the given monitor resolution from Client.

Arguments:

    DmfModule - This Module's handle.
    MonitorResolution - The given monitor resolution.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 VirtualEyeGaze);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // TODO: Validate settings.
    //
    moduleContext->ConfigurationReport.CalibratedScreenHeight = MonitorResolution->Height;
    moduleContext->ConfigurationReport.CalibratedScreenWidth = MonitorResolution->Width;

    ntStatus = DMF_VirtualEyeGaze_TrackerStatusReportSend(DmfModule,
                                                          TRACKER_STATUS_READY);

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

    Sends the given tracker status from Client to HID stack.

Arguments:

    DmfModule - This Module's handle.
    TrackerStatus - The given tracker status.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualEyeGaze* moduleConfig;
    DMF_CONTEXT_VirtualEyeGaze* moduleContext;
    HID_XFER_PACKET hidXferPacket;
    TRACKER_STATUS_REPORT inputReport;

    DMFMODULE_VALIDATE_IN_METHOD_OPENING_OK(DmfModule,
                                            VirtualEyeGaze);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&inputReport,
                  sizeof(inputReport));
    inputReport.ConfigurationStatus = TrackerStatus;
    inputReport.ReportId = moduleContext->TrackerStatusReport.ReportId;

    hidXferPacket.reportBuffer = (UCHAR*)&inputReport;
    hidXferPacket.reportBufferLen = sizeof(TRACKER_STATUS_REPORT);
    hidXferPacket.reportId = inputReport.ReportId;

    ntStatus = DMF_VirtualHidDeviceVhf_ReadReportSend(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                      &hidXferPacket);

    return ntStatus;
}

// eof: Dmf_VirtualEyeGaze.c
//
