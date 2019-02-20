/*++

    Copyright (c) Microsoft Corporation. All Rights Reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Interface_SampleInterface.h

Abstract:

    Companion file to Dmf_Interface_SampleInterface.h.
    Defines a Sample Interface Contract between a Protocol and Transport Module.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Bind Time Data.
// ---------------
//
// Data provided by the Protocol Module.
//
typedef struct _DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA
{
    // Stores this Protocol's Id.
    //
    ULONG ProtocolId;
} DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA;

// Data provided by the Transport Module.
//
typedef struct _DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA
{
    // Stores this Transport's Id.
    //
    ULONG TransportId;
} DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA;

// Declaration Time Data.
// ----------------------
//
// Callbacks provided by Protocol Module.
//
// Callback 1.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1(
    _In_ DMFINTERFACE DmfInterface
    );

// Data that fully describes this Protocol.
//
typedef struct _DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA
{
    // The Protocol Interface Descriptor. 
    // Every Interface must have this as the first member of its Protocol Declaration Data.
    //
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR DmfProtocolDescriptor;
    // Stores callbacks implemented by this Interface Protocol.
    //
    EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1* EVT_SampleInterface_ProtocolCallback1;
} DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA;

// Methods used to initialize Protocol's Declaration Data.
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
    );

// Methods provided by Transport Module.
//
// Bind
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SampleInterface_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA* TransportBindData
    );

// Unbind
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_INTERFACE_SampleInterface_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    );

// Test Method 1.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SampleInterface_TransportMethod1(
    _In_ DMFINTERFACE DmfInterface
    );

// Data that fully describes this Transport.
//
typedef struct _DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA
{
    // The Transport Interface Descriptor.
    // Every Interface must have this as the first member of its Transport Declaration Data.
    //
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR DmfTransportDescriptor;
    // Stores methods implemented by this Interface Transport.
    //
    DMF_INTERFACE_SampleInterface_TransportBind* DMF_SampleInterface_TransportBind;
    DMF_INTERFACE_SampleInterface_TransportUnbind* DMF_SampleInterface_TransportUnbind;
    DMF_INTERFACE_SampleInterface_TransportMethod1* DMF_SampleInterface_TransportMethod1;
} DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA;

// Methods used to initialize Transport's Declaration Data.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_TRANSPORT_SampleInterface_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA* TransportDeclarationData,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ DMF_INTERFACE_SampleInterface_TransportBind* SampleInterfaceTransportBind,
    _In_ DMF_INTERFACE_SampleInterface_TransportUnbind* SampleInterfaceTransportUnbind,
    _In_ DMF_INTERFACE_SampleInterface_TransportMethod1* SampleInterfaceTransportMethod1
    );

// Methods exposed to Protocol.
// ----------------------------
//
DMF_INTERFACE_SampleInterface_TransportBind DMF_SampleInterface_TransportBind;
DMF_INTERFACE_SampleInterface_TransportUnbind DMF_SampleInterface_TransportUnbind;
DMF_INTERFACE_SampleInterface_TransportMethod1 DMF_SampleInterface_TransportMethod1;

// Callbacks exposed to Transport.
// -------------------------------
//
EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1 EVT_SampleInterface_ProtocolCallback1;

// This macro defines the SampleInterfaceProtocolDeclarationDataGet and SampleInterfaceTransportDeclarationDataGet.
// Call this macro after the DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA and
// DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA are defined.
//
DECLARE_DMF_INTERFACE(SampleInterface);

// eof: Dmf_Interface_SampleInterface.h
//