## DMF_VirtualHidAmbientColorSensor

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Exposes a virtual HID Ambient Color Sensor (ACS) and methods to send lux data up the HID stack.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_VirtualHidAmbientColorSensor
````
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
````
Member | Description
----|----
VendorId | The vendor id of the virtual HID color sensor.
ProductId | The product id of the virtual HID color sensor.
VersionNumber | The version number of the virtual HID color sensor.
InputReportDataGet | Allows CLIENT to SET input report data.
FeatureReportDataGet | Allows CLIENT to SET feature report data.
FeatureReportDataSet | Allows CLIENT to GET feature report data.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

##### ACS_FEATURE_REPORT_DATA
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
    SHORT AcsResponseCurve[VirtualHidAmbientColorSensor_MAXIMUM_NUMBER_OF_ACS_CURVE_RECORDS][2];
} VirtualHidAmbientColorSensor_ACS_FEATURE_REPORT_DATA;
````
See the document, *HID-Sensors-Usages.docx*. This document describes these fields in detail.
Document is available here: https://docs.microsoft.com/en-us/windows-hardware/design/whitepapers/hid-sensors-usages

-----------------------------------------------------------------------------------------------------------------------------------
##### ACS_INPUT_REPORT_DATA
````
typedef struct
{
    LONG Illuminance;
    USHORT ChromaticityX;
    USHORT ChromaticityY;
    UCHAR AcsSensorState;
    UCHAR AcsSensorEvent;
} VirtualHidAmbientColorSensor_ACS_INPUT_REPORT_DATA;
````
See the document, *HID-Sensors-Usages.docx*. This document describes these fields in detail.
Document is available here: https://docs.microsoft.com/en-us/windows-hardware/design/whitepapers/hid-sensors-usages

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_VirtualHidAmbientColorSensor_FeatureReportDataGet
````
typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientColorSensor_FeatureReportDataGet(_In_ DMFMODULE DmfModule,
                                                      _Out_ VirtualHidAmbientColorSensor_ACS_FEATURE_REPORT_DATA* FeatureReportData);
````
This callback allows the Client to write Feature Report data.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientColorSensor Module handle.
FeatureReportData | The target buffer where Client writes ACS Feature Report Data.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_VirtualHidAmbientColorSensor_FeatureReportDataSet
````
typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientColorSensor_FeatureReportDataSet(_In_ DMFMODULE DmfModule,
                                                      _Out_ VirtualHidAmbientColorSensor_ACS_FEATURE_REPORT_DATA* FeatureReportData);
````
This callback allows the Client to read Feature Report data from the application. This data contains ACS configuration information.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientColorSensor Module handle.
FeatureReportData | The target buffer where Client reads ACS Feature Report Data.

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_VirtualHidAmbientColorSensor_InputReportDataGet
````
typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_VirtualHidAmbientColorSensor_InputReportDataGet(_In_ DMFMODULE DmfModule,
                                                    _Out_ VirtualHidAmbientColorSensor_ACS_INPUT_REPORT_DATA* InputReportData);
````
This callback allows the Client to write Input Report data. This data contains sensor state as well as current lux data.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientColorSensor Module handle.
InputReportData | The target buffer where Client writes ACS Input Report Data.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_VirtualHidAmbientColorSensor_LuxValueSend

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_VirtualHidAmbientColorSensor_AllValuesSend(
    _In_ DMFMODULE DmfModule,
    _In_ float Illuminance,
    _In_ float ChromaticityX,
    _In_ float ChromaticityY
    );
````

Allows the Client to send xyY (chromacity x/y, illuminance) data up the HID stack.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientColorSensor Module handle.
Illuminance | Illuminance data to send up the HID stack.
ChromaticityX | ChromaticityX data to send up the HID stack.
ChromaticityY | ChromaticityY data to send up the HID stack.

##### Remarks

* The Client uses this method to send the current illuminance and chromaticity values after an interrupt on the ACS has occurred indicating ambient color changed past current threshold.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* IMPORTANT: Vhf.sys must be set as a Lower Filter driver in the Client driver's INF file using the "LowerFilters" registry entry. Otherwise, the VHF API is not available and this Module's Open callback will fail.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_VirtualHidDeviceVhf

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DmfAcs Sample

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Hid

-----------------------------------------------------------------------------------------------------------------------------------

