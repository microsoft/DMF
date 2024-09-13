## DMF_EyeGazeIoctl

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module exposes a device interface that accepts IOCTLs which allow the sender to control eye gaze hardware. In this case, the
underlying device is a virtual HID eye gaze device.

This code based on code at this location: [Gaze HID](https://github.com/MSREnable/GazeHid/)

Please see that location for detailed information about eye gaze device support.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_EyeGazeIoctl
Client uses this structure to configure the Module specific parameters.

````
// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    USHORT VendorId;
    USHORT ProductId;
    USHORT VersionId;
} DMF_CONFIG_EyeGazeIoctl;
````
Member | Description
----|----
VendorId | Indicates the Vendor Id of the underlying virtual HID eye gaze device.
ProductId | Indicates the Product Id of the underlying virtual HID eye gaze device.
Version | Indicates the Version of the underlying virtual HID eye gaze device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* IOCTL_EYEGAZE_GAZE_DATA
Allows Client to send gaze coordinate data.

* IOCTL_EYEGAZE_CONFIGURATION_REPORT
Allows Client to send configuration information.

* IOCTL_EYEGAZE_CAPABILITIES_REPORT
Allows Client to set the capabilities of the device.

* IOCTL_EYEGAZE_CONTROL_REPORT
Allows the Client to control the device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* EyeGazeIoctl (sample driver)

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Hid

-----------------------------------------------------------------------------------------------------------------------------------

