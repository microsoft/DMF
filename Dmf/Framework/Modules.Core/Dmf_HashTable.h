/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_HashTable.h

Abstract:

    Companion file to Dmf_HashTable.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Callback function for client driver to replace the default hashing algorithm 
//
typedef
_Function_class_(EVT_DMF_HashTable_HashCalculate)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ULONG_PTR
EVT_DMF_HashTable_HashCalculate(_In_ DMFMODULE DmfModule,
                                _In_reads_(KeyLength) UCHAR* Key,
                                _In_ ULONG KeyLength);

// Callback function for client driver to process table entry.
//
typedef
_Function_class_(EVT_DMF_HashTable_Find)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_HashTable_Find(_In_ DMFMODULE DmfModule,
                       _In_reads_(KeyLength) UCHAR* Key,
                       _In_ ULONG KeyLength,
                       _Inout_updates_to_(*ValueLength, *ValueLength) UCHAR* Value,
                       _Inout_ ULONG* ValueLength);

// Callback function for client driver to enumerate the table.
//
typedef
_Function_class_(EVT_DMF_HashTable_Enumerate)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_HashTable_Enumerate(_In_ DMFMODULE DmfModule,
                            _In_reads_(KeyLength) UCHAR* Key,
                            _In_ ULONG KeyLength,
                            _In_reads_(ValueLength) UCHAR* Value,
                            _In_ ULONG ValueLength,
                            _In_ VOID* CallbackContext);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Maximum Key length in bytes.
    //
    ULONG MaximumKeyLength;

    // Maximum Value length in bytes.
    //
    ULONG MaximumValueLength;

    // Maximum number of Key-Value pairs to store in the hash table.
    //
    ULONG MaximumTableSize;

    // A callback to customize hashing algorithm.
    //
    EVT_DMF_HashTable_HashCalculate* EvtHashTableHashCalculate;
} DMF_CONFIG_HashTable;

// This macro declares the following functions:
// DMF_HashTable_ATTRIBUTES_INIT()
// DMF_CONFIG_HashTable_AND_ATTRIBUTES_INIT()
// DMF_HashTable_Create()
//
DECLARE_DMF_MODULE(HashTable)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_HashTable_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_HashTable_Enumerate* CallbackEnumerate,
    _In_ VOID* CallbackContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HashTable_Find(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _In_ EVT_DMF_HashTable_Find* CallbackFind
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HashTable_Read(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _Out_writes_(ValueBufferLength) UCHAR* ValueBuffer,
    _In_ ULONG ValueBufferLength,
    _Out_opt_ ULONG* ValueLength
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HashTable_Write(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _In_reads_(ValueLength) UCHAR* Value,
    _In_ ULONG ValueLength
    );

// eof: Dmf_HashTable.h
//
