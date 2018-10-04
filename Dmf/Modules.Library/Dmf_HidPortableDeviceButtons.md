## DMF_HidPortableDeviceButtons

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Exposes a virtual HID Portable Device Buttons device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_HidPortableDeviceButtons
// Client uses this structure to configure the Module specific parameters.
//
````
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
} DMF_CONFIG_HidPortableDeviceButtons;
````
Member | Description
----|----
VendorId | The vendor id of the virtual HID Portable Device Buttons device.
ProductId | The product id of the virtual HID Portable Device Buttons device.
VersionNumber | The version number of the virtual HID Portable Device Buttons device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### HidPortableDeviceButtons_ButtonIdType
````
typedef enum
{
  HidPortableDeviceButtons_ButtonId_Invalid,
  HidPortableDeviceButtons_ButtonId_Power,
  HidPortableDeviceButtons_ButtonId_VolumePlus,
  HidPortableDeviceButtons_ButtonId_VolumeMinus,
  // These are both not supported.
  //
  HidPortableDeviceButtons_ButtonId_Windows,
  HidPortableDeviceButtons_ButtonId_RotationLock,
  HidPortableDeviceButtons_ButtonId_Maximum
} HidPortableDeviceButtons_ButtonIdType;
````
Member | Description
----|----
HidPortableDeviceButtons_ButtonId_Power | "Power" button indicator.
HidPortableDeviceButtons_ButtonId_VolumePlus | "Volume +" button indicator.
HidPortableDeviceButtons_ButtonId_VolumeMinus | "Volume -" button indicator.
HidPortableDeviceButtons_ButtonId_Windows | "Windows" button indicator (not used).
HidPortableDeviceButtons_ButtonId_RotationLock | "Rotation Lock" button indicator (not used).

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

See MSDN documentation for how the VHF callbacks are used. Set the VhfClientContext member of this Module's Config to the
DMFMODULE of the Parent Module. Then the Parent Module can access its own Module Context in these callbacks.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidPortableDeviceButtons_ButtonIsEnabled

````
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_HidPortableDeviceButtons_ButtonIsEnabled(
  _In_ DMFMODULE DmfModule,
  _In_ HidPortableDeviceButtons_ButtonIdType ButtonId
  );
````

Allows the Client to query the state of a button (in response to an external query).

##### Returns

TRUE indicates the button is pressed "down". FALSE indicates the button is released in "up" state.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidPortableDeviceButtons Module handle.
ButtonId | The Id of the button for which the query is made.
Remarks | *

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HidPortableDeviceButtons_ButtonStateChange

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_HidPortableDeviceButtons_ButtonStateChange(
  _In_ DMFMODULE DmfModule,
  _In_ HidPortableDeviceButtons_ButtonIdType ButtonId,
  _In_ ULONG ButtonStateDown
  );
````

Allows the Client to indicate that the state of a button has changed.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidPortableDeviceButtons Module handle.
ButtonId | The Id of the button that has a state change.
ButtonStateDown | TRUE indicates the button is pressed "down". Otherwise, it is release in "up" state.
Remarks | *

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* IMPORTANT: Vhf.sys must be set as a Lower Filter driver in the Client driver's INF file using the "LowerFilters" registry entry. Otherwise, the VHF API is not available and this Module's Open callback will fail.

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

Hid

-----------------------------------------------------------------------------------------------------------------------------------

