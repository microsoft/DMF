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
  // Indicates if the client provides a InstanceId in the PdoRecord. TRUE if Client provides InstanceId. FALSE if Client wants SerialNumber to be used for InstanceId.
  //
  BOOLEAN InstanceIdProvidedByClient;
  // Format string used to format SerialNumber from the PdoRecord into InstanceId assigned to a PDO. Used if InstanceIdProvidedByClient is FALSE.
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
InstanceIdProvidedByClient | Indicates if the client provides a InstanceId in the PdoRecord. TRUE if Client provides InstanceId. FALSE if Client wants SerialNumber to be used for InstanceId. 
InstanceIdFormatString | Format string used to format SerialNumber from the PdoRecord into InstanceId assigned to a PDO. Used if InstanceIdProvidedByClient is FALSE.
DeviceLocation | The description of the bus to assign to all the PDOs that are created.
EvtPdoPnpCapabilities | Callback to get or set PnpCapabilities for each PDO that is created.
EvtPdoPowerCapabilities | Callback to get or set PowerCapabilities for each PDO that is created.
EvtPdoQueryInterfaceAdd | Callback to Add Device Query Interface for each PDO that is created.
EvtPdoHardwareIdFormat | Callback to format HardwareIds strings in PDO_RECORD, for each PDO that is created. If PdoRecords does not contain format strings in HardwareIds - this callback is not needed.
EvtPdoCompatibleIdFormat | Callback to format CompatibleIds strings in PDO_RECORD, for each PDO that is created. If PdoRecords does not contain format strings in CompatibleIds - this callback is not needed.

##### Remarks

* All strings must reside in global memory.
* The PDO table must reside in global memory.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------
##### Pdo_DevicePropertyEntry

Holds information for a single device property.

````
typedef struct
{
    // Device property that can be set by Client.
    //
    WDF_DEVICE_PROPERTY_DATA DevicePropertyData;
    
    // The property type.
    //
    DEVPROPTYPE ValueType;
    
    // The value data for this property.
    //
    VOID* ValueData;

    // The size of the value data.
    //
    ULONG ValueSize;
    
    // BOOL to specify if we should register the device interface GUID.
    //
    BOOLEAN RegisterDeviceInterface;

    // Device interface GUID that will be set on this property, so that
    // we can retrieve the properties at runtime with the CM API's.
    // 
    GUID* DeviceInterfaceGuid;
} Pdo_DevicePropertyEntry;
````

Member | Description
----|----
DevicePropertyData | See MSDN.
ValueType | See MSDN.
ValueData | Pointer to buffer that contains data to set.
ValueSize | Size of data pointed to by ValueData.
RegisterDeviceInterface | TRUE if a corresponding device interface should be created for this property.
DeviceInterfaceGuid | GUID of device interface to set if RegisterDeviceInterface is TRUE.

-----------------------------------------------------------------------------------------------------------------------------------
##### Pdo_DeviceProperty_Table

Indicates the properties to set when a child device is created.

````
typedef struct
{
    // The entries int he branch.
    //
    Pdo_DevicePropertyEntry* TableEntries;

    // The number of entries in the branch.
    //
    ULONG ItemCount;
} Pdo_DeviceProperty_Table;
````

Member | Description
----|----
TableEntries | Table of properties to set.
ItemCount | Number of entries in the table.

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
  // The table entry for this device's properties.
  //
  Pdo_DeviceProperty_Table* DeviceProperties;
  // Set this to true if the PDO has non-default address.
  //
  BOOLEAN UseAddress;
  // The address of the PDO that is to be created.
  //
  ULONG Address
  // Allows Client to allocate custom context.
  //
  WDF_OBJECT_ATTRIBUTES* CustomClientContext;
  // The InstanceId assigned to the PDO that is to be created.
  //
  UNICODE_STRING InstanceId;
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
DeviceProperties | The table entry for this device's properties.
UseAddress | Set this to true if the PDO has non-default address.
Address | The address of the PDO that is to be created.
CustomClientContext | Allows Client to assign a context type that is added to the PDO's WDFDEVICE WDF contexts. This context is private to the Client.
InstanceId | The InstanceId assigned to the PDO that is to be created. Client must set InstanceIdProvidedByClient to TRUE in Module's config to use this.

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
##### EVT_DMF_Pdo_PreCreate
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_Pdo_PreCreate(_In_ DMFMODULE DmfModule,
                      _In_ PWDFDEVICE_INIT DeviceInit,
                      _In_ PDMFDEVICE_INIT DmfDeviceInit,
                      _In_ PDO_RECORD* PdoRecord);
````

Allows the Client to set PDO properties that are assigned prior to the PDO's creation. For example, this callback
is used to assign the PDO's security attributes.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
DeviceInit | The PWDFDEVICE_INIT data used to create the PDO.
DmfDeviceInit | The PDMFDEVICE_INIT data used to create the PDO.
PdoRecord | The PDO record used to create the PDO.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Pdo_PostCreate
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_Pdo_PostCreate(_In_ DMFMODULE DmfModule,
                       _In_ WDFDEVICE ChildDevice,
                       _In_ PDMFDEVICE_INIT DmfDeviceInit,
                       _In_ PDO_RECORD* PdoRecord);
````

Allows the Client to set PDO properties that are assigned after the PDO's creation. For example, this callback
is used to initialize the Client specific PDO context prior to the PDO's static Module's being instantiated (so that
the Client specific context is initialized when the ModulesAdd() callback is called for the created PDO).

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle.
ChildDevice | The WDFDEVICE corresponding to the created PDO.
DmfDeviceInit | The PDMFDEVICE_INIT data used to create the PDO.
PdoRecord | The PDO record used to create the PDO.

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

##### DMF_Pdo_DeviceUnplugAll

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceUnplugAll(
  _In_ DMFMODULE DmfModule
  );
````

Unplugs and destroys any PDO that is a child of the WDFDEVICE corresponding to the given Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Pdo Module handle. (The given Module.)

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
HardwareId | Wide string representing Hardware Id of the PDO to be unplugged.
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
* DMF_Tests_Pdo

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

