/*++

    Copyright (c) Microsoft Corporation. All Rights Reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Interface_BusTarget.h

Abstract:

    Implements an Interface Contract between a PowerMeterDevice (Protocol) and BusTargets
    (Transport). The protocol layer shouldnt not know the below transport layer that it is
    attached to. All it has to do is call the interface functions which routes the calls to the
    right Transport based on INIT.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef struct
{
    ULONG Message;
    union
    {
        struct
        {
            UCHAR* Address;
            ULONG  AddressLength;
            UCHAR* Buffer;
            ULONG  BufferLength;
        } AddressWrite;

        struct
        {
            UCHAR* Address;
            ULONG  AddressLength;
            UCHAR* Buffer;
            ULONG  BufferLength;
        } AddressRead;

        struct
        {
            UCHAR* Buffer;
            ULONG  BufferLength;
        } BufferWrite;

        struct
        {
            UCHAR* Buffer;
            ULONG  BufferLength;
        } BufferRead;
    };
} BusTransport_TransportPayload;

// TransportCall Interface to Chip Module.
//

// Bind Time Data.
// ---------------
//
// Data provided by the Protocol Module.
//
typedef struct _DMF_INTERFACE_PROTOCOL_BusTarget_BIND_DATA
{
    // Dummy for now
    //
    ULONG Dummy;
} DMF_INTERFACE_PROTOCOL_BusTarget_BIND_DATA;

// Data provided by the Transport Module.
//
typedef struct _DMF_INTERFACE_TRANSPORT_BusTarget_BIND_DATA
{
    // Dummy for now
    //
    ULONG Dummy;
} DMF_INTERFACE_TRANSPORT_BusTarget_BIND_DATA;

// Data that fully describes this BusTarget.
//
typedef struct _DMF_INTERFACE_PROTOCOL_BusTarget_DECLARATION_DATA
{
    // The Protocol Interface Descriptor. 
    // Every Interface must have this as the first member of its Protocol Declaration Data.
    //
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR DmfProtocolDescriptor;

} DMF_INTERFACE_PROTOCOL_BusTarget_DECLARATION_DATA;

// Methods used to initialize Protocol's Declaration Data.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_PROTOCOL_BusTarget_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_PROTOCOL_BusTarget_DECLARATION_DATA* ProtocolDeclarationData,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind
    );

// Methods provided by Transport Module.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_BusTarget_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_BusTarget_BIND_DATA* ProtocolBindData,
    _Inout_opt_ DMF_INTERFACE_TRANSPORT_BusTarget_BIND_DATA* TransportBindData
    );

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_INTERFACE_BusTarget_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    );

// Transport Method that is called from the client.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_BusTarget_AddressWrite(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    );

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_BusTarget_AddressRead(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    );

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_BusTarget_AddressReadEx(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload,
    _In_ ULONG RequestTimeoutMilliseconds
    );

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_BusTarget_BufferWriteEx(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload,
    _In_ ULONG RequestTimeoutMilliseconds
    );

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_BusTarget_BufferWrite(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    );

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_BusTarget_BufferRead(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    );

// Data that fully describes this Transport.
//

typedef struct _DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA
{
    // The Transport Interface Descriptor.
    // Every Interface must have this as the first member of its Transport Declaration Data.
    //
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR DmfTransportDescriptor;
    // Stores methods implemented by this Interface Transport.
    //
    DMF_INTERFACE_BusTarget_TransportBind* DMF_BusTarget_TransportBind;
    DMF_INTERFACE_BusTarget_TransportUnbind* DMF_BusTarget_TransportUnbind;
    DMF_INTERFACE_BusTarget_AddressWrite* DMF_BusTarget_AddressWrite;
    DMF_INTERFACE_BusTarget_AddressRead* DMF_BusTarget_AddressRead;
    DMF_INTERFACE_BusTarget_BufferWrite* DMF_BusTarget_BufferWrite;
    DMF_INTERFACE_BusTarget_BufferRead* DMF_BusTarget_BufferRead;
    DMF_INTERFACE_BusTarget_AddressReadEx* DMF_BusTarget_AddressReadEx;
    DMF_INTERFACE_BusTarget_BufferWriteEx* DMF_BusTarget_BufferWriteEx;
} DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA;

// Methods used to initialize Transport's Declaration Data.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_TRANSPORT_BusTarget_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA* TransportDeclarationData,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ DMF_INTERFACE_BusTarget_TransportBind* BusTargetTransportBind,
    _In_ DMF_INTERFACE_BusTarget_TransportUnbind* BusTargetTransportUnbind,
    _In_opt_ DMF_INTERFACE_BusTarget_AddressWrite* BusTarget_AddressWrite,
    _In_opt_ DMF_INTERFACE_BusTarget_AddressRead* BusTarget_AddressRead,
    _In_opt_ DMF_INTERFACE_BusTarget_BufferWrite* BusTarget_BufferWrite,
    _In_opt_ DMF_INTERFACE_BusTarget_BufferRead* BusTarget_BufferRead,
    _In_opt_ DMF_INTERFACE_BusTarget_AddressReadEx* BusTarget_AddressReadEx,
    _In_opt_ DMF_INTERFACE_BusTarget_BufferWriteEx* BusTarget_BufferWriteEx
    );

// Methods exposed to Protocol.
// ----------------------------
//
DMF_INTERFACE_BusTarget_TransportBind DMF_BusTarget_TransportBind;
DMF_INTERFACE_BusTarget_TransportUnbind DMF_BusTarget_TransportUnbind;
DMF_INTERFACE_BusTarget_AddressWrite DMF_BusTarget_AddressWrite;
DMF_INTERFACE_BusTarget_AddressRead Dmf_BusTarget_AddressRead;
DMF_INTERFACE_BusTarget_BufferWrite Dmf_BusTarget_BufferWrite;
DMF_INTERFACE_BusTarget_BufferRead Dmf_BusTarget_BufferRead;
DMF_INTERFACE_BusTarget_AddressReadEx Dmf_BusTarget_AddressReadEx;
DMF_INTERFACE_BusTarget_BufferWriteEx Dmf_BusTarget_BufferWriteEx;

// This macro defines the BusTargetProtocolDeclarationDataGet and BusTargetTransportDeclarationDataGet.
// Call this macro after the DMF_INTERFACE_PROTOCOL_BusTarget_DECLARATION_DATA and
// DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA are defined.
//
DECLARE_DMF_INTERFACE(BusTarget);

// eof: Dmf_Interface_BusTarget.h
//