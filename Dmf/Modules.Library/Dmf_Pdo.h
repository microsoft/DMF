/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Pdo.h

Abstract:

    Companion file to Dmf_Pdo.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Holds information for a single device property.
//
typedef struct
{
    // The device property data that can be set on the SID driver.
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


// Holds information for a branch of registry entries which consist of one
// or more registry entries under a single key.
//
typedef struct
{
    // The entries int he branch.
    //
    Pdo_DevicePropertyEntry* TableEntries;

    // The number of entries in the branch.
    //
    ULONG ItemCount;
} Pdo_DeviceProperty_Table;

// Allows Client to indicate if the that is about to be created PDO is required.
//
typedef
_Function_class_(EVT_DMF_Pdo_IsPdoRequired)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_Pdo_IsPdoRequired(_In_ DMFMODULE DmfModule,
                          _In_ WDF_POWER_DEVICE_STATE PreviousState);

// Allows Client to set PnP Capabilities of PDO that is about to be created.
//
typedef
_Function_class_(EVT_DMF_Pdo_DevicePnpCapabilities)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Pdo_DevicePnpCapabilities(_In_ DMFMODULE DmfModule,
                                  _Out_ PWDF_DEVICE_PNP_CAPABILITIES PnpCapabilities);

// Allows Client to set Power Capabilities of PDO that is about to be created.
//
typedef
_Function_class_(EVT_DMF_Pdo_DevicePowerCapabilities)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Pdo_DevicePowerCapabilities(_In_ DMFMODULE DmfModule,
                                    _Out_ PWDF_DEVICE_POWER_CAPABILITIES PowerCapabilities);

// Allows Client to handle QueryInterfaceAdd.
//
typedef
_Function_class_(EVT_DMF_Pdo_DeviceQueryInterfaceAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_Pdo_DeviceQueryInterfaceAdd(_In_ DMFMODULE DmfModule,
                                    _In_ WDFDEVICE PdoDevice);

// Allows Client to format strings defined in HardwareIds and CompatibleIds.
//
typedef
_Function_class_(EVT_DMF_Pdo_DeviceIdentifierFormat)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_Pdo_DeviceIdentifierFormat(_In_ DMFMODULE DmfModule,
                                   _Out_ LPWSTR  FormattedIdBuffer,
                                   _In_  size_t  BufferLength,
                                   _In_  LPCWSTR FormatString);

// These cannot be greater than 64.
//
#define PDO_RECORD_MAXIMUM_NUMBER_OF_HARDWARE_IDS  (8)
#define PDO_RECORD_MAXIMUM_NUMBER_OF_COMPAT_IDS    (8)

// NOTE: The strings must be in global memory, not stack.
//
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
    // The callback function that instantiates DMF Modules, if applicable.
    //
    PFN_DMF_DEVICE_MODULES_ADD EvtDmfDeviceModulesAdd;

    // The table entry for this device's properties.
    //
    Pdo_DeviceProperty_Table* DeviceProperties;
} PDO_RECORD;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // The table of PDOs to create.
    // NOTE: The table must be in global memory, not stack.
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

// This macro declares the following functions:
// DMF_Pdo_ATTRIBUTES_INIT()
// DMF_CONFIG_Pdo_AND_ATTRIBUTES_INIT()
// DMF_Pdo_Create()
//
DECLARE_DMF_MODULE(Pdo)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceEject(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceEjectUsingSerialNumber(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG SerialNumber
    );

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

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DevicePlugEx(
    _In_ DMFMODULE DmfModule,
    _In_ PDO_RECORD* PdoRecord,
    _Out_opt_ WDFDEVICE* Device
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceUnplug(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceUnplugUsingSerialNumber(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG SerialNumber
    );

// eof: Dmf_Pdo.h
//
