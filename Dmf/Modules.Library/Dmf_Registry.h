/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Registry.h

Abstract:

    Companion file to Dmf_Registry.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// These macros make the populating the entries more accurate and easy as only minimum necessary
// information for each entry needs to be specified and the rest is automatically set.
//
#define Registry_TABLE_ENTRY_REG_DWORD(Name, Value)                 { Name, REG_DWORD, (VOID*)Value, Registry_TABLE_AUTO_SIZE }
#define Registry_TABLE_ENTRY_REG_QWORD(Name, Value)                 { Name, REG_QWORD, (VOID*)Value, Registry_TABLE_AUTO_SIZE }
#define Registry_TABLE_ENTRY_REG_SZ(Name, Value)                    { Name, REG_SZ, Value, Registry_TABLE_AUTO_SIZE }
#define Registry_TABLE_ENTRY_REG_MULTI_SZ(Name, Value)              { Name, REG_MULTI_SZ, Value, Registry_TABLE_AUTO_SIZE }
#define Registry_TABLE_ENTRY_REG_BINARY(Name, Value, ValueSize)     { Name, REG_BINARY, Value, ValueSize }

// Indicates that the size of the data is determined by the data type.
//
#define Registry_TABLE_AUTO_SIZE    0

// It means there is no prefix that should be prepended to all Values Name in the branch.
//
#define Registry_BRANCH_PREFIX_NONE     L""

// Indicates what actions the Registry Action function should do.
//
typedef enum
{
    Registry_ActionTypeInvalid,
    // Write the value.
    //
    Registry_ActionTypeWrite,
    // Delete the value.
    //
    Registry_ActionTypeDelete,
    // Return read value to the caller.
    //
    Registry_ActionTypeRead,
    // Do nothing. The comparison function does the action.
    //
    Registry_ActionTypeNone
} Registry_ActionType;

// Holds information for a single registry entry.
//
typedef struct
{
    // It is always prepended by the Branch Prefix string.
    //
    PWCHAR ValueName;
    // Supported types are: 
    // REG_DWORD, REG_SZ, REG_MULTISZ, REG_BINARY.
    //
    ULONG ValueType;
    // This value has different meanings depending on the ValueType.
    // REG_SZ, REG_MULTISZ, REG_BINARY: Pointer to the data.
    // REG_DWORD: Lower 4 bytes are the DWORD.
    //
    VOID* ValueData;
    // Use Registry_TABLE_AUTO_SIZE for REG_SZ, REG_MULTISZ, REG_DWORD.
    // Otherwise store the size of the data pointed to by ValueData.
    //
    ULONG ValueSize;
} Registry_Entry;

// Holds information for a branch of registry entries which consist of one
// or more registry entries under a single key.
//
typedef struct
{
    // The Key under which all the entries in the array are stored. This name is 
    // prepended onto the name of value in addition to a "\\".
    //
    PWCHAR BranchValueNamePrefix;
    // The entries int he branch.
    //
    Registry_Entry* RegistryTableEntries;
    // The number of entries in the branch.
    //
    ULONG ItemCount;
} Registry_Branch;

// Holds information for a set of registry branches.
//
typedef struct
{
    // Path under the node to all the branches.
    //
    PWCHAR RegistryPath;
    // List of the branches in the tree.
    //
    Registry_Branch* Branches;
    // Number of branches in the tree.
    //
    ULONG NumberOfBranches;
} Registry_Tree;

typedef
_Function_class_(EVT_DMF_Registry_CallbackWork)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_Registry_CallbackWork(_In_ DMFMODULE DmfModule);

typedef struct
{
    EVT_DMF_Registry_CallbackWork** Callbacks;
    ULONG NumberOfCallbacks;
} Registry_ContextScheduledTaskCallback;

typedef
_Function_class_(EVT_DMF_Registry_KeyEnumerationCallback)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_Registry_KeyEnumerationCallback(_In_ VOID* ClientContext,
                                        _In_ HANDLE RootHandle,
                                        _In_ PWCHAR KeyName);

typedef
_Function_class_(EVT_DMF_Registry_ValueComparisonCallback)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_Registry_ValueComparisonCallback(_In_ DMFMODULE DmfModule,
                                         _In_opt_ VOID* ClientContext,
                                         _In_ VOID* ValueDataInRegistry,
                                         _In_ ULONG ValueDataInRegistrySize,
                                         _In_opt_ VOID* ClientDataInRegistry,
                                         _In_ ULONG ClientDataInRegistrySize);

// This macro declares the following functions:
// DMF_Registry_ATTRIBUTES_INIT()
// DMF_Registry_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(Registry)

// Module Methods
//

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_AllSubKeysFromHandleEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_CallbackWork(
    _In_ WDFDEVICE WdfDevice,
    _In_ EVT_DMF_Registry_CallbackWork* CallbackWork
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_CustomAction(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _In_opt_ VOID* ValueDataToCompare,
    _In_ ULONG ValueDataToCompareSize,
    _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
    _In_opt_ VOID* ComparisonCallbackContext
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_EnumerateKeysFromName(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR RootKeyName,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
void
DMF_Registry_HandleClose(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleDelete(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
HANDLE
DMF_Registry_HandleOpenByHandle(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR Name,
    _In_ BOOLEAN TryToCreate
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenById(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG PredefinedKeyId,
    _In_ ULONG AccessMask,
    _Out_ HANDLE* RegistryHandle
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
HANDLE
DMF_Registry_HandleOpenByName(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR Name
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenByNameEx(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR Name,
    _In_ ULONG AccessMask,
    _In_ BOOLEAN Create,
    _Out_ HANDLE* RegistryHandle
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueDelete(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueRead(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONG RegistryType,
    _Out_writes_opt_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ ULONG* BytesRead
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadBinary(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ ULONG* BytesRead
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadDword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadDwordAndValidate(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer,
    _In_ ULONG Minimum,
    _In_ ULONG Maximum
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadMultiString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters,
    _Out_opt_ ULONG* BytesRead
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadQword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONGLONG* Buffer
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadQwordAndValidate(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONGLONG* Buffer,
    _In_ ULONGLONG Minimum,
    _In_ ULONGLONG Maximum
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters,
    _Out_opt_ ULONG* BytesRead
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWrite(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONG RegistryType,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteBinary(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteDword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueData
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteMultiString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteQword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONGLONG ValueData
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_RegistryPathDelete(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR Name
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
ScheduledTask_Result_Type
DMF_Registry_ScheduledTaskCallbackContainer(
    _In_ DMFMODULE DmfScheduledTask,
    _In_ VOID* ClientCallbackContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_SubKeysFromHandleEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_SubKeysFromPathNameContainingStringEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR PathName,
    _In_ PWCHAR LookFor,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    );

NTSTATUS
DMF_Registry_TreeWriteDeferred(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* RegistryTree,
    _In_ ULONG ItemCount
    );

NTSTATUS
DMF_Registry_TreeWriteEx(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* RegistryTree,
    _In_ ULONG ItemCount
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueDelete(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueDeleteIfNeeded(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_opt_ VOID* ValueDataToCompare,
    _In_ ULONG ValueDataToCompareSize,
    _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
    _In_opt_ VOID* ComparisonCallbackContext
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueRead(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _Out_writes_opt_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ ULONG* BytesRead
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadBinary(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ ULONG* BytesRead
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadDword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadDwordAndValidate(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer,
    _In_ ULONG Minimum,
    _In_ ULONG Maximum
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadMultiString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters,
    _Out_opt_ ULONG* BytesRead
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadQword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ ULONGLONG* Buffer
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadQwordAndValidate(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ PULONGLONG Buffer,
    _In_ ULONGLONG Minimum,
    _In_ ULONGLONG Maximum
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters,
    _Out_opt_ ULONG* BytesRead
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWrite(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteBinary(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteDword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueData
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteIfNeeded(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _In_ VOID* ValueDataToWrite,
    _In_ ULONG ValueDataToWriteSize,
    _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
    _In_opt_ VOID* ComparisonCallbackContext,
    _In_ BOOLEAN WriteIfNotFound
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteMultiString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteQword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONGLONG ValueData
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    );

// eof: Dmf_Registry.h
//
