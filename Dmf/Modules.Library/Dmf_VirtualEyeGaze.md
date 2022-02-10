## DMF_VirtualEyeGaze

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Exposes a virtual HID eye gaze device and Methods to allow the device to send eye gaze data up the HID stack.

This code based on code at this location: [Gaze HID](https://github.com/MSREnable/GazeHid/)

Please see that location for detailed information about eye gaze device support.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_VirtualEyeGaze
````
// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Vendor Id of the eye gaze device.
    //
    USHORT VendorId;
    // Product Id of the eye gaze device.
    //
    USHORT ProductId;
    // Version number of the eye gaze device.
    //
    USHORT VersionNumber;
} DMF_CONFIG_VirtualEyeGaze;
````
Member | Description
----|----
VendorId | The vendor id of the virtual HID eye gaze device.
ProductId | The product id of the virtual HID eye gaze device.
VersionNumber | The version number of the virtual HID eye gaze device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

See MSDN documentation for how the VHF callbacks are used. Set the VhfClientContext member of this Module's Config to the
DMFMODULE of the Parent Module. Then the Parent Module can access its own Module Context in these callbacks.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_VirtualEyeGaze_ConfigurationDataSet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualEyeGaze_ConfigurationDataSet(
    _In_ DMFMODULE DmfModule,
    _In_ CONFIGURATION_DATA* ConfigurationData
    );

````

Allows the Client to set the configuration of the eye gaze device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualEyeGaze Module handle.
ConfigurationData | The Configuation Data to set.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_VirtualEyeGaze_CapabilitiesDataSet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualEyeGaze_CapabilitiesDataSet(
    _In_ DMFMODULE DmfModule,
    _In_ CAPABILITIES_DATA* CapabilitiesData
    );
````

Allows the Client to set the capabilities of the eye gaze device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualEyeGaze Module handle.
CapabilitiesData | The Capabilities Data to set.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_VirtualEyeGaze_GazeReportSend

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualEyeGaze_GazeReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ GAZE_DATA* GazeData
    );
````

Allows the Client to send eye gaze coordinate information up the HID stack.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualEyeGaze Module handle.
GazeData | Eye gaze coordinate information to send.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_VirtualEyeGaze_TrackerControlModeGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualEyeGaze_TrackerControlModeGet(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR* Mode
    );
````

Allows the Client read the current control mode of the eye gaze device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualEyeGaze Module handle.
Mode | The current control mode of the eye gaze device.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_VirtualEyeGaze_TrackerStatusReportSend

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualEyeGaze_TrackerStatusReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR TrackerStatus
    );
````

Allows the device send the current status of the eye gaze device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualEyeGaze Module handle.
TrackerStatus | The current status of the device.

-----------------------------------------------------------------------------------------------------------------------------------

##### Remarks

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

* EyeGazeIoctl (sample driver)

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Hid

-----------------------------------------------------------------------------------------------------------------------------------

