/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_CrashDump.h

Abstract:

    Companion file to Dmf_CrashDump.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// The maximum number of data sources that store data in a crash dump.
// 1 is minimum for Client Driver.
//
#define CrashDump_MAXIMUM_NUMBER_OF_DATA_SOURCES     8
#define CrashDump_MINIMUM_NUMBER_OF_DATA_SOURCES     1

#define CrashDump_COMPONENT_NAME_STRING              256

#define RINGBUFFER_INDEX_INVALID          (-1)
#define RINGBUFFER_INDEX_SELF             0
#define RINGBUFFER_INDEX_CLIENT_FIRST     (RINGBUFFER_INDEX_SELF + 1)

// Callback function for client driver to inform OS how much space Client Driver needs 
// to write its data.
//
typedef
_Function_class_(EVT_DMF_CrashDump_Query)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_CrashDump_Query(_In_ DMFMODULE DmfModule,
                        _Out_ VOID** OutputBuffer,
                        _Out_ ULONG* SizeNeededBytes);

// Callback function for client driver to write its own data after the system is crashed.
// Note that this callback is only applicable to the RINGBUFFER_INDEX_SELF instance. Other instances
// are used by User-mode and cannot use this callback.
//
typedef
_Function_class_(EVT_DMF_CrashDump_Write)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_CrashDump_Write(_In_ DMFMODULE DmfModule,
                        _Out_ VOID** OutputBuffer,
                        _In_ ULONG* OutputBufferLength);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // The identifier of this component. It will be in the Bug Check data.
    //
    UCHAR* ComponentName;
    // GUID for this driver's Ring Buffer data.
    //
    GUID RingBufferDataGuid;
    // GUID for this driver's additional data.
    //
    GUID AdditionalDataGuid;
    // Buffer Size for the RINGBUFFER_INDEX_SELF Ring Buffer. (This driver.)
    // NOTE: Use the absolute minimum necessary. Compress data if necessary!.
    //
    ULONG BufferSize;
    // Number of buffers for RINGBUFFER_INDEX_SELF Ring Buffer. (This driver.)
    // NOTE: Use the absolute minimum necessary. Compress data if necessary!.
    //
    ULONG BufferCount;
    // Maximum size of ring buffer to allow.
    //
    ULONG RingBufferMaximumSize;
    // Callbacks for the RINGBUFFER_INDEX_SELF Ring Buffer. (This driver.)
    //
    EVT_DMF_CrashDump_Query* EvtCrashDumpQuery;
    EVT_DMF_CrashDump_Write* EvtCrashDumpWrite;
    // Number of Data Sources for other clients.
    //
    ULONG DataSourceCount;
} DMF_CONFIG_CrashDump;

// This macro declares the following functions:
// DMF_CrashDump_ATTRIBUTES_INIT()
// DMF_CONFIG_CrashDump_AND_ATTRIBUTES_INIT()
// DMF_CrashDump_Create()
//
DECLARE_DMF_MODULE(CrashDump)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_CrashDump_DataSourceWriteSelf(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* Buffer,
    _In_ ULONG BufferLength
    );

// eof: Dmf_CrashDump.h
//
