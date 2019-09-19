/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterface.c

Abstract:

    Implements a Sample Interface Contract between a Protocol and Transport Module.
    This is a sample DMF Interface. It used by the sample Protocol/Transport Modules.
    All DMF Interfaces should define these functions. 

    NOTE: Use this file as  template when creating DMF Interfaces.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"
#include "Dmf_Interface_SampleInterface.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Validation
///////////////////////////////////////////////////////////////////////////////////////////////////////
//


_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_PROTOCOL_SampleInterface_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA* ProtocolDeclarationData,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1* EvtSampleInterfaceProtocolCallback1
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
    EvtSampleInterfaceProtocolCallback1 - This callback is unique to the SampleInterface and must be provided by any Protocol Module of this Interface.

Returns:

    None.
--*/
{
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR_INIT(&(ProtocolDeclarationData->DmfProtocolDescriptor),
                                           "SampleInterface",
                                           DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA,
                                           EvtProtocolBind,
                                           EvtProtocolUnbind,
                                           EvtPostBind,
                                           EvtPreUnbind);

    ProtocolDeclarationData->EVT_SampleInterface_ProtocolCallback1 = EvtSampleInterfaceProtocolCallback1;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_TRANSPORT_SampleInterface_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA* TransportDeclarationData,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ DMF_INTERFACE_SampleInterface_TransportBind* SampleInterfaceTransportBind,
    _In_ DMF_INTERFACE_SampleInterface_TransportUnbind* SampleInterfaceTransportUnbind,
    _In_ DMF_INTERFACE_SampleInterface_TransportMethod1* SampleInterfaceTransportMethod1
    )
/*++

Routine Description:

    Ensures all required methods are provided by Transport Module and populates the Declaration Data structure.

Arguments:

    TransportDeclarationData - The Transport's declaration data.
    EvtPostBind - Optional Post bind callback.
    EvtPreUnbind - Optional Pre Unbind callback.
    SampleInterfaceTransportBind - Transport's Bind method.
    SampleInterfaceTransportUnbind - Transport's Unbind method.
    SampleInterfaceTransportMethod1 - Transport's method1.

Return Value:

    None.
--*/
{
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR_INIT(&(TransportDeclarationData->DmfTransportDescriptor),
                                            "SampleInterface",
                                            DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA,
                                            EvtPostBind,
                                            EvtPreUnbind);

    TransportDeclarationData->DMF_SampleInterface_TransportBind = SampleInterfaceTransportBind;
    TransportDeclarationData->DMF_SampleInterface_TransportUnbind = SampleInterfaceTransportUnbind;
    TransportDeclarationData->DMF_SampleInterface_TransportMethod1 = SampleInterfaceTransportMethod1;

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Protocol Bind/Unbind
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

NTSTATUS
DMF_SampleInterface_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA* TransportBindData
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
    DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_SampleInterface_TransportBind");

    ntStatus = transportData->DMF_SampleInterface_TransportBind(DmfInterface,
                                                                ProtocolBindData, 
                                                                TransportBindData);

    return ntStatus;
}

VOID
DMF_SampleInterface_TransportUnbind(
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
    DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SampleInterface_UnbindSampleInterface_Unbind");

    transportData->DMF_SampleInterface_TransportUnbind(DmfInterface);

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Methods
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SampleInterface_TransportMethod1(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Sample Interface Method called by the given Protocol Module into the given Transport Module. 
    It simply emits logging and calls the Transport's corresponding Method.

Arguments:

    DmfInterfaceTransportModule - The given Transport Module.
    DmfInterfaceProtocolModule -  The given Protocol Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SampleInterface_Method1");

    ntStatus = transportData->DMF_SampleInterface_TransportMethod1(DmfInterface);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_SampleInterface_ProtocolCallback1(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Sample Interface Callback called by the given Transport Module into the given Protocol Module. 
    It simply emits logging and calls the Protocol's corresponding Method.

Arguments:

    DmfInterfaceTransportModule - The given Transport Module.
    DmfInterfaceProtocolModule -  The given Protocol Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA* protocolData;

    protocolData = (DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA*)DMF_InterfaceProtocolDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "EVT_SampleInterface_ProtocolCallback1");

    protocolData->EVT_SampleInterface_ProtocolCallback1(DmfInterface);

    return;
}

// eof: Dmf_Interface_SampleInterface.c
//
