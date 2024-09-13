## DMF_VirtualHidAmbientLightSensor

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

Exposes a virtual HID Ambient Light Sensor (ALS) and methods to send lux data up the HID stack.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_VirtualHidAmbientLightSensor
````
// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Vendor Id of the virtual keyboard.
    //
    USHORT VendorId;
    // Product Id of the virtual keyboard.
    //
    USHORT ProductId;
    // Version number of the virtual keyboard.
    //
    USHORT VersionNumber;
    // Callbacks to get data from ALS hardware.
    // (These are what the HIDALS driver expects.)
    //
    EVT_VirtualHidAmbientLightSensor_InputReportDataGet* InputReportDataGet;
    EVT_VirtualHidAmbientLightSensor_FeatureReportDataGet* FeatureReportDataGet;
    EVT_VirtualHidAmbientLightSensor_FeatureReportDataSet* FeatureReportDataSet;
} DMF_CONFIG_VirtualHidAmbientLightSensor;
````
Member | Description
----|----
VendorId | The vendor id of the virtual HID keyboard.
ProductId | The product id of the virtual HID keyboard.
VersionNumber | The version number of the virtual HID keyboard.
InputReportDataGet | Allows CLIENT to SET input report data.
FeatureReportDataGet | Allows CLIENT to SET feature report data.
FeatureReportDataSet | Allows CLIENT to GET feature report data.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

##### ALS_FEATURE_REPORT_DATA
````
typedef struct
{
    UCHAR ConnectionType;
    UCHAR ReportingState;
    UCHAR PowerState;
    UCHAR SensorState;
    USHORT ChangeSensitivity;
    ULONG ReportInterval;
    UCHAR MinimumReportInterval;
    SHORT AlrResponseCurve[VirtualHidAmbientLightSensor_MAXIMUM_NUMBER_OF_ALR_CURVE_RECORDS][2];
} VirtualHidAmbientLightSensor_ALS_FEATURE_REPORT_DATA;
````
See the document, *HID-Sensors-Usages.docx*. This document describes these fields in detail.
Document is available here: https://docs.microsoft.com/en-us/windows-hardware/design/whitepapers/hid-sensors-usages

##### ALS_INPUT_REPORT_DATA
````
typedef struct
{
    LONG Lux;
    UCHAR AlsSensorState;
    UCHAR AlsSensorEvent;
} VirtualHidAmbientLightSensor_ALS_INPUT_REPORT_DATA;
````
See the document, *HID-Sensors-Usages.docx*. This document describes these fields in detail.
Document is available here: https://docs.microsoft.com/en-us/windows-hardware/design/whitepapers/hid-sensors-usages

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_VirtualHidAmbientLightSensor_FeatureReportDataGet
````
typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientLightSensor_FeatureReportDataGet(_In_ DMFMODULE DmfModule,
                                                      _Out_ VirtualHidAmbientLightSensor_ALS_FEATURE_REPORT_DATA* FeatureReportData);
````
This callback allows the Client to write Feature Report data.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientLightSensor Module handle.
FeatureReportData | The target buffer where Client writes ALS Feature Report Data.

##### EVT_VirtualHidAmbientLightSensor_FeatureReportDataSet
````
typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientLightSensor_FeatureReportDataSet(_In_ DMFMODULE DmfModule,
                                                      _Out_ VirtualHidAmbientLightSensor_ALS_FEATURE_REPORT_DATA* FeatureReportData);
````
This callback allows the Client to read Feature Report data from the application. This data contains ALS configuration information.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientLightSensor Module handle.
FeatureReportData | The target buffer where Client reads ALS Feature Report Data.

##### EVT_VirtualHidAmbientLightSensor_InputReportDataGet
````
typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientLightSensor_InputReportDataGet(_In_ DMFMODULE DmfModule,
                                                    _Out_ VirtualHidAmbientLightSensor_ALS_INPUT_REPORT_DATA* InputReportData);
````
This callback allows the Client to write Input Report data. This data contains sensor state as well as current lux data.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientLightSensor Module handle.
InputReportData | The target buffer where Client writes ALS Input Report Data.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_VirtualHidAmbientLightSensor_LuxValueSend

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualHidAmbientLightSensor_LuxValueSend(
    _In_ DMFMODULE DmfModule,
    _In_ float LuxValue
    );
````

Allows the Client to send Lux data up the HID stack.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientLightSensor Module handle.
LuxValue | Lux data to send up the HID stack.

##### Remarks

* The Client uses this method to send the current lux value read after an interrupt on the ALS has occurred indicating ambient light changed past current threshold.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* IMPORTANT: Vhf.sys must be set as a Lower Filter driver in the Client driver's INF file using the "LowerFilters" registry entry. Otherwise, the VHF API is not available and this Module's Open callback will fail.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DmfAls Sample

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Hid

-----------------------------------------------------------------------------------------------------------------------------------

