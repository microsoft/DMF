/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Interface_BusTarget.c

Abstract:

    Implements an Interface Contract between a Protocol and BusTargets (Transport). 
    The protocol layer shouldnt not know the below transport layer that it is attached to.
    All it has to do is call the interface functions which routes the calls to the
    right Transport.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"
#include "Dmf_Interface_BusTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Validation
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_PROTOCOL_BusTarget_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_PROTOCOL_BusTarget_DECLARATION_DATA* ProtocolDeclarationData,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind
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
    EvtBusTargetProtocolCallback1 - This callback is unique to the BusTarget and must be provided by any Protocol Module of this Interface.

Returns:

    None.
--*/
{
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR_INIT(&(ProtocolDeclarationData->DmfProtocolDescriptor),
                                           "BusTarget",
                                           DMF_INTERFACE_PROTOCOL_BusTarget_DECLARATION_DATA,
                                           EvtProtocolBind,
                                           EvtProtocolUnbind,
                                           EvtPostBind,
                                           EvtPreUnbind);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_TRANSPORT_BusTarget_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA* TransportDeclarationData,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ DMF_INTERFACE_BusTarget_TransportBind* BusTargetTransportBind,
    _In_ DMF_INTERFACE_BusTarget_TransportUnbind* BusTargetTransportUnbind,
    _In_ DMF_INTERFACE_BusTarget_AddressWrite* BusTarget_AddressWrite,
    _In_ DMF_INTERFACE_BusTarget_AddressRead* BusTarget_AddressRead,
    _In_ DMF_INTERFACE_BusTarget_BufferWrite* BusTarget_BufferWrite,
    _In_ DMF_INTERFACE_BusTarget_BufferRead* BusTarget_BufferRead
    )
/*++

Routine Description:

    Ensures all required methods are provided by Transport Module and populates the Declaration Data structure.

Arguments:

    TransportDeclarationData - The Transport's declaration data.
    EvtPostBind - Optional Post bind callback.
    EvtPreUnbind - Optional Pre Unbind callback.
    BusTargetTransportBind - Transport's Bind method.
    BusTargetTransportUnbind - Transport's Unbind method.
    BusTargetTransportMethod - Transport's method.

Return Value:

    None.
--*/
{
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR_INIT(&(TransportDeclarationData->DmfTransportDescriptor),
                                            "BusTarget",
                                            DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA,
                                            EvtPostBind,
                                            EvtPreUnbind);

    TransportDeclarationData->DMF_BusTarget_TransportBind = BusTargetTransportBind;
    TransportDeclarationData->DMF_BusTarget_TransportUnbind = BusTargetTransportUnbind;
    TransportDeclarationData->DMF_BusTarget_AddressWrite = BusTarget_AddressWrite;
    TransportDeclarationData->DMF_BusTarget_AddressRead = BusTarget_AddressRead;
    TransportDeclarationData->DMF_BusTarget_BufferWrite = BusTarget_BufferWrite;
    TransportDeclarationData->DMF_BusTarget_BufferRead = BusTarget_BufferRead;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Protocol Bind/Unbind
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

NTSTATUS
DMF_BusTarget_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_BusTarget_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_BusTarget_BIND_DATA* TransportBindData
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
    DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_BusTarget_TransportBind");

    ntStatus = transportData->DMF_BusTarget_TransportBind(DmfInterface,
                                                          ProtocolBindData, 
                                                          TransportBindData);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BusTarget_TransportBind fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

VOID
DMF_BusTarget_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Unregisters the given Protocol Module from the Transport Module. This is called by Protocol Module.

Arguments:

    DmfInterfaceTransportModule - The given Transport Module.
    DmfInterfaceProtocolModule - The given Protocol Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "BusTarget_Unbind");

    transportData->DMF_BusTarget_TransportUnbind(DmfInterface);

    return;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_BusTarget_AddressWrite(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    Unregisters the given Protocol Module from the Transport Module. This is called by Protocol Module.

Arguments:

    DmfInterfaceTransportModule - The given Transport Module.
    DmfInterfaceProtocolModule - The given Protocol Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    ntStatus = transportData->DMF_BusTarget_AddressWrite(DmfInterface,
                                                         Payload);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BusTarget_AddressWrite fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
Dmf_BusTarget_AddressRead(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    Unregisters the given Protocol Module from the Transport Module. This is called by Protocol Module.

Arguments:

    DmfInterfaceTransportModule - The given Transport Module.
    DmfInterfaceProtocolModule - The given Protocol Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    ntStatus = transportData->DMF_BusTarget_AddressRead(DmfInterface,
                                                        Payload);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BusTarget_AddressRead fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
Dmf_BusTarget_BufferWrite(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    Unregisters the given Protocol Module from the Transport Module. This is called by Protocol Module.

Arguments:

    DmfInterfaceTransportModule - The given Transport Module.
    DmfInterfaceProtocolModule - The given Protocol Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    ntStatus = transportData->DMF_BusTarget_BufferWrite(DmfInterface,
                                                        Payload);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BusTarget_BufferWrite fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
Dmf_BusTarget_BufferRead(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    Unregisters the given Protocol Module from the Transport Module. This is called by Protocol Module.

Arguments:

    DmfInterfaceTransportModule - The given Transport Module.
    DmfInterfaceProtocolModule - The given Protocol Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    ntStatus = transportData->DMF_BusTarget_BufferRead(DmfInterface,
                                                       Payload);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BusTarget_BufferRead fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Methods
///////////////////////////////////////////////////////////////////////////////////////////////////////
//


///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// eof: Dmf_Interface_BusTarget.c
//
