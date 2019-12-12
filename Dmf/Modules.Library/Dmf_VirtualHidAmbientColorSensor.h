/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_VirtualHidAmbientColorSensor.h

Abstract:

    Companion file to Dmf_VirtualHidAmbientColorSensor.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

// It is the number of two column rows in the table.
//
#define VirtualHidAmbientColorSensor_MAXIMUM_NUMBER_OF_ACS_CURVE_RECORDS               24

// Convert a float value to a HID report compatible float which is expressed as an unsigned int
// with 4 implied fixed decimal places. (via HID_UNIT_EXPONENT(0x0C))
//
// See section 4.2.1 in Hid Sensors Usages doc for an explination of how float values are
// expressed in HID reports.
// Doc can be found at http://msdn.microsoft.com/en-us/library/windows/hardware/hh975383
//
#define CONVERT_FLOAT_TO_HID_REPORT_USHORT(floatval)    ((USHORT) (floatval * 10000))

// MAXIMUM_ILLUMINANCITY_VALUE is the largest float value that can be converted to a LONG
//
#define MAXIMUM_ILLUMINANCITY_VALUE ((float) 2147483647.0)

// Input Report
//
#pragma pack(1)
typedef struct
{
    UCHAR ReportId;
    LONG Illuminance;
    USHORT ChromaticityX;
    USHORT ChromaticityY;
    UCHAR AcsSensorState;
    UCHAR AcsSensorEvent;
} VirtualHidAmbientColorSensor_ACS_INPUT_REPORT_DATA;
#pragma pack()

// Feature Report
//
#pragma pack(1)
typedef struct
{
    UCHAR ConnectionType;
    UCHAR ReportingState;
    UCHAR PowerState;
    UCHAR SensorState;
    ULONG ReportInterval;
    UINT32 MinimumReportInterval;
    USHORT SensitivityRelativePercentage;
    USHORT IlluminanceSensitivityAbsolute;
    USHORT ChromaSensitivityAbsolute;
    UCHAR AutoBrightnessPreferred;
    UCHAR AutoColorPreferred;
    SHORT UniqueId[32];
} VirtualHidAmbientColorSensor_ACS_FEATURE_REPORT_DATA;
#pragma pack()

typedef
_Function_class_(EVT_VirtualHidAmbientColorSensor_InputReportDataGet)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientColorSensor_InputReportDataGet(_In_ DMFMODULE DmfModule,
    _Out_ VirtualHidAmbientColorSensor_ACS_INPUT_REPORT_DATA* InputReportData);

typedef
_Function_class_(EVT_VirtualHidAmbientColorSensor_FeatureReportDataGet)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientColorSensor_FeatureReportDataGet(_In_ DMFMODULE DmfModule,
    _Out_ VirtualHidAmbientColorSensor_ACS_FEATURE_REPORT_DATA* FeatureReportData);

typedef
_Function_class_(EVT_VirtualHidAmbientColorSensor_FeatureReportDataSet)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientColorSensor_FeatureReportDataSet(_In_ DMFMODULE DmfModule,
    _In_ VirtualHidAmbientColorSensor_ACS_FEATURE_REPORT_DATA* FeatureReportData);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Vendor Id of the virtual color sensor.
    //
    USHORT VendorId;
    // Product Id of the virtual color sensor.
    //
    USHORT ProductId;
    // Version number of the virtual color sensor.
    //
    USHORT VersionNumber;
    // Callbacks to get data from ACS hardware.
    // (These are what the HIDACS driver expects.)
    //
    EVT_VirtualHidAmbientColorSensor_InputReportDataGet* InputReportDataGet;
    EVT_VirtualHidAmbientColorSensor_FeatureReportDataGet* FeatureReportDataGet;
    EVT_VirtualHidAmbientColorSensor_FeatureReportDataSet* FeatureReportDataSet;
} DMF_CONFIG_VirtualHidAmbientColorSensor;

// This macro declares the following functions:
// DMF_VirtualHidAmbientColorSensor_ATTRIBUTES_INIT()
// DMF_CONFIG_VirtualHidAmbientColorSensor_AND_ATTRIBUTES_INIT()
// DMF_VirtualHidAmbientColorSensor_Create()
//
DECLARE_DMF_MODULE(VirtualHidAmbientColorSensor)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_VirtualHidAmbientColorSensor_AllValuesSend(
    _In_ DMFMODULE DmfModule,
    _In_ float Illuminance,
    _In_ float ChromaticityX,
    _In_ float ChromaticityY
    );

// eof: Dmf_VirtualHidAmbientColorSensor.h
//

