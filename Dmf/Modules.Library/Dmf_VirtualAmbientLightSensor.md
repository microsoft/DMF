## DMF_VirtualHidAmbientLightSensor

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Exposes a virtual HID keyboard and methods to allow the keyboard to send keystrokes to the input stack.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
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
    // Determines how the keyboard is instantiated.
    //
    VirtualHidAmbientLightSensorModeType VirtualHidAmbientLightSensorMode;
} DMF_CONFIG_VirtualHidAmbientLightSensor;
````
Member | Description
----|----
VendorId | The vendor id of the virtual HID keyboard.
ProductId | The product id of the virtual HID keyboard.
VersionNumber | The version number of the virtual HID keyboard.
VirtualHidAmbientLightSensorMode | Indicates how the driver exposes the keyboard interface for other drivers, if at all.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### VirtualHidAmbientLightSensor_ButtonIdType
````
// Indicates how this driver types keystrokes.
//
typedef enum
{
    VirtualHidAmbientLightSensorMode_Invalid,
    // This driver types the keystrokes and does not expose this function to other drivers.
    //
    VirtualHidAmbientLightSensorMode_Standalone,
    // This driver types the keystrokes and exposes this function to other drivers.
    //
    VirtualHidAmbientLightSensorMode_Server,
    // This driver does not type keystrokes directly. It calls another driver to do that.
    //
    VirtualHidAmbientLightSensorMode_Client,
} VirtualHidAmbientLightSensorModeType;
````
Member | Description
----|----
VirtualHidAmbientLightSensorMode_Standalone | This driver types the keystrokes and does not expose this function to other drivers.
VirtualHidAmbientLightSensorMode_Server | This driver types the keystrokes and exposes this function to other drivers.
VirtualHidAmbientLightSensorMode_Client | This driver does not type keystrokes directly. It calls another driver to do that.

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

##### DMF_VirtualHidAmbientLightSensor_Type

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualHidAmbientLightSensor_Type(
    _In_ DMFMODULE DmfModule,
    _In_ PUSHORT const KeysToType,
    _In_ ULONG NumberOfKeys,
    _In_ USHORT UsagePage
    );
````

Type a series of keys using virtual keyboard.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientLightSensor Module handle.
KeysToType | Array of ids of keys to type.
NumberOfKeys | Number of entries in KeysToType.
UsagePage | HID_USAGE_PAGE_KEYBOARD or HID_USAGE_PAGE_CONSUMER. (See the HID report descriptor.)

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_VirtualHidAmbientLightSensor_Toggle

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_VirtualHidAmbientLightSensor_Toggle(
    _In_ DMFMODULE DmfModule,
    _In_ USHORT const KeyToToggle,
    _In_ USHORT UsagePage
    );
````

Toggles a single key on the HID keyboard.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_VirtualHidAmbientLightSensor Module handle.
KeyToToggle | The id of the key to toggle..
UsagePage | HID_USAGE_PAGE_KEYBOARD or HID_USAGE_PAGE_CONSUMER. (See the HID report descriptor.)

##### Remarks

* None

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

