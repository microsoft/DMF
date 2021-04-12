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
// to write its data.  This is called during BugCheck at IRQL = HIGH_LEVEL so must 
// be nonpaged and has restrictions on what it may do.
//
typedef
_Function_class_(EVT_DMF_CrashDump_Query)
_IRQL_requires_same_
VOID
EVT_DMF_CrashDump_Query(_In_ DMFMODULE DmfModule,
                        _Out_ VOID** OutputBuffer,
                        _Out_ ULONG* SizeNeededBytes);

// Callback function for client driver to write its own data after the system is crashed.
// Note that this callback is only applicable to the RINGBUFFER_INDEX_SELF instance. Other instances
// are used by User-mode and cannot use this callback. This is called during BugCheck 
// at IRQL = HIGH_LEVEL so must be nonpaged and has restrictions on what it may do.
//
typedef
_Function_class_(EVT_DMF_CrashDump_Write)
_IRQL_requires_same_
VOID
EVT_DMF_CrashDump_Write(_In_ DMFMODULE DmfModule,
                        _Out_ VOID** OutputBuffer,
                        _In_ ULONG* OutputBufferLength);

// Callback for marking memory regions which should be included in the kernel minidump.
// This is called during BugCheck at IRQL = HIGH_LEVEL so must be nonpaged and
// has restrictions on what it may do.  The bugcheck code and parameters
// are provided so the callback may choose to only add data when certain Bug Checks occur.
//
typedef
_Function_class_(EVT_DMF_CrashDump_StoreTriageDumpData)
_IRQL_requires_same_
VOID
EVT_DMF_CrashDump_StoreTriageDumpData(_In_ DMFMODULE DmfModule,
                                      _In_ ULONG BugCheckCode,
                                      _In_ ULONG_PTR BugCheckParameter1,
                                      _In_ ULONG_PTR BugCheckParameter2,
                                      _In_ ULONG_PTR BugCheckParameter3,
                                      _In_ ULONG_PTR BugCheckParameter4);

typedef struct
{
    // Number of triage dump data entries to allocate. This must be
    // set before using DMF_CrashDumpDataAdd.
    ULONG TriageDumpDataArraySize;
    // Callback for adding triage dump ranges during BugCheck processing.
    // This is optional, even if passing a TriageDumpDataArraySize since
    // buffers can be added prior to a BugCheck occurring.
    //
    EVT_DMF_CrashDump_StoreTriageDumpData* EvtCrashDumpStoreTriageDumpData;
} CrashDump_TriageDumpData;

typedef struct
{
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
} CrashDump_SecondaryData;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // The identifier of this component. It will be in the Bug Check data.
    //
    UCHAR* ComponentName;
    // Secondary (Blob) data callback configuration.
    //
    CrashDump_SecondaryData SecondaryData;
    // TriageDumpData callback configuration.
    //
    CrashDump_TriageDumpData TriageDumpData;
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

_IRQL_requires_same_
NTSTATUS
DMF_CrashDump_TriageDumpDataAdd(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* Data,
    _In_ ULONG DataLength
    );

// eof: Dmf_CrashDump.h
//
