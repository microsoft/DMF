/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_VirtualHidAmbientLightSensor.h

Abstract:

    Companion file to Dmf_VirtualHidAmbientLightSensor.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

// This code is only supported in Kernel-mode.
//
#if !defined(DMF_USER_MODE) && defined(NTDDI_WINTHRESHOLD) && (NTDDI_VERSION >= NTDDI_WINTHRESHOLD)

// It is the number of two column rows in the table.
//
#define VirtualHidAmbientLightSensor_MAXIMUM_NUMBER_OF_ALR_CURVE_RECORDS               24

// Input Report
//
#pragma pack(1)
typedef struct
{
    LONG Lux;
    UCHAR AlsSensorState;
    UCHAR AlsSensorEvent;
} VirtualHidAmbientLightSensor_ALS_INPUT_REPORT_DATA;
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
    USHORT ChangeSensitivityRelativePercentage;
    USHORT ChangeSensitivityAbsolute;
    ULONG ReportInterval;
    UCHAR MinimumReportInterval;
    SHORT AlrResponseCurve[VirtualHidAmbientLightSensor_MAXIMUM_NUMBER_OF_ALR_CURVE_RECORDS][2];
} VirtualHidAmbientLightSensor_ALS_FEATURE_REPORT_DATA;
#pragma pack()

typedef
_Function_class_(EVT_VirtualHidAmbientLightSensor_InputReportDataGet)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientLightSensor_InputReportDataGet(_In_ DMFMODULE DmfModule,
                                                    _Out_ VirtualHidAmbientLightSensor_ALS_INPUT_REPORT_DATA* InputReportData);

typedef
_Function_class_(EVT_VirtualHidAmbientLightSensor_FeatureReportDataGet)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientLightSensor_FeatureReportDataGet(_In_ DMFMODULE DmfModule,
                                                      _Out_ VirtualHidAmbientLightSensor_ALS_FEATURE_REPORT_DATA* FeatureReportData);

typedef
_Function_class_(EVT_VirtualHidAmbientLightSensor_FeatureReportDataSet)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientLightSensor_FeatureReportDataSet(_In_ DMFMODULE DmfModule,
                                                      _In_ VirtualHidAmbientLightSensor_ALS_FEATURE_REPORT_DATA* FeatureReportData);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Vendor Id of the virtual light sensor.
    //
    USHORT VendorId;
    // Product Id of the virtual light sensor.
    //
    USHORT ProductId;
    // Version number of the virtual light sensor.
    //
    USHORT VersionNumber;
    // Callbacks to get data from ALS hardware.
    // (These are what the HIDALS driver expects.)
    //
    EVT_VirtualHidAmbientLightSensor_InputReportDataGet* InputReportDataGet;
    EVT_VirtualHidAmbientLightSensor_FeatureReportDataGet* FeatureReportDataGet;
    EVT_VirtualHidAmbientLightSensor_FeatureReportDataSet* FeatureReportDataSet;
} DMF_CONFIG_VirtualHidAmbientLightSensor;

// This macro declares the following functions:
// DMF_VirtualHidAmbientLightSensor_ATTRIBUTES_INIT()
// DMF_CONFIG_VirtualHidAmbientLightSensor_AND_ATTRIBUTES_INIT()
// DMF_VirtualHidAmbientLightSensor_Create()
//
DECLARE_DMF_MODULE(VirtualHidAmbientLightSensor)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualHidAmbientLightSensor_LuxValueSend(
    _In_ DMFMODULE DmfModule,
    _In_ float LuxValue
    );

#endif // !defined(DMF_USER_MODE) && defined(NTDDI_WINTHRESHOLD) && (NTDDI_VERSION >= NTDDI_WINTHRESHOLD)

// eof: Dmf_VirtualHidAmbientLightSensor.h
//
