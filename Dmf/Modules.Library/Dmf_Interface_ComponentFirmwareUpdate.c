/*++

    Copyright (c) Microsoft Corporation. All Rights Reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Transport_ComponentFirmwareUpdate.c

Abstract:

    Contract between Dmf_ComponentFirmwareUpdate and it's Transport.

Environment:

    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_Interface_ComponentFirmwareUpdate.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Validation
///////////////////////////////////////////////////////////////////////////////////////////////////////
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
    )
/*++

Routine Description:

    Ensures all required callbacks are provided by Protocol Module and populates the Declaration Data structure.

Arguments:

    ProtocolDeclarationData - The Protocol's declaration data.
    EvtProtocolBind - The Bind callback. Must be provided by all Protocol Modules.
    EvtProtocolUnBind - The Unbind callback. Must be provided by all Protocol Modules.
    EvtPostBind - Optional Post bind callback.
    EvtPreUnbind - Optional Pre Unbind callback.
    EvtComponentFirmwareUpdate_FirmwareVersionResponse - Callback for Firmware Versions response.
    EvtComponentFirmwareUpdate_OfferResponse - Callback for Offer response.
    EvtComponentFirmwareUpdate_PayloadResponse - Callback for Payload response.

Returns:

    None.

--*/
{
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR_INIT(&(ProtocolDeclarationData->DmfProtocolDescriptor),
                                           "ComponentFirmwareUpdate",
                                           DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA,
                                           EvtProtocolBind,
                                           EvtProtocolUnbind,
                                           EvtPostBind,
                                           EvtPreUnbind);

    ProtocolDeclarationData->EVT_ComponentFirmwareUpdate_FirmwareVersionResponse = EvtComponentFirmwareUpdate_FirmwareVersionResponse;
    ProtocolDeclarationData->EVT_ComponentFirmwareUpdate_OfferResponse = EvtComponentFirmwareUpdate_OfferResponse;
    ProtocolDeclarationData->EVT_ComponentFirmwareUpdate_PayloadResponse = EvtComponentFirmwareUpdate_PayloadResponse;
}

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
    )
/*++

Routine Description:

    Ensures all required methods are provided by Transport Module and populates the Declaration Data structure.

Arguments:

    TransportDeclarationData - The Transport's declaration data.
    EvtPostBind - Optional Post bind callback.
    EvtPreUnbind - Optional Pre Unbind callback.
    ComponentFirmwareUpdate_TransportBind - Transport's Bind method.
    ComponentFirmwareUpdate_TransportUnbind - Transport's Unbind method.
    ComponentFirmwareUpdate_TransportFirmwareVersionGet - Transport's Get Firmware versions method.
    ComponentFirmwareUpdate_TransportOfferInformationSend - Transport's Send Offer Information method.
    ComponentFirmwareUpdate_TransportOfferCommandSend - Transport's Send Offer Command method. 
    ComponentFirmwareUpdate_TransportOfferSend - Transport's Send Offer method.
    ComponentFirmwareUpdate_TransportPayloadSend - Transport's Send Payload method.
    ComponentFirmwareUpdate_TransportProtocolStart - Transport's Protocol Start method.
    ComponentFirmwareUpdate_TransportProtocolStop - Transport's Protocol Stop method.

Return Value:

    None.

--*/
{
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR_INIT(&(TransportDeclarationData->DmfTransportDescriptor),
                                            "ComponentFirmwareUpdate",
                                            DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA,
                                            EvtPostBind,
                                            EvtPreUnbind);

    TransportDeclarationData->DMF_ComponentFirmwareUpdate_TransportBind = ComponentFirmwareUpdate_TransportBind;
    TransportDeclarationData->DMF_ComponentFirmwareUpdate_TransportUnbind = ComponentFirmwareUpdate_TransportUnbind;
    TransportDeclarationData->DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet = ComponentFirmwareUpdate_TransportFirmwareVersionGet;
    TransportDeclarationData->DMF_ComponentFirmwareUpdate_TransportOfferInformationSend = ComponentFirmwareUpdate_TransportOfferInformationSend;
    TransportDeclarationData->DMF_ComponentFirmwareUpdate_TransportOfferCommandSend = ComponentFirmwareUpdate_TransportOfferCommandSend;
    TransportDeclarationData->DMF_ComponentFirmwareUpdate_TransportOfferSend = ComponentFirmwareUpdate_TransportOfferSend;
    TransportDeclarationData->DMF_ComponentFirmwareUpdate_TransportPayloadSend = ComponentFirmwareUpdate_TransportPayloadSend;
    TransportDeclarationData->DMF_ComponentFirmwareUpdate_TransportProtocolStart = ComponentFirmwareUpdate_TransportProtocolStart;
    TransportDeclarationData->DMF_ComponentFirmwareUpdate_TransportProtocolStop = ComponentFirmwareUpdate_TransportProtocolStop;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Protocol Bind/Unbind
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

NTSTATUS
DMF_ComponentFirmwareUpdate_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA* TransportBindData
    )
/*++

Routine Description:

    Registers Protocol Module with the Transport Module. This is called by Protocol Module.

Arguments:

    DmfInterface - Interface handle.
    ProtocolBindData - Bind time data provided by Protocol to the Transport.
    TransportBindData - Bind time data provided by Transport to the Protocol.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_ComponentFirmwareUpdate_TransportBind");

    ntStatus = transportData->DMF_ComponentFirmwareUpdate_TransportBind(DmfInterface,
                                                                        ProtocolBindData, 
                                                                        TransportBindData);

    return ntStatus;
}

VOID
DMF_ComponentFirmwareUpdate_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Unregisters the given Protocol Module from the Transport Module. This is called by Protocol Module.

Arguments:

    DmfInterface - Interface handle.

Return Value:

    None.

--*/
{
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ComponentFirmwareUpdate_UnbindComponentFirmwareUpdate_Unbind");

    transportData->DMF_ComponentFirmwareUpdate_TransportUnbind(DmfInterface);

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Methods
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Retrieves the firmware versions from the device.

Arguments:

    DmfInterface - Interface handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet");

    ntStatus = transportData->DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet(DmfInterface);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportOfferInformationSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer information command to the device.

Parameters:

    DmfInterface - Interface handle.
    Buffer - Header, followed by Offer Information to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the begining of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_ComponentFirmwareUpdate_TransportOfferInformationSend");

    ntStatus = transportData->DMF_ComponentFirmwareUpdate_TransportOfferInformationSend(DmfInterface,
                                                                                        Buffer,
                                                                                        BufferSize,
                                                                                        HeaderSize);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportOfferCommandSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer information command to the device.

Parameters:

    DmfInterface - Interface handle.
    Buffer - Header followed by Offer Command to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the begining of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_ComponentFirmwareUpdate_TransportOfferCommandSend");

    ntStatus = transportData->DMF_ComponentFirmwareUpdate_TransportOfferCommandSend(DmfInterface,
                                                                                    Buffer,
                                                                                    BufferSize,
                                                                                    HeaderSize);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportOfferSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer to the device.

Parameters:

    DmfInterface - Interface handle.
    Buffer - Header followed by Offer to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the begining of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_ComponentFirmwareUpdate_TransportOfferSend");

    ntStatus = transportData->DMF_ComponentFirmwareUpdate_TransportOfferSend(DmfInterface,
                                                                             Buffer,
                                                                             BufferSize,
                                                                             HeaderSize);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportPayloadSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends Payload to the device.

Parameters:

    DmfInterface - Interface handle.
    Buffer - Header, followed by Payload to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the begining of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_ComponentFirmwareUpdate_TransportPayloadSend");

    ntStatus = transportData->DMF_ComponentFirmwareUpdate_TransportPayloadSend(DmfInterface,
                                                                               Buffer,
                                                                               BufferSize,
                                                                               HeaderSize);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportProtocolStart(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Setup the transport for protocol transaction.

Parameters:

    DmfInterface - Interface handle.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_ComponentFirmwareUpdate_TransportProtocolStart");

    ntStatus = transportData->DMF_ComponentFirmwareUpdate_TransportProtocolStart(DmfInterface);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportProtocolStop(
    _In_ DMFINTERFACE DmfInterface
)
/*++

Routine Description:

    Clean up the transport as the protocol is being stopped.

Parameters:

    DmfInterface - Interface handle.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_ComponentFirmwareUpdate_TransportProtocolStop");

    ntStatus = transportData->DMF_ComponentFirmwareUpdate_TransportProtocolStop(DmfInterface);

    return ntStatus;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_ComponentFirmwareUpdate_FirmwareVersionResponse(
    _In_ DMFINTERFACE DmfInterface,
    _In_reads_bytes_(FirmwareVersionBufferSize) UCHAR* FirmwareVersionBuffer,
    _In_ size_t FirmwareVersionBufferSize,
    _In_ NTSTATUS ntStatus
    )
/*++

Routine Description:

    Callback to indicate the firmware versions.

Arguments:

    DmfInterface - Interface handle.
    FirmwareVersionsBuffer - Buffer with firmware information.
    FirmwareVersionsBufferSize - size of the above in bytes.
    ntStatus -  NTSTATUS for the FirmwareVersionGet.

Return Value:

    None.

--*/
{
    DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA* protocolData;

    protocolData = (DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceProtocolDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "EVT_ComponentFirmwareUpdate_FirmwareVersionResponse");

    protocolData->EVT_ComponentFirmwareUpdate_FirmwareVersionResponse(DmfInterface,
                                                                      FirmwareVersionBuffer,
                                                                      FirmwareVersionBufferSize,
                                                                      ntStatus);

    return;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_ComponentFirmwareUpdate_OfferResponse(
    _In_ DMFINTERFACE DmfInterface,
    _In_reads_bytes_(ResponseBufferSize) UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatus
    )
/*++

Routine Description:

    Callback to indicate the response to an offer that was sent to device.

Arguments:

    DmfInterface - Interface handle.
    ResponseBuffer - Buffer with response information.
    ResponseBufferSize - size of the above in bytes.
    ntStatusCallback -  NTSTATUS for the command that was sent down.

Return Value:

    None.

--*/
{
    DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA* protocolData;

    protocolData = (DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceProtocolDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "EVT_ComponentFirmwareUpdate_OfferResponse");

    protocolData->EVT_ComponentFirmwareUpdate_OfferResponse(DmfInterface,
                                                            ResponseBuffer,
                                                            ResponseBufferSize,
                                                            ntStatus);

    return;
}



_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_ComponentFirmwareUpdate_PayloadResponse(
    _In_ DMFINTERFACE DmfInterface,
    _In_reads_bytes_(ResponseBufferSize) UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatus
    )
/*++

Routine Description:

    Callback to indicate the response to a payload that was sent to device.

Arguments:

    DmfInterface - Interface handle.
    ResponseBuffer - Buffer with response information.
    ResponseBufferSize - size of the above in bytes.
    ntStatusCallback - NTSTATUS for the command that was sent down.

Return Value:

    None.

--*/
{
    DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA* protocolData;

    protocolData = (DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA*)DMF_InterfaceProtocolDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "EVT_ComponentFirmwareUpdate_PayloadResponse");

    protocolData->EVT_ComponentFirmwareUpdate_PayloadResponse(DmfInterface,
                                                              ResponseBuffer,
                                                              ResponseBufferSize,
                                                              ntStatus);

    return;
}

// eof: Dmf_Interface_ComponentFirmwareUpdate.c
//
