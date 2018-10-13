/*++

    Copyright (c) Microsoft Corporation. All Rights Reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Transport_ComponentFirmwareUpdate.h

Abstract:

    Contract between Dmf_ComponentFirmwareUpdate and it's Transport.

Environment:

    User-mode Driver Framework

--*/

#pragma once

////////////////////////
//  Enum definitions  //
////////////////////////


typedef enum _COMPONENT_FIRMWARE_UPDATE_OFFER_INFORMATION_CODE
{
    COMPONENT_FIRMWARE_UPDATE_OFFER_INFO_START_ENTIRE_TRANSACTION = 0x00, // To indicate that the Windows driver is new, or has been reloaded, and the entire offer processing is (re)starting. 
    COMPONENT_FIRMWARE_UPDATE_OFFER_INFO_START_OFFER_LIST = 0x01,         // Indicates the beginning of the Offer list from the Windows driver, in case the Accessory has download rules associated with ensuring one subcomponent is updated prior to another subcomponent in the system.
    COMPONENT_FIRMWARE_UPDATE_OFFER_INFO_END_OFFER_LIST = 0x02,           // Indicates the end of the Offer list from the Windows driver. 
} COMPONENT_FIRMWARE_UPDATE_OFFER_INFORMATION_CODE;

typedef enum _COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_CODE
{
    COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_NOTIFY_ON_READY = 0x01,       // Issued by the host when the offer has previously been rejected via COMPONENT_FIRMWARE_UPDATE_OFFER_BUSY response from the device. The Accepted response for this will pend from the device until the device is no longer busy. 
} COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_CODE;

typedef enum _COMPONENT_FIRMWARE_UPDATE_PAYLOAD_RESPONSE
{
    COMPONENT_FIRMWARE_UPDATE_SUCCESS = 0x00,                             // No Error, the requested function(s) succeeded.
    COMPONENT_FIRMWARE_UPDATE_ERROR_PREPARE = 0x01,                       // Could not either: 1) Erase the upper block; 2) Initialize the swap command scratch block; 3) Copy the configuration data to the upper block.
    COMPONENT_FIRMWARE_UPDATE_ERROR_WRITE = 0x02,                         // Could not write the bytes.
    COMPONENT_FIRMWARE_UPDATE_ERROR_COMPLETE = 0x03,                      // Could not set up the swap, in response to COMPONENT_FIRMWARE_UPDATE_FLAG_LAST_BLOCK.
    COMPONENT_FIRMWARE_UPDATE_ERROR_VERIFY = 0x04,                        // Verification of the DWord failed, in response to COMPONENT_FIRMWARE_UPDATE_FLAG_VERIFY.
    COMPONENT_FIRMWARE_UPDATE_ERROR_CRC = 0x05,                           // CRC of the image failed, in response to COMPONENT_FIRMWARE_UPDATE_FLAG_LAST_BLOCK.
    COMPONENT_FIRMWARE_UPDATE_ERROR_SIGNATURE = 0x06,                     // Firmware signature verification failed, in response to COMPONENT_FIRMWARE_UPDATE_FLAG_LAST_BLOCK.
    COMPONENT_FIRMWARE_UPDATE_ERROR_VERSION = 0x07,                       // Firmware version verification failed, in response to COMPONENT_FIRMWARE_UPDATE_FLAG_LAST_BLOCK.
    COMPONENT_FIRMWARE_UPDATE_ERROR_SWAP_PENDING = 0x08,                  // Firmware has already been updated and a swap is pending.  No further Firmware Update commands can be accepted until the device has been reset.
    COMPONENT_FIRMWARE_UPDATE_ERROR_INVALID_ADDR = 0x09,                  // Firmware has detected an invalid destination address within the message data content.
    COMPONENT_FIRMWARE_UPDATE_ERROR_NO_OFFER = 0x0A,                      // The Firmware Update Content Command was received without first receiving a valid & accepted FW Update Offer. 
    COMPONENT_FIRMWARE_UPDATE_ERROR_INVALID = 0x0B                        // General error for the Firmware Update Content command, such as an invalid applicable Data Length.  
} COMPONENT_FIRMWARE_UPDATE_PAYLOAD_RESPONSE;

typedef enum _COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE
{
    COMPONENT_FIRMWARE_UPDATE_OFFER_SKIP = 0x00,                          // The offer needs to be skipped at this time, indicating to the host to please offer again during next applicable period.
    COMPONENT_FIRMWARE_UPDATE_OFFER_ACCEPT = 0x01,                        // If the update applies, Accept is returned.
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT = 0x02,                        // If the update does not apply, a Reject is returned.
    COMPONENT_FIRMWARE_UPDATE_OFFER_BUSY = 0x03,                          // The offer needs to be delayed at this time.  The device has nowhere to put the incoming blob.
    COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_READY = 0x04,                 // Used with the Offer Other response for the OFFER_NOTIFY_ON_READY request, when the Accessory is ready to accept additional Offers.

    COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_NOT_SUPPORTED = 0xFF
} COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE;

typedef enum _COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON
{
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_OLD_FW = 0x00,                 // The offer was rejected by the product due to the offer version being older than the currently downloaded/existing firmware. 
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_INV_MCU = 0x01,                // The offer was rejected due to it not being applicable to the product’s primary MCU. 
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_SWAP_PENDING = 0x02,           // MCU Firmware has been updated and a swap is currently pending.  No further Firmware Update processing can occur until the blade has been reset.
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_MISMATCH = 0x03,               // The offer was rejected due to a Version mismatch (Debug/Release for example).
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_BANK = 0x04,                   // The offer was rejected due to it being for the wrong firmware bank.
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_PLATFORM = 0x05,               // The offer’s Platform ID does not correlate to the receiving hardware product. 
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_MILESTONE = 0x06,              // The offer’s Milestone does not correlate to the receiving hardware’s Build ID. 
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_INV_PCOL_REV = 0x07,           // The offer indicates an interface Protocol Revision that the receiving product does not support. 
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_VARIANT = 0x08,                // The combination of Milestone & Compatibility Variants Mask did not match the HW. 
} COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON;

typedef enum _COMPONENT_FIRMWARE_UPDATE_FLAG
{
    COMPONENT_FIRMWARE_UPDATE_FLAG_DEFAULT = 0x00,
    COMPONENT_FIRMWARE_UPDATE_FLAG_FIRST_BLOCK = 0x80,        // Denotes the first block of a firmware payload.
    COMPONENT_FIRMWARE_UPDATE_FLAG_LAST_BLOCK = 0x40,         // Denotes the last block of a firmware payload.
    COMPONENT_FIRMWARE_UPDATE_FLAG_VERIFY = 0x08,             // If set, the firmware verifies the byte array in the upper block at the specified address.
} COMPONENT_FIRMWARE_UPDATE_FLAG;

/////////////////////////////////////
//  Message Structure definitions  //
/////////////////////////////////////

#define MAX_NUMBER_OF_IMAGE_PAIRS (7)
typedef struct _COMPONENT_FIRMWARE_VERSIONS
{
    BYTE componentCount;
    BYTE ComponentIdentifiers[MAX_NUMBER_OF_IMAGE_PAIRS];
    DWORD FirmwareVersion[MAX_NUMBER_OF_IMAGE_PAIRS];
} COMPONENT_FIRMWARE_VERSIONS;

// Defines the response from the device for an offer related command.
//
typedef struct _OFFER_RESPONSE
{
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE OfferResponseStatus;
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON OfferResponseReason;
} OFFER_RESPONSE;

//===================================================
//----- Callback Declaration------------------------=
//===================================================

// Callback to indicate Firmware versions of all components that the device supports.
//
typedef
_Function_class_(EVT_DMF_ComponentFirmwareUpdate_FirmwareVersionResponse)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ComponentFirmwareUpdate_FirmwareVersionResponse(
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule, 
    _In_reads_bytes_(FirmwareVersionBufferSize) UCHAR* FirmwareVersionBuffer,
    _In_ size_t FirmwareVersionBufferSize,
    _In_ NTSTATUS ntStatus
    );
typedef EVT_DMF_ComponentFirmwareUpdate_FirmwareVersionResponse *PFN_ComponentFirmwareUpdate_EvtFirmwareVersionResponse;

// Callback to indicate response to offer that was sent to device.
//
typedef
_Function_class_(EVT_DMF_ComponentFirmwareUpdate_OfferResponse)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ComponentFirmwareUpdate_OfferResponse(
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_reads_bytes_(ResponseBufferSize) UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatus
    );
typedef EVT_DMF_ComponentFirmwareUpdate_OfferResponse *PFN_ComponentFirmwareUpdate_EvtOfferResponse;

// Callback to indicate response to payload that was sent to device.
//
typedef
_Function_class_(EVT_DMF_ComponentFirmwareUpdate_PayloadResponse)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ComponentFirmwareUpdate_PayloadResponse(
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_reads_bytes_(ResponseBufferSize) UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatus
    );
typedef EVT_DMF_ComponentFirmwareUpdate_PayloadResponse *PFN_ComponentFirmwareUpdate_EvtPayloadResponse;


//===================================================
//----- Interface Declaration-----------------------=
//===================================================

typedef struct _INTF_CALLBACK_ComponentFirmwareUpdate
{
    PFN_ComponentFirmwareUpdate_EvtFirmwareVersionResponse  Evt_ComponentFirmwareUpdate_FirmwareVersionResponse;
    PFN_ComponentFirmwareUpdate_EvtOfferResponse            Evt_ComponentFirmwareUpdate_OfferResponse;
    PFN_ComponentFirmwareUpdate_EvtPayloadResponse          Evt_ComponentFirmwareUpdate_PayloadResponse;
} INTF_CALLBACK_ComponentFirmwareUpdate;

typedef struct _INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate
{
    INTF_CALLBACK_ComponentFirmwareUpdate Callbacks;
} INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate, ComponentFirmwareUpdateProtocolConfigGet)

typedef struct _INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdateTransport
{
    // Wait Time out in Ms for response from transport.
    //
    ULONG WaitTimeout;
    // Size of TransportHeader in bytes.
    // The protocol module will allocate header block at the beginning of the buffer for to transport to use.
    //
    ULONG TransportHeaderSize;
    // Required size of Firmware Payload Buffer this transport needs (excluding the TransportHeaderSize above).
    //
    ULONG TransportFirmwarePayloadBufferRequiredSize;
    // Required size of Offer Buffer this transport needs (excluding the TransportHeaderSize above).
    //
    ULONG TransportOfferBufferRequiredSize;
    // Required size of FirmwareVersion Buffer this transport needs (excluding the TransportHeaderSize above).
    //
    ULONG TransportFirmwareVersionBufferRequiredSize;
} INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdateTransport;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdateTransport, ComponentFirmwareUpdateTransportConfigGet)

// Interface to Bind a DmfComponentFirmwareUpdateModuleTransport to DmfComponentFirmwareUpdateModule.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
INTF_DMF_ComponentFirmwareUpdateTransport_Bind(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _In_ INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate* ComponentFirmwareUpdateConfig,
    _Out_ INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdateTransport* ComponentFirmwareUpdateTransportConfig
    );

// Interface to Unbind a DmfComponentFirmwareUpdateModuleTransport from DmfComponentFirmwareUpdateModule.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
INTF_DMF_ComponentFirmwareUpdateTransport_Unbind(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    );

// Interface to Get Firmware Version from Transport.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
INTF_DMF_ComponentFirmwareUpdateTransport_FirmwareVersionGet(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    );

// Interface to Send an Offer Information to Transport.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
INTF_DMF_ComponentFirmwareUpdateTransport_OfferInformationSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    );

// Interface to Send an Offer Command to Transport.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
INTF_DMF_ComponentFirmwareUpdateTransport_OfferCommandSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    );

// Interface to Send an Offer Blob to Transport.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
INTF_DMF_ComponentFirmwareUpdateTransport_OfferSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    );

// Interface to Send a Payload buffer to Transport.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
INTF_DMF_ComponentFirmwareUpdateTransport_PayloadSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    );

// Interface to Start Transport.
// Transport implementations can use it to do any preparation work before the protocol sequence are initiated.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
INTF_DMF_ComponentFirmwareUpdateTransport_ProtocolStart(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    );

// Interface to Stop Transport. 
// Transport can use this to do any clearn up as the protocol sequence is being stopped.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
INTF_DMF_ComponentFirmwareUpdateTransport_ProtocolStop(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    );

//===============================================================
//----- (Transport/Protocol) Callable Methods ------------------=
//===============================================================


//=============================================================
//----- Transport's Interface to Implementation Binding ------=
//=============================================================

// This structure defines the interfaces all component firmware transport should implement.
// All the interfaces functions are mandatory.
//
typedef struct
{
    INTF_DMF_ComponentFirmwareUpdateTransport_Bind *Intf_ComponentFirmwareUpdateTransport_Bind;
    INTF_DMF_ComponentFirmwareUpdateTransport_Unbind *Intf_ComponentFirmwareUpdateTransport_Unbind;
    INTF_DMF_ComponentFirmwareUpdateTransport_ProtocolStart* Intf_ComponentFirmwareUpdateTransport_ProtocolStart;
    INTF_DMF_ComponentFirmwareUpdateTransport_ProtocolStop* Intf_ComponentFirmwareUpdateTransport_ProtocolStop;

    INTF_DMF_ComponentFirmwareUpdateTransport_FirmwareVersionGet* Intf_ComponentFirmwareUpdateTransport_FirmwareVersionGet;
    INTF_DMF_ComponentFirmwareUpdateTransport_OfferInformationSend* Intf_ComponentFirmwareUpdateTransport_OfferInformationSend;
    INTF_DMF_ComponentFirmwareUpdateTransport_OfferCommandSend* Intf_ComponentFirmwareUpdateTransport_OfferCommandSend;
    INTF_DMF_ComponentFirmwareUpdateTransport_OfferSend* Intf_ComponentFirmwareUpdateTransport_OfferSend;
    INTF_DMF_ComponentFirmwareUpdateTransport_PayloadSend* Intf_ComponentFirmwareUpdateTransport_PayloadSend;
} INTF_DMF_ComponentFirmwareUpdateTransport;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(INTF_DMF_ComponentFirmwareUpdateTransport, IntfComponentFirmwareUpdateTransportGet)

FORCEINLINE
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_BindInterface(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ INTF_DMF_ComponentFirmwareUpdateTransport* Intf_ComponentFirmwareUpdateTransport
    )
/*++

Routine Description:

    Bind the componenent firmware interface to a particular transport implementation. This is called by Transport module.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    Intf_ComponentFirmwareUpdateTransport - Interface table for the Component Firmware Update Transports.

Return:

    NTSTATUS

--*/
{
    NTSTATUS  ntStatus;
    WDF_OBJECT_ATTRIBUTES attributes;
    INTF_DMF_ComponentFirmwareUpdateTransport* intfComponentFirmwareUpdateTransport;

    if ((Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_Bind == NULL) ||
        (Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_Unbind == NULL) ||
        (Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_FirmwareVersionGet == NULL) ||
        (Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferInformationSend == NULL) ||
        (Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferCommandSend == NULL) ||
        (Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_PayloadSend == NULL) ||
        (Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_ProtocolStart == NULL) ||
        (Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_ProtocolStop == NULL))
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            INTF_DMF_ComponentFirmwareUpdateTransport);
    ntStatus = WdfObjectAllocateContext(DmfComponentFirmwareUpdateTransportModule,
                                        &attributes, 
                                        (VOID**) &intfComponentFirmwareUpdateTransport);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    intfComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_Bind = Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_Bind;
    intfComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_Unbind = Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_Unbind;
    intfComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_FirmwareVersionGet = Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_FirmwareVersionGet;
    intfComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferInformationSend = Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferInformationSend;
    intfComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferCommandSend = Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferCommandSend;
    intfComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferSend = Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferSend;
    intfComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_PayloadSend = Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_PayloadSend;
    intfComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_ProtocolStart = Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_ProtocolStart;
    intfComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_ProtocolStop = Intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_ProtocolStop;

Exit:

    return ntStatus;
}

//==============================================
//------- Interfaces to Transport -------------=
//==============================================
//
FORCEINLINE
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_Bind(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _In_ INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate* ComponentFirmwareUpdateConfig,
    _Out_ INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdateTransport* ComponentFirmwareUpdateTransportConfig
    )
/*++

Routine Description:

    Registers protocol module with the transport module. This is called by Protocol module.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    ComponentFirmwareUpdateConfig - Information about the callback to protocol module.
    ComponentFirmwareUpdateTransportConfig - Transport specific configuration.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INTF_DMF_ComponentFirmwareUpdateTransport* intf_ComponentFirmwareUpdateTransport;
    intf_ComponentFirmwareUpdateTransport = IntfComponentFirmwareUpdateTransportGet(DmfComponentFirmwareUpdateTransportModule);
    if (intf_ComponentFirmwareUpdateTransport == NULL) 
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ntStatus = intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_Bind(DmfComponentFirmwareUpdateTransportModule,
                                                                                                 DmfComponentFirmwareUpdateModule,
                                                                                                 ComponentFirmwareUpdateConfig, 
                                                                                                 ComponentFirmwareUpdateTransportConfig);

Exit:

    return ntStatus;
}

FORCEINLINE
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_Unbind(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    )
/*++

Routine Description:

    Deregister protocol module from the transport module. This is called by Protocol module.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INTF_DMF_ComponentFirmwareUpdateTransport* intf_ComponentFirmwareUpdateTransport;
    intf_ComponentFirmwareUpdateTransport = IntfComponentFirmwareUpdateTransportGet(DmfComponentFirmwareUpdateTransportModule);
    if (intf_ComponentFirmwareUpdateTransport == NULL) 
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ntStatus = intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_Unbind(DmfComponentFirmwareUpdateTransportModule,
                                                                                                   DmfComponentFirmwareUpdateModule);

Exit:

    return ntStatus;
}

FORCEINLINE
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_FirmwareVersionGet(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    )
/*++

Routine Description:

    Retrieves the firmware versions from the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INTF_DMF_ComponentFirmwareUpdateTransport* intf_ComponentFirmwareUpdateTransport;
    intf_ComponentFirmwareUpdateTransport = IntfComponentFirmwareUpdateTransportGet(DmfComponentFirmwareUpdateTransportModule);
    if (intf_ComponentFirmwareUpdateTransport == NULL) 
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ntStatus = intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_FirmwareVersionGet(DmfComponentFirmwareUpdateTransportModule,
                                                                                                               DmfComponentFirmwareUpdateModule);

Exit:

    return ntStatus;
}

FORCEINLINE
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_OfferInformationSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer information command to the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    Buffer - Header, followed by Offer Information to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the begining of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INTF_DMF_ComponentFirmwareUpdateTransport* intf_ComponentFirmwareUpdateTransport;
    intf_ComponentFirmwareUpdateTransport = IntfComponentFirmwareUpdateTransportGet(DmfComponentFirmwareUpdateTransportModule);
    if (intf_ComponentFirmwareUpdateTransport == NULL) 
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ntStatus = intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferInformationSend(DmfComponentFirmwareUpdateTransportModule,
                                                                                                                 DmfComponentFirmwareUpdateModule,
                                                                                                                 Buffer,
                                                                                                                 BufferSize,
                                                                                                                 HeaderSize);

Exit:

    return ntStatus;
}

FORCEINLINE
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_OfferCommandSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer information command to the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    Buffer - Header followed by Offer Command to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the begining of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INTF_DMF_ComponentFirmwareUpdateTransport* intf_ComponentFirmwareUpdateTransport;
    intf_ComponentFirmwareUpdateTransport = IntfComponentFirmwareUpdateTransportGet(DmfComponentFirmwareUpdateTransportModule);
    if (intf_ComponentFirmwareUpdateTransport == NULL) 
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ntStatus = intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferCommandSend(DmfComponentFirmwareUpdateTransportModule,
                                                                                                             DmfComponentFirmwareUpdateModule,
                                                                                                             Buffer,
                                                                                                             BufferSize,
                                                                                                             HeaderSize);

Exit:

    return ntStatus;
}

FORCEINLINE
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_OfferSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer to the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    Buffer - Header, followed by Offer Content to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the begining of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INTF_DMF_ComponentFirmwareUpdateTransport* intf_ComponentFirmwareUpdateTransport;
    intf_ComponentFirmwareUpdateTransport = IntfComponentFirmwareUpdateTransportGet(DmfComponentFirmwareUpdateTransportModule);
    if (intf_ComponentFirmwareUpdateTransport == NULL) 
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ntStatus = intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_OfferSend(DmfComponentFirmwareUpdateTransportModule,
                                                                                                      DmfComponentFirmwareUpdateModule,
                                                                                                      Buffer,
                                                                                                      BufferSize,
                                                                                                      HeaderSize);

Exit:

    return ntStatus;
}

FORCEINLINE
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_PayloadSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends Payload to the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    Buffer - Header, followed by Payload to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the begining of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INTF_DMF_ComponentFirmwareUpdateTransport* intf_ComponentFirmwareUpdateTransport;
    intf_ComponentFirmwareUpdateTransport = IntfComponentFirmwareUpdateTransportGet(DmfComponentFirmwareUpdateTransportModule);
    if (intf_ComponentFirmwareUpdateTransport == NULL) 
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ntStatus = intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_PayloadSend(DmfComponentFirmwareUpdateTransportModule,
                                                                                                        DmfComponentFirmwareUpdateModule,
                                                                                                        Buffer,
                                                                                                        BufferSize,
                                                                                                        HeaderSize);

Exit:

    return ntStatus;
}

FORCEINLINE
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_ProtocolStart(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    )
/*++

Routine Description:

    Setup the transport for protocol transaction.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INTF_DMF_ComponentFirmwareUpdateTransport* intf_ComponentFirmwareUpdateTransport;
    intf_ComponentFirmwareUpdateTransport = IntfComponentFirmwareUpdateTransportGet(DmfComponentFirmwareUpdateTransportModule);
    if (intf_ComponentFirmwareUpdateTransport == NULL) 
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ntStatus = intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_ProtocolStart(DmfComponentFirmwareUpdateTransportModule,
                                                                                                          DmfComponentFirmwareUpdateModule);

Exit:

    return ntStatus;
}

FORCEINLINE
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdateTransport_ProtocolStop(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    )
/*++

Routine Description:

    Clean up the transport as the protocol is being stopped.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INTF_DMF_ComponentFirmwareUpdateTransport* intf_ComponentFirmwareUpdateTransport;
    intf_ComponentFirmwareUpdateTransport = IntfComponentFirmwareUpdateTransportGet(DmfComponentFirmwareUpdateTransportModule);
    if (intf_ComponentFirmwareUpdateTransport == NULL) 
    {
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ntStatus = intf_ComponentFirmwareUpdateTransport->Intf_ComponentFirmwareUpdateTransport_ProtocolStop(DmfComponentFirmwareUpdateTransportModule,
                                                                                                         DmfComponentFirmwareUpdateModule);

Exit:

    return ntStatus;
}

// eof: Dmf_Transport_ComponentFirmwareUpdate.h
//