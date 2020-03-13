## DMF_Pdo

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Creates PDOs (Physical Device Objects) based on a table supplied during instantiation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_Pdo
````
typedef struct
{
  // The table of PDOs to create.
  //
  PDO_RECORD* PdoRecords;
  // Number of records in the above table.
  //
  ULONG PdoRecordCount;
  // Instance Id format string.
  //
  PWSTR InstanceIdFormatString;
  // Description of the bus where child devices are discovered.
  //
  PWSTR DeviceLocation;
  // Callback to get or set PnpCapabilities.
  //
  EVT_DMF_Pdo_DevicePnpCapabilities* EvtPdoPnpCapabilities;
  // Callback to get or set PowerCapabilities.
  //
  EVT_DMF_Pdo_DevicePowerCapabilities* EvtPdoPowerCapabilities;
  // Callback to Add Device Query Interface.
  //
  EVT_DMF_Pdo_DeviceQueryInterfaceAdd* EvtPdoQueryInterfaceAdd;
  // Callback to format HardwareIds strings.
  //
  EVT_DMF_Pdo_DeviceIdentifierFormat* EvtPdoHardwareIdFormat;
  // Callback to format CompatibleIds strings.
  //
  EVT_DMF_Pdo_DeviceIdentifierFormat* EvtPdoCompatibleIdFormat;
} DMF_CONFIG_Pdo;
````
Member | Description
----|----
PdoRecords | The table that lists all the PDOs that this Module should create.
PdoRecordCount | The number of table entries in PdoRecords.
InstanceIdFormatString | The Instance Id to assign to all the PDOs that are created.
DeviceLocation | The description of the bus to assign to all the PDOs that are created.
EvtPdoPnpCapabilities | Callback to get or set PnpCapabilities for each PDO that is created.
EvtPdoPowerCapabilities | Callback to get or set PowerCapabilities for each PDO that is created.
EvtPdoQueryInterfaceAdd | Callback to Add Device Query Interface for each PDO that is created.
EvtPdoHardwareIdFormat | Callback to format HardwareIds strings in PDO_RECORD, for each PDO that is created. If PdoRecords does not contain format strings in HardwareIds - this callback is not needed.
EvtPdoCompatibleIdFormat | Callback to format CompatibleIds strings in PDO_RECORD, for each PDO that is created. If PdoRecords does not contain format strings in CompatibleIds - this callback is not needed.

##### Remarks

* All strings must reside in global memory.
* That PDO table must reside in global memory.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------
##### PDO_RECORD
````
typedef struct
{
  // Array of Wide string Hardware Id of the PDO to be created.
  //
  PWSTR HardwareIds[PDO_RECORD_MAXIMUM_NUMBER_OF_HARDWARE_IDS];
  // Array of Wide string Compatible Id of the PDO to be created.
  //
  PWSTR CompatibleIds[PDO_RECORD_MAXIMUM_NUMBER_OF_COMPAT_IDS];
  // The number of strings in HardwareIds array.
  //
  USHORT HardwareIdsCount;
  // The number of strings in CompatibleIds array.
  //
  USHORT CompatibleIdsCount;
  // The Description of the PDO that is to be created.
  //
  PWSTR Description;
  // The Serial Number of the PDO that is to be created.
  //
  ULONG SerialNumber;
  // Callback to indicate if the PDO is actually required so it can be (determined
  // at runtime).
  //
  EVT_DMF_Pdo_IsPdoRequired* EvtPdoIsPdoRequired;
  // Set to TRUE if the PDO exposes a raw device.
  //
  BOOLEAN RawDevice;
  // Raw device GUID if the PDO exposes a raw device.
  //
  const GUID* RawDeviceClassGuid;
  // Indicates if the PDO will instantiate DMF Modules.
  //
  BOOLEAN EnableDmf;
  // The callback that instantiates DMF Modules, if applicable.
  //
  EVT_DMF_DEVICE_MODULES_ADD* EvtDmfDeviceModulesAdd;
} PDO_RECORD;
````
Member | Description
----|----
HardwareId | Array of Wide string Hardware Id of the PDO to be created. The strings can contain format strings that will be formatted by the client at PDO creation time using EvtPdoHardwareIdFormat callback.
CompatibleIds | Array of Wide string Compatible Id of the PDO to be created. The strings can contain format strings, that will be formatted by the client at PDO creation time using EvtPdoCompatibleIdFormat callback.
HardwareIdsCount | The number of strings in HardwareIds array.
CompatibleIdsCount | The number of strings in CompatibleIds array.
Description | The Description of the PDO that is to be created.
SerialNumber | The Serial Number of the PDO that is to be created.
EvtPdoIsPdoRequired | Callback to indicate if the PDO is actually required.
RawDevice | Set to TRUE if the PDO exposes a raw device.
RawDeviceClassGuid | Raw device GUID if the PDO exposes a raw device.
EnableDmf | Indicates if the PDO will instantiate DMF Modules.
EvtDmfDeviceModulesAdd | The callback that instantiates DMF Modules, if applicable.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Pdo_IsPdoRequired
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_Pdo_IsPdoRequired(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );
````

Allows the Client to indicate if the PDO is actually required so that the decision can be made at runtime.

##### Returns

TRUE if the PDO is required. FALSE otherwise.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
Previous State | Indicates the Client Driver's previous Power State.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Pdo_DevicePnpCapabilities
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Pdo_DevicePnpCapabilities(
    _In_ DMFMODULE DmfModule,
    _Out_ PWDF_DEVICE_PNP_CAPABILITIES PnpCapabilities
    );
````

Allows the Client to get or set PnpCapabilities for each PDO that is created.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
PnpCapabilities | PnpCapabilities that will be assigned to the PDO.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Pdo_DevicePowerCapabilities
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Pdo_DevicePowerCapabilities(
    _In_ DMFMODULE DmfModule,
    _Out_ PWDF_DEVICE_POWER_CAPABILITIES PowerCapabilities
    );
````

Allows the Client to get or set PowerCapabilities for each PDO that is created.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
PowerCapabilities | PowerCapabilities that will be assigned to the PDO.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Pdo_DeviceQueryInterfaceAdd
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_Pdo_DeviceQueryInterfaceAdd(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE PdoDevice
    );
````

Allows the Client to Add Device Query Interface for each PDO that is created.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
PdoDevice | WDFDEVICE handle for the new PDO.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Pdo_DeviceIdentifierFormat
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_Pdo_DeviceIdentifierFormat(
    _In_ DMFMODULE DmfModule,
    _Out_ LPWSTR FormattedIdBuffer,
    _In_ size_t BufferLength,
    _In_ LPCWSTR FormatString
    );
````

Allows Client to format strings defined in HardwareIds and CompatibleIds. This callback is called for each string in the
corresponding device identifier array of PDO_RECORD.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
FormattedIdBuffer | The output buffer to store the formatted device identifier string.
BufferLength | The length of FormattedIdBuffer buffer, in number of characters.
FormatString | A string specifying the format of the device identifier.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Pdo_DeviceEject

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceEject(
  _In_ DMFMODULE DmfModule,
  _In_ WDFDEVICE Device
  );
````

Eject and destroy a static PDO from the Client Driver's FDO.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
Device | PDO to be ejected.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Pdo_DeviceEjectUsingSerialNumber

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceEjectUsingSerialNumber(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG SerialNumber
  );
````

Eject and destroy a static PDO from the Client Driver's FDO.
PDO is identified by matching the provided serial number.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
SerialNumber | Serial number of the PDO to be ejected.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Pdo_DevicePlug

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DevicePlug(
  _In_ DMFMODULE DmfModule,
  _In_reads_(HardwareIdsCount) PWSTR HardwareIds[],
  _In_ USHORT HardwareIdsCount,
  _In_reads_opt_(CompatibleIdsCount) PWSTR CompatibleIds[],
  _In_ USHORT CompatibleIdsCount,
  _In_ PWSTR Description,
  _In_ ULONG SerialNumber,
  _Out_opt_ WDFDEVICE* Device
  );
````

Create and attach a static PDO to the Client Driver's FDO.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
HardwareIds | Array of Wide string Hardware Id of the new PDO.
HardwareIdsCount | The number of strings in HardwareIds array.
CompatibleIds | Array of Wide string Compatible Id of the new PDO.
CompatibleIdsCount | The number of strings in CompatibleIdsCount array.
Description | Device description of the new PDO.
SerialNumber | Serial number of the new PDO.
Device | The newly created PDO (optional).

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Pdo_DevicePlugEx

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DevicePlugEx(
    _In_ DMFMODULE DmfModule,
    _In_ PDO_RECORD* PdoRecord,
    _Out_opt_ WDFDEVICE* Device
    );

````

Create and attach a static PDO to the Client Driver's FDO. This version of the API allows the Client to create a
PDO that can instantiate DMF Modules.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
PdoRecord | Contains the parameters that define the characteristic of the PDO.
Device | The newly created PDO (optional).

##### Remarks

* This version allows the Client to create PDO that is able to instantiate Modules.
* This API is a superset of the non-Ex version.
* In addition to specifying the characteristics of the PDO, set the `EnableDmf` flag and the name of the callback
function that tells DMF what Modules to instantiate (if Modules are required).

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Pdo_DeviceUnplug

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceUnplug(
  _In_ DMFMODULE DmfModule,
  _In_ WDFDEVICE Device
  );
````

Unplug and destroy the given PDO from the Client Driver's FDO.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
Device | PDO to be unplugged.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Pdo_DeviceUnPlugEx

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceUnPlugEx(
    _In_ DMFMODULE DmfModule,
    _In_ PWSTR HardwareId,
    _In_ ULONG SerialNumber
    );

````

Unplug and destroy a static PDO from the Client Driver's FDO.
PDO is identified by matching the provided hardware ID and serial number.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
HardwareId | Wide string represenging Hardware Id of the PDO to be unplugged.
SerialNumber | Serial number of the PDO to be unplugged.

##### Remarks

* This version allows the Client to remove PDO based on HardwareID and SerialNumber.
* Useful when client bus driver is capable of enumerating child devices belonging to different device catogories 
having same serial number. 
-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Pdo_DeviceUnplugUsingSerialNumber

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceUnplugUsingSerialNumber(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG SerialNumber
  );
````

Unplug and destroy a static PDO from the Client Driver's FDO.
PDO is identified by matching the provided serial number.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
SerialNumber | Serial number of the PDO to be unplugged.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_ScheduledTask

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DMF_ToasterBus

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

