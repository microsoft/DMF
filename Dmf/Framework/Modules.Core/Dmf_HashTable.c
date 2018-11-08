/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_HashTable.c

Abstract:

    Implements a Hash Table.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Core.h"
#include "DmfModules.Core.Trace.h"

#include "Dmf_HashTable.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Type of DataTable entry.
//
typedef struct
{
    // The actual length of the Key data in bytes.
    //
    ULONG KeyLength;

    // The length of the Value data in bytes.
    //
    ULONG ValueLength;

    // Next data entry, in case of a collision.
    //
    ULONG NextEntryIndex;

    // A buffer to store key and value data. Key data comes first, value data immediately follows it.
    //
    UCHAR RawData[ANYSIZE_ARRAY];
} DATA_ENTRY;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Maximum Key length in bytes.
    //
    ULONG MaximumKeyLength;

    // Maximum Value length in bytes.
    //
    ULONG MaximumValueLength;

    // Size of DataTable entry in bytes.
    //
    ULONG DataEntrySize;

    // Number of elements in HashMap.
    //
    ULONG HashMapSize;

    // Number of elements in DataTable.
    //
    ULONG DataTableSize;

    // Total number of allocated entries in DataTable.
    //
    ULONG DataEntriesAllocated;

    // An array mapping a hash of a key to an index in DataTable, where an actual key-value data is stored.
    // A hash of a key is used as an index in HashMap, and HashMap entry data is used as an index in DataTable.
    // In case of a collision, index in HashMap will point to a first entry in a linked list of entries having the same hash.
    //
    ULONG* HashMap;
    WDFMEMORY HashMapMemory;

    // Array containing actual key-value data entries.
    // In case of a collision, entries with the same hash are linked into a list.
    //
    VOID* DataTable;
    WDFMEMORY DataTableMemory;

    // A function used for hash calculation.
    //
    EVT_DMF_HashTable_HashCalculate* EvtHashTableHashCalculate;
} DMF_CONTEXT_HashTable;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(HashTable)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(HashTable)

// Memory Pool Tag.
//
#define MemoryTag 'oMTH'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// A value to indicate unused entries in a hash map.
//
#define INVALID_INDEX               (ULONG)(-1)

// A multiplier for a hash map size. The bigger the multiplier - the fewer the number of collisions.
//
#define HASH_MAP_SIZE_MULTIPLIER  2

static
inline
DATA_ENTRY*
HashTable_IndexToDataEntry(
    _In_ DMF_CONTEXT_HashTable* ModuleContext,
    _In_ ULONG EntryIndex
    )
/*++

Routine Description:

    Returns a pointer to the data entry with specified index.

Arguments:

    ModuleContext - This Module's context.
    EntryIndex - Entry's index within DataTable.

Return Value:

    Pointer to an entry with specified index

--*/
{
    return (DATA_ENTRY*)((UCHAR*)ModuleContext->DataTable + ModuleContext->DataEntrySize * EntryIndex);
}

static
inline
UCHAR*
HashTable_KeyBufferGet(
    DATA_ENTRY* DataEntry
    )
/*++

Routine Description:

    Returns a pointer to the first byte of Key buffer in specified DataEntry.

Arguments:

    DataEntry - Pointer to a DataTable entry.

Return Value:

    Pointer to Key buffer of the given DataEntry.

--*/
{
    return (&DataEntry->RawData[0]);
}

static
inline
UCHAR*
HashTable_ValueBufferGet(
    DATA_ENTRY* DataEntry
    )
/*++

Routine Description:

    Returns a pointer to the first byte of Value buffer in specified DataEntry.

Arguments:

    DataEntry - Pointer to a DataTable entry.

Return Value:

    Pointer to Value buffer of the given DataEntry.

--*/
{
    return (&DataEntry->RawData[DataEntry->KeyLength]);
}

static
ULONG_PTR
HashTable_HashCalculate(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength
    )
/*++

Routine Description:

    Default hash function. Calculates FNV-1a hash for specified buffer.

Arguments:

    DmfModule - DMF Module.
    Key - Address of the buffer containing Key data to calculate the hash.
    KeyLength - Length of Key data in bytes.

Return Value:

    FNV-1a hash of the data specified in Key buffer.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

#if defined(_WIN64)
    const ULONG_PTR offsetBasis = 14695981039346656037ULL;
    const ULONG_PTR prime = 1099511628211ULL;
#else
    const ULONG_PTR offsetBasis = 2166136261U;
    const ULONG_PTR prime = 16777619U;
#endif // defined(_WIN64)

    ULONG_PTR result;
    ULONG keyIndex;

    result = offsetBasis;

    for (keyIndex = 0; keyIndex < KeyLength; ++keyIndex)
    {
        result ^= (ULONG_PTR)Key[keyIndex];
        result *= prime;
    }

    return (result);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
HashTable_ContextCleanup(
    _Inout_ DMF_CONTEXT_HashTable* ModuleContext
    )
/*++

Routine Description:

    Cleans up the Module Context.

Arguments:

    ModuleContext - This Module's Context.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(NULL != ModuleContext);

    if (NULL != ModuleContext->HashMap)
    {
        WdfObjectDelete(ModuleContext->HashMapMemory);
        ModuleContext->HashMap = NULL;
    }

    if (NULL != ModuleContext->DataTable)
    {
        WdfObjectDelete(ModuleContext->DataTableMemory);
        ModuleContext->DataTable = NULL;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
HashTable_ContextInitialize(
    _In_ DMF_CONFIG_HashTable* ModuleConfig,
    _Inout_ DMF_CONTEXT_HashTable* ModuleContext
    )
/*++

Routine Description:

    Initializes the Module Context.

Arguments:

    ModuleConfig - This Module's Config.
    ModuleContext - This Module's Context to initialize.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    size_t sizeToAllocate;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(NULL != ModuleConfig);
    ASSERT(NULL != ModuleContext);
    ASSERT(NULL == ModuleContext->HashMap);
    ASSERT(NULL == ModuleContext->DataTable);

    ModuleContext->MaximumKeyLength = ModuleConfig->MaximumKeyLength;
    ModuleContext->MaximumValueLength = ModuleConfig->MaximumValueLength;

    // Calculate the size of DATA_ENTRY structure and make sure it's properly aligned.
    //
    ModuleContext->DataEntrySize = FIELD_OFFSET(DATA_ENTRY, RawData[ModuleConfig->MaximumKeyLength + ModuleConfig->MaximumValueLength]);
    ModuleContext->DataEntrySize = (ModuleContext->DataEntrySize + MAX_NATURAL_ALIGNMENT - 1) & ~(MAX_NATURAL_ALIGNMENT - 1);

    ModuleContext->HashMapSize = ModuleConfig->MaximumTableSize * HASH_MAP_SIZE_MULTIPLIER;
    ModuleContext->DataTableSize = ModuleConfig->MaximumTableSize;

    ModuleContext->DataEntriesAllocated = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE,
                "Create hash table: MaximumKeyLength=%u, MaximumValueLength=%u, DataEntrySize=%u, MaximumTableSize=%u",
                ModuleContext->MaximumKeyLength,
                ModuleContext->MaximumValueLength,
                ModuleContext->DataEntrySize,
                ModuleConfig->MaximumTableSize);

    // Use the default hash function, if a custom function is not specified.
    //
    if (ModuleConfig->EvtHashTableHashCalculate != NULL)
    {
        // Custom function.
        //
        ModuleContext->EvtHashTableHashCalculate = ModuleConfig->EvtHashTableHashCalculate;
    }
    else
    {
        // Default function.
        //
        ModuleContext->EvtHashTableHashCalculate = HashTable_HashCalculate;
    }

    sizeToAllocate = ModuleContext->HashMapSize * sizeof(ULONG);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    // 'Error annotation: __formal(3,BufferSize) cannot be zero.'.
    //
    #pragma warning(suppress:28160)
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeToAllocate,
                               &ModuleContext->HashMapMemory,
                               (VOID**)&ModuleContext->HashMap);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    RtlFillMemory(ModuleContext->HashMap,
                  sizeToAllocate,
                  INVALID_INDEX);

    sizeToAllocate = ModuleContext->DataTableSize * ModuleContext->DataEntrySize;
    ASSERT(sizeToAllocate != 0);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    // 'Error annotation: __formal(3,BufferSize) cannot be zero.'.
    //
    #pragma warning(suppress:28160)
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeToAllocate,
                               &ModuleContext->DataTableMemory,
                               (VOID**)&ModuleContext->DataTable);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(ModuleContext->DataTable,
                  sizeToAllocate);

    ntStatus = STATUS_SUCCESS;

Exit:

    if (! NT_SUCCESS(ntStatus))
    {
        HashTable_ContextCleanup(ModuleContext);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HashTable_DataEntryAllocate(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_CONTEXT_HashTable* ModuleContext,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _Out_ ULONG* NewEntryIndex
    )
/*++

Routine Description:

    Allocates data entry for specified key and returns its index.

Arguments:

    ModuleContext - This Module's context.
    Key - Address of the buffer containing Key data.
    KeyLength - Length of Key data in bytes.
    NewEntryIndex - A pointer to store the index of the allocated data entry.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    NTSTATUS ntStatus;
    DATA_ENTRY* entry;
    ULONG entryIndex;
    UCHAR* keyBuffer;

    UNREFERENCED_PARAMETER(DmfModule);

    ASSERT(DMF_ModuleIsLocked(DmfModule));

    ASSERT(NewEntryIndex != NULL);

    if (ModuleContext->DataEntriesAllocated >= ModuleContext->DataTableSize)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No more free slots available");
        ASSERT(FALSE);
        goto Exit;
    }

    entryIndex = ModuleContext->DataEntriesAllocated;
    ++(ModuleContext->DataEntriesAllocated);

    entry = HashTable_IndexToDataEntry(ModuleContext,
                                       entryIndex);

    entry->KeyLength = KeyLength;
    entry->ValueLength = 0;
    entry->NextEntryIndex = INVALID_INDEX;

    keyBuffer = HashTable_KeyBufferGet(entry);

    RtlCopyMemory(keyBuffer,
                  Key,
                  KeyLength);

    *NewEntryIndex = entryIndex;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HashTable_DataEntryFindOrAllocate(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _Out_ DATA_ENTRY** DataEntry
    )
/*++

Routine Description:

    Finds the entry with specified key. If the entry with this key does not exist - it will be created.

Arguments:

    DmfModule - DMF Module.
    Key - Address of the buffer containing Key data.
    KeyLength - Length of Key data in bytes.
    DataEntry - A pointer to store the resulting data entry.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    ULONG_PTR hash;
    ULONG entryIndex;
    NTSTATUS ntStatus;
    DMF_CONTEXT_HashTable* moduleContext;

    ASSERT(DataEntry != NULL);

    ASSERT(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    hash = moduleContext->EvtHashTableHashCalculate(DmfModule,
                                                    Key,
                                                    KeyLength);

    // Adjust the hash value to the size of the hash table, so that we can use the hash as an index in this table.
    //
    hash = hash % moduleContext->HashMapSize;

    entryIndex = moduleContext->HashMap[hash];

    if (INVALID_INDEX == entryIndex)
    {
        ntStatus = HashTable_DataEntryAllocate(DmfModule,
                                               moduleContext,
                                               Key,
                                               KeyLength,
                                               &entryIndex);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        moduleContext->HashMap[hash] = entryIndex;
        *DataEntry = HashTable_IndexToDataEntry(moduleContext,
                                                entryIndex);
    }
    else
    {
        DATA_ENTRY* currentEntry;

        // Search the table for the given key.
        //
        do
        {
            currentEntry = HashTable_IndexToDataEntry(moduleContext,
                                                      entryIndex);
            if ((currentEntry->KeyLength == KeyLength) &&
                (RtlCompareMemory(HashTable_KeyBufferGet(currentEntry),
                                  Key,
                                  KeyLength) == KeyLength))
            {
                // We have found the element with the key we are looking for.
                //
                *DataEntry = currentEntry;
                break;
            }

            entryIndex = currentEntry->NextEntryIndex;

        } while (entryIndex != INVALID_INDEX);

        if (INVALID_INDEX == entryIndex)
        {
            ntStatus = HashTable_DataEntryAllocate(DmfModule,
                                                   moduleContext,
                                                   Key,
                                                   KeyLength,
                                                   &entryIndex);
            if (! NT_SUCCESS(ntStatus))
            {
                goto Exit;
            }

            currentEntry->NextEntryIndex = entryIndex;
            *DataEntry = HashTable_IndexToDataEntry(moduleContext,
                                                    entryIndex);
        }
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HashTable_DataEntryFind(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _Out_ DATA_ENTRY** DataEntry
    )
/*++

Routine Description:

    Finds the entry with specified key.

Arguments:

    DmfModule - DMF Module.
    Key - Address of the buffer containing Key data.
    KeyLength - Length of Key data in bytes.
    DataEntry - A pointer to store the resulting data entry.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    ULONG_PTR hash;
    DATA_ENTRY* currentEntry;
    ULONG entryIndex;
    NTSTATUS ntStatus;
    DMF_CONTEXT_HashTable* moduleContext;

    ASSERT(DataEntry != NULL);

    ASSERT(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    hash = moduleContext->EvtHashTableHashCalculate(DmfModule,
                                                    Key,
                                                    KeyLength);

    // Adjust the hash value to the size of the hash table, so that we can use the hash as an index in this table.
    //
    hash = hash % moduleContext->HashMapSize;

    entryIndex = moduleContext->HashMap[hash];
    if (INVALID_INDEX == entryIndex)
    {
        ntStatus = STATUS_NOT_FOUND;
        goto Exit;
    }

    // Search the table for the given key.
    //
    do
    {
        currentEntry = HashTable_IndexToDataEntry(moduleContext,
                                                  entryIndex);

        if ((currentEntry->KeyLength == KeyLength) &&
            (RtlCompareMemory(HashTable_KeyBufferGet(currentEntry),
                              Key,
                              KeyLength) == KeyLength))
        {
            // We have found the element with the key we are looking for.
            //
            break;
        }

        entryIndex = currentEntry->NextEntryIndex;
    } while (entryIndex != INVALID_INDEX);

    if (INVALID_INDEX == entryIndex)
    {
        ntStatus = STATUS_NOT_FOUND;
        goto Exit;
    }

    *DataEntry = currentEntry;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_HashTable_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type HashTable.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    DMF_CONFIG_HashTable* moduleConfig;
    DMF_CONTEXT_HashTable* moduleContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = HashTable_ContextInitialize(moduleConfig,
                                           moduleContext);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_HashTable_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type HashTable.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_HashTable* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    HashTable_ContextCleanup(moduleContext);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_HashTable;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_HashTable;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HashTable_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_HashTable);
    DmfCallbacksDmf_HashTable.DeviceOpen = DMF_HashTable_Open;
    DmfCallbacksDmf_HashTable.DeviceClose = DMF_HashTable_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_HashTable,
                                            HashTable,
                                            DMF_CONTEXT_HashTable,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_HashTable.CallbacksDmf = &DmfCallbacksDmf_HashTable;
    DmfModuleDescriptor_HashTable.ModuleConfigSize = sizeof(DMF_CONFIG_HashTable);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_HashTable,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_HashTable_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_HashTable_Enumerate* CallbackEnumerate,
    _In_ VOID* CallbackContext
    )
/*++

Routine Description:

    Enumerates the content of the hash table and calls a callback function for each entry.

Arguments:

    DmfModule - This Module's handle.
    CallbackEnumerate - The callback to be called during enumeration. Enumeration stops when the callback returns FALSE.
    CallbackContext - Context pointer to pass into callback function.

Return Value:

    None

--*/
{
    DMF_CONTEXT_HashTable* moduleContext;
    ULONG entryIndex;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_HashTable);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Synchronize with calls to add items to table.
    //
    DMF_ModuleLock(DmfModule);

    for (entryIndex = 0; entryIndex < moduleContext->DataEntriesAllocated; ++entryIndex)
    {
        DATA_ENTRY* dataEntry = HashTable_IndexToDataEntry(moduleContext, entryIndex);

        if (! CallbackEnumerate(DmfModule,
                                HashTable_KeyBufferGet(dataEntry),
                                dataEntry->KeyLength,
                                HashTable_ValueBufferGet(dataEntry),
                                dataEntry->ValueLength,
                                CallbackContext))
        {
            break;
        }
    }

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HashTable_Find(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _In_ EVT_DMF_HashTable_Find* CallbackFind
    )
/*++

Routine Description:

    Finds the specified key in the hash table and calls a callback function to process the value associated with the key.
    In case the key is absent in the hash table, it will be added with the ValueLength set to zero, and then the callback will be called.

Arguments:

    DmfModule - This Module's handle.
    Key - Address of the buffer containing Key data.
    KeyLength - Length of Key data in bytes
    CallbackFind - The callback to process the value

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    DMF_CONTEXT_HashTable* moduleContext;
    NTSTATUS ntStatus;
    DATA_ENTRY* dataEntry;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_HashTable);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Synchronize with Methods to read, write and enumerate entries in table.
    //
    DMF_ModuleLock(DmfModule);

    ASSERT(KeyLength <= moduleContext->MaximumKeyLength);

    ntStatus = HashTable_DataEntryFindOrAllocate(DmfModule,
                                                 Key,
                                                 KeyLength,
                                                 &dataEntry);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ASSERT(CallbackFind != NULL);
    CallbackFind(DmfModule,
                 Key,
                 KeyLength,
                 HashTable_ValueBufferGet(dataEntry),
                 &dataEntry->ValueLength);

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Read the Value associated with the specified Key.

Arguments:

    DmfModule - This Module's handle.
    Key - Address of the buffer containing Key data.
    KeyLength - Length of Key data in bytes
    ValueBuffer - Address of the buffer to store Value data
    ValueBufferLength - The length of ValueBuffer in bytes
    ValueLength - Actual length in bytes of the data written to ValueBuffer, or the required buffer length if ValueBufferLength is too small

Return Value:

    STATUS_SUCCESS - The key was found and its value was successfully stored to the output buffer.
    STATUS_NOT_FOUND - The specified key was not found in the hash table.
    STATUS_BUFFER_TOO_SMALL - The key was found, but the output buffer is too small to store the value.

--*/
{
    DMF_CONTEXT_HashTable* moduleContext;
    NTSTATUS ntStatus;
    DATA_ENTRY* dataEntry;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_HashTable);

    DMF_ModuleLock(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = HashTable_DataEntryFind(DmfModule,
                                       Key,
                                       KeyLength,
                                       &dataEntry);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (ValueBufferLength < dataEntry->ValueLength)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    if (ValueLength != NULL)
    {
        *ValueLength = dataEntry->ValueLength;
    }

    RtlCopyMemory(ValueBuffer,
                  HashTable_ValueBufferGet(dataEntry),
                  dataEntry->ValueLength);

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HashTable_Write(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _In_reads_(ValueLength) UCHAR* Value,
    _In_ ULONG ValueLength
    )
/*++

Routine Description:

    Writes Key-Value pair to the hash table.
    If an element with the specified key already exists - its value will be updated.

Arguments:

    DmfModule - This Module's handle.
    Key - Address of the buffer containing Key data.
    KeyLength - Length of Key data in bytes
    Value - Address of the buffer containing Value data.
    ValueLength - Length of Value data in bytes

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    DMF_CONTEXT_HashTable* moduleContext;
    NTSTATUS ntStatus;
    DATA_ENTRY* dataEntry;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_HashTable);

    DMF_ModuleLock(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(KeyLength <= moduleContext->MaximumKeyLength);

    if (ValueLength > moduleContext->MaximumValueLength)
    {
        ASSERT(FALSE);
        ntStatus = STATUS_BUFFER_OVERFLOW;
        goto Exit;
    }

    ntStatus = HashTable_DataEntryFindOrAllocate(DmfModule,
                                                 Key,
                                                 KeyLength,
                                                 &dataEntry);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    dataEntry->ValueLength = ValueLength;
    RtlCopyMemory(HashTable_ValueBufferGet(dataEntry), Value, ValueLength);

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

// eof: Dmf_HashTable.c
//
