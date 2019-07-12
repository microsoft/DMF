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
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_INV_MCU = 0x01,                // The offer was rejected due to it not being applicable to the product's primary MCU. 
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_SWAP_PENDING = 0x02,           // MCU Firmware has been updated and a swap is currently pending.  No further Firmware Update processing can occur until the blade has been reset.
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_MISMATCH = 0x03,               // The offer was rejected due to a Version mismatch (Debug/Release for example).
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_BANK = 0x04,                   // The offer was rejected due to it being for the wrong firmware bank.
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_PLATFORM = 0x05,               // The offer's Platform ID does not correlate to the receiving hardware product. 
    COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_MILESTONE = 0x06,              // The offer's Milestone does not correlate to the receiving hardware's Build ID. 
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

// Bind Time Data.
// ---------------
//
// Data provided by the Protocol Module.
//
typedef struct _DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA
{
    ULONG Dummy;
} DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA;

// Data provided by the Transport Module.
//
typedef struct _DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA
{
    // Wait Time out in Ms for response from transport.
    //
    ULONG TransportWaitTimeout;
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
    // Payload buffer fill alignment this transport needs.
    //
    UINT TransportPayloadFillAlignment;
} DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA;

// Declaration Time Data.
// ----------------------
//
// Callbacks provided by Protocol Module.
//

// Callback to indicate Firmware versions of all components that the device supports.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_ComponentFirmwareUpdate_FirmwareVersionResponse(
    _In_ DMFINTERFACE DmfInterface,
    _In_reads_bytes_(FirmwareVersionBufferSize) UCHAR* FirmwareVersionBuffer,
    _In_ size_t FirmwareVersionBufferSize,
    _In_ NTSTATUS ntStatus
    );

// Callback to indicate response to offer that was sent to device.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_ComponentFirmwareUpdate_OfferResponse(
    _In_ DMFINTERFACE DmfInterface,
    _In_reads_bytes_(ResponseBufferSize) UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatus
    );

// Callback to indicate response to payload that was sent to device.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_ComponentFirmwareUpdate_PayloadResponse(
    _In_ DMFINTERFACE DmfInterface,
    _In_reads_bytes_(ResponseBufferSize) UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatus
    );

// Data that fully describes this Protocol.
//
typedef struct _DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA
{
    // The Protocol Interface Descriptor. 
    // Every Interface must have this as the first member of its Protocol Declaration Data.
    //
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR DmfProtocolDescriptor;
    // Stores callbacks implemented by this Interface Protocol.
    //
    EVT_DMF_INTERFACE_ComponentFirmwareUpdate_FirmwareVersionResponse* EVT_ComponentFirmwareUpdate_FirmwareVersionResponse;
    EVT_DMF_INTERFACE_ComponentFirmwareUpdate_OfferResponse* EVT_ComponentFirmwareUpdate_OfferResponse;
    EVT_DMF_INTERFACE_ComponentFirmwareUpdate_PayloadResponse* EVT_ComponentFirmwareUpdate_PayloadResponse;
} DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA;

// Methods used to initialize Protocol's Declaration Data.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA* ProtocolDeclarationData,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ EVT_DMF_INTERFACE_ComponentFirmwareUpdate_FirmwareVersionResponse* EvtComponentFirmwareUpdate_FirmwareVersionResponse,
    _In_ EVT_DMF_INTERFACE_ComponentFirmwareUpdate_OfferResponse* EvtComponentFirmwareUpdate_OfferResponse,
    _In_ EVT_DMF_INTERFACE_ComponentFirmwareUpdate_PayloadResponse* EvtComponentFirmwareUpdate_PayloadResponse
    );

// Methods provided by Transport Module.
//
// Bind
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA* TransportBindData
    );

// Unbind
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    );

// Interface to Get Firmware Version from Transport.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_ComponentFirmwareUpdate_TransportFirmwareVersionGet(
    _In_ DMFINTERFACE DmfInterface
    );

// Interface to Send an Offer Information to Transport.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferInformationSend(
    _In_ DMFINTERFACE DmfInterface,
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
DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferCommandSend(
    _In_ DMFINTERFACE DmfInterface,
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
DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferSend(
    _In_ DMFINTERFACE DmfInterface,
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
DMF_INTERFACE_ComponentFirmwareUpdate_TransportPayloadSend(
    _In_ DMFINTERFACE DmfInterface,
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
DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStart(
    _In_ DMFINTERFACE DmfInterface
    );

// Interface to Stop Transport. 
// Transport can use this to do any clearn up as the protocol sequence is being stopped.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStop(
    _In_ DMFINTERFACE DmfInterface
    );


// Data that fully describes this Transport.
//
typedef struct _DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA
{
    // The Transport Interface Descriptor.
    // Every Interface must have this as the first member of its Transport Declaration Data.
    //
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR DmfTransportDescriptor;
    // Stores methods implemented by this Interface Transport.
    //
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind* DMF_ComponentFirmwareUpdate_TransportBind;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind* DMF_ComponentFirmwareUpdate_TransportUnbind;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportFirmwareVersionGet* DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferInformationSend* DMF_ComponentFirmwareUpdate_TransportOfferInformationSend;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferCommandSend* DMF_ComponentFirmwareUpdate_TransportOfferCommandSend;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferSend* DMF_ComponentFirmwareUpdate_TransportOfferSend;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportPayloadSend* DMF_ComponentFirmwareUpdate_TransportPayloadSend;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStart* DMF_ComponentFirmwareUpdate_TransportProtocolStart;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStop* DMF_ComponentFirmwareUpdate_TransportProtocolStop;
} DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA;

// Methods used to initialize Transport's Declaration Data.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* TransportDeclarationData,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind* ComponentFirmwareUpdate_TransportBind,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind* ComponentFirmwareUpdate_TransportUnbind,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportFirmwareVersionGet* ComponentFirmwareUpdate_TransportFirmwareVersionGet,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferInformationSend* ComponentFirmwareUpdate_TransportOfferInformationSend,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferCommandSend* ComponentFirmwareUpdate_TransportOfferCommandSend,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferSend* ComponentFirmwareUpdate_TransportOfferSend,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportPayloadSend* ComponentFirmwareUpdate_TransportPayloadSend,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStart* ComponentFirmwareUpdate_TransportProtocolStart,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStop* ComponentFirmwareUpdate_TransportProtocolStop
    );

// Methods exposed to Protocol.
// ----------------------------
//
DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind DMF_ComponentFirmwareUpdate_TransportBind;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind DMF_ComponentFirmwareUpdate_TransportUnbind;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportFirmwareVersionGet DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferInformationSend DMF_ComponentFirmwareUpdate_TransportOfferInformationSend;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferCommandSend DMF_ComponentFirmwareUpdate_TransportOfferCommandSend;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferSend DMF_ComponentFirmwareUpdate_TransportOfferSend;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportPayloadSend DMF_ComponentFirmwareUpdate_TransportPayloadSend;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStart DMF_ComponentFirmwareUpdate_TransportProtocolStart;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStop DMF_ComponentFirmwareUpdate_TransportProtocolStop;

// Callbacks exposed to Transport.
// -------------------------------
//
EVT_DMF_INTERFACE_ComponentFirmwareUpdate_FirmwareVersionResponse EVT_ComponentFirmwareUpdate_FirmwareVersionResponse;
EVT_DMF_INTERFACE_ComponentFirmwareUpdate_OfferResponse EVT_ComponentFirmwareUpdate_OfferResponse;
EVT_DMF_INTERFACE_ComponentFirmwareUpdate_PayloadResponse EVT_ComponentFirmwareUpdate_PayloadResponse;

// This macro defines the ComponentFirmwareUpdateProtocolDeclarationDataGet and ComponentFirmwareUpdateTransportDeclarationDataGet.
// Call this macro after the DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA and
// DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA are defined.
//
DECLARE_DMF_INTERFACE(ComponentFirmwareUpdate);

// eof: Dmf_Interface_ComponentFirmwareUpdate.h
//