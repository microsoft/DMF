## DMF_HidTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module gives the Client access to a HID device. It allows the Client to specify the device the Client wants and has Methods
for communicating with the device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_HidTarget
````
typedef struct
{
  // The Vendor Id of the Device to open.
  //
  UINT16 VendorId;
  // List of HID PIDs that are supported by the Client.
  //
  UINT16 PidsOfDevicesToOpen[DMF_HidTarget_DEVICES_TO_OPEN];
  // Number of entries in the above array.
  //
  ULONG PidCount;
  // Information needed to select the device to open.
  //
  UINT16 VendorUsage;
  UINT16 VendorUsagePage;
  UINT8 ExpectedReportId;
  // Transaction Parameters.
  //
  ULONG ReadTimeoutMs;
  ULONG Retries;
  ULONG ReadTimeouSubsequenttMs;
  // Open in Read or Write mode.
  //
  ULONG OpenMode;
  // Share Access.
  //
  ULONG ShareAccess;
  // Input report call back.
  //
  EVT_DMF_HidTarget_InputReport* EvtHidInputReport;
  // Skip search for device and use the provided hid device explicitly.
  //
  BOOLEAN SkipHidDeviceEnumerationSearch;
  // Device to open if search is not to be done.
  //
  WDFDEVICE HidTargetToConnect;
  // Allow the client to select the desired device after the Module matches
  // a device, based on the look up criteria the client provided.
  //
  EVT_DMF_HidTarget_DeviceSelectionCallback* EvtHidTargetDeviceSelectionCallback;
} DMF_CONFIG_HidTarget;
````
Member | Description
----|----
VendorId | The Vendor Id of the HID device the Client wants to communicate with.
PidsOfDevicesToOpen | An array of the Product Ids of any of the devices the Client wants to communicate with.
PidCount | The number of entries in PidsOfDevicesToOpen.
VendorUsage | The Vendor Usage the Client needs to access. While searching for the device the Client wants to communicate with, the search function only opens the underlying HID device if this VendorUsage is supported.
VendorUsagePage | The Vendor Usage Page the Client needs to access. While searching for the device the Client wants to communicate with, the search function only opens the underlying HID device if this VendorUsagePage is supported.
ExpectedReportId | The Report Id the Client needs to access. While searching for the device the Client wants to communicate with, the search function only opens the underlying HID device if this ExpectedReportId is supported.
ReadTimeoutMs | Indicates the Read Timeout in milliseconds the instance of this Module uses when reading from the HID device.
Retries | Indicates how many retries the instance of this Module uses when reading from the HID device.
ReadTimeouSubsequenttMs | Indicates the Read Timeout in milliseconds the instance of this Module uses when reading from the HID device during retries.
OpenMode | Indicates if the HID device opened for Read, Write or both.
ShareAccess | Indicates if the HID device is opened in exclusive or shared mode.
EvtHidTargetInputReport | Allows the Client to populate the Input report buffer that the instance of this Module has created.
SkipHidDeviceEnumerationSearch | Indicates that this instance of the Module will not search for the HID device. Instead, a WDFIOTARGET will be passed using HidTargetToConnect.
HidTargetToConnect | The HID device to connect to when SkipHidDeviceEnumerationSearch is TRUE.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_HidTarget_InputReport
````
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
EVT_DMF_HidTarget_InputReport(
    _In_ DMFMODULE DmfModule,
    _In_reads_(BufferLength) UCHAR* Buffer,
    _In_ ULONG BufferLength
    );
````

Client specific callback that allows client to populate the HID input report.

##### Returns

None

##### Parameters
Parameter | Description
----|----
Buffer | The buffer the Client populates.
BufferLength | The size of Buffer in bytes.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_HidTarget_DeviceSelectionCallback
````
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
EVT_DMF_HidTarget_DeviceSelectionCallback(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING DevicePath,
    _In_ WDFIOTARGET IoTarget,
    _In_ PHIDP_PREPARSED_DATA PreparsedHidData,
    _In_ HID_COLLECTION_INFORMATION* HidCollectionInformation
    );
````

Allows the Client to select the exact target the Client wants to open.

##### Returns

TRUE if the Client wants to open the given WDFIOTARGET associated with the other passed parameters.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.
DevicePath | The Feature Id to send.
IoTarget | The given WDFIOTARGET.
PreparsedHidData | Allows the Client to access the HID API to determine more HID specific information about the given WDFIOTARGET.
HidCollectionInformation | Allows the Client to access the HID API to determine more HID specific information about the given WDFIOTARGET.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidTarget_BufferRead

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_BufferRead(
  _In_ DMFMODULE DmfModule,
  _Inout_updates_(BufferLength) VOID* Buffer,
  _In_ ULONG BufferLength,
  _In_ ULONG TimeoutMs
  );
````

Allows the Client to read data from the HID device connected the instance of this Module into a given buffer.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.
Buffer | The given buffer.
BufferLength | This size in bytes of Buffer.
BufferSize | The size of Buffer in bytes.
TimeoutMs | Indicates that the Read transaction should fail after TimeoutMs milliseconds. Set to zero to indicate there is no timeout.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidTarget_BufferWrite

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_BufferWrite(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* Buffer,
  _In_ ULONG BufferLength,
  _In_ ULONG TimeoutMs
  );
````

Allows the Client to write data to the HID device connected the instance of this Module from a given buffer.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.
Buffer | The given buffer.
BufferLength | This size in bytes of Buffer.
BufferSize | The size of Buffer in bytes.
TimeoutMs | Indicates that the Write transaction should fail after TimeoutMs milliseconds. Set to zero to indicate there is no timeout.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidTarget_FeatureGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_FeatureGet(
  _In_ DMFMODULE DmfModule,
  _In_ UCHAR FeatureId,
  _In_ UCHAR* Buffer,
  _In_ ULONG BufferSize,
  _In_ ULONG OffsetOfDataToCopy,
  _In_ ULONG NumberOfBytesToCopy
  );
````

Allows the Client to send a "Get Feature" command to the HID device connected the instance of this Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.
FeatureId | The Feature Id to send.
Buffer | The Client buffer that will receive data associated with FeatureId.
BufferSize | The size of Buffer in bytes.
OffsetOfDataToCopy | The offset in Buffer where received data is written.
NumberOfBytesToCopy | The number of bytes that should be received.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidTarget_FeatureSet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_FeatureSet(
  _In_ DMFMODULE DmfModule,
  _In_ UCHAR FeatureId,
  _In_ UCHAR* Buffer,
  _In_ ULONG BufferSize,
  _In_ ULONG OffsetOfDataToCopy,
  _In_ ULONG NumberOfBytesToCopy
  );
````

Allows the Client to send a "Set Feature" command to the HID device connected the instance of this Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.
FeatureId | The Feature Id to send.
Buffer | The Client buffer that contains the data to send to the device associated with FeatureId.
BufferSize | The size of Buffer in bytes.
OffsetOfDataToCopy | The offset in Buffer where the data to send begins.
NumberOfBytesToCopy | The number of bytes that should be sent.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidTarget_FeatureSetEx

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_FeatureSet(
  _In_ DMFMODULE DmfModule,
  _In_ UCHAR FeatureId,
  _In_ UCHAR* Buffer,
  _In_ ULONG BufferSize,
  _In_ ULONG OffsetOfDataToCopy,
  _In_ ULONG NumberOfBytesToCopy
  );
````

Allows the Client to send a "Set Feature" command to the HID device connected the instance of this Module.
Will search all 'data' report ids for the right one and use the corresponding size.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.
FeatureId | The Feature Id to send.
Buffer | The Client buffer that contains the data to send to the device associated with FeatureId.
BufferSize | The size of Buffer in bytes.
OffsetOfDataToCopy | The offset in Buffer where the data to send begins.
NumberOfBytesToCopy | The number of bytes that should be sent.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidTarget_InputRead

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_InputRead(
  _In_ DMFMODULE DmfModule
  );
````

Allows the Client to send a "Input Report Read" command to the HID device connected the instance of this Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidTarget_OutputReportSet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_OutputReportSet(
  _In_ DMFMODULE DmfModule,
  _In_ UCHAR* Buffer,
  _In_ ULONG BufferSize,
  _In_ ULONG TimeoutMs
  );
````

Allows the Client to send an Output report to the HID device connected the instance of this Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.
Buffer | The given buffer.
BufferLength | This size in bytes of Buffer.
BufferSize | The size of Buffer in bytes.
TimeoutMs | Indicates that the transaction should fail after TimeoutMs milliseconds. Set to zero to indicate there is no timeout.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidTarget_PreparsedDataGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_PreparsedDataGet(
  _In_ DMFMODULE DmfModule,
  _Out_ PHIDP_PREPARSED_DATA* PreparsedData
  );
````

Allows the Client to retrieve the HID Preparsed Data associated with the target device. The Client may then use this data to
determine more information about the target device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.
PreparsedData | The address where the address of the Preparsed data is written.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidTarget_ReportCreate

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_ReportCreate(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG ReportType,
  _In_ UCHAR ReportId,
  _Out_ WDFMEMORY* ReportMemory
  );
````

Creates a properly formed Report buffer with the given Report Type and Report Id and returns the WDFMEMORY associated with it.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidTarget Module handle.
ReportType | The given Report Type.
ReportId | The given Report Id.
ReportMemory | The WDFMEMORY for the newly created Report buffer.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Targets

-----------------------------------------------------------------------------------------------------------------------------------

