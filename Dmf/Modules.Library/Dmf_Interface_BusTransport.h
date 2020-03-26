/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Interface_BusTransport.h

Abstract:

    Defines legacy BusTransport interface.

    PTREMOVE: Remove this after upgrade to new Protocol-Transport code.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// {875C6494-09D6-4F7D-A9D9-01E35C9343BB}
//
DEFINE_GUID(BusTransport_Interface_Guid,
    0x875c6494, 0x9d6, 0x4f7d, 0xa9, 0xd9, 0x1, 0xe3, 0x5c, 0x93, 0x43, 0xbb);

// Transport Module Specific.
//
#define BusTransport_TransportMessage_AddressWrite          0
#define BusTransport_TransportMessage_AddressRead           1
#define BusTransport_TransportMessage_BufferWrite           2
#define BusTransport_TransportMessage_BufferRead            3

#define BusTransport_TransportMessage_Hid_FeatureGet        4
#define BusTransport_TransportMessage_Hid_FeatureSet        5

typedef struct
{
    ULONG Message;
    union
    {
        struct
        {
            UCHAR FeatureId;
            UCHAR* Buffer;
            ULONG BufferLength;
            ULONG Offset;
            ULONG BytesToCopy;
        } HidFeatureSet;

        struct
        {
            UCHAR FeatureId;
            UCHAR* Buffer;
            ULONG BufferLength;
            ULONG Offset;
            ULONG BytesToCopy;
        } HidFeatureGet;

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

// eof: Dmf_Interface_BusTransport.h
//
