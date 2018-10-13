/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

   Dmf_ComponentFirmwareUpdate.h

Abstract:

   Companion file to Dmf_ComponentFirmwareUpdate.c.

Environment:

   User-mode Driver Framework

--*/

#pragma once

// Client Driver callback function to provide the firmware offer/payload blob when being called.
//
typedef
_Function_class_(EVT_DMF_ComponentFirmwareUpdate_FirmwareGet)
_Must_inspect_result_
NTSTATUS
EVT_DMF_ComponentFirmwareUpdate_FirmwareGet(_In_ DMFMODULE DmfModule,
                                            _In_ DWORD FirmwareComponentIndex,
                                            _Out_ BYTE** FirmwareBuffer,
                                            _Out_ size_t* BufferLength);

// Maximum length of characters of the instance identifier if client provides one.
//
#define MAX_INSTANCE_IDENTIFIER_LENGTH 256

typedef enum
{
    ComponentFirmwareUpdate_InvalidTransportType,
    ComponentFirmwareUpdate_HidTransportType,
    ComponentFirmwareUpdate_BtleTransportType,
    ComponentFirmwareUpdate_MaximumTransportType
} ComponentFirmwareUpdate_TransportType;

typedef union _TRANSPORT_CONFIG_SELECTOR
{
    DMF_CONFIG_ComponentFirmwareUpdateHidTransport HidTransportConfig;
} TRANSPORT_CONFIG_SELECTOR;

// Configurations for Transport
//
typedef struct
{
    // Underlying Size of the Transport Config {for validation}
    //
    size_t Size;
    // Transport Type.
    //
    ComponentFirmwareUpdate_TransportType TransportType;
    // Currently Selected Transport.
    //
    TRANSPORT_CONFIG_SELECTOR SelectedTransportConfig;
} ComponentFirmwareUpdate_TRANSPORT_CONFIG;

// Configuration of the module
//
typedef struct
{
    // Transport Config.
    //
    ComponentFirmwareUpdate_TRANSPORT_CONFIG TransportConfig;

    //-----START: Firmware binary related ---------
    //
    //Number of Firmware binary pairs that this component needs to work with.
    //
    DWORD NumberOfFirmwareComponents;

    // ComponentFirmwareUpdate callback function to be implemented by client to provide the firmware offer bits.
    //
    EVT_DMF_ComponentFirmwareUpdate_FirmwareGet* EvtComponentFirmwareUpdateFirmwareOfferGet;

    // ComponentFirmwareUpdate callback function to be implemented by client to provide the firmware payload bits.
    //
    EVT_DMF_ComponentFirmwareUpdate_FirmwareGet* EvtComponentFirmwareUpdateFirmwarePayloadGet;

    // Firmware Buffer Attribute to control whether this Module maintains local copy of the firmware buffers internally or not.
    //
    BOOLEAN FirmwareBuffersNotInPresistantMemory;
    //-----END: Firmware binary related ---------
    //

    //---- START: CFU protocol related -------
    //
    // Does this component support resuming from a previously interrupted update?.
    //
    BOOLEAN SupportResumeOnConnect;

    // Does this configuration support skipping the entire protocol transaction for an already known all up-to-date Firmware?
    //
    BOOLEAN SupportProtocolTransactionSkipOptimization;

    // Request "a force immediate reset" during offer stage? (This is typically set for SELFHOST build).
    //
    BOOLEAN ForceImmediateReset;

    // Request "a force ignoring version" during offer stage? (This is typically set for DEBUG build).
    //
    BOOLEAN ForceIgnoreVersion;

    //----- END:  CFU protocol related -------
    //

    //---- START: Book keeping related  ------- (Optional)
    // Module updates registry with status information about the Firmware Update protocol stages.
    // If the below Identifier string is provided, Registry NameValue will be prefixed with this string.
    // This helps in external tools to distinguish status information for different instances under a device hardware key.
    //
    WCHAR InstanceIdentifier[MAX_INSTANCE_IDENTIFIER_LENGTH];

    // Should be 0 is client is not providing the string.
    //
    USHORT InstanceIdentifierLength;        // number of characters, excluding the terminal NULL.
    //
    //---- END: Book keeping related  -------
    //

} DMF_CONFIG_ComponentFirmwareUpdate;

// This macro declares the following functions:
// DMF_ComponentFirmwareUpdate_ATTRIBUTES_INIT()
// DMF_CONFIG_ComponentFirmwareUpdate_AND_ATTRIBUTES_INIT()
// DMF_ComponentFirmwareUpdate_Create()
//
DECLARE_DMF_MODULE(ComponentFirmwareUpdate)

VOID
FORCEINLINE
DMF_COMPONENT_FIRMWARE_UPDATE_CONFIG_INIT_TRANSPORT_HID(
    _Inout_ DMF_CONFIG_ComponentFirmwareUpdate* ProtocolConfig,
    _Out_ DMF_CONFIG_ComponentFirmwareUpdateHidTransport** HidTransportConfig
    )
{
    RtlZeroMemory(&ProtocolConfig->TransportConfig,
                  sizeof(ComponentFirmwareUpdate_TRANSPORT_CONFIG));

    ProtocolConfig->TransportConfig.Size = sizeof(DMF_CONFIG_ComponentFirmwareUpdateHidTransport);
    ProtocolConfig->TransportConfig.TransportType = ComponentFirmwareUpdate_HidTransportType;
    *HidTransportConfig = &ProtocolConfig->TransportConfig.SelectedTransportConfig.HidTransportConfig;
}

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_Start(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ComponentFirmwareUpdate_Stop(
    _In_ DMFMODULE DmfModule
    );

// eof: Dmf_ComponentFirmwareUpdate.h
//
