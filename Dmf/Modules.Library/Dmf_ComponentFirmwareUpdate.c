/*++

   Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT license.

Module Name:

    Dmf_ComponentFirmwareUpdate.c

Abstract:

    This Module handles Component Firmware Update Protocol.

Environment:

    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_ComponentFirmwareUpdate.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include <intsafe.h>

// Defines a single offer and payload content.
//
typedef struct _FIRMWARE_INFORMATION
{
    // Size in bytes of the Offer and Payload that is sent to device.
    //
    size_t OfferSize;
    size_t PayloadSize;
    // Holds the buffer either created locally or the client given.
    //
    WDFMEMORY OfferContentMemory;
    // Holds the buffer either created locally or the client given.
    //
    WDFMEMORY PayloadContentMemory;
} FIRMWARE_INFORMATION;

// Defines all the firmware update status that are used internally.
// These values are updated in the registry to mark various stages of protocol sequence.
//
typedef enum _FIRMWARE_UPDATE_STATUS
{
    FIRMWARE_UPDATE_STATUS_NOT_STARTED = 0x00,
    FIRMWARE_UPDATE_STATUS_UPDATE_REJECTED,
    FIRMWARE_UPDATE_STATUS_DOWNLOADING_UPDATE,
    FIRMWARE_UPDATE_STATUS_BUSY_PROCESSING_UPDATE,
    FIRMWARE_UPDATE_STATUS_PENDING_RESET,
    FIRMWARE_UPDATE_STATUS_UP_TO_DATE,

    FIRMWARE_UPDATE_STATUS_ERROR = 0xFF
} FIRMWARE_UPDATE_STATUS;

// Size of the maximum value name in registry (per MSDN).
//
#define MAXIMUM_VALUE_NAME_SIZE  16382

// Protocol versions.
//
#define PROTOCOL_VERSION_2 0x2
#define PROTOCOL_VERSION_4 0x4

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context.
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Structure to hold a response to a payload chunk that was sent to device.
// Response message includes a sequence number and the response status.
//
typedef struct _PAYLOAD_RESPONSE
{
    UINT16 SequenceNumber;
    COMPONENT_FIRMWARE_UPDATE_PAYLOAD_RESPONSE ResponseStatus;
} PAYLOAD_RESPONSE;

// This context associated with the plugged in protocol Module.
//
typedef struct _CONTEXT_ComponentFirmwareUpdateTransaction
{
    // Asynchronous Response Handling Contexts.
    // ----------------------------------------
    //
    // Callback Status.
    NTSTATUS ntStatus;
    // Buffer List to hold the Payload Responses.
    //
    DMFMODULE DmfModuleBufferQueue;
    // Offer Response.
    //
    OFFER_RESPONSE OfferResponse;
    // Firmware Versions.
    //
    COMPONENT_FIRMWARE_VERSIONS FirmwareVersions;
    // Event to Signal threads that are waiting for a response from transport.
    //
    DMF_PORTABLE_EVENT DmfResponseCompletionEvent;
    // Event to Signal Cancellation of protocol transaction.
    //
    DMF_PORTABLE_EVENT DmfProtocolTransactionCancelEvent;
    // Asynchronous Response Handling Contexts.
    // ----------------------------------------
} CONTEXT_ComponentFirmwareUpdateTransaction;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTEXT_ComponentFirmwareUpdateTransaction, ComponentFirmwareUpdateTransactionContextGet)

// Private context the Protocol Module associates with an Interface.
//
typedef struct _CONTEXT_ComponentFirmwareUpdateTransport
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
} CONTEXT_ComponentFirmwareUpdateTransport;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CONTEXT_ComponentFirmwareUpdateTransport, ComponentFirmwareUpdateTransportContextGet)

typedef struct _DMF_CONTEXT_ComponentFirmwareUpdate
{
    // Protocol sequence Thread Handle.
    //
    DMFMODULE DmfModuleThread;

    // Interface Handle.
    //
    DMFINTERFACE DmfInterfaceComponentFirmwareUpdate;

    // Firmware blob containing the firmware data (offers & payloads) that this Module needs to send to device.
    //
    WDFCOLLECTION FirmwareBlobCollection;

    // Registry Key to store Firmware Update Process related book keeping information in registry.
    //
    WDFKEY DeviceRegistryKey;

    // Is a protocol transaction in progress?.
    //
    BOOLEAN TransactionInProgress;
} DMF_CONTEXT_ComponentFirmwareUpdate;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(ComponentFirmwareUpdate)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ComponentFirmwareUpdate)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Registry Keys Firmware Update Status (Keeping the existing registry key names).
//
PCWSTR ComponentFirmwareUpdate_CurrentFwVersionValueName = L"CurrentFwVersion";
PCWSTR ComponentFirmwareUpdate_OfferFwVersionValueName = L"OfferFwVersion";
PCWSTR ComponentFirmwareUpdate_FirmwareUpdateStatusValueName = L"FirmwareUpdateStatus";
PCWSTR ComponentFirmwareUpdate_FirmwareUpdateStatusRejectReasonValueName = L"FirmwareUpdateStatusRejectReason";
 
PCWSTR ComponentFirmwareUpdate_ResumePayloadBufferBinRecordIndexValueName = L"ResumePayloadBinRecordIndex";
PCWSTR ComponentFirmwareUpdate_ResumePayloadBufferBinRecordDataOffsetValueName = L"ResumePayloadBufferBinRecordDataOffset";
PCWSTR ComponentFirmwareUpdate_ResumeSequenceNumberValueName = L"ResumeSequenceNumber";
PCWSTR ComponentFirmwareUpdate_ResumeOnConnectValueName = L"ResumeOnConnect";

// Based on Specification.
//
// Each time 60 bytes of Payload sent.
//
#define SizeOfPayload (60)
// Offer is 16 bytes long.
//
#define SizeOfOffer (4*sizeof(ULONG))
// Firmware Version is 60 bytes long.
//
#define SizeOfFirmwareVersion (60)

#define Thread_NumberOfWaitObjects (2)
const BYTE FWUPDATE_DRIVER_TOKEN = 0xA0;
const BYTE FWUPDATE_INFORMATION_TOKEN = 0xFF;
const BYTE FWUPDATE_COMMAND_TOKEN = 0xFE;

// Memory Pool Tag.
//
#define MemoryTag 'tUFC'

// This macro takes a variable name (usually an enum) and has the function return.
// a c-string interpretation of the variable name.
// Ex: MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_SUCCESS) returns "COMPONENT_FIRMWARE_UPDATE_SUCCESS".
//
#define MAKE_CASE(x) case x: return #x;

PCSTR
ComponentFirmwareUpdatePayloadResponseString(
    _In_ const COMPONENT_FIRMWARE_UPDATE_PAYLOAD_RESPONSE firmwareUpdateDataStatus
    )
{
    switch (firmwareUpdateDataStatus)
    {
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_SUCCESS);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_PREPARE);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_WRITE);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_COMPLETE);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_VERIFY);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_CRC);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_SIGNATURE);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_VERSION);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_SWAP_PENDING);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_INVALID_ADDR);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_NO_OFFER);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_ERROR_INVALID);
        default: 
            DmfAssert(FALSE);
            return "Unknown";
    }
}

PCSTR
ComponentFirmwareUpdateOfferInformationCodeString(
    _In_ const COMPONENT_FIRMWARE_UPDATE_OFFER_INFORMATION_CODE FirmwareUpdateOfferInformationCode
    )
{
    switch (FirmwareUpdateOfferInformationCode)
    {
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_INFO_START_ENTIRE_TRANSACTION);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_INFO_START_OFFER_LIST);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_INFO_END_OFFER_LIST);
        default:
            DmfAssert(FALSE);
            return "Unknown";
    }
}

PCSTR
ComponentFirmwareUpdateOfferCommandCodeString(
    _In_ const COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_CODE FirmwareUpdateOfferCommandCode
    )
{
    switch (FirmwareUpdateOfferCommandCode)
    {
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_NOTIFY_ON_READY);
        default:
            DmfAssert(FALSE);
            return "Unknown";
    }
}

PCSTR
ComponentFirmwareUpdateOfferResponseString(
    _In_ const COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE FirmwareUpdateOfferResponse
    )
{
    switch (FirmwareUpdateOfferResponse)
    {
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_SKIP);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_ACCEPT);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_BUSY);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_READY);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_NOT_SUPPORTED);
        default:
            DmfAssert(FALSE);
            return "Unknown";
    }
}

PCSTR
ComponentFirmwareUpdateOfferResponseRejectString(
    _In_ const COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON FirmwareUpdateOfferRejectReason
    )
{
    switch (FirmwareUpdateOfferRejectReason)
    {
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_OLD_FW);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_INV_MCU);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_SWAP_PENDING);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_MISMATCH);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_BANK);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_PLATFORM);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_MILESTONE);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_INV_PCOL_REV);
        MAKE_CASE(COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_VARIANT);
        default:
            if (FirmwareUpdateOfferRejectReason >= COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_VENDOR_SPECIFIC_MIN &&
                FirmwareUpdateOfferRejectReason <= COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_VENDOR_SPECIFIC_MAX)
            {
                return "COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_VENDOR_SPECIFIC"; 
            }
            else 
            {
                DmfAssert(FALSE);
                return "Unknown";
            }
    }
}

//-- Helper functions ---
//--------START----------
//
#pragma code_seg("PAGE")
NTSTATUS
ComponentFirmwareUpdate_WaitForResponse(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG TransportWaitTimeoutMs
    )
/*++

Routine Description:

    Helper function that waits until there is either a response or timeout or error in wait.

Parameters:

    DmfModule - This Module's DMF Object.
    TransportWaitTimeoutMs - TransportWaitTimeout in Milliseconds.

Return:

    NTSTATUS This returns STATUS_SUCCESS only when there is an actual response completion in time.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;

    DMF_PORTABLE_EVENT* waitObjects[Thread_NumberOfWaitObjects];
    NTSTATUS waitStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    // Wait for response.
    //
    waitObjects[0] = &componentFirmwareUpdateTransactionContext->DmfResponseCompletionEvent;
    waitObjects[1] = &componentFirmwareUpdateTransactionContext->DmfProtocolTransactionCancelEvent;
    waitStatus = DMF_Portable_EventWaitForMultiple(ARRAYSIZE(waitObjects),
                                                   waitObjects,
                                                   FALSE,
                                                   &TransportWaitTimeoutMs,
                                                   FALSE);
    switch (waitStatus)
    {
        case STATUS_WAIT_0:
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, 
                        DMF_TRACE, 
                        "Response Received.");
            ntStatus = STATUS_SUCCESS;
            break;
        }
        case STATUS_WAIT_1:
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, 
                        DMF_TRACE, 
                        "Operation cancelled.");
            ntStatus = STATUS_TRANSACTION_ABORTED;
            goto Exit;
        }
        case WAIT_TIMEOUT:
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "Read operation timed out");
            ntStatus = STATUS_INVALID_DEVICE_STATE;
            goto Exit;
        }
        default:
        {
            DmfAssert(FALSE);
            ntStatus = STATUS_INVALID_DEVICE_STATE;
            goto Exit;
        }
    }

Exit:

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
ComponentFirmwareUpdate_PayloadResponseProcess(
    _In_ DMFMODULE DmfModule,
    _In_ UINT16 ExpectedSequenceNumber,
    _Out_ COMPONENT_FIRMWARE_UPDATE_PAYLOAD_RESPONSE* PayloadResponse
    )
/*++

Routine Description:

    Helper is a function that Waits for & then processe the response to a payload message that was send to device.
    The response have a sequence number; This function matches the response sequence number to the one specified as argument.

Parameters:

    DmfModule - This Module's DMF Object.
    ExpectedSequenceNumber - Sequence Number that we are expecting see in the response.
    PayloadResponse - Response retrieved from the device.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;
    CONTEXT_ComponentFirmwareUpdateTransport* componentFirmwareUpdateTransportContext;

    const UINT maxSequenceNumberMatchAttempts = 3;
    UINT sequenceNumberMatchAttempts;
    BOOL sequenceNumberMatches;

    VOID* clientBuffer = NULL;
    VOID* clientBufferContext = NULL;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    componentFirmwareUpdateTransportContext = ComponentFirmwareUpdateTransportContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransportContext != NULL);

    ntStatus = STATUS_SUCCESS;

    // Loop until we receive a response with the matching sequence number.
    // Only loop a maximum number of times.
    //
    sequenceNumberMatches = FALSE;
    sequenceNumberMatchAttempts = 0;
    while (!sequenceNumberMatches && (sequenceNumberMatchAttempts < maxSequenceNumberMatchAttempts))
    {
        // Wait for response.
        //
        ntStatus = ComponentFirmwareUpdate_WaitForResponse(DmfModule,
                                                           componentFirmwareUpdateTransportContext->TransportWaitTimeout);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "WaitForResponse fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }

        // Get all the completed responses and see if we have a matching sequence number.
        //
        do
        {
            clientBufferContext = NULL;
            clientBuffer = NULL;

            // Process the response.
            //
            ntStatus = DMF_BufferQueue_Dequeue(componentFirmwareUpdateTransactionContext->DmfModuleBufferQueue,
                                               &clientBuffer,
                                               &clientBufferContext);
            if (!NT_SUCCESS(ntStatus))
            {
                // There is no data buffer for the processing.
                //
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            DMF_TRACE,
                            "No more buffer for Payload Response Processing");

                // We dont expect to hit this first time in this loop.
                //
                DmfAssert(sequenceNumberMatchAttempts != 0);
                break;
            }

            DmfAssert(clientBuffer != NULL);
            DmfAssert(clientBufferContext != NULL);

            PAYLOAD_RESPONSE* payloadResponse = (PAYLOAD_RESPONSE*)clientBuffer;
#if defined(DEBUG)
            ULONG* payloadResponseLength = (ULONG*)clientBufferContext;
            DmfAssert(*payloadResponseLength == sizeof(PAYLOAD_RESPONSE));
#endif // defined(DEBUG)
            if (ExpectedSequenceNumber > payloadResponse->SequenceNumber)
            {
                // This can happen if the device resends a message {Historical reason may be}.
                //
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "sequenceNumber(%d) > responseSequenceNumber(%d) in sequenceNumberMatchAttempt(%d)",
                            ExpectedSequenceNumber,
                            payloadResponse->SequenceNumber,
                            sequenceNumberMatchAttempts);

                // Continue with the loop.
                //
            }
            else if (ExpectedSequenceNumber < payloadResponse->SequenceNumber)
            {
                // This is an error case.
                //
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "sequenceNumber(%d) < responseSequenceNumber(%d) in sequenceNumberMatchAttempt(%d)",
                            ExpectedSequenceNumber,
                            payloadResponse->SequenceNumber,
                            sequenceNumberMatchAttempts);
                ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
            }
            else
            {
                // We found a matching sequence number. (ExpectedSequenceNumber == responseSequenceNumber)
                //
                *PayloadResponse = payloadResponse->ResponseStatus;
                sequenceNumberMatches = TRUE;
            }

            // NOTE: clientBuffer is always valid here and it needs to be returned.
            //

            // We are done with the buffer from consumer; put it back to producer.
            //
            DMF_BufferQueue_Reuse(componentFirmwareUpdateTransactionContext->DmfModuleBufferQueue,
                                  clientBuffer);
            clientBuffer = NULL;

            if (!NT_SUCCESS(ntStatus))
            {
                break;
            }

            sequenceNumberMatchAttempts++;
        } while (!sequenceNumberMatches && sequenceNumberMatchAttempts < maxSequenceNumberMatchAttempts);
    }

    if (!sequenceNumberMatches)
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "Never matched sequence number.");
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_PayloadBufferFill(
    _In_ DMFMODULE DmfModule,
    _In_ const UINT16 SequenceNumber,
    _In_reads_(PayloadBufferSize) const BYTE* PayloadBuffer,
    _In_ const size_t PayloadBufferSize,
    _Inout_ ULONG &PayloadBufferBinRecordStartIndex,
    _Inout_ BYTE &PayloadBufferBinRecordDataOffset,
    _Out_writes_(TransferBufferSize) UCHAR* TransferBuffer,
    _In_ const BYTE TransferBufferSize
    )
/*++

Routine Description:

    Reads whole payload data and fill up a payload chunk ready to send to device.
    TransferBuffer holds the prepared data and
    PayloadBufferBinRecordStartIndex updated to index to the next unread entry in payloadBuffer and
    PayloadBufferBinRecordDataOffset updated to how far into the entry we have read.

Arguments:

    DmfModule - This Module's DMF Object.
    SequenceNumber - Sequence number to be used in this payload.
    PayloadBuffer - Payload data from the blob. This is the whole payload.
    PayloadBufferSize - Size of the above buffer.
    PayloadBufferBinRecordStartIndex - Index into PayloadBuffer that corresponds to a bin record's beginning.
    PayloadBufferBinRecordDataOffset - Data offset into the current bin record.
    TransferBuffer - Buffer where the data will be written. This is the current payload chunk.
    TransferBufferSize - Size of TransferBuffer.

Return Value:

    NTSTATUS

--*/
{
    // Enable 1 byte packing for our structs.
    //
    #include <pshpack1.h>
    // This private structure holds the CFU formatted bin file, which is ||ADDR|L|DATA....
    // Addr is 4 bytes, length is 1 bytes, and data[] as defined by length.
    //
    typedef struct _BIN_RECORD
    {
        ULONG Address;
        BYTE Length;
        BYTE BinData[1];
    } BIN_RECORD;
    // This private structure is used to build the Transfer buffer. 
    //
    typedef struct _PAYLOAD
    {
        BYTE Flags;
        BYTE DataLength;
        UINT16 SequenceNumber;
        ULONG Address;
        BYTE PayloadData[1];
    } PAYLOAD;
    #include <poppack.h>

    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    CONTEXT_ComponentFirmwareUpdateTransport* componentFirmwareUpdateTransportContext;

    errno_t errorNumber;
    BYTE dataLength;
    BYTE remainingPayloadBufferLength;

    UINT32 lastAddressConsumed;
    BYTE payloadBufferOffset;

    BIN_RECORD *currentBinRecord;
    PAYLOAD *payload;
    const UINT32 BinRecordHeaderLength = sizeof(ULONG) + sizeof(BYTE);
    const UINT32 PayloadHeaderLength = sizeof(BYTE) + sizeof(BYTE) + sizeof(UINT16) + sizeof(ULONG);
    
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(PayloadBuffer != NULL);
    DmfAssert(TransferBuffer != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    componentFirmwareUpdateTransportContext = ComponentFirmwareUpdateTransportContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);

    ntStatus = STATUS_SUCCESS;

    // Check if the input buffer size has the minimum length requirement for Address and Length. 
    // We will check if the data field is valid later.
    //
    if ( PayloadBufferSize - PayloadBufferBinRecordStartIndex < BinRecordHeaderLength)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Payload Buffer is corrupted. Size remaining %Iu is less than the minimum required (%Iu)",
                    (PayloadBufferSize - PayloadBufferBinRecordStartIndex),
                    BinRecordHeaderLength);
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    // Clear the output buffer.
    //
    ZeroMemory(TransferBuffer, 
               TransferBufferSize);

    currentBinRecord = (BIN_RECORD*) (PayloadBuffer + PayloadBufferBinRecordStartIndex);
    payload = (PAYLOAD*) TransferBuffer;

    payload->Address = currentBinRecord->Address + PayloadBufferBinRecordDataOffset;
    payload->SequenceNumber = SequenceNumber;
    payload->Flags = COMPONENT_FIRMWARE_UPDATE_FLAG_DEFAULT;

    remainingPayloadBufferLength = TransferBufferSize - PayloadHeaderLength;

    // Adjust the RemainingPayloadBufferLength as per the alignment.
    //
    if (remainingPayloadBufferLength % componentFirmwareUpdateTransportContext->TransportPayloadFillAlignment == 0)
    {
        // Do nothing, already aligned.
    }
    else 
    {
        remainingPayloadBufferLength -= (remainingPayloadBufferLength % componentFirmwareUpdateTransportContext->TransportPayloadFillAlignment);
        TraceEvents(TRACE_LEVEL_VERBOSE, 
                    DMF_TRACE, 
                    "Setting buffer length to %d to meet alignment requirements.", 
                    remainingPayloadBufferLength);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "PayloadBufferIndex[0x%x] address 0x%x length %d PayloadBufferSize 0x%Ix TransferBufferSize 0x%Ix", 
                PayloadBufferBinRecordStartIndex,
                currentBinRecord->Address,
                currentBinRecord->Length, 
                PayloadBufferSize, 
                TransferBufferSize);

    payloadBufferOffset = 0;
    
    while (1)
    {
        // We check if the length of the payload buffer has the length specified by the length field.
        // Note: we can only check if the required length is satisfied, we have no way to know if the data 
        // is correct or not.
        //
        if (PayloadBufferBinRecordStartIndex + BinRecordHeaderLength + currentBinRecord->Length > PayloadBufferSize)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "Payload Buffer is corrupted. Length of buffer remaining is %Iu, which is less than the length specified (%Iu)",
                        (PayloadBufferSize - PayloadBufferBinRecordStartIndex - BinRecordHeaderLength),
                        currentBinRecord->Length);
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit;
        }

        // PayloadBufferBinRecordDataOffset should always be smaller than the currentBinRecord length.
        //
        DmfAssert(PayloadBufferBinRecordDataOffset < currentBinRecord->Length);

        // Add flags depending on whether this is the first block.
        //
        if (PayloadBufferBinRecordStartIndex == 0 && PayloadBufferBinRecordDataOffset == 0)
        {
            payload->Flags |= COMPONENT_FIRMWARE_UPDATE_FLAG_FIRST_BLOCK;
        }

        lastAddressConsumed = currentBinRecord->Address + PayloadBufferBinRecordDataOffset;

        // dataLength is the number of uncopied bytes in the current bin record, or the 
        // remaining size of the payload. Whichever is less.
        //
        dataLength = currentBinRecord->Length - PayloadBufferBinRecordDataOffset;
        if (dataLength > remainingPayloadBufferLength)
        {
            dataLength = remainingPayloadBufferLength;
        }

        errorNumber = memcpy_s(&(payload->PayloadData[payloadBufferOffset]),
                               remainingPayloadBufferLength, 
                               &(currentBinRecord->BinData[PayloadBufferBinRecordDataOffset]),
                               dataLength);
        if (errorNumber)
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "memcpy_s fails with errno_t: %d", 
                        errorNumber);
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit;
        }

        // Keep track of how many bytes remain in our payload.
        //
        remainingPayloadBufferLength -= dataLength;
        // Gather the address we have consumed so far.
        //
        lastAddressConsumed += dataLength;
        // Increment pointer to the next empty byte in our payload.
        //
        payloadBufferOffset += dataLength;
        // Keep track of how many bytes we consumed of the current bin record.
        //
        PayloadBufferBinRecordDataOffset += dataLength;
        payload->DataLength = payloadBufferOffset;

        // If we are done reading this bin record. Advance to the next one.
        //
        if (PayloadBufferBinRecordDataOffset == currentBinRecord->Length)
        {
            PayloadBufferBinRecordStartIndex += BinRecordHeaderLength + currentBinRecord->Length;
            // Check if this was the last block.
            //
            if (PayloadBufferBinRecordStartIndex == PayloadBufferSize)
            {
                // We consumed all the data.
                //
                payload->Flags |= COMPONENT_FIRMWARE_UPDATE_FLAG_LAST_BLOCK;
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            DMF_TRACE,
                            "Last Block at PayloadBufferIndex[0x%x] length 0x%x TransferBufferSize 0x%Ix",
                            PayloadBufferBinRecordStartIndex - BinRecordHeaderLength - currentBinRecord->Length,
                            dataLength,
                            TransferBufferSize);
                break;
            }

            // Check if there is enough buffer left.
            //
            if (PayloadBufferSize - PayloadBufferBinRecordStartIndex < BinRecordHeaderLength)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "Payload Buffer is corrupted. Size %Iu is less than the minimum required (%u)",
                            (PayloadBufferSize - PayloadBufferBinRecordStartIndex),
                            BinRecordHeaderLength);
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                goto Exit;
            }

            currentBinRecord = (BIN_RECORD*)(PayloadBuffer + PayloadBufferBinRecordStartIndex);
            PayloadBufferBinRecordDataOffset = 0;
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "next block: PayloadBufferIndex[0x%x] address 0x%x length 0x%x",
                        PayloadBufferBinRecordStartIndex,
                        currentBinRecord->Address,
                        currentBinRecord->Length);
        }


        if (remainingPayloadBufferLength == 0)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "Buffer Full PayloadBufferIndex[0x%x] Position: [0x%x]",
                        PayloadBufferBinRecordStartIndex,
                        currentBinRecord->Address + PayloadBufferBinRecordDataOffset);
            break;
        }

        // Verify that nextAddress is sequentially after address.
        //
        if (lastAddressConsumed != currentBinRecord->Address)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "Cannot clump nonsequential messages");
            break;
        }

    } 

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "Clumped %x data till lastAddress[0x%x]",
                payloadBufferOffset,
                lastAddressConsumed);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "Exiting with PayloadBufferStartIndex[0x%x]",
                PayloadBufferBinRecordStartIndex);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()
//-- Helper functions ---
//--------END------------

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
ComponentFirmwareUpdate_ComponentFirmwareUpdateDeinitialize(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy the context information.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Clean up registry keys.
    //
    if (NULL != moduleContext->DeviceRegistryKey)
    {
        WdfRegistryClose(moduleContext->DeviceRegistryKey);
        moduleContext->DeviceRegistryKey = NULL;
    }

    // Clean up collection.
    //
    if (NULL != moduleContext->FirmwareBlobCollection)
    {
        WdfObjectDelete(moduleContext->FirmwareBlobCollection);
        moduleContext->FirmwareBlobCollection = NULL;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
ComponentFirmwareUpdate_ComponentFirmwareUpdateInitialize(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize's this Module's context.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    ULONG firmwareComponentIndex;
    size_t offerBufferSize;
    size_t payloadBufferSize;
    BYTE* payloadBufferFromClient = NULL;
    BYTE* offerBufferFromClient = NULL;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Validate the Module Config.
    //

    // Callback functions are mandatory as they provide the payload and offer blob.
    //
    if ((moduleConfig->EvtComponentFirmwareUpdateFirmwareOfferGet == NULL) ||
        (moduleConfig->EvtComponentFirmwareUpdateFirmwarePayloadGet == NULL))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "Invalid callback function to get offer/payload 0x%p 0x%p", 
                    moduleConfig->EvtComponentFirmwareUpdateFirmwareOfferGet,
                    moduleConfig->EvtComponentFirmwareUpdateFirmwarePayloadGet);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Check to ensure the length is in bound.
    // (+1) to Consider the NULL termination also.
    //
    if ((moduleConfig->InstanceIdentifierLength + 1) > MAX_INSTANCE_IDENTIFIER_LENGTH)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Invalid size of Instance Identifier String %d",
                    moduleConfig->InstanceIdentifierLength);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "Number of Firmaware components %d",
                moduleConfig->NumberOfFirmwareComponents);


    // Open ServiceName registry subkey under the device's hardware key.
    // Registry location {DESKTOP}: HKLM\SYSTEM\CurrentControlSet\Enum\{5E9A8CDC-14AB-4609-A017-68BCE594AB68}\<ServiceName>\.
    //
    WDFKEY key = NULL;
    ntStatus = WdfDeviceOpenRegistryKey(device,
                                        PLUGPLAY_REGKEY_DEVICE | WDF_REGKEY_DEVICE_SUBKEY,
                                        (KEY_READ | KEY_SET_VALUE),
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &key);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfDeviceOpenRegistryKey failed to open device hw key ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    moduleContext->DeviceRegistryKey = key;

    // Create a collection to hold all the firmware informations.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfCollectionCreate(&objectAttributes,
                                   &moduleContext->FirmwareBlobCollection);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE,
                    DMF_TRACE,
                    "WdfCollectionCreate fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    // For each of the firmware components that we need to consume, get the firmware information from the client.
    //
    for (firmwareComponentIndex = 0;
         firmwareComponentIndex < moduleConfig->NumberOfFirmwareComponents;
         ++firmwareComponentIndex)
    {
        // Get payload buffer and payload size.
        //
        payloadBufferSize = 0;
        ntStatus = moduleConfig->EvtComponentFirmwareUpdateFirmwarePayloadGet(DmfModule,
                                                                              firmwareComponentIndex,
                                                                              &payloadBufferFromClient,
                                                                              &payloadBufferSize);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "EvtComponentFirmwareUpdateFirmwarePayloadGet at %d fails: ntStatus=%!STATUS!",
                        firmwareComponentIndex,
                        ntStatus);
            goto Exit;
        }

        // Get offer buffer and offer size.
        //
        offerBufferSize = 0;
        ntStatus = moduleConfig->EvtComponentFirmwareUpdateFirmwareOfferGet(DmfModule,
                                                                            firmwareComponentIndex,
                                                                            &offerBufferFromClient,
                                                                            &offerBufferSize);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "EvtComponentFirmwareUpdateFirmwareOfferGet at %d fails: ntStatus=%!STATUS!",
                        firmwareComponentIndex,
                        ntStatus);
            goto Exit;
        }

        // Can not have zero size offer or payload buffers.
        //
        if (payloadBufferSize == 0 ||
            offerBufferSize == 0)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "EvtComponentFirmwareUpdateFirmwareOfferGet at %d fails: 0 size firmware!",
                        firmwareComponentIndex);
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

        // Can not have null offer or payload buffers.
        //
        if (offerBufferFromClient == NULL ||
            payloadBufferFromClient == NULL)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "EvtComponentFirmwareUpdateFirmwareOfferGet at %d fails: null firmware buffer!",
                        firmwareComponentIndex);
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

        WDFMEMORY firmwareMemory;
        FIRMWARE_INFORMATION* firmwareInformation;
        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = moduleContext->FirmwareBlobCollection;
        ntStatus = WdfMemoryCreate(&objectAttributes,
                                   NonPagedPoolNx,
                                   0,
                                   sizeof(FIRMWARE_INFORMATION),
                                   &firmwareMemory,
                                   (VOID**)&firmwareInformation);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "WdfMemoryCreate for Firmware fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }

        ZeroMemory(firmwareInformation,
                   sizeof(*firmwareInformation));

        firmwareInformation->OfferSize = offerBufferSize;
        firmwareInformation->PayloadSize = payloadBufferSize;
        firmwareInformation->OfferContentMemory = WDF_NO_HANDLE;
        firmwareInformation->PayloadContentMemory = WDF_NO_HANDLE;

        // Client's firmware buffers are not persisted. So will need to keep a copy internally.
        // Allocate memory locally and copy the firmware buffer contents.
        //
        if (moduleConfig->FirmwareBuffersNotInPresistantMemory)
        {
            WDFMEMORY payloadMemory;
            BYTE* payloadBufferLocallyCreated;
            WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
            objectAttributes.ParentObject = firmwareMemory;
            ntStatus = WdfMemoryCreate(&objectAttributes,
                                       NonPagedPoolNx,
                                       MemoryTag,
                                       firmwareInformation->PayloadSize,
                                       &payloadMemory,
                                       (VOID**)&payloadBufferLocallyCreated);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "WdfMemoryCreate for Firmware fails: ntStatus=%!STATUS!",  
                            ntStatus);
                goto Exit;
            }

            CopyMemory(payloadBufferLocallyCreated,
                       payloadBufferFromClient,
                       payloadBufferSize);

            firmwareInformation->PayloadContentMemory = payloadMemory;
        }
        else
        {
            // Use the Buffer from client; Don't copy.
            //
            WDFMEMORY payloadMemory;
            WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
            objectAttributes.ParentObject = firmwareMemory;
            ntStatus = WdfMemoryCreatePreallocated(&objectAttributes,
                                                   payloadBufferFromClient,
                                                   payloadBufferSize,
                                                   &payloadMemory);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "WdfMemoryCreatePreallocated for Firmware fails: ntStatus=%!STATUS!",
                            ntStatus);
                goto Exit;
            }
            firmwareInformation->PayloadContentMemory = payloadMemory;
        }

        // Client's firmware buffers are not persisted. So will need to keep a copy internally.
        // Allocate memory locally and copy the firmware buffer contents.
        //
        if (moduleConfig->FirmwareBuffersNotInPresistantMemory)
        {
            WDFMEMORY offerMemory;
            BYTE* offerBufferLocallyCreated;
            WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
            objectAttributes.ParentObject = firmwareMemory;
            ntStatus = WdfMemoryCreate(&objectAttributes,
                                       NonPagedPoolNx,
                                       MemoryTag,
                                       firmwareInformation->OfferSize,
                                       &offerMemory,
                                       (VOID**)&offerBufferLocallyCreated);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "WdfMemoryCreate for Firmware fails: ntStatus=%!STATUS!",  
                            ntStatus);
                goto Exit;
            }

            CopyMemory(offerBufferLocallyCreated,
                       offerBufferFromClient,
                       offerBufferSize);

            firmwareInformation->OfferContentMemory = offerMemory;
        }
        else
        {
            // Use the Buffer from client; Don't copy.
            //
            WDFMEMORY offerMemory;
            WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
            objectAttributes.ParentObject = firmwareMemory;
            ntStatus = WdfMemoryCreatePreallocated(&objectAttributes,
                                                   offerBufferFromClient,
                                                   offerBufferSize,
                                                   &offerMemory);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "WdfMemoryCreatePreallocated for Firmware fails: ntStatus=%!STATUS!",
                            ntStatus);
                goto Exit;
            }
            firmwareInformation->OfferContentMemory = offerMemory;
        }

        // Add the memory to collection. These will be retrieved during the protocol sequence.
        //
        ntStatus = WdfCollectionAdd(moduleContext->FirmwareBlobCollection,
                                    firmwareMemory);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "WdfCollectionAdd for firmware fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }
    }

Exit:

    // Cleanup everything if we ran into a problem.
    //
    if (! NT_SUCCESS(ntStatus))
    {
        ComponentFirmwareUpdate_ComponentFirmwareUpdateDeinitialize(DmfModule);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// Registry Related (BEGIN)
//-------------------------

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_RegistryComponentValueNameGet(
    _In_ DMFMODULE DmfModule,
    _In_ PCWSTR RegistryValueName,
    _In_ BYTE ComponentIdentifier,
    _Inout_ UNICODE_STRING* RegistryValueNameString
    )
/*++

Routine Description:

    Build a registry Name value string based on the provided Component Identifier and ValueName.

Arguments:

    DmfModule - This Module's DMF Object.
    RegistryValueName - Value Name to be used while generating the registry value name string.
    ComponentIdentifier - Component Identifier to be used to uniquely identify registry entry.
    RegistryValueNameString- A Unicode string that returns the generated registry value name string.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    HRESULT hr;

    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(RegistryValueNameString != NULL);
    DmfAssert(RegistryValueNameString->MaximumLength != 0);

    RegistryValueNameString->Length = 0;

    // Create the Key Value Name string with the information provided.
    //
    // Key Value Name:  generate could be 
    // InstanceID:<ModuleConfig->InstanceIdentifier>:Component<ComponentIdentifier><RegistryValueName>
    //   OR
    // Component<ComponentIdentifier><RegistryValueName>
    // depending on whether Module configuration has an instance identifier or not.
    //

    // E.g.
    // If RegistryValueName is "FwUpdateStatus" & ModuleConfig->InstanceIdentifier is "84229" & ComponentIdentifier is 7
    //    the function returns the following string in RegistryValueNameString "InstanceID:84229:Component7FwUpdateStatus".
    //
    if (moduleConfig->InstanceIdentifierLength != 0)
    {
        // Create the registry value name as InstanceID:<ModuleConfig->InstanceIdentifier>:Component<ComponentIdentifier><ValueName>.
        //
        hr = StringCchPrintf(RegistryValueNameString->Buffer,
                             RegistryValueNameString->MaximumLength,
                             L"InstanceID:%s:Component%02X%s",
                             moduleConfig->InstanceIdentifier,
                             ComponentIdentifier,
                             RegistryValueName);
    }
    else
    {
        // Create the registry value name as Component<ComponentIdentifier><ValueName>.
        //
        hr = StringCchPrintf(RegistryValueNameString->Buffer,
                             RegistryValueNameString->MaximumLength,
                             L"Component%02X%s",
                             ComponentIdentifier,
                             RegistryValueName);
    }

    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "StringCchPrintf fails: %!HRESULT!",
                    hr);
        // This can fail with STRSAFE_E_INVALID_PARAMETER/STRSAFE_E_INSUFFICIENT_BUFFER.
        // Treat as INVALID_PARAMETER.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    size_t combinedValueNameLength;
    // NOTE: combinedValueNameLength excludes terminating null.
    //
    hr = StringCbLength(RegistryValueNameString->Buffer,
                        RegistryValueNameString->MaximumLength,
                        &combinedValueNameLength);
    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "StringCbLength fails: %!HRESULT!",
                    hr);
        // This can fail with STRSAFE_E_INVALID_PARAMETER.
        // Treat as INVALID_PARAMETER.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    RegistryValueNameString->Length = (USHORT)combinedValueNameLength;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_RegistryRemoveComponentValue(
    _In_ DMFMODULE DmfModule,
    _In_ PCWSTR RegistryValueName,
    _In_ BYTE ComponentIdentifier
    )
/*++

Routine Description:

    Removes a single value to the registry with the Value Name based on Component Indentifier and ValueName.

Arguments:

    DmfModule - This Module's DMF Object.
    ValueName - Name of the Registry Value to be removed.
    ComponentIdentifier - Component Identifier for identifying the unique registry entry.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    const size_t combinedValueNameMaxLength = MAXIMUM_VALUE_NAME_SIZE;
    WCHAR combinedValueNameBuffer[combinedValueNameMaxLength] = { 0 };
    UNICODE_STRING registryValueNameString;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(RegistryValueName != NULL);

    RtlInitUnicodeString(&registryValueNameString,
                         combinedValueNameBuffer);
    registryValueNameString.MaximumLength = (USHORT)combinedValueNameMaxLength;

    ntStatus = ComponentFirmwareUpdate_RegistryComponentValueNameGet(DmfModule,
                                                                     RegistryValueName,
                                                                     ComponentIdentifier,
                                                                     &registryValueNameString);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ComponentFirmwareUpdate_RegistryComponentValueNameGet fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    // Remove from Device registry location.
    //
    ntStatus = WdfRegistryRemoveValue(moduleContext->DeviceRegistryKey,
                                      &registryValueNameString);
    if (ntStatus == STATUS_OBJECT_NAME_NOT_FOUND)
    {
        ntStatus = STATUS_SUCCESS;
    }
    else if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceRegistryRemoveValue fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus = %!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_RegistryAssignComponentUlong(
    _In_ DMFMODULE DmfModule,
    _In_ PCWSTR RegistryValueName,
    _In_ BYTE ComponentIdentifier,
    _In_ ULONG RegistryValue
    )
/*++

Routine Description:

    Writes a single value to the registry with the Value Name based on Component Identifier and ValueName.

Arguments:

    DmfModule - This Module's DMF Object.
    RegistryValueName - Name of the Registry Value to be assigned.
    ComponentIdentifier - Component Identifier for identifying the unique registry entry.
    RegistryValue - Registry value to be assigned to.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    const size_t combinedValueNameMaxLength = MAXIMUM_VALUE_NAME_SIZE;
    WCHAR combinedValueNameBuffer[combinedValueNameMaxLength] = { 0 };
    UNICODE_STRING registryValueNameString;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(RegistryValueName != NULL);
    RtlInitUnicodeString(&registryValueNameString,
                         combinedValueNameBuffer);
    registryValueNameString.MaximumLength = (USHORT)combinedValueNameMaxLength;

    ntStatus = ComponentFirmwareUpdate_RegistryComponentValueNameGet(DmfModule,
                                                                     RegistryValueName,
                                                                     ComponentIdentifier, 
                                                                     &registryValueNameString);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "FirmwareUpdate_RegistryComponentValueNameGet fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    ntStatus = WdfRegistryAssignULong(moduleContext->DeviceRegistryKey,
                                      &registryValueNameString,
                                      RegistryValue);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "RegistryAssignComponentUlong fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus = %!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_RegistryQueryComponentUlong(
    _In_ DMFMODULE DmfModule,
    _In_ PCWSTR RegistryValueName,
    _In_ BYTE ComponentIdentifier,
    _In_ ULONG* RegistryValue
    )
/*++

Routine Description:

    Query a ulong value from registry with the Value Name based on Component Identifier and ValueName.

Arguments:

    DmfModule - This Module's DMF Object.
    RegistryValueName - Name of the Registry Value to be assigned.
    ComponentIdentifier - Component Identifier for identifying the unique registry entry.
    RegistryValue - Registry value returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    const size_t combinedValueNameMaxLength = MAXIMUM_VALUE_NAME_SIZE;
    WCHAR combinedValueNameBuffer[combinedValueNameMaxLength] = { 0 };
    UNICODE_STRING registryValueNameString;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(RegistryValueName != NULL);
    DmfAssert(RegistryValue != NULL);

    RtlInitUnicodeString(&registryValueNameString,
                         combinedValueNameBuffer);
    registryValueNameString.MaximumLength = (USHORT)combinedValueNameMaxLength;

    ntStatus = ComponentFirmwareUpdate_RegistryComponentValueNameGet(DmfModule,
                                                                     RegistryValueName,
                                                                     ComponentIdentifier, 
                                                                     &registryValueNameString);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "FirmwareUpdate_RegistryComponentValueNameGet fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    ntStatus = WdfRegistryQueryULong(moduleContext->DeviceRegistryKey,
                                     &registryValueNameString,
                                     RegistryValue);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "WdfRegistryQueryULong fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// Registry Related  (END)
//------------------------

// Transport Related (BEGIN)
//--------------------------

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_OfferCommandSend(
    _In_ DMFMODULE DmfModule,
    _In_ COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_CODE OfferCommandCode,
    _Out_ COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE* OfferResponseStatus,
    _Out_ COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON* OfferResponseReason
    )
/*++

Routine Description:

    Send an offer command to the device and retrieve response.

Arguments:

    DmfModule - This Module's DMF Object.
    OfferCommandCode - Offer Command to Send.
    OfferResponseStatus - Response received for the command.
    OfferResponseReason - Reason for the response.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;
    CONTEXT_ComponentFirmwareUpdateTransport* componentFirmwareUpdateTransportContext;

    ULONG transportWaitTimeout;
    ULONG offerCommand;

    const BYTE informationPacketMarker = FWUPDATE_COMMAND_TOKEN;
    const BYTE outputToken = FWUPDATE_DRIVER_TOKEN;

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY offerCommandMemory;
    UCHAR* bufferHeader;
    UCHAR* offerCommandBuffer;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(OfferResponseStatus != NULL);
    DmfAssert(OfferResponseReason != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    componentFirmwareUpdateTransportContext = ComponentFirmwareUpdateTransportContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransportContext != NULL);

    size_t allocatedSize = componentFirmwareUpdateTransportContext->TransportOfferBufferRequiredSize + componentFirmwareUpdateTransportContext->TransportHeaderSize;
    offerCommandMemory = WDF_NO_HANDLE;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                                NonPagedPoolNx,
                                0,
                                allocatedSize,
                                &offerCommandMemory,
                                (VOID**)&bufferHeader);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfMemoryCreate for OfferCommandSend fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    RtlZeroMemory(bufferHeader,
                  allocatedSize);

    // Update Byte 0, 2 and 3.
    //
    offerCommand = 0;
    offerCommand |= ((BYTE)OfferCommandCode << 0);
    offerCommand |= (informationPacketMarker << 16);
    offerCommand |= (outputToken << 24);

    offerCommandBuffer = bufferHeader + componentFirmwareUpdateTransportContext->TransportHeaderSize;
    RtlCopyMemory(offerCommandBuffer,
                  &offerCommand,
                  sizeof(offerCommand));

    ntStatus = DMF_ComponentFirmwareUpdate_TransportOfferCommandSend(moduleContext->DmfInterfaceComponentFirmwareUpdate,
                                                                     bufferHeader,
                                                                     allocatedSize,
                                                                     componentFirmwareUpdateTransportContext->TransportHeaderSize);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_ComponentFirmwareUpdate_TransportOfferCommandSend fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Wait for response availability.
    // Adjust the timeout based on the specification.
    // for COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_NOTIFY_ON_READY - timeout is INFINITE.
    //
    if (OfferCommandCode == COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_NOTIFY_ON_READY)
    {
        transportWaitTimeout = INFINITE;
    }
    else
    {
        transportWaitTimeout = componentFirmwareUpdateTransportContext->TransportWaitTimeout;
    }

    // Wait for response.
    //
    ntStatus = ComponentFirmwareUpdate_WaitForResponse(DmfModule,
                                                       transportWaitTimeout);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WaitForResponse fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Offer Response Received.");

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    ntStatus = componentFirmwareUpdateTransactionContext->ntStatus;
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "OfferCommandSend fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    *OfferResponseStatus = componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus;
    *OfferResponseReason = componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseReason;

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Offer Command for %s(%d) returned response status %s(%d)", 
                ComponentFirmwareUpdateOfferCommandCodeString(OfferCommandCode), 
                OfferCommandCode, 
                ComponentFirmwareUpdateOfferResponseString(componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus),
                componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus);

    // Decide the next course of actions based on the response status.
    // 
    // In the absence of a formal state machine implementation, decisions are done in a switch case.
    //
    switch (componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus)
    {
        case COMPONENT_FIRMWARE_UPDATE_OFFER_ACCEPT:
        {
            // Expected Normal Result.
            //
            ;
            break;
        }
        case COMPONENT_FIRMWARE_UPDATE_OFFER_SKIP:
            // Fall through.
            //
        case COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT:
        {
            // These are unexpected returns.
            //
            ntStatus = STATUS_ABANDONED;

            TraceEvents(TRACE_LEVEL_WARNING,
                        DMF_TRACE,
                        "Offer Reject Reason Code %s(%d)",
                        ComponentFirmwareUpdateOfferResponseRejectString(componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseReason),
                        componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseReason);
            goto Exit;
        }
        case COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_READY:
        {
            // Expected Result.
            //
            ;
            break;
        }
        case COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_NOT_SUPPORTED:
        {
            // Expected Result.
            //
            ;

            TraceEvents(TRACE_LEVEL_WARNING, 
                        DMF_TRACE, 
                        "Offer Command for %s(%d) was not supported", 
                        ComponentFirmwareUpdateOfferCommandCodeString(OfferCommandCode), 
                        OfferCommandCode);
            break;
        }
        default:
        {
            // Unexpected returns.
            //
            ntStatus = STATUS_ABANDONED;

            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "Received unknown offerResponseStatus %d", 
                        componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus);
            goto Exit;
        }
    }

Exit:

    if (offerCommandMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(offerCommandMemory);
        offerCommandMemory = WDF_NO_HANDLE;
    }

    FuncExit(DMF_TRACE, "%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_SendOfferInformation(
    _In_ DMFMODULE DmfModule,
    _In_ COMPONENT_FIRMWARE_UPDATE_OFFER_INFORMATION_CODE OfferInformationCode,
    _Out_ COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE* OfferResponseStatus,
    _Out_ COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON* OfferResponseReason
    )
/*++

Routine Description:

    Send an offer information meta data to the transport and retrieve response.

Arguments:

    DmfModule - This Module's DMF Object.
    OfferInformationCode - Offer Information Code to Send.
    OfferResponseStatus - Response to the command.
    OfferResponseReason - Reason for the response.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;

    ULONG offerInformation;
    const BYTE informationPacketMarker = FWUPDATE_INFORMATION_TOKEN;
    const BYTE outputToken = FWUPDATE_DRIVER_TOKEN;

    WDFMEMORY offerInformationMemory;
    UCHAR* bufferHeader;
    UCHAR* offerInformationBuffer;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(OfferResponseStatus != NULL);
    DmfAssert(OfferResponseReason != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    CONTEXT_ComponentFirmwareUpdateTransport* componentFirmwareUpdateTransportContext;
    componentFirmwareUpdateTransportContext = ComponentFirmwareUpdateTransportContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransportContext != NULL);

    size_t allocatedSize = componentFirmwareUpdateTransportContext->TransportOfferBufferRequiredSize + componentFirmwareUpdateTransportContext->TransportHeaderSize;
    offerInformationMemory = WDF_NO_HANDLE;

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                                NonPagedPoolNx,
                                0,
                                allocatedSize,
                                &offerInformationMemory,
                                (VOID**)&bufferHeader);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfMemoryCreate for OfferInformationSend fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    RtlZeroMemory(bufferHeader,
                  allocatedSize);

    // Update Byte 0, 2 and 3.
    //
    offerInformation = 0;
    offerInformation |= ((BYTE)OfferInformationCode << 0);
    offerInformation |= (informationPacketMarker << 16);
    offerInformation |= (outputToken << 24);

    offerInformationBuffer = bufferHeader + componentFirmwareUpdateTransportContext->TransportHeaderSize;
    RtlCopyMemory(offerInformationBuffer,
                  &offerInformation, 
                  sizeof(offerInformation));

    ntStatus = DMF_ComponentFirmwareUpdate_TransportOfferInformationSend(moduleContext->DmfInterfaceComponentFirmwareUpdate,
                                                                         bufferHeader,
                                                                         allocatedSize,
                                                                         componentFirmwareUpdateTransportContext->TransportHeaderSize);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_ComponentFirmwareUpdateTransport_OfferInformationSend fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Wait for response.
    //
    ntStatus = ComponentFirmwareUpdate_WaitForResponse(DmfModule,
                                                       componentFirmwareUpdateTransportContext->TransportWaitTimeout);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WaitForResponse fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Offer Response Received.");

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    ntStatus = componentFirmwareUpdateTransactionContext->ntStatus;
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "OfferInformationSend fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    *OfferResponseStatus = componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus;
    *OfferResponseReason = componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseReason;

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Offer Information for %s(%d) returned response status %s(%d)", 
                ComponentFirmwareUpdateOfferInformationCodeString(OfferInformationCode), 
                OfferInformationCode, 
                ComponentFirmwareUpdateOfferResponseString(componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus),
                componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus);

    // Decide the next course of actions based on the response status.
    // 
    // In the absence of a formal state machine implementation, decisions are done in a switch case.
    //
    switch (*OfferResponseStatus)
    {
        case COMPONENT_FIRMWARE_UPDATE_OFFER_ACCEPT:
        {
            // Expected Normal Result.
            //
            ;
            break;
        }
        case COMPONENT_FIRMWARE_UPDATE_OFFER_SKIP:
            // Fall through.
            //
        case COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT:
        {
            // These are unexpected returns.
            //
            ntStatus = STATUS_ABANDONED;

            TraceEvents(TRACE_LEVEL_VERBOSE, 
                        DMF_TRACE, 
                        "Offer Reject Reason Code %s(%d)", 
                        ComponentFirmwareUpdateOfferResponseRejectString(componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseReason),
                        componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseReason);
            goto Exit;
        }
        default:
        {
            // These are unexpected returns.
            //
            ntStatus = STATUS_ABANDONED;

            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "Received unknown offerResponseStatus %d", 
                        componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus);
            goto Exit;
        }
    };

Exit:

    if (offerInformationMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(offerInformationMemory);
        offerInformationMemory = WDF_NO_HANDLE;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_OfferSend(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(OfferBufferSize) ULONG* OfferBuffer,
    _In_ ULONG OfferBufferSize,
    _Out_ COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE* OfferResponseStatus,
    _Out_ COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON* OfferResponseReason
    )
/*++

Routine Description:

    Send the offer data to the device and receive the response.

Arguments:

    DmfModule - This Module's DMF Object.
    OfferBuffer -- offer data to be sent to the device.
    OfferBufferSize - offer data size.
    OfferResponseStatus -- response received from device.
    OfferResponseRejectReason - reject reason code for the response.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;

    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;
    CONTEXT_ComponentFirmwareUpdateTransport* componentFirmwareUpdateTransportContext;

    const UINT numberOfUlongsInOffer = 4;
    const ULONG offerSize = sizeof(ULONG) * numberOfUlongsInOffer;
    const BYTE outputToken = FWUPDATE_DRIVER_TOKEN;

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY offerMemory;
    UCHAR* bufferHeader;
    ULONG* offerBuffer;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(OfferBuffer != NULL);
    DmfAssert(OfferResponseStatus != NULL);
    DmfAssert(OfferResponseReason != NULL);

    UNREFERENCED_PARAMETER(OfferBufferSize);

    ntStatus = STATUS_SUCCESS;
    offerMemory = WDF_NO_HANDLE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    componentFirmwareUpdateTransportContext = ComponentFirmwareUpdateTransportContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransportContext != NULL);

    size_t allocatedSize = componentFirmwareUpdateTransportContext->TransportOfferBufferRequiredSize + componentFirmwareUpdateTransportContext->TransportHeaderSize;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                                NonPagedPoolNx,
                                0,
                                allocatedSize,
                                &offerMemory,
                                (VOID**)&bufferHeader);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfMemoryCreate for Offer fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    RtlZeroMemory(bufferHeader,
                  allocatedSize);

    offerBuffer = (ULONG*) (bufferHeader + componentFirmwareUpdateTransportContext->TransportHeaderSize);
    ULONG* offersFromBlob = (ULONG*)OfferBuffer;

    for (USHORT blobIndex = 0; blobIndex < numberOfUlongsInOffer; ++blobIndex)
    {
        offerBuffer[blobIndex] = offersFromBlob[blobIndex];
    }

    // Update Component info field of offer as needed.
    //
    // Set the Most Significant Byte to outputToken.
    //
    offerBuffer[0] = (offerBuffer[0] & 0x00FFFFFF) | (outputToken << 24);

    if (moduleConfig->ForceImmediateReset)
    {
        // Set the Force Immediate Reset bit to 1.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Setting Force Immediate Reset bit");
        offerBuffer[0] |= (1 << 14);
    }

    if (moduleConfig->ForceIgnoreVersion)
    {
        // Set the Force Ignore Version bit to 1.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Setting Force Ignore Version bit");
        offerBuffer[0] |= (1 << 15);
    }

    ntStatus = DMF_ComponentFirmwareUpdate_TransportOfferSend(moduleContext->DmfInterfaceComponentFirmwareUpdate,
                                                              bufferHeader,
                                                              allocatedSize,
                                                              componentFirmwareUpdateTransportContext->TransportHeaderSize);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_ComponentFirmwareUpdate_TransportOfferSend fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Wait for response.
    //
    ntStatus = ComponentFirmwareUpdate_WaitForResponse(DmfModule,
                                                       componentFirmwareUpdateTransportContext->TransportWaitTimeout);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WaitForResponse fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Offer Response Received.");

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    ntStatus = componentFirmwareUpdateTransactionContext->ntStatus;
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "OfferSend Fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    *OfferResponseStatus = componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus;
    *OfferResponseReason = componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseReason;

Exit:

    if (offerMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(offerMemory);
        offerMemory = WDF_NO_HANDLE;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_SendPayload(
    _In_ DMFMODULE DmfModule,
    _In_ UINT32 PayloadIndex,
    _In_ BYTE ComponentIdentifier,
    _Out_ COMPONENT_FIRMWARE_UPDATE_PAYLOAD_RESPONSE* PayloadResponse
    )
/*++

Routine Description:

    Retrieve the payload data for the specified index from the context and send it to the device & receive the response.

Arguments:

    DmfModule - This Module's DMF Object.
    PayloadIndex - Index of this payload in the payload collection.
    ComponentIdentifier - Component Indentifier that uniquely identifies this component being updated.
    PayloadResponse - Response received from the device.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;

    FIRMWARE_INFORMATION* firmwareInformation;
    WDFMEMORY firmwareInformationMemory;

    ULONG* payloadContent;
    size_t payloadSizeFromCollection;

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY payloadChunkMemory;

    // Buffer that is send down to hardware.
    //
    UCHAR* bufferHeader;
    UCHAR* payloadBuffer;
    BYTE payloadBufferLength;

    // Index in the whole payload buffer that tracks the begining of a Bin Record.
    // Each Bin Record has {address, length, data}
    //
    ULONG payloadBufferBinRecordStartIndex = 0;

    // Keep track of where we left off.
    //
    ULONG resumePayloadBufferBinRecordStartIndex = 0;

    // Offset in the current bin record in the payload buffer.
    //
    BYTE payloadBufferBinRecordDataOffset = 0;

    // Keep track of this offset in case of interruption.
    //
    ULONG resumePayloadBufferBinRecordDataOffset = 0;

    // Do not start at 0 due to firmware limitations.
    //
    const UINT16 sequenceNumberStart = 0x0001;
    UINT16 sequenceNumber = 0;
    UINT16 resumeSequenceNumber = 0;

    // Read a ULONG from registry.
    //
    ULONG resumeSequenceNumberFromRegistry = 0;

    BOOL updateInterruptedFromIoFailure = FALSE;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(PayloadResponse != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(PayloadIndex < WdfCollectionGetCount(moduleContext->FirmwareBlobCollection));

    firmwareInformationMemory = (WDFMEMORY)WdfCollectionGetItem(moduleContext->FirmwareBlobCollection, 
                                                                PayloadIndex);

    firmwareInformation = (FIRMWARE_INFORMATION*)WdfMemoryGetBuffer(firmwareInformationMemory, 
                                                                    NULL);

    payloadContent = (ULONG*)WdfMemoryGetBuffer(firmwareInformation->PayloadContentMemory,
                                                &payloadSizeFromCollection);

    DmfAssert(payloadSizeFromCollection == firmwareInformation->PayloadSize);
    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    CONTEXT_ComponentFirmwareUpdateTransport* componentFirmwareUpdateTransportContext;
    componentFirmwareUpdateTransportContext = ComponentFirmwareUpdateTransportContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransportContext != NULL);

    payloadChunkMemory = WDF_NO_HANDLE;

    // Allocate memory for payload chunk, and reuse it for the sending the whole payload.
    //
    size_t allocatedSize = componentFirmwareUpdateTransportContext->TransportFirmwarePayloadBufferRequiredSize + componentFirmwareUpdateTransportContext->TransportHeaderSize;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               0,
                               allocatedSize,
                               &payloadChunkMemory,
                               (VOID**)&bufferHeader);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfMemoryCreate for Offer fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    RtlZeroMemory(bufferHeader,
                  allocatedSize);

    // Ensure the driver is packing 60 bytes of payload everytime.
    //
    payloadBuffer = (UCHAR*)(bufferHeader + componentFirmwareUpdateTransportContext->TransportHeaderSize);
    payloadBufferLength = SizeOfPayload;

    // Check whether the update should resume from a previously interrupted update.
    // This can only occur if the same pair 'that was interrupted last attempt matches the first pair to be accepted this attempt'.
    //
    payloadBufferBinRecordStartIndex = 0;
    sequenceNumber = sequenceNumberStart;
    do
    {
        // No need to check further if the Resume On Connect is not supported.
        //
        if (moduleConfig->SupportResumeOnConnect == FALSE)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, 
                        DMF_TRACE, 
                        "ResumeOnConnect is not supported");
            break;
        }

        ULONG resumeOnConnect = 0;
        ULONG resumePairIndex = 0;

        // Check whether a resume is desired. 
        // This is TRUE if we had an interruption on our previous payload send attempt for the same component.
        //
        ntStatus = ComponentFirmwareUpdate_RegistryQueryComponentUlong(DmfModule,
                                                                       ComponentFirmwareUpdate_ResumeOnConnectValueName,
                                                                       ComponentIdentifier,
                                                                       &resumeOnConnect);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "ComponentFirmwareUpdate_RegistryQueryComponentUlong fails for ResumeOnConnect ntStatus=%!STATUS!", 
                        ntStatus);
            break;
        }

        // Skip if we dont have any interruption that is resumable.
        //
        if (resumeOnConnect == 0)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, 
                        DMF_TRACE, 
                        "No interrupted and Resumable previous failed Payload Send attempt.");
            break;
        }

        // Make sure to set ResumeOnConnect to false so the next pair does not go through this again.
        //
        ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule, 
                                                                        ComponentFirmwareUpdate_ResumeOnConnectValueName,
                                                                        ComponentIdentifier, 
                                                                        FALSE);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "WdfRegistryAssignULong fails for ResumeOnConnect ntStatus=%!STATUS!", 
                        ntStatus);
            break;
        }

        // Get the payload data index to use upon resume.
        //
        ntStatus = ComponentFirmwareUpdate_RegistryQueryComponentUlong(DmfModule,
                                                                       ComponentFirmwareUpdate_ResumePayloadBufferBinRecordIndexValueName, 
                                                                       ComponentIdentifier, 
                                                                       &resumePayloadBufferBinRecordStartIndex);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "ComponentFirmwareUpdate_RegistryQueryComponentUlong failed for ResumeResourceDataIndex ntStatus=%!STATUS!", 
                        ntStatus);
            break;
        }

        // Get the sequence number to use upon resume.
        //
        ntStatus = ComponentFirmwareUpdate_RegistryQueryComponentUlong(DmfModule,
                                                                       ComponentFirmwareUpdate_ResumeSequenceNumberValueName,
                                                                       ComponentIdentifier, 
                                                                       &resumeSequenceNumberFromRegistry);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "WdfRegistryQueryULong fails for ResumeSequenceNumber ntStatus=%!STATUS!", 
                        ntStatus);
            break;
        }

        // Get the payload buffer offset to use upon resume.
        //
        ntStatus = ComponentFirmwareUpdate_RegistryQueryComponentUlong(DmfModule,
                                                                       ComponentFirmwareUpdate_ResumePayloadBufferBinRecordDataOffsetValueName,
                                                                       ComponentIdentifier, 
                                                                       &resumePayloadBufferBinRecordDataOffset);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "WdfRegistryQueryULong fails for ResumePayloadBufferAddressOffset ntStatus=%!STATUS!", 
                        ntStatus);
            break;
        }

        // PayloadBufferBinRecordDataOffset is of type Byte, this is a sanity check for registry value to make sure
        // the registry didn't corrupt the value.
        //
        DmfAssert(resumePayloadBufferBinRecordDataOffset <= BYTE_MAX);

        // Sequence number size is 2 Bytes.
        //
        DmfAssert(resumeSequenceNumberFromRegistry <= UINT16_MAX);
        resumeSequenceNumber = (UINT16) resumeSequenceNumberFromRegistry;
        TraceEvents(TRACE_LEVEL_INFORMATION, 
                    DMF_TRACE, 
                    "Resuming interrupted update on PairIndex %d at ResourceDataIndex %d with SequenceNumber %d", 
                    resumePairIndex, 
                    resumePayloadBufferBinRecordStartIndex,
                    resumeSequenceNumber);

        payloadBufferBinRecordStartIndex = resumePayloadBufferBinRecordStartIndex;
        sequenceNumber = resumeSequenceNumber;
        payloadBufferBinRecordDataOffset = (BYTE) resumePayloadBufferBinRecordDataOffset;
    } while (0);

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Started sending firmware data");

    // Index of the payload contents that need to be send to the device cannot be beyond or equal to the payload size. 
    // There needs to be have some payload content to be send (applicable for both resume from interrupted case or normal) 
    // otherwise it is an error case.
    //
    if (payloadBufferBinRecordStartIndex >= firmwareInformation->PayloadSize)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Proceed while there is some payload data still needed to send..
    //
    while (payloadBufferBinRecordStartIndex < firmwareInformation->PayloadSize)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Current sequenceNumber: %d, PayloadIndex: %d, Payload Total size: %Iu", 
                    sequenceNumber,
                    payloadBufferBinRecordStartIndex,
                    firmwareInformation->PayloadSize);
        
        // Preserve the currently completed numbers as a checkpoint.
        //
        resumeSequenceNumber = sequenceNumber;
        resumePayloadBufferBinRecordStartIndex = payloadBufferBinRecordStartIndex;
        resumePayloadBufferBinRecordDataOffset = payloadBufferBinRecordDataOffset;

        // Fill the output buffer with the next chunk of payload to send.
        //      Content is Copied From payloadContent to payloadBuffer.
        //      payloadBufferIndex is updated inside as the payloadBuffer is filled up.
        //
        ntStatus = ComponentFirmwareUpdate_PayloadBufferFill(DmfModule,
                                                             sequenceNumber,
                                                             (BYTE*)payloadContent,
                                                             firmwareInformation->PayloadSize,
                                                             payloadBufferBinRecordStartIndex,
                                                             payloadBufferBinRecordDataOffset,
                                                             payloadBuffer, 
                                                             payloadBufferLength);
        if (FAILED(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "PayloadBufferFill fails: ntStatus=%!STATUS!", 
                        ntStatus);
            goto Exit;
        }

        ntStatus = DMF_ComponentFirmwareUpdate_TransportPayloadSend(moduleContext->DmfInterfaceComponentFirmwareUpdate,
                                                                    bufferHeader,
                                                                    allocatedSize,
                                                                    componentFirmwareUpdateTransportContext->TransportHeaderSize);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "DMF_ComponentFirmwareUpdateTransport_PayloadSend fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }

        // Wait for response with the right sequence number.
        //
        ntStatus = ComponentFirmwareUpdate_PayloadResponseProcess(DmfModule,
                                                                  sequenceNumber,
                                                                  PayloadResponse);
        if (!NT_SUCCESS(ntStatus))
        {
            // Treat timeout as IoFailure.
            //
            if (ntStatus == STATUS_INVALID_DEVICE_STATE)
            {
                updateInterruptedFromIoFailure = TRUE;
            }

            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "PayloadResponseProcess fails: ntStatus=%!STATUS!", 
                        ntStatus);
            goto Exit;
        }

        if (*PayloadResponse != COMPONENT_FIRMWARE_UPDATE_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "PayloadResponseProcess returns: %d", 
                        *PayloadResponse);
            goto Exit;
            // Do not flag this with ntStatus.
            //
        }

        ++sequenceNumber;
    }

Exit:

    if (payloadChunkMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(payloadChunkMemory);
        payloadChunkMemory = WDF_NO_HANDLE;
        payloadBuffer = NULL;
    }

    // If the update was interrupted and the device supports resume on connect, store the current progress in the registry
    // Make sure to mark ResumeOnConnect last and not set it to true if any of the others fail.
    //
    if (moduleConfig->SupportResumeOnConnect && updateInterruptedFromIoFailure)
    {
        do
        {
            NTSTATUS ntStatusLocal;
            ntStatusLocal = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                 ComponentFirmwareUpdate_ResumePayloadBufferBinRecordIndexValueName,
                                                                                 ComponentIdentifier, 
                                                                                 resumePayloadBufferBinRecordStartIndex);
            if (!NT_SUCCESS(ntStatusLocal))
            {
                TraceEvents(TRACE_LEVEL_ERROR, 
                            DMF_TRACE, 
                            "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for ResumePayloadBufferBinRecordIndex: ntStatus=%!STATUS!", 
                            ntStatusLocal);
                break;
            }

            ntStatusLocal = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                 ComponentFirmwareUpdate_ResumePayloadBufferBinRecordDataOffsetValueName,
                                                                                 ComponentIdentifier, 
                                                                                 resumePayloadBufferBinRecordDataOffset);
            if (!NT_SUCCESS(ntStatusLocal))
            {
                TraceEvents(TRACE_LEVEL_ERROR, 
                            DMF_TRACE, 
                            "DMF_ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for ResumePayloadBufferBinRecordDataOffset: ntStatus=%!STATUS!", 
                            ntStatusLocal);
                break;
            }

            ntStatusLocal = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                 ComponentFirmwareUpdate_ResumeSequenceNumberValueName,
                                                                                 ComponentIdentifier, 
                                                                                 resumeSequenceNumber);
            if (!NT_SUCCESS(ntStatusLocal))
            {
                TraceEvents(TRACE_LEVEL_ERROR, 
                            DMF_TRACE, 
                            "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for ResumeSequenceNumber: ntStatus=%!STATUS!", 
                            ntStatusLocal);
                break;
            }

            ntStatusLocal = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                 ComponentFirmwareUpdate_ResumeOnConnectValueName,
                                                                                 ComponentIdentifier, 
                                                                                 TRUE);
            if (!NT_SUCCESS(ntStatusLocal))
            {
                TraceEvents(TRACE_LEVEL_ERROR, 
                            DMF_TRACE, 
                            "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for ResumeOnConnect: ntStatus=%!STATUS!", 
                            ntStatusLocal);
                break;
            }
        } while (0);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_FirmwareVersionsGet(
    _In_ DMFMODULE DmfModule,
    _Out_ COMPONENT_FIRMWARE_VERSIONS* VersionsOfFirmware
    )
/*++

Routine Description:

    Get the current version of the firmware from device.

Arguments:

    DmfModule - This Module's DMF Object.
    VersionsOfFirmware - returned version information from the device.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;
    CONTEXT_ComponentFirmwareUpdateTransport* componentFirmwareUpdateTransportContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(VersionsOfFirmware != NULL);
    ZeroMemory(VersionsOfFirmware, sizeof(*VersionsOfFirmware));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_ComponentFirmwareUpdateTransport_FirmwareVersionGet fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    componentFirmwareUpdateTransportContext = ComponentFirmwareUpdateTransportContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransportContext != NULL);

    // Wait for response.
    //
    ntStatus = ComponentFirmwareUpdate_WaitForResponse(DmfModule,
                                                       componentFirmwareUpdateTransportContext->TransportWaitTimeout);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WaitForResponse fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Firmware version Response Received.");

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    ntStatus = componentFirmwareUpdateTransactionContext->ntStatus;
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "FirmwareVersionGet fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    // Copy Firmware Information.
    //
    RtlCopyMemory(VersionsOfFirmware,
                  &componentFirmwareUpdateTransactionContext->FirmwareVersions,
                  sizeof(componentFirmwareUpdateTransactionContext->FirmwareVersions));
 
Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_ProtocolStop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This cleans up the protocol transaction and let the transport do its specific actions needed when the protocol is being stopped.

Parameters:

    DmfModule - This Module's DMF Object.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    // Set Cancel Event so that any pending wait for responses are returned.
    //
    DMF_Portable_EventSet(&componentFirmwareUpdateTransactionContext->DmfProtocolTransactionCancelEvent);

    // Let the specific action be done at the interface implementation.
    //
    ntStatus = DMF_ComponentFirmwareUpdate_TransportProtocolStop(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_ComponentFirmwareUpdate_TransportProtocolStop fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_ProtocolStart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This prepares for the protocol transaction and let the transport do its specific actions needed when the protocol is about to be started.

Parameters:

    DmfModule - This Module's DMF Object.

Return:

    NTSTATUS

--*/

{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    // Clear the Cancel Event that may have been set and not cleared.
    //
    DMF_Portable_EventReset(&componentFirmwareUpdateTransactionContext->DmfProtocolTransactionCancelEvent);

    // Let the specific action be done at the interface implementation.
    //
    ntStatus = DMF_ComponentFirmwareUpdate_TransportProtocolStart(moduleContext->DmfInterfaceComponentFirmwareUpdate);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_ComponentFirmwareUpdateTransport_ProtocolStart fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// Transport Related (END)
//--------------------------

// CFU Protocol Related (BEGIN)
//=============================

BOOL
ComponentFirmwareUpdate_IsProtocolStopRequestPending(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Checks whether a protocol stop request is already made or not.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    TRUE if stop is already requested, otherwise FALSE.

--*/
{
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    return DMF_Thread_IsStopPending(moduleContext->DmfModuleThread);
}

_Must_inspect_result_
static
NTSTATUS 
ComponentFirmwareUpdate_OfferVersionsRegistryDelete(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Helper function to delete all the offer versions in registry that may have been
    saved earlier.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;
    HANDLE handle;
    LPTSTR valueNameMemoryBuffer;
    WDFMEMORY valueNameMemory;
    DWORD  valueNameCount = 0;
    DWORD valueNameElementCountMaximum = 0;
    WDFCOLLECTION registryValueNamesToBeDeleted;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    registryValueNamesToBeDeleted = WDF_NO_HANDLE;
    valueNameMemory = WDF_NO_HANDLE;
    handle = WdfRegistryWdmGetHandle(moduleContext->DeviceRegistryKey);

    ntStatus = RegQueryInfoKey((HKEY)handle,
                                NULL,
                                0,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                &valueNameCount,
                                &valueNameElementCountMaximum,
                                NULL,
                                NULL,
                                NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "RegQueryInfoKey fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }


    // Enumerate all values in this registry and delete the 
    // keys that matches the offer versions pattern ("[InstanceID:%s:]Offer:*")
    //

    // If there are no values, no need to do anything further.
    //
    if (0 == valueNameCount)
    {
        goto Exit;
    }


    // Build the pattern that is to be matched. 
    // It is either "InstanceID:.*:Offer:.*" or "Offer:.*"
    //
    WCHAR patternToMatch[MAXIMUM_VALUE_NAME_SIZE];
    HRESULT hr;
    if (moduleConfig->InstanceIdentifierLength != 0)
    {
        hr = StringCchPrintf(patternToMatch,
                             _countof(patternToMatch),
                             L"InstanceID:%s:Offer:",
                             moduleConfig->InstanceIdentifier);
    }
    else
    {
        hr = StringCchPrintf(patternToMatch,
                             _countof(patternToMatch),
                             L"Offer:");
    }

    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "StringCchPrintf fails: %!HRESULT!",
                    hr);
        // This can fail with STRSAFE_E_INVALID_PARAMETER/STRSAFE_E_INSUFFICIENT_BUFFER.
        // Treat as INVALID_PARAMETER.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    size_t patternToMatchLength;
    hr = StringCbLength(patternToMatch,
                        sizeof(patternToMatch),
                        &patternToMatchLength);
    if (FAILED(hr))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "StringCbLength fails: %!HRESULT!",
                    hr);
        // This can fail with STRSAFE_E_INVALID_PARAMETER/STRSAFE_E_INSUFFICIENT_BUFFER.
        // Treat as INVALID_PARAMETER.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    ntStatus = WdfCollectionCreate(&objectAttributes,
                                   &registryValueNamesToBeDeleted);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfCollectionCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Create buffer(s) which is/are big enough to hold the largest valuename.
    // Account for NULL as well. Note: No overflow check as the registry value length maximum is limited.
    //
    DWORD valueNameElementCount = valueNameElementCountMaximum + 1;
    const size_t valueNameBytesRequired = (valueNameElementCount * sizeof(TCHAR));
    valueNameMemoryBuffer = NULL;

    // Compare the pattern with the value name in the registry and 
    // delete the matched entry
    //
    for (DWORD valueIndex = 0; valueIndex < valueNameCount; valueIndex++)
    {
        // Create new or reuse the existing memory object.
        //
        if (WDF_NO_HANDLE == valueNameMemory)
        {
            ntStatus = WdfMemoryCreate(&objectAttributes,
                                       PagedPool,
                                       MemoryTag,
                                       valueNameBytesRequired,
                                       &valueNameMemory,
                                       (PVOID*)&valueNameMemoryBuffer);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "WdfMemoryCreate fails: ntStatus=%!STATUS!",
                            ntStatus);
                goto Exit;
            }

            SecureZeroMemory(valueNameMemoryBuffer,
                             valueNameBytesRequired);
        }

        valueNameElementCount = valueNameElementCountMaximum + 1;

        // Read the value name.
        //
        ntStatus = RegEnumValue((HKEY)handle,
                                valueIndex,
                                valueNameMemoryBuffer,
                                (LPDWORD)&valueNameElementCount,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "RegEnumValue fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }

        size_t bytesMatched = RtlCompareMemory((VOID*)valueNameMemoryBuffer,
                                               patternToMatch,
                                               patternToMatchLength);
        // The start of the string should match.
        //
        if (bytesMatched != patternToMatchLength)
        {
            continue;
        }
        else
        {
            // Add to collection.
            //
            ntStatus = WdfCollectionAdd(registryValueNamesToBeDeleted, 
                                        valueNameMemory);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "WdfCollectionAdd fails: ntStatus=%!STATUS!",
                            ntStatus);
                goto Exit;
            }

            // We consumed this object. 
            //
            valueNameMemory = WDF_NO_HANDLE;
        }
    }

    // Remove the registry value names collected.
    //
    ULONG numberOfValueNamesToBeDeleted = WdfCollectionGetCount(registryValueNamesToBeDeleted);
    for (ULONG index = 0; index < numberOfValueNamesToBeDeleted; index++)
    {
        WDFMEMORY registryValueNameMemory = (WDFMEMORY)WdfCollectionGetItem(registryValueNamesToBeDeleted,
                                                                            index);
        valueNameMemoryBuffer = (LPTSTR)WdfMemoryGetBuffer(registryValueNameMemory, 
                                                           NULL);
        // Delete the name value.
        //
        ntStatus = RegDeleteValue((HKEY)handle,
                                   valueNameMemoryBuffer);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "RegDeleteValue fails: ntStatus=%!STATUS!", 
                        ntStatus );
            goto Exit;
        }
    }

Exit:

    if (registryValueNamesToBeDeleted != WDF_NO_HANDLE)
    {
        WDFMEMORY registryValueNameMemory;
        while ((registryValueNameMemory = (WDFMEMORY)WdfCollectionGetFirstItem(registryValueNamesToBeDeleted)) != NULL)
        {
            WdfCollectionRemoveItem(registryValueNamesToBeDeleted,
                                    0);
            WdfObjectDelete(registryValueNameMemory);
        }
        WdfObjectDelete(registryValueNamesToBeDeleted);
    }

    if (valueNameMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(valueNameMemory);
    }

    return ntStatus;
}

typedef
NTSTATUS
ComponentFirmwareUpdate_OfferVersionsEnumerationFunctionType(
    _In_ VOID* ClientContext,
    _In_ UNICODE_STRING* OfferString
    );

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_OfferVersionSave (
    _In_ VOID* ClientContext,
    _In_ UNICODE_STRING* OfferString
    )
/*++

Routine Description:

    This callback function saves the given value name string in registry.

Arguments:

    ClientContext - The client context. This Module's DMF Object.
    OfferString - Value name string to save.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    ULONG registryValue = 1;

    dmfModule = (DMFMODULE)ClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    ntStatus = WdfRegistryAssignULong(moduleContext->DeviceRegistryKey,
                                      OfferString,
                                      registryValue);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "WdfRegistryAssignULong fails: for offerVersion %S ntStatus=%!STATUS!", 
                    OfferString->Buffer,
                    ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_OfferVersionQuery (
    _In_ VOID* ClientContext,
    _In_ UNICODE_STRING* OfferString
    )
/*++

Routine Description:

    This callback function queries for a given value name string from the registry.

Arguments:

    ClientContext - The client context. This Module's DMF Object.
    OfferString - value name string to query.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    ULONG registryValue;

    dmfModule = (DMFMODULE)ClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    ntStatus = WdfRegistryQueryULong(moduleContext->DeviceRegistryKey,
                                     OfferString,
                                     &registryValue);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "WdfRegistryQueryULong fails: for offerVersion %S ntStatus=%!STATUS!", 
                    OfferString->Buffer,
                    ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_EnumeratesAllOffers (
    _In_ DMFMODULE DmfModule,
    _In_ ComponentFirmwareUpdate_OfferVersionsEnumerationFunctionType* OfferVersionsEnumerationFunction
    )
/*++

Routine Description:

    Helper function to enumerate all the offers that this driver has and for each offer, invoke 
    the callback the client has provided.

Arguments:

    DmfModule - This Module's DMF Object.
    OfferVersionsEnumerationFunction - The function that does the actual work with the enumerated offer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;
    FIRMWARE_INFORMATION* firmwareInformation;
    WDFMEMORY firmwareInformationMemory;

    ULONG* offerContent;
    size_t offerSizeFromCollection;

    // OfferList Contents should have 1 offer.
    //
    ULONG* offerListData;
    size_t offerListDataSize;

    UNICODE_STRING offerVersionNameValueString;
    WCHAR offerString[MAXIMUM_VALUE_NAME_SIZE];

    // Size of each offer is 4 ULONGs as per spec.
    //
    const UINT sizeOfOneOffer = SizeOfOffer;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = STATUS_UNSUCCESSFUL;

    // For each offer file, get list of offers in it and call the client callback with it.
    //
    ULONG countOfOffers = WdfCollectionGetCount(moduleContext->FirmwareBlobCollection);
    for (UINT offerFileIndex = 0; offerFileIndex < countOfOffers; ++offerFileIndex)
    {
        // Retrieve and validate the offer data.
        //
        firmwareInformationMemory = (WDFMEMORY)WdfCollectionGetItem(moduleContext->FirmwareBlobCollection, 
                                                                    offerFileIndex);

        firmwareInformation = (FIRMWARE_INFORMATION*)WdfMemoryGetBuffer(firmwareInformationMemory,
                                                                        NULL);

        offerContent = (ULONG*)WdfMemoryGetBuffer(firmwareInformation->OfferContentMemory,
                                                  &offerSizeFromCollection);
        DmfAssert(offerSizeFromCollection == firmwareInformation->OfferSize);

        offerListData = (ULONG*)offerContent;
        offerListDataSize = firmwareInformation->OfferSize;

        // As per Specificiation the offer file should contain atmost one offer which is 16 bytes.
        //
        if (offerListDataSize != sizeOfOneOffer)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "OfferDataSize(%Id) is not 16 bytes (offer size)",
                        offerListDataSize);
            ntStatus = STATUS_BAD_DATA;
            goto Exit;
        }

        // Loop through each offer in the offerlist.
        //
        ULONG* currentOffer;
        HRESULT hr;

        RtlZeroMemory(offerString, 
                        sizeof(offerString));
        currentOffer = offerListData;

        if (moduleConfig->InstanceIdentifierLength != 0)
        {
            hr = StringCchPrintf(offerString,
                                    _countof(offerString),
                                    L"InstanceID:%s:Offer:%X%X%X%X",
                                    moduleConfig->InstanceIdentifier,
                                    currentOffer[0],
                                    currentOffer[1],
                                    currentOffer[2],
                                    currentOffer[3]);
        } else
        {
            hr = StringCchPrintf(offerString,
                                    _countof(offerString),
                                    L"Offer:%X%X%X%X",
                                    currentOffer[0],
                                    currentOffer[1],
                                    currentOffer[2],
                                    currentOffer[3]);
        }

        if (FAILED(hr))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "StringCchPrintf fails: %!HRESULT!",
                        hr);
            // This can fail with STRSAFE_E_INVALID_PARAMETER/STRSAFE_E_INSUFFICIENT_BUFFER.
            // Treat as INVALID_PARAMETER.
            //
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

        // Call the client callback.
        // Do not enumerate further if the client callback fails.
        //
        RtlInitUnicodeString(&offerVersionNameValueString,
                                (PCWSTR)offerString);

        ntStatus = OfferVersionsEnumerationFunction(DmfModule,
                                                    &offerVersionNameValueString);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "OfferVersionsEnumerationFunction fails: for offerVersion %S ntStatus=%!STATUS!", 
                        offerVersionNameValueString.Buffer,
                        ntStatus);
            goto Exit;
        }

    // for each of the offer file.
    //
    } 

Exit:

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_OfferVersionsRegistryUpdate (
    _In_ DMFMODULE DmfModule,
    _In_ BOOL StoreOfferVersions
    )
/*++

Routine Description:

    Helper function to update the offer versions in registry.
    If the Skip optimization feature is enabled, 
        this function will delete the obselete offer versions in the registry. 
        If needed, the new versions will be added back to the registry.

Arguments:

    DmfModule - This Module's DMF Object.
    StoreOfferVersions - Indicates whether to update the new set of offer versions or not.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // No need to do anything further if the Skip optimization of CFU transaction is not supported.
    //
    if (moduleConfig->SupportProtocolTransactionSkipOptimization == FALSE)
    {
        ntStatus = STATUS_SUCCESS;
        TraceEvents(TRACE_LEVEL_VERBOSE,
                    DMF_TRACE,
                    "Transaction Skip Optimization is not supported");
        goto Exit;
    }

    // Remove all offer versions that may have been stored earlier.
    //
    ntStatus = ComponentFirmwareUpdate_OfferVersionsRegistryDelete(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (StoreOfferVersions)
    {
        ntStatus = ComponentFirmwareUpdate_EnumeratesAllOffers(DmfModule,
                                                               ComponentFirmwareUpdate_OfferVersionSave);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
BOOLEAN
ComponentFirmwareUpdate_CurrentAndLastOfferVersionsCompare(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Helper function to compare the versions of the offers that the driver currently has and 
    the one it had recorded in registry earlier.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    BOOLEAN -  TRUE if the versions matched, otherwise FALSE.

--*/
{
    NTSTATUS ntStatus;
    BOOLEAN offersMatched;

    FuncEntry(DMF_TRACE);
    offersMatched = FALSE;

    // Enumerate the offer versions and check whether all the offer versions are present in the registy.
    // Callback function, which does the query of the offer version, will fail if the offer is not found 
    // in registry.
    //
    ntStatus = ComponentFirmwareUpdate_EnumeratesAllOffers(DmfModule,
                                                           ComponentFirmwareUpdate_OfferVersionQuery);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // All the enumerated functions returned SUCCESS; which means
    // all the offer versions the driver has currently are all matched fully in the registry.
    //
    offersMatched = TRUE;

Exit:

    FuncExit(DMF_TRACE, "offersMatched=%d", offersMatched);

    return offersMatched;
}

BOOLEAN 
ComponentFirmwareUpdate_IsProtocolTransactionSkippable(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Checks whether the CFU Protocol transaction can be skipped altogether or not.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    TRUE if there is no need for continuing CFU protocol transaction, otherwise FALSE.

--*/
{
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;
    BOOLEAN transactionSkippable = FALSE;

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Protocol Transaction is skippable 'iff'
    // Protocol Transaction Skip option setting is enabled AND
    // Previous transaction indicates the firmware as all Up-to-date AND
    // Current Offers the drive has is same as the one that was offered (and found to be up-to-date).
    //

    // No need to check further if the Skip optimization of CFU transaction is not supported.
    //
    if (moduleConfig->SupportProtocolTransactionSkipOptimization == FALSE)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, 
                    DMF_TRACE, 
                    "Transaction Skip Optimization is not supported");
        goto Exit;
    }

    // Compare the current offers and the last offers.
    // If they match, Protocol transaction can be skipped.
    //
    transactionSkippable = ComponentFirmwareUpdate_CurrentAndLastOfferVersionsCompare(DmfModule);

Exit:

    return transactionSkippable;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_OfferListSend(
    _In_ DMFMODULE DmfModule,
    _In_ UINT32 OfferIndex,
    _Out_ BYTE* ComponentIdentifier,
    _Out_ BOOLEAN* OfferAccepted,
    _Out_ BOOLEAN* OfferSkipped,
    _Out_ BOOLEAN* OfferUpToDate
    )
/*++

Routine Description:

    Read offer data for the specified index from the context and send each offers one by one to the transport & receive the response.

Arguments:

    DmfModule - This Module's DMF Object.
    OfferIndex - Index of the offerlist in {offerlist,payload} pairs.
    ComponentIdentifier - Component identifier from the offer data.
    OfferAccepted - Indicates whether any of the offer is accepted or not.
    OfferSkipped - Indicates whether any of the offer is skipped or not.
    OfferUpToDate - FALSE if any offer was ACCEPT or SWAP_PENDING, TRUE otherwise.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_CODE offerCommandCode;
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE offerResponse;
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON offerResponseRejectReason;

    FIRMWARE_INFORMATION* firmwareInformation;
    WDFMEMORY firmwareInformationMemory;

    ULONG* offerContent;
    size_t offerSizeFromCollection;

    // OfferList Contents should have 1 offer.
    //
    ULONG* offerListData = NULL;
    size_t offerListDataSize;

    // Size of each offer is 4 ULONGs as per spec.
    //
    const UINT sizeOfOneOffer = SizeOfOffer;

    DWORD offerVersion;
    BYTE componentIdentifier;

    BOOLEAN retryOffer;

    FuncEntry(DMF_TRACE);

    DmfAssert(ComponentIdentifier != NULL);
    DmfAssert(OfferAccepted != NULL);
    DmfAssert(OfferSkipped != NULL);
    DmfAssert(OfferUpToDate != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    *OfferAccepted = FALSE;
    *OfferSkipped = FALSE;
    *OfferUpToDate = TRUE; // Default to true until an offer is accepted or rejected with SWAP_PENDING.

    // Retrieve and validate the offer data.
    //
    firmwareInformationMemory = (WDFMEMORY)WdfCollectionGetItem(moduleContext->FirmwareBlobCollection,
                                                                OfferIndex);

    firmwareInformation = (FIRMWARE_INFORMATION*)WdfMemoryGetBuffer(firmwareInformationMemory,
                                                                    NULL);

    offerContent = (ULONG*)WdfMemoryGetBuffer(firmwareInformation->OfferContentMemory,
                                              &offerSizeFromCollection);
    DmfAssert(offerSizeFromCollection == firmwareInformation->OfferSize);

    offerListData = (ULONG*)offerContent;
    offerListDataSize = firmwareInformation->OfferSize;

    // Spec says the offer should contain the atmost one offer which is 16 bytes.
    //
    if (offerListDataSize != sizeOfOneOffer)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "OfferDataSize(%Id) is not 16 bytes (offer size)",
                    offerListDataSize);
        ntStatus = STATUS_BAD_DATA;
        goto Exit;
    }

    // Get Component Identifier and Offer Version from the offer data.
    //
    componentIdentifier = ((offerListData)[0] >> 16) & 0xFF;
    offerVersion = (offerListData)[1];
    *ComponentIdentifier = componentIdentifier;

    // Store the firmware offer version in the Registry.
    //
    ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                    ComponentFirmwareUpdate_OfferFwVersionValueName,
                                                                    componentIdentifier,
                                                                    offerVersion);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for %ws with Component%x and value 0x%x: ntStatus=%!STATUS!", 
                    ComponentFirmwareUpdate_OfferFwVersionValueName, 
                    componentIdentifier, 
                    offerVersion, 
                    ntStatus);
        goto Exit;
    }

    // Send that offer and retry as necessary.
    //
    while (1)
    {
        // Clear the retry flag.
        //
        retryOffer = FALSE;

        // Send an offer from the offerlist.
        //
        ntStatus = ComponentFirmwareUpdate_OfferSend(DmfModule,
                                                     offerListData,
                                                     sizeOfOneOffer,
                                                     &offerResponse,
                                                     &offerResponseRejectReason);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "FirmwareUpdate_SendReceiveOffer fails: ntStatus=%!STATUS!", 
                        ntStatus);
            goto Exit;
        }

        // Process the response to the offer.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Offer from pair %d with Component%x returned response %s(%d)",
                    OfferIndex,
                    componentIdentifier,
                    ComponentFirmwareUpdateOfferResponseString(offerResponse),
                    offerResponse);

        // Decide the next course of actions based on the response status.
        // 
        // In the absense of a formal state machine implementation, decisions are made to a swich case.
        //
        switch (offerResponse)
        {
            case COMPONENT_FIRMWARE_UPDATE_OFFER_ACCEPT:
            {
                // Offer was accepted.
                //
                *OfferAccepted = TRUE;
                *OfferUpToDate = FALSE;
                goto Exit;
            }
            case COMPONENT_FIRMWARE_UPDATE_OFFER_SKIP:
            {
                // The device can choose to Skip an offer if it wants to control the order of Accepted payloads.
                // Mark the flags so the caller can know to retry this function.
                //
                *OfferSkipped = TRUE;
                break;
            }
            case COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT:
            {
                // The device Rejects the offer. Store the Status and RejectReason in the registry.
                // If the device Rejects with specific reasons, we can be confident that the device is up-to-date.
                //
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            DMF_TRACE,
                            "Offer rejected due to reason %s(%d)",
                            ComponentFirmwareUpdateOfferResponseRejectString(offerResponseRejectReason),
                            offerResponseRejectReason);

                // For historical telemetry reasons, we use a different code for UP_TO_DATE than REJECTED, so we remap it here.
                // Response Reason FIRMWARE_UPDATE_OFFER_REJECT_OLD_FW means firmware on the device is uptodate (FIRMWARE_UPDATE_STATUS_UP_TO_DATE).
                //
                if (offerResponseRejectReason == COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_OLD_FW)
                {
                    ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                    ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                                                                    componentIdentifier,
                                                                                    FIRMWARE_UPDATE_STATUS_UP_TO_DATE);
                    if (! NT_SUCCESS(ntStatus))
                    {
                        TraceEvents(TRACE_LEVEL_ERROR,
                                    DMF_TRACE,
                                    "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for %ws with Component%x and value 0x%x: ntStatus=%!STATUS!",
                                    ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                    componentIdentifier,
                                    FIRMWARE_UPDATE_STATUS_UP_TO_DATE,
                                    ntStatus);
                        goto Exit;
                    }
                }
                else
                {
                    ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                    ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                                                                    componentIdentifier,
                                                                                    FIRMWARE_UPDATE_STATUS_UPDATE_REJECTED);
                    if (! NT_SUCCESS(ntStatus))
                    {
                        TraceEvents(TRACE_LEVEL_ERROR,
                                    DMF_TRACE,
                                    "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for %ws with Component%x and value 0x%x: ntStatus=%!STATUS!",
                                    ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                    componentIdentifier,
                                    FIRMWARE_UPDATE_STATUS_UPDATE_REJECTED,
                                    ntStatus);
                        goto Exit;
                    }
                }

                ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                ComponentFirmwareUpdate_FirmwareUpdateStatusRejectReasonValueName,
                                                                                componentIdentifier,
                                                                                offerResponseRejectReason);
                if (! NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR,
                                DMF_TRACE,
                                "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for %ws with Component%x and value 0x%x: ntStatus=%!STATUS!",
                                ComponentFirmwareUpdate_FirmwareUpdateStatusRejectReasonValueName,
                                componentIdentifier,
                                offerResponseRejectReason,
                                ntStatus);
                    goto Exit;
                }

                // Not up to date if a swap is still pending.
                //
                if (offerResponseRejectReason == COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_SWAP_PENDING)
                {
                    *OfferUpToDate = FALSE;
                }

                break;
            }
            case COMPONENT_FIRMWARE_UPDATE_OFFER_BUSY:
            {
                // The device can respond that it is Busy and needs to delay before an offer can be processed.
                // In this case, we send OFFER_COMMAND_NOTIFY_ON_READY which waits infinitely for a response. The device responds whenever it is ready.
                // Once we get the response, we retry the offer that originally received the OFFER_BUSY response.
                //
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            DMF_TRACE,
                            "Waiting for the firmware to no longer be busy");

                ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                                                                componentIdentifier,
                                                                                FIRMWARE_UPDATE_STATUS_BUSY_PROCESSING_UPDATE);
                if (! NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR,
                                DMF_TRACE,
                                "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for %ws with Component%x and value 0x%x: ntStatus=%!STATUS!",
                                ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                componentIdentifier,
                                FIRMWARE_UPDATE_STATUS_BUSY_PROCESSING_UPDATE,
                                ntStatus);
                    goto Exit;
                }

                // Wait for firmware to be ready.
                //
                COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE offerResponseStatus;
                COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON offerResponseReason;
                offerCommandCode = COMPONENT_FIRMWARE_UPDATE_OFFER_COMMAND_NOTIFY_ON_READY;
                ntStatus = ComponentFirmwareUpdate_OfferCommandSend(DmfModule,
                                                                    offerCommandCode,
                                                                    &offerResponseStatus,
                                                                    &offerResponseReason);
                if (! NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR,
                                DMF_TRACE,
                                "FirmwareUpdate_SendReceiveOfferInformation fails for offerCommandCode %s(%d): ntStatus=%!STATUS!",
                                ComponentFirmwareUpdateOfferCommandCodeString(offerCommandCode),
                                offerCommandCode,
                                ntStatus);
                    goto Exit;
                }

                TraceEvents(TRACE_LEVEL_INFORMATION,
                            DMF_TRACE,
                            "Retrying offer for Component%x",
                            componentIdentifier);
                retryOffer = TRUE;
                break;
            }
            default:
            {
                // Unexpected offer response.
                //
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "Received unknown offerResponse %d",
                            offerResponse);
                ntStatus = STATUS_INVALID_PARAMETER;
                goto Exit;
            }
        }

        // exit the loop if not retrying.
        //
        if (! retryOffer)
        {
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdate_OfferPayloadPairsSendAll(
    _In_ DMFMODULE DmfModule,
    _Out_ BOOLEAN* AnyAccepted,
    _Out_ BOOLEAN* AnySkipped,
    _Out_ BOOLEAN* AllUpToDate
    )
/*++

Routine Description:

    Outer functions for the protocol. Picks up the offers and send to transport; Based on the response send the payload.

Arguments:

    DmfModule - DMFModule Handle for this Module.
    AnyAccepted - Indicates whether any of the offer is accepted or not.
    AnySkipped - Indicates whether any of the offer is skipped or not.
    AllUpToDate - Indicates whether current Firmware is uptodate or not.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    BYTE componentIdentifier;
    ULONG countOfOfferPayloadPairs;
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE offerResponseStatus;
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON offerResponseReason;
    COMPONENT_FIRMWARE_UPDATE_PAYLOAD_RESPONSE payloadResponse;
    COMPONENT_FIRMWARE_UPDATE_OFFER_INFORMATION_CODE offerInformationCode;

    // Update protocol is aborted or not.
    //
    BOOLEAN forcedExit;

    // True if any offer was accepted and payload transferred successfully.
    //
    BOOLEAN anyAccepted;
    BOOLEAN anySkipped;
    BOOLEAN payloadUpdateFailed;
    // Initialized to TRUE, set to false if anything was accepted, skipped, or rejected for bad reasons.
    //
    BOOLEAN allUpToDate;

    FuncEntry(DMF_TRACE);

    DmfAssert(AnyAccepted != NULL);
    DmfAssert(AnySkipped != NULL);
    DmfAssert(AllUpToDate != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    payloadUpdateFailed = FALSE;
    componentIdentifier = 0;
    forcedExit = FALSE;
    anyAccepted = FALSE;
    anySkipped = FALSE;
    allUpToDate = TRUE;

    *AnyAccepted = FALSE;
    *AnySkipped = FALSE;
    *AllUpToDate = TRUE;

    countOfOfferPayloadPairs = WdfCollectionGetCount(moduleContext->FirmwareBlobCollection);
    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "Sending %d image pairs",
                countOfOfferPayloadPairs);

    // Send a meta-command to notify the device that this is the start of the list.
    //
    offerInformationCode = COMPONENT_FIRMWARE_UPDATE_OFFER_INFO_START_OFFER_LIST;
    ntStatus = ComponentFirmwareUpdate_SendOfferInformation(DmfModule,
                                                            offerInformationCode,
                                                            &offerResponseStatus,
                                                            &offerResponseReason);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE, 
                    "FirmwareUpdate_SendReceiveOfferInformation fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    // Send each offer/firmware pair. If any are Accepted or Skipped, mark restartLoop as TRUE.
    //
    for (UINT32 pairIndex = 0; pairIndex < countOfOfferPayloadPairs; ++pairIndex)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Sending image pair %d (zero index) of %d",
                    pairIndex,
                    countOfOfferPayloadPairs);

        BOOLEAN currentOfferAccepted = FALSE;
        BOOLEAN currentOfferSkipped = FALSE;
        BOOLEAN currentStatusUpToDate = FALSE;
        payloadResponse = COMPONENT_FIRMWARE_UPDATE_SUCCESS;

        // Skip the protocol if the client has requested a stop request already.
        //
        if (ComponentFirmwareUpdate_IsProtocolStopRequestPending(DmfModule))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "FirmwareUpdate protocol Stopped");
            forcedExit = TRUE;
            goto Exit;
        }

        // Send all the offers in the offer file and determine whether to send the payload.
        //
        ntStatus = ComponentFirmwareUpdate_OfferListSend(DmfModule,
                                                         pairIndex,
                                                         &componentIdentifier,
                                                         &currentOfferAccepted,
                                                         &currentOfferSkipped,
                                                         &currentStatusUpToDate);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "ComponentFirmwareUpdate_OfferPayloadPairsSendReceiveOne fails: ntStatus=%!STATUS!", 
                        ntStatus);
            goto Exit;
        }

        anySkipped |= currentOfferSkipped;
        // Clear if any offer response indicates firmware is not up to date.
        //
        allUpToDate &= currentStatusUpToDate;

        // Skip the protocol if the client has requested a stop request already.
        //
        if (ComponentFirmwareUpdate_IsProtocolStopRequestPending(DmfModule))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "FirmwareUpdate protocol Stopped");

            forcedExit = TRUE;
            goto Exit;
        }

        if (currentOfferAccepted)
        {
            DmfAssert(allUpToDate == FALSE);

            // The device wants the driver to deliver the payload.
            // Mark the download status in the registry while delivering the payload.
            //
            ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                            ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                                                            componentIdentifier,
                                                                            FIRMWARE_UPDATE_STATUS_DOWNLOADING_UPDATE);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, 
                            DMF_TRACE, 
                            "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for %ws with Component%x and value 0x%x: ntStatus=%!STATUS!", 
                            ComponentFirmwareUpdate_FirmwareUpdateStatusValueName, 
                            componentIdentifier, 
                            FIRMWARE_UPDATE_STATUS_DOWNLOADING_UPDATE, 
                            ntStatus);
                goto Exit;
            }

            ntStatus = ComponentFirmwareUpdate_SendPayload(DmfModule,
                                                           pairIndex,
                                                           componentIdentifier,
                                                           &payloadResponse);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, 
                            DMF_TRACE, 
                            "FirmwareUpdate_SendReceiveFirmware fails for firmwareIntegerValue %d: ntStatus=%!STATUS!", 
                            pairIndex, 
                            ntStatus);
                goto Exit;
            }

            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE,
                        "Firmware from pair %d returned response %s(%d)",
                        pairIndex,
                        ComponentFirmwareUpdatePayloadResponseString(payloadResponse),
                        payloadResponse);

            if (payloadResponse == COMPONENT_FIRMWARE_UPDATE_SUCCESS)
            {
                // Payload sent successfully, mark this current offer as accepted.
                //
                anyAccepted |= currentOfferAccepted;

                ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                                                                componentIdentifier,
                                                                                FIRMWARE_UPDATE_STATUS_PENDING_RESET);
                if (! NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, 
                                DMF_TRACE, 
                                "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for %ws with Component%x and value 0x%x: ntStatus=%!STATUS!", 
                                ComponentFirmwareUpdate_FirmwareUpdateStatusValueName, 
                                componentIdentifier, 
                                FIRMWARE_UPDATE_STATUS_PENDING_RESET, 
                                ntStatus);
                    goto Exit;
                }
            }
        }

        // if the offer was accepted and yet the payload is rejected, exit.
        //
        if (currentOfferAccepted && (payloadResponse != COMPONENT_FIRMWARE_UPDATE_SUCCESS))
        {
            payloadUpdateFailed = TRUE;
            goto Exit;
        }
    }

    // Skip the protocol if the client has requested a stop request already.
    //
    if (ComponentFirmwareUpdate_IsProtocolStopRequestPending(DmfModule))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "FirmwareUpdate protocol Stopped");
        forcedExit = TRUE;
        goto Exit;
    }

    // Send a meta-command to notify the device that this is the end of the list.
    //
    offerInformationCode = COMPONENT_FIRMWARE_UPDATE_OFFER_INFO_END_OFFER_LIST;
    ntStatus = ComponentFirmwareUpdate_SendOfferInformation(DmfModule,
                                                            offerInformationCode,
                                                            &offerResponseStatus,
                                                            &offerResponseReason);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "FirmwareUpdate_SendReceiveOfferInformation fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    *AnyAccepted = anyAccepted;
    *AnySkipped = anySkipped;
    *AllUpToDate = allUpToDate;

Exit:

    if (! NT_SUCCESS(ntStatus) || payloadUpdateFailed)
    {
        // Write the status as Error in the case of an error.
        //
        NTSTATUS ntStatus2 = ComponentFirmwareUpdate_RegistryAssignComponentUlong(DmfModule,
                                                                                  ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                                                                  componentIdentifier,
                                                                                  FIRMWARE_UPDATE_STATUS_ERROR);
        if (!NT_SUCCESS(ntStatus2))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "ComponentFirmwareUpdate_RegistryAssignComponentUlong fails for %ws with Component%x and value 0x%x: ntStatus=%!STATUS!", 
                        ComponentFirmwareUpdate_FirmwareUpdateStatusValueName, 
                        componentIdentifier, 
                        FIRMWARE_UPDATE_STATUS_ERROR, 
                        ntStatus2);
        }
    }

    if (forcedExit)
    {
        ntStatus = STATUS_ABANDONED;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Function_class_(EVT_DMF_Thread_Function)
VOID
ComponentFirmwareUpdate_FirmwareUpdatePre(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Child DMF Module Thread Pre.
    Opens the transport.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModuleComponentFirmwareUpdate;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    FuncEntry(DMF_TRACE);

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    dmfModuleComponentFirmwareUpdate = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleComponentFirmwareUpdate);

    DMF_ModuleLock(dmfModuleComponentFirmwareUpdate);
    moduleContext->TransactionInProgress = TRUE;
    DMF_ModuleUnlock(dmfModuleComponentFirmwareUpdate);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "Sending a Open command to transport");

    // Call Open to the transport to allow it to preform any preparation steps to receive the protocol transaction.
    //
    ntStatus = ComponentFirmwareUpdate_ProtocolStart(dmfModuleComponentFirmwareUpdate);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ComponentFirmwareUpdate_ProtocolStart fails: ntStatus=%!STATUS!",
                    ntStatus);

        // Don't do the 'work' when Pre fails.
        //
        DMF_Thread_Stop(DmfModule);

        goto Exit;
    }

Exit:

    FuncExitVoid(DMF_TRACE);
}

_Function_class_(EVT_DMF_Thread_Function)
VOID
ComponentFirmwareUpdate_FirmwareUpdatePost(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Child DMF Module Thread Post.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    None

--*/
{
    DMFMODULE dmfModuleComponentFirmwareUpdate;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    FuncEntry(DMF_TRACE);

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    dmfModuleComponentFirmwareUpdate = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleComponentFirmwareUpdate);

    DMF_ModuleLock(dmfModuleComponentFirmwareUpdate);
    moduleContext->TransactionInProgress = FALSE;
    DMF_ModuleUnlock(dmfModuleComponentFirmwareUpdate);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "CFU Transaction finished");

    FuncExitVoid(DMF_TRACE);
}

_Function_class_(EVT_DMF_Thread_Function)
VOID
ComponentFirmwareUpdate_FirmwareUpdateWork(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Child DMF Module Thread.
    "Work" is to perform firmware update protocol as per the specification.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModuleComponentFirmwareUpdate;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;

    COMPONENT_FIRMWARE_VERSIONS firmwareVersions = { 0 };
    BOOLEAN skipProtocolTransaction;
    UINT8 loopIteration;
    BOOLEAN restartLoop;
    BOOLEAN anyAccepted;
    BOOLEAN anySkipped;
    BOOLEAN allUpToDate;

    FuncEntry(DMF_TRACE);

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    dmfModuleComponentFirmwareUpdate = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleComponentFirmwareUpdate);
    moduleConfig = DMF_CONFIG_GET(dmfModuleComponentFirmwareUpdate);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "Start of the CFU protocol.");

    anyAccepted = FALSE;
    anySkipped = FALSE;
    allUpToDate = FALSE;
    skipProtocolTransaction = FALSE;
    loopIteration = 0;

    ULONG countOfOfferPayloadPairs = WdfCollectionGetCount(moduleContext->FirmwareBlobCollection);
    if (countOfOfferPayloadPairs == 0)
    {
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE,
                    "No Firmware available to process. Skipping the entire transaction.");
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "Component Firmware Update Transaction Start");

    // Get the firmware versions of each components from device and store in the registry.
    // These can be useful for external tools that scans the registry and collect information on various stages 
    // of firmware update protocol.
    // The version returned from this call is not used in any decision making. So a failure here is 
    // NOT considered critical and is ignored.
    //
    ntStatus = ComponentFirmwareUpdate_FirmwareVersionsGet(dmfModuleComponentFirmwareUpdate,
                                                           &firmwareVersions);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "FirmwareUpdate_GetFirmwareVersions fails: %!STATUS!, but continuing because this is not a critical failure",
                    ntStatus);

        ntStatus = STATUS_SUCCESS;
        ZeroMemory(&firmwareVersions,
                   sizeof(firmwareVersions));
    }
    else
    {
        for (UINT componentIndex = 0;
             componentIndex < firmwareVersions.componentCount;
             ++componentIndex)
        {
            BYTE componentIdentifier = firmwareVersions.ComponentIdentifiers[componentIndex];
            DWORD componentFirmwareVersion = firmwareVersions.FirmwareVersion[componentIndex];
            ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(dmfModuleComponentFirmwareUpdate,
                                                                            ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                                                                            componentIdentifier,
                                                                            FIRMWARE_UPDATE_STATUS_NOT_STARTED);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "ComponentFirmwareUpdate_RegistryAssignComponentUlong failed for %ws with Component%x and value 0x%x %!STATUS!, but ignoring error",
                            ComponentFirmwareUpdate_FirmwareUpdateStatusValueName,
                            componentIdentifier,
                            FIRMWARE_UPDATE_STATUS_NOT_STARTED,
                            ntStatus);

                ntStatus = STATUS_SUCCESS;
            }

            ntStatus = ComponentFirmwareUpdate_RegistryAssignComponentUlong(dmfModuleComponentFirmwareUpdate,
                                                                            ComponentFirmwareUpdate_CurrentFwVersionValueName,
                                                                            componentIdentifier,
                                                                            componentFirmwareVersion);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "ComponentFirmwareUpdate_RegistryAssignComponentUlong failed for %ws with Component%x and value 0x%x %!STATUS!, but ignoring error",
                            ComponentFirmwareUpdate_CurrentFwVersionValueName,
                            componentIdentifier,
                            componentFirmwareVersion,
                            ntStatus);

                ntStatus = STATUS_SUCCESS;
            }

            ntStatus = ComponentFirmwareUpdate_RegistryRemoveComponentValue(dmfModuleComponentFirmwareUpdate,
                                                                            ComponentFirmwareUpdate_FirmwareUpdateStatusRejectReasonValueName, 
                                                                            componentIdentifier);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "FirmwareUpdate_RegistryRemoveComponentValue failed for %ws with Component%x %!STATUS!, but ignoring error",
                            ComponentFirmwareUpdate_FirmwareUpdateStatusRejectReasonValueName,
                            componentIdentifier,
                            ntStatus);

                ntStatus = STATUS_SUCCESS;
            }
        }
    }

    // Skip the protocol if the client has requested a stop request already.
    //
    if (ComponentFirmwareUpdate_IsProtocolStopRequestPending(dmfModuleComponentFirmwareUpdate))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "FirmwareUpdate protocol Stopped");
        goto Exit;
    }

    // Skip the protocol if there is nothing new to offer to the firmware and its already known to be up-to-date.
    //
    skipProtocolTransaction = ComponentFirmwareUpdate_IsProtocolTransactionSkippable(dmfModuleComponentFirmwareUpdate);
    if (skipProtocolTransaction)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Skipping the entire transaction");
        goto Exit;
    }

    // Send a meta-command to notify the device that this is the start of the entire transaction.
    //
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE offerResponseStatus;
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON offerResponseReason;
    COMPONENT_FIRMWARE_UPDATE_OFFER_INFORMATION_CODE offerInformationCode = COMPONENT_FIRMWARE_UPDATE_OFFER_INFO_START_ENTIRE_TRANSACTION;
    ntStatus = ComponentFirmwareUpdate_SendOfferInformation(dmfModuleComponentFirmwareUpdate,
                                                            offerInformationCode,
                                                            &offerResponseStatus,
                                                            &offerResponseReason);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "FirmwareUpdate_SendReceiveOfferInformation fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    // Send every payload pair. Repeat until all of the offers are Rejected. 
    // This allows the device to control the order that payloads are taken.
    //
    restartLoop = FALSE;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Start sending %d offer/payload pairs",
                WdfCollectionGetCount(moduleContext->FirmwareBlobCollection));
    do
    {
        // Skip the protocol if the client has requested a stop request already.
        //
        if (ComponentFirmwareUpdate_IsProtocolStopRequestPending(dmfModuleComponentFirmwareUpdate))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "FirmwareUpdate protocol Stopped");
            ntStatus = STATUS_ABANDONED;
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Start iteration %d",
                    loopIteration);

        ntStatus = ComponentFirmwareUpdate_OfferPayloadPairsSendAll(dmfModuleComponentFirmwareUpdate,
                                                                    &anyAccepted,
                                                                    &anySkipped,
                                                                    &allUpToDate);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "FirmwareUpdate_SendReceiveOfferPayloadPairs fails: ntStatus=%!STATUS!", 
                        ntStatus);
            goto Exit;
        }

        // If nothing was accepted on an iteration after the first, do not restart the loop to prevent infinite loop.
        //
        if (
            (loopIteration == 0 && (anyAccepted || anySkipped)) ||
            (loopIteration > 0 && anyAccepted)
            )
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "Restarting loop with loopIteration(%d), anyAccepted(%d), anySkipped(%d)",
                        loopIteration,
                        anyAccepted,
                        anySkipped);
            restartLoop = TRUE;
        }
        else
        {
            restartLoop = FALSE;
        }

        if (moduleConfig->ForceIgnoreVersion)
        {
            // If we are force ignoring the version, every offer will be accepted. We have to prevent an infinite loop.
            //
            restartLoop = FALSE;
        }

        ++loopIteration;
    } while (restartLoop == TRUE);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Exited the loop normally after %d iterations.",
                loopIteration);

    if (allUpToDate)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE,
                    DMF_TRACE,
                    "Firmware is all up-to-date");
    }

    // Update the firmware versions in Registry as needed.
    //
    NTSTATUS ntStatus2 = ComponentFirmwareUpdate_OfferVersionsRegistryUpdate(dmfModuleComponentFirmwareUpdate, 
                                                                             allUpToDate);
    if (! NT_SUCCESS(ntStatus2))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "ComponentFirmwareUpdate_OfferVersionsRegistryUpdate fails: ntStatus=%!STATUS!", 
                    ntStatus2);
        goto Exit;
    }

Exit:

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE, 
                "End of the CFU protocol.");

    FuncExitVoid(DMF_TRACE);
}

// CFU Protocol Related (END)
//=============================

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Protocol Generic Callbacks.
// (Implementation of publicly accessible callbacks required by the Interface.)
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_ComponentFirmwareUpdate_PostBind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    This callback tells the given Protocol Module that it is bound to the given
    Transport Module.

Arguments:

    DmfInterface - Interface handle.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfInterface);

    FuncEntry(DMF_TRACE);

    // NOP.

    // It is now possible to use Methods provided by the Transport.
    //

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_ComponentFirmwareUpdate_PreUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    This callback tells the given Protocol Module that it is about to be unbound from
    the given Transport Module.

Arguments:

    DmfInterface - Interface handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfInterface);

    FuncEntry(DMF_TRACE);

    // NOP.
    //

    // Stop using Methods provided by Transport after this callback completes (except for Unbind).
    //

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdate_Bind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Binds the given Protocol Module to the given Transport Module.

Arguments:

    DmfInterface - Interface handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA protocolBindData;
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA transportBindData;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;
    CONTEXT_ComponentFirmwareUpdateTransport* configComponentFirmwareUpdateTransport;
    DMFMODULE protocolModule;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    protocolModule = DMF_InterfaceProtocolModuleGet(DmfInterface);

    moduleContext = DMF_CONTEXT_GET(protocolModule);
    moduleConfig = DMF_CONFIG_GET(protocolModule);

    RtlZeroMemory(&protocolBindData,
                  sizeof(protocolBindData));

    // Call the Interface's Bind function.
    //
    ntStatus = DMF_ComponentFirmwareUpdate_TransportBind(DmfInterface,
                                                         &protocolBindData,
                                                         &transportBindData);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "DMF_ComponentFirmwareUpdate_TransportBind fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    // Save the Interface handle representing the interface binding.
    //
    moduleContext->DmfInterfaceComponentFirmwareUpdate = DmfInterface;

    // Check the TransportPayloadRequiredSize to ensure that it meets minimal packet size requirement.
    // Driver needs the following size per specification.
    // Offer Command is 16 bytes.
    // Offer Information is 16 bytes.
    // Offer is 16 bytes. 
    // Payload Chunk size is variable; The maximum driver can send is 60 bytes.
    //
    if (transportBindData.TransportFirmwarePayloadBufferRequiredSize < SizeOfPayload)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Transport payload size (%Id) is insufficient",
                    transportBindData.TransportFirmwarePayloadBufferRequiredSize);
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    // Check for Overflow.
    //
    if (transportBindData.TransportFirmwarePayloadBufferRequiredSize > transportBindData.TransportFirmwarePayloadBufferRequiredSize + transportBindData.TransportHeaderSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Payload size Overflow (%d)+(%d)",
                    transportBindData.TransportFirmwarePayloadBufferRequiredSize,
                    transportBindData.TransportHeaderSize);
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    if (transportBindData.TransportFirmwareVersionBufferRequiredSize < SizeOfFirmwareVersion)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Transport Firmware Version size (%Id) is insufficient",
                    transportBindData.TransportFirmwareVersionBufferRequiredSize);
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    // Check for Overflow.
    //
    if (transportBindData.TransportFirmwareVersionBufferRequiredSize > transportBindData.TransportFirmwareVersionBufferRequiredSize + transportBindData.TransportHeaderSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Payload size Overflow (%d)+(%d)",
                    transportBindData.TransportFirmwareVersionBufferRequiredSize,
                    transportBindData.TransportHeaderSize);
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    if (transportBindData.TransportOfferBufferRequiredSize < SizeOfOffer)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Transport Offer size (%Id) is insufficient",
                    transportBindData.TransportOfferBufferRequiredSize);
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    // Check for Overflow.
    //
    if (transportBindData.TransportOfferBufferRequiredSize > transportBindData.TransportOfferBufferRequiredSize + transportBindData.TransportHeaderSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Payload size Overflow (%d)+(%d)",
                    transportBindData.TransportOfferBufferRequiredSize,
                    transportBindData.TransportHeaderSize);
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    if (transportBindData.TransportPayloadFillAlignment == 0)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Invalid TransportPayloadFillAlignment. It can not be 0");
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    configComponentFirmwareUpdateTransport = ComponentFirmwareUpdateTransportContextGet(DmfInterface);
    configComponentFirmwareUpdateTransport->TransportHeaderSize = transportBindData.TransportHeaderSize;
    configComponentFirmwareUpdateTransport->TransportFirmwarePayloadBufferRequiredSize = transportBindData.TransportFirmwarePayloadBufferRequiredSize;
    configComponentFirmwareUpdateTransport->TransportFirmwareVersionBufferRequiredSize = transportBindData.TransportFirmwareVersionBufferRequiredSize;
    configComponentFirmwareUpdateTransport->TransportOfferBufferRequiredSize = transportBindData.TransportOfferBufferRequiredSize;
    configComponentFirmwareUpdateTransport->TransportWaitTimeout = transportBindData.TransportWaitTimeout;
    configComponentFirmwareUpdateTransport->TransportPayloadFillAlignment = transportBindData.TransportPayloadFillAlignment;

    // Allocate a Context to keep items for transaction response specific processing.
    //
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            CONTEXT_ComponentFirmwareUpdateTransaction);
    ntStatus = WdfObjectAllocateContext(DmfInterface,
                                        &attributes,
                                        (VOID**) &componentFirmwareUpdateTransactionContext);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "WdfObjectAllocateContext fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = protocolModule;
    device = DMF_ParentDeviceGet(protocolModule);

    // BufferQueue
    // -----------
    //
    DMF_CONFIG_BufferQueue bufferQueueModuleConfig;
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&bufferQueueModuleConfig,
                                               &moduleAttributes);
    bufferQueueModuleConfig.SourceSettings.EnableLookAside = TRUE;
    bufferQueueModuleConfig.SourceSettings.BufferCount = 5;
    bufferQueueModuleConfig.SourceSettings.BufferSize = sizeof(PAYLOAD_RESPONSE);
    bufferQueueModuleConfig.SourceSettings.BufferContextSize = sizeof(ULONG);
    bufferQueueModuleConfig.SourceSettings.PoolType = NonPagedPoolNx;
    ntStatus = DMF_BufferQueue_Create(device,
                                      &moduleAttributes,
                                      &attributes,
                                      &componentFirmwareUpdateTransactionContext->DmfModuleBufferQueue);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_BufferQueue_Create fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Create the Work Ready Event.
    //
    DMF_Portable_EventCreate(&componentFirmwareUpdateTransactionContext->DmfResponseCompletionEvent,
                             SynchronizationEvent,
                             FALSE);

    // Create the Protocol Transaction Cancel Event.
    //
    DMF_Portable_EventCreate(&componentFirmwareUpdateTransactionContext->DmfProtocolTransactionCancelEvent,
                             SynchronizationEvent,
                             FALSE);


    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_ComponentFirmwareUpdate_Bind success");

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_ComponentFirmwareUpdate_Unbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Unbinds the given Protocol Module from the given Transport Module.

Arguments:

    DmfInterface - Interface handle.

Return Value:

    None

--*/
{
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Call the Interface's Unbind function.
    //
    DMF_ComponentFirmwareUpdate_TransportUnbind(DmfInterface);

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(DmfInterface);
    DMF_Portable_EventClose(&componentFirmwareUpdateTransactionContext->DmfResponseCompletionEvent);
    DMF_Portable_EventClose(&componentFirmwareUpdateTransactionContext->DmfProtocolTransactionCancelEvent);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

// Callback Implementation
// --------START----------
//
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_ComponentFirmwareUpdate_FirmwareVersionResponseEvt(
    _In_ DMFINTERFACE DmfInterface,
    _In_ UCHAR* FirmwareVersionsBuffer,
    _In_ size_t FirmwareVersionsBufferSize,
    _In_ NTSTATUS ntStatusCallback
    )
/*++

Routine Description:

    Callback to indicate the firmware versions. 
    This unpacks the message and store the response in a context and signal event
    to wakeup the thread that is waiting for response.

Parameters:

    DmfInterface - Interface handle.
    FirmwareVersionsBuffer - Buffer with firmware information.
    FirmwareVersionsBufferSize - size of the above in bytes.
    ntStatusCallback -  NTSTATUS for the FirmwareVersionGet.

Return:

    NTSTATUS

--*/
{
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(FirmwareVersionsBufferSize);

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(DmfInterface);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    componentFirmwareUpdateTransactionContext->ntStatus = ntStatusCallback;
    if (!NT_SUCCESS(ntStatusCallback))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "FirmwareVersionGet fails: ntStatus=%!STATUS!", 
                    ntStatusCallback);
        goto Exit;
    }

    DmfAssert(FirmwareVersionsBuffer != NULL);

    // Parse and store the response data.
    //
    BYTE componentCount;
    BYTE firmwareUpdateProtocolRevision;
    
    // Byte 0 is Component Count.
    //
    componentCount = (BYTE)FirmwareVersionsBuffer[0];
    if (componentCount == 0)
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "Invalid Response from Device. ComponentCount == 0.");
        componentFirmwareUpdateTransactionContext->ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    // We have a limitation on the number of components (7).
    //
    if (componentFirmwareUpdateTransactionContext->FirmwareVersions.componentCount >= MAX_NUMBER_OF_IMAGE_PAIRS)
    {
        DmfAssert(FALSE);
        TraceError(DMF_TRACE, 
                   "Invalid ComponentCount(%d) greater than max supported(%d).", 
                   componentCount, 
                   MAX_NUMBER_OF_IMAGE_PAIRS);
        componentFirmwareUpdateTransactionContext->ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    componentFirmwareUpdateTransactionContext->FirmwareVersions.componentCount = componentCount;

    firmwareUpdateProtocolRevision = FirmwareVersionsBuffer[3] & 0xF;
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                DMF_TRACE, 
                "Device is using FW Update Protocol Revision %d", 
                firmwareUpdateProtocolRevision);

    DWORD* firmwareVersion = NULL;
    if (firmwareUpdateProtocolRevision == PROTOCOL_VERSION_2 || firmwareUpdateProtocolRevision == PROTOCOL_VERSION_4)
    {
        // Header is 4 bytes.
        //
        const UINT versionTableOffset = 4;
        // Component ID is 6th byte.
        //
        const UINT componentIDOffset = 5;
        // Each component takes up 8 bytes.
        //
        const UINT componentDataSize = 8;

        DmfAssert(FirmwareVersionsBufferSize >= versionTableOffset + componentCount * componentDataSize);
        for (int componentIndex = 0; componentIndex < componentCount; ++componentIndex)
        {
            componentFirmwareUpdateTransactionContext->FirmwareVersions.ComponentIdentifiers[componentIndex] = (BYTE)FirmwareVersionsBuffer[versionTableOffset + componentIndex * componentDataSize + componentIDOffset];
            firmwareVersion = &componentFirmwareUpdateTransactionContext->FirmwareVersions.FirmwareVersion[componentIndex];
            *firmwareVersion = 0;
            *firmwareVersion |= ((FirmwareVersionsBuffer[versionTableOffset + componentIndex * componentDataSize + 0] & 0xFF) << 0);
            *firmwareVersion |= ((FirmwareVersionsBuffer[versionTableOffset + componentIndex * componentDataSize + 1] & 0xFF) << 8);
            *firmwareVersion |= ((FirmwareVersionsBuffer[versionTableOffset + componentIndex * componentDataSize + 2] & 0xFF) << 16);
            *firmwareVersion |= ((FirmwareVersionsBuffer[versionTableOffset + componentIndex * componentDataSize + 3] & 0xFF) << 24);
            TraceEvents(TRACE_LEVEL_VERBOSE, 
                        DMF_TRACE, 
                        "Component%02x has version 0x%x",
                        componentFirmwareUpdateTransactionContext->FirmwareVersions.ComponentIdentifiers[componentIndex],
                        *firmwareVersion);
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "Unrecognized FW Update Protocol Revision %d", 
                    firmwareUpdateProtocolRevision);
        componentFirmwareUpdateTransactionContext->ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

Exit:

    // Set Event the event so that the sending thread gets the response.
    //
    DMF_Portable_EventSet(&componentFirmwareUpdateTransactionContext->DmfResponseCompletionEvent);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_ComponentFirmwareUpdate_OfferResponseEvt(
    _In_ DMFINTERFACE DmfInterface,
    _In_ UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatusCallback
    )
/*++

Routine Description:

    Callback to indicate the response to an offer that was sent to device.

Parameters:

    DmfInterface - Interface handle.
    ResponseBuffer - Buffer with response information.
    ResponseBufferSize - size of the above in bytes.
    ntStatusCallback -  NTSTATUS for the command that was sent down.

Return:

    NTSTATUS

--*/
{
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;

    ULONG numberOfUlongsInResponse = 4;
    const BYTE outputToken = FWUPDATE_DRIVER_TOKEN;

    FuncEntry(DMF_TRACE);

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(DmfInterface);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    componentFirmwareUpdateTransactionContext->ntStatus = ntStatusCallback;
    if (!NT_SUCCESS(ntStatusCallback))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE,
                    "Offer send fails: ntStatus=%!STATUS!", 
                    ntStatusCallback);
        goto Exit;
    }

    // Offer response size is 4 * ULONG .
    //
    if (numberOfUlongsInResponse * sizeof(ULONG) > ResponseBufferSize)
    {
        componentFirmwareUpdateTransactionContext->ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE,
                    "Return Buffer size (%Id) insufficient", 
                    ResponseBufferSize);
        goto Exit;
    }

    ULONG* offerResponseLocal = (ULONG*)ResponseBuffer;

    // Get Token (Byte 3) and Validate it.
    //
    BYTE tokenResponse = (offerResponseLocal[0] >> 24) & 0xFF;
    if (outputToken != tokenResponse)
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "Output Token(%d) did not match Returned Token(%d)", 
                    outputToken, 
                    tokenResponse);
        componentFirmwareUpdateTransactionContext->ntStatus = STATUS_INVALID_DEVICE_STATE;
        goto Exit;
    }

    // Get Offer Response Reason (Byte 0).
    //
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON offerResponseReason = (COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON)((offerResponseLocal[2]) & 0xFF);

    // Get Offer Response Status (Byte 0).
    //
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE offerResponseStatus = (COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE)((offerResponseLocal[3]) & 0xFF);

    componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseStatus = offerResponseStatus;
    componentFirmwareUpdateTransactionContext->OfferResponse.OfferResponseReason = offerResponseReason;
    componentFirmwareUpdateTransactionContext->ntStatus = STATUS_SUCCESS;

Exit:

    // Set Event the event so that the sending thread gets the response.
    //
    DMF_Portable_EventSet(&componentFirmwareUpdateTransactionContext->DmfResponseCompletionEvent);

    FuncExitVoid(DMF_TRACE);
}

// Callback to response to payload that is sent to device.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_ComponentFirmwareUpdate_PayloadResponseEvt(
    _In_ DMFINTERFACE DmfInterface,
    _In_ UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatusCallback
    )
/*++

Routine Description:

    Callback to indicate the response to a payload that was sent to device.

Parameters:

    DmfInterface - Interface handle.
    ResponseBuffer - Buffer with response information.
    ResponseBufferSize - size of the above in bytes.
    ntStatusCallback - NTSTATUS for the command that was sent down.

Return:

    NTSTATUS

--*/
{
    CONTEXT_ComponentFirmwareUpdateTransaction* componentFirmwareUpdateTransactionContext;

    ULONG numberOfUlongsInResponse = 4;
    VOID* clientBuffer= NULL;
    VOID* clientBufferContext = NULL;

    FuncEntry(DMF_TRACE);

    componentFirmwareUpdateTransactionContext = ComponentFirmwareUpdateTransactionContextGet(DmfInterface);
    DmfAssert(componentFirmwareUpdateTransactionContext != NULL);

    DmfAssert(ResponseBuffer != NULL);

    componentFirmwareUpdateTransactionContext->ntStatus = ntStatusCallback;
    if (!NT_SUCCESS(ntStatusCallback))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "PayloadSend fails: ntStatus=%!STATUS!",
                    ntStatusCallback);
        goto Exit;
    }

    // Payload response size is 4 * ULONG.
    //
    if (numberOfUlongsInResponse * sizeof(ULONG) > ResponseBufferSize)
    {
        componentFirmwareUpdateTransactionContext->ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Return Buffer size (%Id) in sufficient",
                    ResponseBufferSize);
        goto Exit;
    }

    ULONG* payloadResponseLocal = (ULONG*)ResponseBuffer;

    // Get Response Sequence Number (Bytes 0-1).
    //
    UINT16 responseSequenceNumber = (UINT16)((payloadResponseLocal[0] >> 0) & 0xFFFF);

    // Get Payload Response Status (Byte 0).
    //
    COMPONENT_FIRMWARE_UPDATE_PAYLOAD_RESPONSE responseSequenceStatus = (COMPONENT_FIRMWARE_UPDATE_PAYLOAD_RESPONSE)((payloadResponseLocal[1] >> 0) & 0xFF);

    // Get a buffer from Producer for feature Report.
    //
    componentFirmwareUpdateTransactionContext->ntStatus = DMF_BufferQueue_Fetch(componentFirmwareUpdateTransactionContext->DmfModuleBufferQueue,
                                                                                &clientBuffer,
                                                                                &clientBufferContext);
    if (!NT_SUCCESS(componentFirmwareUpdateTransactionContext->ntStatus))
    {
        // There is no data buffer to save the response.
        //
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_BufferQueue_ClientBufferGetProducer fails: ntStatus=%!STATUS!",
                    componentFirmwareUpdateTransactionContext->ntStatus);
        goto Exit;
    }

    DmfAssert(clientBuffer != NULL);
    DmfAssert(clientBufferContext != NULL);

    PAYLOAD_RESPONSE* payloadResponse = (PAYLOAD_RESPONSE*)clientBuffer;
    ULONG* payloadResponseSize = (ULONG*)clientBufferContext;

    payloadResponse->SequenceNumber = responseSequenceNumber;
    payloadResponse->ResponseStatus = responseSequenceStatus;

    // put this to the consumer.
    //
    *payloadResponseSize = sizeof(PAYLOAD_RESPONSE);
    DMF_BufferQueue_Enqueue(componentFirmwareUpdateTransactionContext->DmfModuleBufferQueue,
                            clientBuffer);

Exit:

    // Set Event the event so that the sending thread gets the response.
    //
    DMF_Portable_EventSet(&componentFirmwareUpdateTransactionContext->DmfResponseCompletionEvent);

    FuncExitVoid(DMF_TRACE);
}
// Callback Implementation
// --------END------------

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ComponentFirmwareUpdate_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_Thread threadModuleConfig;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);
    UNREFERENCED_PARAMETER(DmfModuleInit);

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(moduleContext,
                  sizeof(DMF_CONTEXT_ComponentFirmwareUpdate));

    // Thread
    // ------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&threadModuleConfig,
                                          &moduleAttributes);
    threadModuleConfig.ThreadControlType = ThreadControlType_DmfControl;
    threadModuleConfig.ThreadControl.DmfControl.EvtThreadPre = ComponentFirmwareUpdate_FirmwareUpdatePre;
    threadModuleConfig.ThreadControl.DmfControl.EvtThreadPost = ComponentFirmwareUpdate_FirmwareUpdatePost;
    threadModuleConfig.ThreadControl.DmfControl.EvtThreadWork = ComponentFirmwareUpdate_FirmwareUpdateWork;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThread);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_ComponentFirmwareUpdate_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type ComponentFirmwareUpdate.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_ComponentFirmwareUpdate* moduleConfig;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = ComponentFirmwareUpdate_ComponentFirmwareUpdateInitialize(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "ComponentFirmwareUpdate_ComponentFirmwareUpdateInitialize fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_ComponentFirmwareUpdate_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type ComponentFirmwareUpdate.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ComponentFirmwareUpdate_ComponentFirmwareUpdateDeinitialize(DmfModule);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Component Firmware Update.

Arguments:

    Device - This device.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_ComponentFirmwareUpdate;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_ComponentFirmwareUpdate;
    DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA protocolDeclarationData;
    DMFMODULE dmfModule;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_ComponentFirmwareUpdate);
    dmfCallbacksDmf_ComponentFirmwareUpdate.ChildModulesAdd = DMF_ComponentFirmwareUpdate_ChildModulesAdd;
    dmfCallbacksDmf_ComponentFirmwareUpdate.DeviceOpen = DMF_ComponentFirmwareUpdate_Open;
    dmfCallbacksDmf_ComponentFirmwareUpdate.DeviceClose = DMF_ComponentFirmwareUpdate_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_ComponentFirmwareUpdate,
                                            ComponentFirmwareUpdate,
                                            DMF_CONTEXT_ComponentFirmwareUpdate,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_ComponentFirmwareUpdate.CallbacksDmf = &dmfCallbacksDmf_ComponentFirmwareUpdate;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_ComponentFirmwareUpdate,
                                &dmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_ModuleCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
    }

    // Initialize Protocol's declaration data.
    //
    DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DESCRIPTOR_INIT(&protocolDeclarationData,
                                                                   DMF_ComponentFirmwareUpdate_Bind,
                                                                   DMF_ComponentFirmwareUpdate_Unbind,
                                                                   DMF_ComponentFirmwareUpdate_PostBind,
                                                                   DMF_ComponentFirmwareUpdate_PreUnbind,
                                                                   DMF_ComponentFirmwareUpdate_FirmwareVersionResponseEvt,
                                                                   DMF_ComponentFirmwareUpdate_OfferResponseEvt,
                                                                   DMF_ComponentFirmwareUpdate_PayloadResponseEvt);

    // An optional context can be set by the Protocol module on the bind instance
    // This is a unique context for each instance of Protocol Transport binding. 
    // E.g. in case a protocol module is bound to multiple modules, the Protocol 
    // Module will get a unique instance of this context each binding. 
    // 
    DMF_INTERFACE_DESCRIPTOR_SET_CONTEXT_TYPE(&protocolDeclarationData, 
                                              CONTEXT_ComponentFirmwareUpdateTransport);

    // Add the interface to the Protocol Module.
    //
    ntStatus = DMF_ModuleInterfaceDescriptorAdd(dmfModule,
                                                (DMF_INTERFACE_DESCRIPTOR*)&protocolDeclarationData);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleInterfaceDescriptorAdd fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    *DmfModule = dmfModule;
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts the Component Firmware Update Protocol.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ComponentFirmwareUpdate);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ULONG countOfOfferPayloadPairs = WdfCollectionGetCount(moduleContext->FirmwareBlobCollection);
    if (countOfOfferPayloadPairs == 0)
    {
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE,
                    "No Firmware available to process. Skipping the entire transaction.");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    BOOLEAN TransactionInProgress = FALSE;
    DMF_ModuleLock(DmfModule);
    TransactionInProgress = moduleContext->TransactionInProgress;
    DMF_ModuleUnlock(DmfModule);

    // allow only one protocol transaction at a time.
    //
    if (TransactionInProgress == TRUE)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Protocol thread is already runinng. Skipping the request to start protocol.");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Creating a thread to start the protocol sequence.");

    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_Thread_Start fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    DMF_Thread_WorkReady(moduleContext->DmfModuleThread);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ComponentFirmwareUpdate_Stop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stop the Component Firmware Update Protocol.

Arguments:

    DmfModule - This Module's DMF Object.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdate* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ComponentFirmwareUpdate);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ULONG countOfOfferPayloadPairs = WdfCollectionGetCount(moduleContext->FirmwareBlobCollection);
    if (countOfOfferPayloadPairs == 0)
    {
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE,
                    "No Firmware available to process. Skipping Stop request.");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    BOOLEAN transactionInProgress = FALSE;
    DMF_ModuleLock(DmfModule);
    transactionInProgress = moduleContext->TransactionInProgress;
    DMF_ModuleUnlock(DmfModule);

    // We allow only 1 protocol sequence at a time.
    //
    if (transactionInProgress == FALSE)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Protocol thread is not runinng. Skipping the Stop protocol request.");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Sending a PreClose command to transport");

    // Send Protocol Stop to Transport as we are about to wind up the protocol sequences.
    // 
    ntStatus = ComponentFirmwareUpdate_ProtocolStop(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE,
                    "ComponentFirmwareUpdate_ProtocolStop fails: ntStatus=%!STATUS!",
                    ntStatus);

        //Continue Stopping the thread.
        //
        ntStatus = STATUS_SUCCESS;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Stopping the protocol sequence thread.");

    // Signal the thread to stop and wait for it to complete.
    //
    DMF_Thread_Stop(moduleContext->DmfModuleThread);

Exit:

    FuncExitVoid(DMF_TRACE);

}
#pragma code_seg()

// eof: Dmf_ComponentFirmwareUpdate.c
//
