## DMF_VirtualHidMini

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module provides functionality from the VHIDMINI2 sample. This allows Client to create virtual HID device
in both Kernel-mode and User-mode.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_VirtualHidMini
Client uses this structure to configure the Module specific parameters.

````
typedef struct
{
    // Describe HID Device.
    // NOTE: In most cases this data is static memory so a pointer to that data
    //       is maintained. This prevents us from creating arbitrary size buffers.
    //
    USHORT VendorId;
    USHORT ProductId;
    USHORT VersionNumber;

    const HID_DESCRIPTOR* HidDescriptor;
    ULONG HidDescriptorLength;

    const UCHAR* HidReportDescriptor;
    ULONG HidReportDescriptorLength;

    HID_DEVICE_ATTRIBUTES HidDeviceAttributes;

    size_t StringSizeCbManufacturer;
    PWSTR StringManufacturer;
    size_t StringSizeCbProduct;
    PWSTR StringProduct;
    size_t StringSizeCbSerialNumber;
    PWSTR StringSerialNumber;

    PWSTR* Strings;
    ULONG NumberOfStrings;

    // Client callback handlers.
    //
    EVT_VirtualHidMini_WriteReport* WriteReport;
    EVT_VirtualHidMini_GetFeature* GetFeature;
    EVT_VirtualHidMini_SetFeature* SetFeature;
    EVT_VirtualHidMini_GetInputReport* GetInputReport;
    EVT_VirtualHidMini_SetOutputReport* SetOutputReport;
} DMF_CONFIG_VirtualHidMini;
````
Member | Description
----|----
VendorId | The vendor id of the virtual HID device.
ProductId | The product id of the virtual HID device.
VersionNumber | The version number of the virtual HID device.
HidDescriptor | The HID device descriptor of the Virtual Hid device.
HidDescriptorLength | The size in bytes of HidDescriptor.
HidReportDescriptor | The HID report descriptor of the Virtual Hid device.
HidDeviceAttributes | The HID device attributes of the Virtual Hid device.
StringSizeCbManufacturer | Size of device Manufacturer string in bytes.
StringManufacturer | Manufacturer string.
StringSizeCbProduct | Size of device Product string in bytes.
StringProduct | Product string.
StringSizeCbSerialNumber | Size of device Serial Number string in bytes.
StringSerialNumber | Serial Number string.
Strings | Pointer to array of device strings.
NumberOfStrings | Number of string in Strings array.
WriteReport | IOCTL_HID_WRITE_REPORT callback.
GetFeature | IOCTL_HID_GET_FEATURE callback.
SetFeature | IOCTL_HID_SET_FEATURE callback.
GetInputReport | IOCTL_HID_GET_INPUT_REPORT callback.
SetOutputReport | IOCTL_HID_SET_OUTPUT_REPORT callback.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_EVT_VirtualHidMini_GetFeature
````
NTSTATUS
EVT_VirtualHidMini_GetFeature(_In_ DMFMODULE DmfModule,
                              _In_ WDFREQUEST Request,
                              _In_ HID_XFER_PACKET* Packet,
                              _Out_ ULONG* ReportSize);
````

Allows the Client to act upon receipt of IOCTL_HID_GET_FEATURE.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidMini Module handle.
Request | The WDFREQUEST submitted by caller.
Packet | The HID_XFER_PACKET extracted from Request.
ReportSize | Size of the returned to the Client.

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_EVT_VirtualHidMini_GetInputReport
````
NTSTATUS
EVT_VirtualHidMini_GetInputReport(_In_ DMFMODULE DmfModule,
                                  _In_ WDFREQUEST Request,
                                  _In_ HID_XFER_PACKET* Packet,
                                  _Out_ ULONG* ReportSize);
````

Allows the Client to act upon receipt of IOCTL_HID_GET_INPUT_REPORT.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidMini Module handle.
Request | The WDFREQUEST submitted by caller.
Packet | The HID_XFER_PACKET extracted from Request.
ReportSize | Size of the returned to the Client.

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_EVT_VirtualHidMini_InputReportProcess
````
NTSTATUS
EVT_VirtualHidMini_InputReportProcess(_In_ DMFMODULE DmfModule,
                                      _In_ WDFREQUEST Request,
                                      _Out_ UCHAR** Buffer,
                                      _Out_ ULONG* BufferSize);
````

Allows the Client to populate the next input report in the queue.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidMini Module handle.
Request | The WDFREQUEST submitted by caller.
Buffer | Buffer returned by Client which contains data to write to input report.
BufferSize | Size of the data in Buffer in bytes.

##### Remarks

* Return `STATUS_PENDING` to indicate that Client holds on to request. In this case, Client must eventually call `DMF_VirtualHidMini_InputReportComplete`.

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_EVT_VirtualHidMini_SetFeature
````
NTSTATUS
EVT_VirtualHidMini_SetFeature(_In_ DMFMODULE DmfModule,
                              _In_ WDFREQUEST Request,
                              _In_ HID_XFER_PACKET* Packet,
                              _Out_ ULONG* ReportSize);
````

Allows the Client to act upon receipt of IOCTL_HID_SET_FEATURE.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidMini Module handle.
Request | The WDFREQUEST submitted by caller.
Packet | The HID_XFER_PACKET extracted from Request.
ReportSize | Size of the returned to the Client.

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_EVT_VirtualHidMini_SetOutputReport
````
NTSTATUS
EVT_VirtualHidMini_SetOutputReport(_In_ DMFMODULE DmfModule,
                                   _In_ WDFREQUEST Request,
                                   _In_ HID_XFER_PACKET* Packet,
                                   _Out_ ULONG* ReportSize);
````

Allows the Client to act upon receipt of IOCTL_HID_SET_OUTPUT_REPORT.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidMini Module handle.
Request | The WDFREQUEST submitted by caller.
Packet | The HID_XFER_PACKET extracted from Request.
ReportSize | Size of the returned to the Client.

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_EVT_VirtualHidMini_WriteReport
````
NTSTATUS
EVT_VirtualHidMini_WriteReport(_In_ DMFMODULE DmfModule,
                               _In_ WDFREQUEST Request,
                               _In_ HID_XFER_PACKET* Packet,
                               _Out_ ULONG* ReportSize);
````

Allows the Client to act upon receipt of IOCTL_HID_WRITE_REPORT.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidMini Module handle.
Request | The WDFREQUEST submitted by caller.
Packet | The HID_XFER_PACKET extracted from Request.
ReportSize | Size of the returned to the Client.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_VirtualHidMini_InputReportComplete

````
VOID
DMF_VirtualHidMini_InputReportComplete(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ UCHAR* ReadReport,
    _In_ ULONG ReadReportSize,
    _In_ NTSTATUS NtStatus
    );
````

Completes a given WDFREQUEST that the caller held pending from a call DMF_VirtualHidMini_InputReportGenerate()
using a given NTSTATUS as well as data. NOTE: Only use this Method if the call to DMF_VirtualHidMini_InputReportGenerate() returned STATUS_PENDING.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidMini Module handle.
Request | The Request for which STATUS_PENDING was previously returned.
ReadReport | The data buffer to return in the Request.
ReadReportSize | The size of the data buffer to return in the Request.
NtStatus | The NTSTATUS to return in the Request.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_VirtualHidMini_InputReportGenerate

````
NTSTATUS
DMF_VirtualHidMini_InputReportGenerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_VirtualHidMini_InputReportProcess* InputReportProcess
    );
````

Allows the Client to populate the next available a HID_READ_REPORT. This call causes the Client's `RetrieveNextInputReport` callback
to be called if a pending request is available.

##### Returns

STATUS_SUCCESS if a request was present and processed.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidMini Module handle.
InputReportProcess | Client callback that writes data to input report buffer.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* Client sets `InputReportProcess` based on the data that has just arrived and needs to be written.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DMF_VirtualHidMiniSample

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Add Client callbacks for more IOCTLs. (See TODO in code.)

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Hid

-----------------------------------------------------------------------------------------------------------------------------------

