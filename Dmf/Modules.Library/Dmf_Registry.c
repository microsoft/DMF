/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Registry.c

Abstract:

    Performs registry operations. Currently, only registry write is supported.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_Registry.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define REGISTRY_ROOT_LENGTH  (sizeof("\\Registry\\Machine") - 1)  // This is the number of characters in \Registry\Machine, which is the root of all registry access

// Context data for registry enumeration functions.
//
typedef struct
{
    // Context used by filter enumeration function.
    //
    VOID* FilterEnumeratorContext;
    // The client callback function.
    //
    EVT_DMF_Registry_KeyEnumerationCallback* RegistryKeyEnumerationFunction;
    // The client callback function context.
    //
    VOID* ClientCallbackContext;
} RegistryKeyEnumerationContextType;

// List of possible deferred operations.
//
typedef enum
{
    Registry_DeferredOperationInvalid = 0,
    Registry_DeferredOperationWrite = 1,
    // Not supported yet.
    //
    Registry_DeferredOperationDelete = 2,
} Registry_DeferredOperationType;

// There can be multiple outstanding deferred operations. Each deferred operation
// has its own context. These contexts are held in a list which is iterated through
// upon timer expiration.
//
typedef struct
{
    // The operation to perform until it is successful.
    //
    Registry_DeferredOperationType DeferredOperation;
    // The array of registry trees to perform the operation on.
    //
    Registry_Tree* RegistryTree;
    // Number of trees in the above array.
    //
    ULONG ItemCount;
    // Used for list management.
    //
    LIST_ENTRY ListEntry;
} REGISTRY_DEFERRED_CONTEXT;

// It is the time interval to use for polling (how often to attempt
// the deferred operations).
//
const ULONG Registry_DeferredRegistryWritePollingIntervalMs = 1000;

// Context for CustomActionHandler used by this Module for Registry reads.
//
typedef struct
{
    // Where the data will be written.
    //
    UCHAR* Buffer;
    // The size in bytes of Buffer.
    //
    ULONG BufferSize;

    // These are written by the Module Method.
    //

    // Number of bytes written to Buffer.
    //
    ULONG* BytesRead;
    // Indicates if the client's request succeeded.
    //
    NTSTATUS NtStatus;
} Registry_CustomActionHandler_Read_Context;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Timer for deferred operations.
    //
    WDFTIMER Timer;
    // Stores data needed to perform deferred operations.
    //
    LIST_ENTRY ListDeferredOperations;
} DMF_CONTEXT_Registry;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Registry)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Registry)

// Memory Pool Tag.
//
#define MemoryTag 'MgeR'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
Registry_CustomActionHandler_Read(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientContext,
    _In_ VOID* ValueDataInRegistry,
    _In_ ULONG ValueDataInRegistrySize,
    _In_ VOID* ClientDataInRegistry,
    _In_ ULONG ClientDataInRegistrySize
    )
{
    BOOLEAN returnValue;
    Registry_CustomActionHandler_Read_Context* customActionHandlerContextRead;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientDataInRegistry);
    UNREFERENCED_PARAMETER(ClientDataInRegistrySize);

    PAGED_CODE();

    // NOTE: This context is specific to this instance of this handler.
    //
    customActionHandlerContextRead = (Registry_CustomActionHandler_Read_Context*)ClientContext;
    ASSERT(customActionHandlerContextRead != NULL);

    // If the value matches the value defined by the caller, then caller gets
    // back TRUE, otherwise false.
    //
    if (ValueDataInRegistrySize <= customActionHandlerContextRead->BufferSize)
    {
        // The value is of the correct type. Read it.
        //
        ASSERT(customActionHandlerContextRead->Buffer != NULL);
        RtlCopyMemory(customActionHandlerContextRead->Buffer,
                      ValueDataInRegistry,
                      ValueDataInRegistrySize);
        returnValue = TRUE;
        customActionHandlerContextRead->NtStatus = STATUS_SUCCESS;
    }
    else
    {
        // It is cleared by the caller if the parameter is present.
        //
        returnValue = FALSE;
        // Tell the caller that buffer was too small to write the read value.
        //
        customActionHandlerContextRead->NtStatus = STATUS_BUFFER_TOO_SMALL;
    }

    // NOTE: Bytes read is optional for caller.
    // In both cases, tell caller how many bytes are required. Caller may have passed
    // NULL buffer to inquire about size prior to buffer allocation.
    //
    if (customActionHandlerContextRead->BytesRead != NULL)
    {
        // It is cleared by the caller if the parameter is present.
        //
        ASSERT(0 == *(customActionHandlerContextRead->BytesRead));
        *(customActionHandlerContextRead->BytesRead) = ValueDataInRegistrySize;
    }

    // No action is every performed by caller so this return value does not matter.
    // It tells the Client Driver if the value was read.
    //
    return returnValue;
}

//-----------------------------------------------------------------------------------------------------
// Registry Write
//-----------------------------------------------------------------------------------------------------
//

static
NTSTATUS
Registry_ValueSizeGet(
    _In_ Registry_Entry* Entry,
    _Out_ ULONG* ValueSize
    )
/*++

Routine Description:

    Determine the size of the value data to be written.

Arguments:

    Entry - Contains information about the value that is written.
    ValueSize - The size of the data type corresponding to the Value in Entry.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    size_t valueSize;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;

    ASSERT(Entry != NULL);
    ASSERT((Entry->ValueType == REG_SZ) ||
        (Entry->ValueType == REG_DWORD) ||
           (Entry->ValueType == REG_QWORD) ||
           (Entry->ValueType == REG_BINARY) ||
           (Entry->ValueType == REG_MULTI_SZ));

    switch (Entry->ValueType)
    {
        case REG_DWORD:
        {
            *ValueSize = sizeof(DWORD);
            break;
        }
        case REG_QWORD:
        {
            *ValueSize = sizeof(ULONGLONG);
            break;
        }
        case REG_SZ:
        {
            ntStatus = RtlStringCchLengthW((STRSAFE_PCNZWCH)Entry->ValueData,
                                           NTSTRSAFE_MAX_CCH,
                                           &valueSize);
            if (! NT_SUCCESS(ntStatus))
            {
                ASSERT(FALSE);
                goto Exit;
            }
            // The above function returns the length in characters. The function
            // that writes the values requires the full size of the buffer.
            //
            *ValueSize = (ULONG)valueSize * sizeof(WCHAR);
            break;
        }
        case REG_MULTI_SZ:
        {
            PWCHAR current;
            ULONG count;
            BOOLEAN done;

            ASSERT(Entry->ValueName != NULL);
            current = (PWCHAR)Entry->ValueData;
            count = 0;
            done = FALSE;
            while (! done)
            {
                // Loop through all the characters of the current string in the
                // set of strings in the buffer.
                //
                while (*current != L'\0')
                {
                    current++;
                    count++;
                }
                // First trailing zero is counted as part of the string.
                //
                current++;
                count++;
                // This is the check for the second consecutive \0.
                //
                if (L'\0' == *current)
                {
                    // Second consecutive \0 is found.
                    //
                    done = TRUE;
                    break;
                }
                // Second consecutive \0 is not found.
                //
            }
            // Add one WCHAR for the second /0.
            //
            count += 1;
            *ValueSize = count * sizeof(WCHAR);
            break;
        }
        case REG_BINARY:
        {
            ASSERT(Entry->ValueSize != 0);
            *ValueSize = Entry->ValueSize;
            break;
        }
        default:
        {
            *ValueSize = 0;
            ASSERT(FALSE);
            goto Exit;
        }
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
Registry_EntryWrite(
    _In_ PWCHAR FullPathName,
    _In_ Registry_Entry* Entry
    )
/*++

Routine Description:

    Write a single registry entry.

Arguments:

    FullPathName - This is the whole name of the path where the entry will be written.
    Entry - Contains information about the value that is written.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG valueSize;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(FullPathName != NULL);
    ASSERT(Entry != NULL);
    ASSERT(Entry->ValueName != NULL);
    ASSERT((Entry->ValueType == REG_SZ) ||
        (Entry->ValueType == REG_DWORD) ||
           (Entry->ValueType == REG_QWORD) ||
           (Entry->ValueType == REG_BINARY) ||
           (Entry->ValueType == REG_MULTI_SZ));

    // Get the size of the value to be written. It depends on the kind
    // of value it is.
    //
    ntStatus = Registry_ValueSizeGet(Entry,
                                     &valueSize);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid code path Registry_ValueSizeGet");
        ASSERT(FALSE);
        goto Exit;
    }

    switch (Entry->ValueType)
    {
        case REG_DWORD:
        {
            // Write the registry entry. The data field is a DWORD so convert it to DWORD.
            //
            DWORD dword;

            dword = (DWORD)(ULONGLONG)Entry->ValueData;
            ntStatus = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                                             FullPathName,
                                             Entry->ValueName,
                                             Entry->ValueType,
                                             &dword,
                                             valueSize);
            break;
        }
        case REG_QWORD:
        {
            ULONGLONG qword;

            // Write the registry entry. The data field is a QWORD so convert it to QWORD.
            //
            qword = (ULONGLONG)Entry->ValueData;
            ntStatus = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                                             FullPathName,
                                             Entry->ValueName,
                                             Entry->ValueType,
                                             &qword,
                                             valueSize);
            break;
        }
        default:
        {
            // Write the registry entry. The data field is an address of the data to write.
            //
            ntStatus = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                                             FullPathName,
                                             Entry->ValueName,
                                             Entry->ValueType,
                                             Entry->ValueData,
                                             valueSize);
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
Registry_RecursivePathCreate(
    _Inout_ PWCHAR RegistryPath
    )
/*++

Routine Description:

    Checks if the given registry path exists. If not, removes one key and tries again.
    Continues recursively until all keys of the path are created. In the event of a failure,
    RegistryPath is left modified as the minimal path that failed.

Arguments:

    RegistryPath - The path to test/create.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    size_t mainRegistryPathNameLength;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = RtlCheckRegistryKey(RTL_REGISTRY_ABSOLUTE,
                                   RegistryPath);
    if (! NT_SUCCESS(ntStatus))
    {
        // Remove one key from the path and try again. If that succeeds, create the key.
        //
        ntStatus = RtlStringCchLengthW(RegistryPath,
                                       NTSTRSAFE_MAX_CCH,
                                       &mainRegistryPathNameLength);
        if (NT_SUCCESS(ntStatus))
        {
            if (mainRegistryPathNameLength > 0)
            {
                // Counting backwards from the end of the string. Skip the null.
                //
                mainRegistryPathNameLength--;

                // Here the root of the path will be \Registry\Machine. 17 characters. 
                //
                while (mainRegistryPathNameLength > REGISTRY_ROOT_LENGTH && RegistryPath[mainRegistryPathNameLength] != '\\')
                {
                    mainRegistryPathNameLength--;
                }
                if (mainRegistryPathNameLength > REGISTRY_ROOT_LENGTH && RegistryPath[mainRegistryPathNameLength] == '\\')
                {
                    RegistryPath[mainRegistryPathNameLength] = '\0';
                    ntStatus = Registry_RecursivePathCreate(RegistryPath);
                    if (NT_SUCCESS(ntStatus))
                    {
                        // Restore the key and try to create it.
                        //
                        RegistryPath[mainRegistryPathNameLength] = '\\';
                        ntStatus = RtlCreateRegistryKey(RTL_REGISTRY_ABSOLUTE,
                                                        RegistryPath);
                    }
                }
                else
                {
                    // A poorly formatted registry path, or
                    // a misspelled hive, or
                    // the path does not start with \Registry\Machine, or
                    // registry at the specified path is not ready yet.
                    //
                    ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;
                }
            }
            else
            {
                // A poorly formatted registry path.
                //
                ASSERT(FALSE);
                ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;
            }
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
Registry_BranchWrite(
    _In_ PWCHAR RegistryPath,
    _In_ Registry_Branch* Branches,
    _In_ ULONG NumberOfBranches
    )
/*++

Routine Description:

    Writes an array of registry branches to the registry.

Arguments:

    Branches - The array of registry branches.
    NumberOfBranches - The number of entries in the array.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG branchIndex;
    PWCHAR fullPathName;
    size_t mainRegistryPathNameLength;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;
    fullPathName = NULL;

    // Get the length of main registry path.
    //
    ntStatus = RtlStringCchLengthW(RegistryPath,
                                   NTSTRSAFE_MAX_CCH,
                                   &mainRegistryPathNameLength);
    if (! NT_SUCCESS(ntStatus))
    {
        ASSERT(FALSE);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid code path RtlStringCchLengthW ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // For each branch, create full path that consists of the main path plus an option branch path.
    // Then, write all the values for that branch.
    //
    for (branchIndex = 0; branchIndex < NumberOfBranches; branchIndex++)
    {
        Registry_Branch* branch;
        size_t prefixPathNameLength;
        size_t fullPathNameLength;
        size_t fullPathNameSize;

        branch = &Branches[branchIndex];

        // Get the name of the prefix to append to all Value names.
        //
        ntStatus = RtlStringCchLengthW(branch->BranchValueNamePrefix,
                                       NTSTRSAFE_MAX_CCH,
                                       &prefixPathNameLength);
        if (! NT_SUCCESS(ntStatus))
        {
            ASSERT(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid code path RtlStringCchLengthW ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Calculate the full length of the path name.
        // NOTE: Trailing '/' are in the strings.
        // NOTE: Add one character from the zero-termination.
        //
        fullPathNameLength = mainRegistryPathNameLength + prefixPathNameLength + 1;

        // Calculate the size of the buffer needed for that name.
        //
        fullPathNameSize = fullPathNameLength * sizeof(WCHAR);

        // Allocate a buffer for the full path name.
        //
        fullPathName = (PWCHAR)ExAllocatePoolWithTag(PagedPool,
                                                     fullPathNameSize,
                                                     MemoryTag);
        if (NULL == fullPathName)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Out of Memory");
            goto Exit;
        }

        // Copy the main path into the buffer.
        //
        ntStatus = RtlStringCchCopyW(fullPathName,
                                     fullPathNameSize,
                                     RegistryPath);
        if (! NT_SUCCESS(ntStatus))
        {
            ASSERT(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid code path RtlStringCchCopyW ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Copy the prefix into the full path name buffer.
        // NOTE: The prefix must have a '\' at front if there are any characters.
        //
        ASSERT((L'\0' == branch->BranchValueNamePrefix[0]) ||
            (L'\\' == branch->BranchValueNamePrefix[0]));
        ntStatus = RtlStringCchCatW(fullPathName,
                                    fullPathNameSize,
                                    branch->BranchValueNamePrefix);
        if (! NT_SUCCESS(ntStatus))
        {
            ASSERT(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid code path RtlStringCchCatW ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Check that the RegistryPath exists in the registry and create it if it does not.
        //
        ntStatus = Registry_RecursivePathCreate(fullPathName);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Unable to create RegistryPath=%S ntStatus=%!STATUS!", fullPathName, ntStatus);
            goto Exit;
        }

        for (ULONG entryIndex = 0; entryIndex < branch->ItemCount; entryIndex++)
        {
            Registry_Entry* entry;

            entry = &branch->RegistryTableEntries[entryIndex];

            // Write the value at the full path name.
            //
            ntStatus = Registry_EntryWrite(fullPathName,
                                           entry);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid code path Registry_EntryWrite ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }

        // Free the buffer allocated above for the next iteration in the loop.
        //
        ExFreePoolWithTag(fullPathName,
                          MemoryTag);
        fullPathName = NULL;
    }

Exit:

    if (fullPathName != NULL)
    {
        ExFreePoolWithTag(fullPathName,
                          MemoryTag);
        fullPathName = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
Registry_TreeWrite(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* Tree,
    _In_ ULONG NumberOfTrees
    )
/*++

Routine Description:

    Writes an array of registry trees to the registry.

Arguments:

    Tree - The array of registry trees.
    NumberOfTrees - The number of entries in the array.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG treeIndex;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;

    ASSERT(NumberOfTrees > 0);

    for (treeIndex = 0; treeIndex < NumberOfTrees; treeIndex++)
    {
        Registry_Tree* tree;

        tree = &Tree[treeIndex];
        ntStatus = Registry_BranchWrite(tree->RegistryPath,
                                        tree->Branches,
                                        tree->NumberOfBranches);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid code path Registry_BranchWrite ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

//-----------------------------------------------------------------------------------------------------
// Registry Enumeration
//-----------------------------------------------------------------------------------------------------
//

HANDLE
Registry_HandleOpenByName(
    _In_ PWCHAR Name
    )
/*++

Routine Description:

    Open a registry key by path name.

Arguments:

    Name - Path name of the key relative to handle.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    HANDLE handle;
    NTSTATUS ntStatus;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING nameString;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = RtlUnicodeStringInit(&nameString,
                                    Name);
    if (! NT_SUCCESS(ntStatus))
    {
        ASSERT(FALSE);
        handle = NULL;
        goto Exit;
    }
    InitializeObjectAttributes(&objectAttributes,
                               &nameString,
                               OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    ntStatus = ZwOpenKey(&handle,
                         GENERIC_ALL,
                         &objectAttributes);
    if (! NT_SUCCESS(ntStatus))
    {
        handle = NULL;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return handle;
}

NTSTATUS
Registry_HandleOpenByPredefinedKey(
    _In_ WDFDEVICE Device,
    _In_ ULONG PredefinedKeyId,
    _In_ ULONG AccessMask,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open a registry key of the device.

Arguments:

    Device - Handle to Device object.
    AccessMask - The access mask to pass.
    RegistryHandle - Handle to open registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PDEVICE_OBJECT deviceObject;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    deviceObject = WdfDeviceWdmGetPhysicalDevice(Device);

    // Open the device registry key of the instance of the device.
    //
    ntStatus = IoOpenDeviceRegistryKey(deviceObject,
                                       PredefinedKeyId,
                                       AccessMask,
                                       RegistryHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        *RegistryHandle = NULL;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

NTSTATUS
Registry_HandleOpenByNameEx(
    _In_ PWCHAR Name,
    _In_ ULONG AccessMask,
    _In_ BOOLEAN Create,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open a registry key by path name and access mask.

Arguments:

    Name - Path name of the key relative to handle.
    AccessMask - The access mask to pass.
    Create - Creates the key if it cannot be opened.
    RegistryHandle - Handle to open registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING nameString;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = RtlUnicodeStringInit(&nameString,
                                    Name);
    if (! NT_SUCCESS(ntStatus))
    {
        ASSERT(FALSE);
        *RegistryHandle = NULL;
        goto Exit;
    }
    // Registry names are case insensitive.
    //
    InitializeObjectAttributes(&objectAttributes,
                               &nameString,
                               OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    if (Create)
    {
        // Open existing or create new.
        //
        ntStatus = ZwCreateKey(RegistryHandle,
                               AccessMask,
                               &objectAttributes,
                               0,
                               NULL,
                               REG_OPTION_NON_VOLATILE,
                               NULL);
    }
    else
    {
        // Open existing.
        //
        ntStatus = ZwOpenKey(RegistryHandle,
                             AccessMask,
                             &objectAttributes);
    }
    if (! NT_SUCCESS(ntStatus))
    {
        *RegistryHandle = NULL;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

HANDLE
Registry_HandleOpenByHandle(
    _In_ HANDLE Handle,
    _In_ PWCHAR Name,
    _In_ BOOLEAN TryToCreate
    )
/*++

Routine Description:

    Given a registry handle, open a handle relative to that handle.

Arguments:

    Handle - Handle to open registry key.
    Name - Path name of the key relative to handle.
    TryToCreate - Indicates if the function should call create instead of open.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    HANDLE handle;
    NTSTATUS ntStatus;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING nameString;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = RtlUnicodeStringInit(&nameString,
                                    Name);
    if (! NT_SUCCESS(ntStatus))
    {
        ASSERT(FALSE);
        handle = NULL;
        goto Exit;
    }
    InitializeObjectAttributes(&objectAttributes,
                               &nameString,
                               OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               Handle,
                               NULL);
    if (TryToCreate)
    {
        // Try to create/open.
        //
        ntStatus = ZwCreateKey(&handle,
                               KEY_ALL_ACCESS,
                               &objectAttributes,
                               0,
                               NULL,
                               REG_OPTION_NON_VOLATILE,
                               NULL);
    }
    else
    {
        // Try to open.
        //
        ntStatus = ZwOpenKey(&handle,
                             KEY_ALL_ACCESS,
                             &objectAttributes);
    }

    if (! NT_SUCCESS(ntStatus))
    {
        handle = NULL;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "handle=0x%p", handle);

    return handle;
}

void
Registry_HandleClose(
    _In_ HANDLE Handle
    )
/*++

Routine Description:

    Given a registry handle, close the handle.

Arguments:

    Handle - handle to open registry key.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ZwClose(Handle);
    Handle = NULL;

    FuncExitVoid(DMF_TRACE);
}

BOOLEAN
Registry_SubKeysFromHandleEnumerate(
    _In_ HANDLE Handle,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* RegistryEnumerationFunction,
    _In_ VOID* Context
    )
/*++

Routine Description:

    Given a registry handle, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    Handle - The handle the registry key.
    RegistryEnumerationFunction - The enumeration function to call for each sub key.
    Context - The client context to pass into the enumeration function.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    BOOLEAN returnValue;
    NTSTATUS ntStatus;
    ULONG resultLength;
    ULONG currentSubKeyIndex;
    BOOLEAN done;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;
    done = FALSE;
    currentSubKeyIndex = 0;
    while (! done)
    {
        // If there is a key to enumerate, since the function is passed NULL and 0 as buffer and buffer
        // size the return value will indicate "buffer too small" and the resultLenght will be the 
        // amount of memory needed to read it.
        //
        resultLength = 0;
        ntStatus = ZwEnumerateKey(Handle,
                                  currentSubKeyIndex,
                                  KeyBasicInformation,
                                  NULL,
                                  0,
                                  &resultLength);
        if (! NT_SUCCESS(ntStatus))
        {
            // This is the expected result because the driver needs to know the length.
            //
            if ((ntStatus == STATUS_BUFFER_OVERFLOW) ||
                (ntStatus == STATUS_BUFFER_TOO_SMALL))
            {
                PKEY_BASIC_INFORMATION keyInformationBuffer;
                ULONG keyInformationBufferSize;

                // This driver needs to zero terminate the name that is returned. So, add 1 to the length
                // to the size needed to allocate.
                //
                #pragma warning(suppress: 6102)
                resultLength += sizeof(WCHAR);

                // Allocate a buffer for the path name.
                //
                keyInformationBuffer = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(PagedPool,
                                                                                     resultLength,
                                                                                     MemoryTag);
                if (NULL == keyInformationBuffer)
                {
                    returnValue = FALSE;
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Out of memory");
                    goto Exit;
                }

                // It is the size of the buffer passed including space for final zero.
                //
                keyInformationBufferSize = resultLength;

                // Enumerate the next key.
                //
                ntStatus = ZwEnumerateKey(Handle,
                                          currentSubKeyIndex,
                                          KeyBasicInformation,
                                          keyInformationBuffer,
                                          keyInformationBufferSize,
                                          &resultLength);
                if (! NT_SUCCESS(ntStatus))
                {
                    returnValue = FALSE;
                    goto Exit;
                }

                // Zero terminate the name. (It is in the buffer that was just allocated,
                // so it is OK to do so.)
                //
                keyInformationBuffer->Name[keyInformationBuffer->NameLength / sizeof(WCHAR)] = L'\0';

                // Call the client enumeration function.
                //
                returnValue = RegistryEnumerationFunction(Context,
                                                          Handle,
                                                          keyInformationBuffer->Name);

                // Prepare to get next subkey.
                //
                ExFreePoolWithTag(keyInformationBuffer,
                                  MemoryTag);
                keyInformationBuffer = NULL;
                keyInformationBufferSize = 0;
                currentSubKeyIndex++;

                if (! returnValue)
                {
                    goto Exit;
                }
            }
            else
            {
                // It means there are no more entires to enumerate.
                //
                done = TRUE;
            }
        }
    }

    returnValue = TRUE;

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

// Enumeration Filter Functions. Add more here as needed for external use.
//

BOOLEAN
Registry_KeyEnumerationFilterAllSubkeys(
    _In_ VOID* ClientContext,
    _In_ HANDLE Handle,
    _In_ PWCHAR KeyName
    )
{
    RegistryKeyEnumerationContextType* context;
    BOOLEAN returnValue;
    HANDLE subKeyHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    subKeyHandle = Registry_HandleOpenByHandle(Handle,
                                               KeyName,
                                               FALSE);
    if (NULL == subKeyHandle)
    {
        // This is an error because the key was just enumerated. It should still
        // be here.
        //
        returnValue = FALSE;
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Registry_HandleOpenByHandle NULL");
        goto Exit;
    }

    context = (RegistryKeyEnumerationContextType*)ClientContext;
    returnValue = context->RegistryKeyEnumerationFunction(context->ClientCallbackContext,
                                                          subKeyHandle,
                                                          KeyName);

    Registry_HandleClose(subKeyHandle);
    subKeyHandle = NULL;

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

BOOLEAN
Registry_KeyEnumerationFilterStrstr(
    _In_ VOID* ClientContext,
    _In_ HANDLE Handle,
    _In_ PWCHAR KeyName
    )
{
    RegistryKeyEnumerationContextType* context;
    PWCHAR lookFor;
    BOOLEAN returnValue;
    HANDLE subKeyHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    returnValue = TRUE;
    context = (RegistryKeyEnumerationContextType*)ClientContext;
    lookFor = (PWCHAR)context->FilterEnumeratorContext;
    if (wcsstr(KeyName,
               lookFor))
    {
        subKeyHandle = Registry_HandleOpenByHandle(Handle,
                                                   KeyName,
                                                   FALSE);
        if (NULL == subKeyHandle)
        {
            // This is an error because the key was just enumerated. It should still
            // be here.
            //
            returnValue = FALSE;
            TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Registry_HandleOpenByHandle NULL");
            goto Exit;
        }

        returnValue = context->RegistryKeyEnumerationFunction(context->ClientCallbackContext,
                                                              subKeyHandle,
                                                              KeyName);

        Registry_HandleClose(subKeyHandle);
        subKeyHandle = NULL;
    }

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_ValueActionIfNeeded(
    _In_ Registry_ActionType ActionType,
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _In_opt_ VOID* ValueDataToWrite,
    _In_ ULONG ValueDataToWriteSize,
    _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
    _In_opt_ VOID* ComparisonCallbackContext,
    _In_ BOOLEAN WriteIfNotFound
    )
/*++

Routine Description:

    Perform an action to a value after calling a client comparison function to determine if that
    action should be taken.

Arguments:

    ActionType - Determines what action to take if client comparison function returns TRUE.
    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The Registry Type of the value.
    ValueDataToWrite - The data to write if the value is not set to one or it does not exist.
    ValueDataToWriteSize - The size of the buffer at ValueDataToWrite
    ComparisonCallback - Caller's comparison function.
    ComparisonCallbackContext - Caller's context sent to comparison. function.
    WriteIfNotFound - Indicates if the value should be written if it does not exist.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    UNICODE_STRING valueNameString;
    ULONG resultLength;
    BOOLEAN needsAction;
    KEY_VALUE_PARTIAL_INFORMATION* keyValuePartialInformation;
    VOID* existingValueSetting;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Check if action needed [%ws]...", ValueName);

    ASSERT(ActionType != Registry_ActionTypeInvalid);

    // Indicate if action will be taken...default is no.
    //
    needsAction = FALSE;

    RtlInitUnicodeString(&valueNameString,
                         ValueName);

    // Find out how much memory is needed to retrieve the value if it is there.
    //
    ntStatus = ZwQueryValueKey(Handle,
                               &valueNameString,
                               KeyValuePartialInformation,
                               NULL,
                               0,
                               &resultLength);
    if ((STATUS_OBJECT_NAME_NOT_FOUND == ntStatus))
    {
        // The value is not there. Write it.
        //
        if (WriteIfNotFound)
        {
            needsAction = TRUE;
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "%ws not found (action will happen)", ValueName);
        }
        else
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "%ws not found (no action will happen)", ValueName);
        }
    }
    else if (STATUS_BUFFER_TOO_SMALL == ntStatus)
    {
        // The registry record containing the data will be here.
        // Suppress (resultLength may not be initialized.)
        //
        #pragma warning(suppress: 6102)
        ASSERT(resultLength > 0);
        #pragma warning(suppress: 6102)
        keyValuePartialInformation = (KEY_VALUE_PARTIAL_INFORMATION*)ExAllocatePoolWithTag(NonPagedPoolNx,
                                                                                           resultLength,
                                                                                           MemoryTag);
        if (NULL == keyValuePartialInformation)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ExAllocatePoolWithTag ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // The registry value setting will be here.
        //
        existingValueSetting = (VOID*)&keyValuePartialInformation->Data;

        // TODO: Validate the ValueType.
        //

        // Retrieve the setting of the value.
        //
        ntStatus = ZwQueryValueKey(Handle,
                                   &valueNameString,
                                   KeyValuePartialInformation,
                                   keyValuePartialInformation,
                                   resultLength,
                                   &resultLength);
        if (! NT_SUCCESS(ntStatus))
        {
            // Fall through to free memory. Let the caller decide what to do.
            // Generally, this code path should not happen.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ZwQueryValueKey ntStatus=%!STATUS!", ntStatus);
        }
        else
        {
            // Call the caller's comparison function.
            //
            if (ComparisonCallback(DmfModule,
                                   ComparisonCallbackContext,
                                   existingValueSetting,
                                   keyValuePartialInformation->DataLength,
                                   ValueDataToWrite,
                                   ValueDataToWriteSize))
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "%ws found needs action", ValueName);
                needsAction = TRUE;
            }
            else
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "%ws found but needs no action", ValueName);
            }
        }

        ExFreePoolWithTag(keyValuePartialInformation,
                          MemoryTag);
        keyValuePartialInformation = NULL;
    }
    else
    {
        // Any other status means something is wrong.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ZwQueryValueKey ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (needsAction)
    {
        switch (ActionType)
        {
            case Registry_ActionTypeWrite:
            {
                if (NULL == ValueDataToWrite)
                {
                    ASSERT(FALSE);
                    ntStatus = STATUS_INVALID_PARAMETER;
                    goto Exit;
                }

                ntStatus = RtlWriteRegistryValue(RTL_REGISTRY_HANDLE,
                                                 (PCWSTR)Handle,
                                                 ValueName,
                                                 ValueType,
                                                 (VOID*)ValueDataToWrite,
                                                 ValueDataToWriteSize);
                if (! NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlWriteRegistryValue %ws...ntStatus=%!STATUS!", ValueName, ntStatus);
                }
                break;
            }
            case Registry_ActionTypeDelete:
            {
                ntStatus = RtlDeleteRegistryValue(RTL_REGISTRY_HANDLE,
                                                  (PCWSTR)Handle,
                                                  ValueName);
                if (! NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlDeleteRegistryValue %ws...ntStatus=%!STATUS!", ValueName, ntStatus);
                }
                break;
            }
            case Registry_ActionTypeRead:
            case Registry_ActionTypeNone:
            {
                // Action was done in the comparison function.
                //
                if (needsAction)
                {
                    // Comparison function returns success.
                    //
                    ntStatus = STATUS_SUCCESS;
                }
                else
                {
                    // Comparison function returns fail.
                    //
                    ntStatus = STATUS_UNSUCCESSFUL;
                }
                break;
            }
            default:
            {
                ASSERT(FALSE);
                break;
            }
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "%ws: no action taken", ValueName);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// "Returning uninitialized memory '*BytesRead'."
//
#pragma warning(suppress:6101)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_ValueActionAlways(
    _In_ Registry_ActionType ActionType,
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _Inout_opt_ VOID* ValueDataBuffer,
    _In_ ULONG ValueDataBufferSize,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Perform an action to a registry value always. Client does not filter.

Arguments:

    ActionType - Determines what action to take if client comparison function returns TRUE.
    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The Registry Type of the value.
    ValueDataBuffer - The data to write if the value is not set to one or it does not exist.
    ValueDataBufferSize - The size of the buffer at ValueDataToWrite
    BytesRead - Used for read handler to inform caller of needed size.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Action always [%ws]...", ValueName);

    ASSERT(ActionType != Registry_ActionTypeInvalid);

    // For SAL.
    //
    ntStatus = STATUS_UNSUCCESSFUL;

    switch (ActionType)
    {
        case Registry_ActionTypeWrite:
        {
            // Just perform the action now.
            //
            ASSERT(ValueDataBuffer != NULL);
            ASSERT(NULL == BytesRead);
            ntStatus = RtlWriteRegistryValue(RTL_REGISTRY_HANDLE,
                                             (PCWSTR)Handle,
                                             ValueName,
                                             ValueType,
                                             (VOID*)ValueDataBuffer,
                                             ValueDataBufferSize);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlWriteRegistryValue %ws...ntStatus=%!STATUS!", ValueName, ntStatus);
            }
            break;
        }
        case Registry_ActionTypeDelete:
        {
            // Just perform the action now.
            //
            ASSERT(NULL == BytesRead);
            ntStatus = RtlDeleteRegistryValue(RTL_REGISTRY_HANDLE,
                                              (PCWSTR)Handle,
                                              ValueName);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlDeleteRegistryValue %ws...ntStatus=%!STATUS!", ValueName, ntStatus);
            }
            break;
        }
        case Registry_ActionTypeRead:
        {
            // Call the "if needed" code because "always" is just a subset of "if needed".
            // The code to read the value and determine its size is already there. That
            // non-trivial code does not need to be written again.
            // NOTE: The caller can use "if needed" directly also.
            //
            Registry_CustomActionHandler_Read_Context customActionHandlerContextRead;

            ASSERT(((ValueDataBuffer != NULL) && (ValueDataBufferSize > 0)) ||
                ((NULL == ValueDataBuffer) && (0 == ValueDataBufferSize) && (BytesRead != NULL)));

            // Give the Custom Action Handler the information it needs.
            //
            RtlZeroMemory(&customActionHandlerContextRead,
                          sizeof(customActionHandlerContextRead));
            customActionHandlerContextRead.Buffer = (UCHAR*)ValueDataBuffer;
            customActionHandlerContextRead.BufferSize = ValueDataBufferSize;
            customActionHandlerContextRead.BytesRead = BytesRead;

            // Call the "if needed" function to do the work.
            // TODO: Validate that ValueType is the value type of the value being read.
            //
            ntStatus = Registry_ValueActionIfNeeded(Registry_ActionTypeRead,
                                                    DmfModule,
                                                    Handle,
                                                    ValueName,
                                                    ValueType,
                                                    ValueDataBuffer,
                                                    ValueDataBufferSize,
                                                    Registry_CustomActionHandler_Read,
                                                    &customActionHandlerContextRead,
                                                    FALSE);
            if (NT_SUCCESS(ntStatus))
            {
                // Override successful NTSTATUS with callback's NTSTATUS in case callback
                // indicates error.
                //
                ntStatus = customActionHandlerContextRead.NtStatus;
            }
            break;
        }
        case Registry_ActionTypeNone:
        {
            // Client has asked for no action to always be taken.
            //
            ASSERT(FALSE);
            break;
        }
        default:
        {
            ASSERT(FALSE);
            break;
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

//-----------------------------------------------------------------------------------------------------
// Registry Deferred Operations
//-----------------------------------------------------------------------------------------------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Registry_DeferredOperationTimerStart(
    _In_ WDFTIMER Timer
    )
/*++

Routine Description:

    Starts the deferred operation timer.

Parameters:

    Timer - The timer that will expire causing the deferred routine to run.

Return:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Start timer");

    WdfTimerStart(Timer,
                  WDF_REL_TIMEOUT_IN_MS(Registry_DeferredRegistryWritePollingIntervalMs));

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Registry_DeferredOperationAdd(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* RegistryTree,
    _In_ ULONG ItemCount,
    _In_ Registry_DeferredOperationType DeferredOperationType
    )
/*++

Routine Description:

    Adds a deferred operation to the deferred operation list.

Parameters:

    DmfModule - This Module's handle.
    RegistryTree - Array of trees to perform deferred operation on.
    ItemCount - Number of entries in the array.
    DeferredOperationType - The deferred operation to perform.

Return:

    STATUS_SUCCESS if successful or STATUS_INSUFFICIENT_RESOURCES if there is not
    enough memory.

--*/
{
    NTSTATUS ntStatus;
    REGISTRY_DEFERRED_CONTEXT* deferredContext;
    DMF_CONTEXT_Registry* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Allocate space for the deferred operation. If it cannot be allocated an error code
    // is returned and the operation is not deferred.
    //
    deferredContext = (REGISTRY_DEFERRED_CONTEXT*)ExAllocatePoolWithTag(PagedPool,
                                                                        sizeof(REGISTRY_DEFERRED_CONTEXT),
                                                                        MemoryTag);
    if (NULL == deferredContext)
    {
        // Out of memory.
        //
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Out of memory %!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Create deferred operation context");

    // Populate the deferred operation context.
    //
    RtlZeroMemory(deferredContext,
                  sizeof(REGISTRY_DEFERRED_CONTEXT));
    deferredContext->DeferredOperation = DeferredOperationType;
    deferredContext->RegistryTree = RegistryTree;
    deferredContext->ItemCount = ItemCount;

    // Add the operation to the list of operations.
    //
    DMF_ModuleLock(DmfModule);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Insert deferred operation context into list listEntry=0x%p", &deferredContext->ListEntry);
    InsertTailList(&moduleContext->ListDeferredOperations,
                   &deferredContext->ListEntry);
    // Since there is a least one entry in the list, start the timer.
    //
    Registry_DeferredOperationTimerStart(moduleContext->Timer);
    DMF_ModuleUnlock(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

EVT_WDF_TIMER Registry_DeferredOperationHandler;

VOID
Registry_DeferredOperationHandler(
    _In_ WDFTIMER WdfTimer
    )
/*++

Routine Description:

Parameters:

    WdfTimer - The timer object that contains the DEVICE_CONTEXT.

Return:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Registry* moduleContext;
    REGISTRY_DEFERRED_CONTEXT* deferredContext;
    PLIST_ENTRY listEntry;
    PLIST_ENTRY nextListEntry;
    BOOLEAN needToRestartTimer;

    // 'The current function is permitted to run at an IRQ level above the maximum permitted for '__PREfastPagedCode' (1). Prior function calls or annotation are inconsistent with use of that function:  The current function may need _IRQL_requires_max_, or it may be that the limit is set by some prior call. Maximum legal IRQL was last set to 2 at line 1649.'
    // NOTE: Timer handler is set to run in PASSIVE_LEVEL.
    //
    #pragma warning(suppress:28118)
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Polling timer expires");

    dmfModule = (DMFMODULE)WdfTimerGetParentObject(WdfTimer);
    ASSERT(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    DMF_ModuleLock(dmfModule);

    // Point to the first entry in the list.
    //
    listEntry = moduleContext->ListDeferredOperations.Flink;
    needToRestartTimer = FALSE;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Begin list iteration");
    // The loop ends when the current list entry points to the list header.
    //
    while (listEntry != &moduleContext->ListDeferredOperations)
    {
        // Get the next entry in the list now before it is removed.
        //
        nextListEntry = listEntry->Flink;

        deferredContext = CONTAINING_RECORD(listEntry,
                                            REGISTRY_DEFERRED_CONTEXT,
                                            ListEntry);
        switch (deferredContext->DeferredOperation)
        {
            case Registry_DeferredOperationWrite:
            {
                NTSTATUS ntStatus;

                ASSERT(deferredContext->RegistryTree != NULL);
                ntStatus = Registry_TreeWrite(dmfModule,
                                              deferredContext->RegistryTree,
                                              deferredContext->ItemCount);
                if (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus)
                {
                    // Leave it in the list because driver needs to try again.
                    //
                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "STATUS_OBJECT_NAME_NOT_FOUND...try again");
                    needToRestartTimer = TRUE;
                }
                else
                {
                    if (NT_SUCCESS(ntStatus))
                    {
                        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Registry_TreeWriteEx returns ntStatus=%!STATUS!", ntStatus);
                    }
                    else
                    {
                        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Registry_TreeWrite returns ntStatus=%!STATUS! (no retry)", ntStatus);
                    }
                    // Remove it from the list.
                    //
                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Remove deferredContext=0x%p", deferredContext);
                    RemoveEntryList(listEntry);
                    ExFreePoolWithTag(deferredContext,
                                      MemoryTag);
                    deferredContext = NULL;
                }
                break;
            }
            default:
            {
                ASSERT(FALSE);
                break;
            }
        }

        // Point to the next entry in the list.
        //
        listEntry = nextListEntry;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "End list iteration needToRestartTimer=%d", needToRestartTimer);

    if (needToRestartTimer)
    {
        // It means there are still pending deferred operations to perform.
        //
        Registry_DeferredOperationTimerStart(moduleContext->Timer);
    }

    DMF_ModuleUnlock(dmfModule);

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Registry_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Registry.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Registry* moduleContext;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Initialize the list to empty.
    //
    InitializeListHead(&moduleContext->ListDeferredOperations);

    // Create the timer for deferred operations.
    //
    WDF_TIMER_CONFIG_INIT(&timerConfig,
                          Registry_DeferredOperationHandler);
    timerConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = DmfModule;
    timerAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    ntStatus = WdfTimerCreate(&timerConfig,
                              &timerAttributes,
                              &moduleContext->Timer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Registry_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Registry.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Registry* moduleContext;
    PLIST_ENTRY listEntry;
    PLIST_ENTRY nextListEntry;
    REGISTRY_DEFERRED_CONTEXT* deferredContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->Timer != NULL)
    {
        WdfTimerStop(moduleContext->Timer,
                     TRUE);
        WdfObjectDelete(moduleContext->Timer);
        moduleContext->Timer = NULL;
    }
    else
    {
        // This can happen in cases of partial initialization.
        //
    }

    // Remove all pending deferred operations.
    //
    DMF_ModuleLock(DmfModule);

    // Get the first entry in the list.
    //
    listEntry = moduleContext->ListDeferredOperations.Flink;
    if (NULL == listEntry)
    {
        // This can happen in cases of partial initialization.
        //
        goto SkipListIteration;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Begin list iteration");
    // Loop ends when the current entry points to the list header.
    //
    while (listEntry != &moduleContext->ListDeferredOperations)
    {
        // Get the next entry now before current entry is removed.
        //
        nextListEntry = listEntry->Flink;

        deferredContext = CONTAINING_RECORD(listEntry,
                                            REGISTRY_DEFERRED_CONTEXT,
                                            ListEntry);
        // Remove from list.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Remove deferredContext=0x%p", deferredContext);
        RemoveEntryList(listEntry);
        // Free its allocated memory.
        //
        ExFreePoolWithTag(deferredContext,
                          MemoryTag);
        deferredContext = NULL;

        // Get the next entry.
        //
        listEntry = nextListEntry;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "End list iteration");

SkipListIteration:

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Registry_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Registry.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Registry;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Registry;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Registry);
    dmfCallbacksDmf_Registry.DeviceOpen = DMF_Registry_Open;
    dmfCallbacksDmf_Registry.DeviceClose = DMF_Registry_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Registry,
                                            Registry,
                                            DMF_CONTEXT_Registry,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Registry.CallbacksDmf = &dmfCallbacksDmf_Registry;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Registry,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

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
    )
/*++

Routine Description:

    Given a registry handle, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    DmfModule - This Module's handle.
    Handle - An open registry key.
    RegistryEnumerationFunction - The client's enumeration callback function.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    RegistryKeyEnumerationContextType context;
    BOOLEAN returnValue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // There is nothing to pass in this context. (All subkeys are presented to enumerator callback.)
   //
    context.FilterEnumeratorContext = NULL;
    // For each subkey of the current key, this function will be called. It will actually create the entries.
    //
    context.RegistryKeyEnumerationFunction = ClientCallback;
    context.ClientCallbackContext = ClientCallbackContext;

    returnValue = Registry_SubKeysFromHandleEnumerate(Handle,
                                                      Registry_KeyEnumerationFilterAllSubkeys,
                                                      &context);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_CallbackWork(
    _In_ WDFDEVICE WdfDevice,
    _In_ EVT_DMF_Registry_CallbackWork* CallbackWork
    )
/*++

Routine Description:

    Create and open a Registry Module, perform work, close and destroy Registry Module.

Parameters Description:
    WdfDevice - WDFDEVICE to associate with the new Registry Module.
    CallbackWork - The function that does the work.

Return Value:

    NTSTATUS

--*/
{
    DMFMODULE dmfModuleRegistry;
    NTSTATUS ntStatus;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;
    dmfModuleRegistry = NULL;

    // Registry
    // --------
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = WdfDevice;
    DMF_Registry_ATTRIBUTES_INIT(&moduleAttributes);
    ntStatus = DMF_Registry_Create(WdfDevice,
                                   &moduleAttributes,
                                   &attributes,
                                   &dmfModuleRegistry);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Registry_Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    ASSERT(NULL != dmfModuleRegistry);

    // Do the work using the Module instance.
    //
    ntStatus = CallbackWork(dmfModuleRegistry);

Exit:

    // Close and destroy the Registry Module.
    //
    if (dmfModuleRegistry != NULL)
    {
        WdfObjectDelete(dmfModuleRegistry);
    }

    FuncExit(DMF_TRACE, "returnValue=%d", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Allow the caller to perform a custom action in the comparison handler for a value.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ComparisonCallback - Caller's comparison function.
    ComparisonCallbackContext - Caller's context sent to comparison. function.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // Value type is not needed for Delete.
   // ValueDataToCompare is optional...it will be passed to comparison function.
   //
    ntStatus = Registry_ValueActionIfNeeded(Registry_ActionTypeNone,
                                            DmfModule,
                                            Handle,
                                            ValueName,
                                            ValueType,
                                            ValueDataToCompare,
                                            ValueDataToCompareSize,
                                            ComparisonCallback,
                                            ComparisonCallbackContext,
                                            FALSE);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_EnumerateKeysFromName(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR RootKeyName,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    )
/*++

Routine Description:

    Given a registry path name, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    DmfModule - This Module's handle.
    RootKeyName - Path name of the registry key.
    ClientCallback - The enumeration function to call for each sub key.
    ClientCallbackContext - The client context to pass into the enumeration function.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    BOOLEAN returnValue;
    HANDLE handle;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    returnValue = FALSE;

    handle = Registry_HandleOpenByName(RootKeyName);
    if (NULL == handle)
    {
        goto Exit;
    }

    returnValue = Registry_SubKeysFromHandleEnumerate(handle,
                                                      ClientCallback,
                                                      ClientCallbackContext);

    Registry_HandleClose(handle);
    handle = NULL;

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
void
DMF_Registry_HandleClose(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle
    )
/*++

Routine Description:

    Given a registry handle, close the handle.

Arguments:

    DmfModule - This Module's handle.
    Handle - The given registry handle to an open registry key.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    Registry_HandleClose(Handle);
    Handle = NULL;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleDelete(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle
    )
/*++

Routine Description:

    Delete a registry key by path name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Registry key handle to delete.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // Delete the key.
   //
    ntStatus = ZwDeleteKey(Handle);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
HANDLE
DMF_Registry_HandleOpenByHandle(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR Name,
    _In_ BOOLEAN TryToCreate
    )
/*++

Routine Description:

    Given a registry handle, open a handle relative to that handle.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to open registry key.
    Name - Path name of the key relative to handle.
    TryToCreate - Indicates if the function should call create instead of open.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    HANDLE handle;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    handle = Registry_HandleOpenByHandle(Handle,
                                         Name,
                                         TryToCreate);

    FuncExit(DMF_TRACE, "handle=0x%p", handle);

    return handle;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenById(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG PredefinedKeyId,
    _In_ ULONG AccessMask,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open a predefined registry key.

Arguments:

    DmfModule - This Module's handle.
    PredefinedKeyId - The Id of the predefined key to open.
                      See IoOpenDeviceRegistryKey documentation for a list of Ids.
    AccessMask - Access mask to use to open the handle.
    RegistryHandle - Handle to open registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = Registry_HandleOpenByPredefinedKey(device,
                                                  PredefinedKeyId,
                                                  AccessMask,
                                                  RegistryHandle);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
HANDLE
DMF_Registry_HandleOpenByName(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR Name
    )
/*++

Routine Description:

    Open a registry key by path name.

Arguments:

    DmfModule - This Module's handle.
    Name - Path name of the key relative to handle.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    HANDLE handle;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    handle = Registry_HandleOpenByName(Name);

    FuncExit(DMF_TRACE, "handle=0x%p", handle);

    return handle;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenByNameEx(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR Name,
    _In_ ULONG AccessMask,
    _In_ BOOLEAN Create,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open a registry key by path name and access mask.

Arguments:

    DmfModule - This Module's handle.
    Name - Path name of the key relative to handle. NULL to open the device instance registry key.
           Note: Use DMF_Registry_HandleOpenById() instead to open the Device Key.
    AccessMask - Access mask to use to open the handle.
    Create - Creates the key if it cannot be opened.
    RegistryHandle - Handle to open registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    device = DMF_ParentDeviceGet(DmfModule);

    if (Name != NULL)
    {
        ntStatus = Registry_HandleOpenByNameEx(Name,
                                               AccessMask,
                                               Create,
                                               RegistryHandle);
    }
    else
    {
        // Deprecated path. Use DMF_Registry_HandleOpenById() instead.
        //
        ntStatus = Registry_HandleOpenByPredefinedKey(device,
                                                      PLUGPLAY_REGKEY_DEVICE,
                                                      AccessMask,
                                                      RegistryHandle);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// PathAndValue[Read | Write | Delete ][RegistryValueType]
//
// These functions work with a Path and Value. They open a handle to  the path, perform 
// the operation on the value and close the handle to the path.
//
/////////////////////////////////////////////////////////////////////////////////////////////
//

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueDelete(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName
    )
/*++

Routine Description:

    Delete a value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to delete.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    HANDLE registryPathHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);

    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_HandleOpenByNameEx(DmfModule,
                                               RegistryPathName,
                                               KEY_SET_VALUE,
                                               FALSE,
                                               &registryPathHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ASSERT(registryPathHandle != NULL);
    ntStatus = DMF_Registry_ValueDelete(DmfModule,
                                        registryPathHandle,
                                        ValueName);

    // Handle is no longer needed. Close it.
    //
    DMF_Registry_HandleClose(DmfModule,
                             registryPathHandle);
    registryPathHandle = NULL;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a value (of any REG_* type) given a registry path and value name.
    This functions is called by other Module Methods or can be directly by
    the Client Driver.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    HANDLE registryPathHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);

    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(((Buffer != NULL) && (BufferSize > 0)) || 
           ((NULL == Buffer) && (0 == BufferSize) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_HandleOpenByNameEx(DmfModule,
                                               RegistryPathName,
                                               KEY_READ,
                                               FALSE,
                                               &registryPathHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        if (BytesRead != NULL)
        {
            // Explicitly clear here for the above failure.
            // In case the above function succeeds, it is not necessary
            // to explicitly clear *BytesRead in this function.
            //
            *BytesRead = 0;
        }
        goto Exit;
    }

    ASSERT(registryPathHandle != NULL);
    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      registryPathHandle,
                                      ValueName,
                                      RegistryType,
                                      Buffer,
                                      BufferSize,
                                      BytesRead);

    // Handle is no longer needed. Close it.
    //
    DMF_Registry_HandleClose(DmfModule,
                             registryPathHandle);
    registryPathHandle = NULL;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_BINARY value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);

    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(((Buffer != NULL) && (BufferSize > 0)) || 
           ((NULL == Buffer) && (0 == BufferSize) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_BINARY,
                                             Buffer,
                                             BufferSize,
                                             BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadDword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer
    )
/*++

Routine Description:

    Reads a REG_DWORD value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_DWORD,
                                             (UCHAR*)Buffer,
                                             sizeof(ULONG),
                                             &bytesRead);
    // "Using 'bytesRead' from failed function call.
    //
    #pragma warning(suppress: 6102)
    ASSERT((NT_SUCCESS(ntStatus) && (sizeof(ULONG) == bytesRead)) || 
           (0 == bytesRead));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_DWORD value given a registry path and value name.
    Validate the read value against a minimum and maximum.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    Minimum - Caller's minimum expected value.
    Maximum - Caller's maximum expected value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueReadDword(DmfModule,
                                                  RegistryPathName,
                                                  ValueName,
                                                  Buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (*Buffer < Minimum)
    {
        // Read value is too low.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    if (*Buffer > Maximum)
    {
        // Read value is too high.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_MULTI_SZ value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    NumberOfCharacters - Number of WCHAR pointed to by Buffer.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);

    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(((Buffer != NULL) && (NumberOfCharacters > 0)) || 
           ((NULL == Buffer) && (0 == NumberOfCharacters) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_MULTI_SZ,
                                             (UCHAR*)Buffer,
                                             bufferSizeBytes,
                                             BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadQword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONGLONG* Buffer
    )
/*++

Routine Description:

    Reads a REG_QWORD value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_QWORD,
                                             (UCHAR*)Buffer,
                                             sizeof(ULONGLONG),
                                             &bytesRead);
    // "Using 'bytesRead' from failed function call.
    //
    #pragma warning(suppress: 6102)
    ASSERT((NT_SUCCESS(ntStatus) && (sizeof(ULONGLONG) == bytesRead)) || 
           (0 == bytesRead));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_QWORD value given a registry path and value name.
    Validate the read value against a minimum and maximum.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    Minimum - Caller's minimum expected value.
    Maximum - Caller's maximum expected value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueReadQword(DmfModule,
                                                  RegistryPathName,
                                                  ValueName,
                                                  Buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (*Buffer < Minimum)
    {
        // Read value is too low.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    if (*Buffer > Maximum)
    {
        // Read value is too high.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_SZ value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    NumberOfCharacters - Number of WCHAR in the array pointed to by Buffer.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);

    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(((Buffer != NULL) && (NumberOfCharacters > 0)) || 
           ((NULL == Buffer) && (0 == NumberOfCharacters) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_SZ,
                                             (UCHAR*)Buffer,
                                             bufferSizeBytes,
                                             BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Write a value (of any REG_* type) given a registry path and value name.
    This functions is called by other Module Methods or can be directly by
    the Client Driver.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to write.
    Buffer - The data that is written to the value.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    HANDLE registryPathHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);

    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_HandleOpenByNameEx(DmfModule,
                                               RegistryPathName,
                                               KEY_SET_VALUE,
                                               TRUE,
                                               &registryPathHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ASSERT(registryPathHandle != NULL);
    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       registryPathHandle,
                                       ValueName,
                                       RegistryType,
                                       Buffer,
                                       BufferSize);

    // Handle is no longer needed. Close it.
    //
    DMF_Registry_HandleClose(DmfModule,
                             registryPathHandle);
    registryPathHandle = NULL;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteBinary(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    )
/*++

Routine Description:

    Write a REG_BINARY value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to write.
    Buffer - The data that is written to value.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);

    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_BINARY,
                                              Buffer,
                                              BufferSize);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteDword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueData
    )
/*++

Routine Description:

    Write a REG_DWORD value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    ValueData - The data to write to the value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_DWORD,
                                              (UCHAR*)&ValueData,
                                              sizeof(ULONG));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteMultiString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    )
/*++

Routine Description:

    Write a REG_MULTI_SZ value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to write.
    Buffer - The data that is written to value.
    NumberOfCharacters - Number of WCHAR pointed to by Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);

    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_MULTI_SZ,
                                              (UCHAR*)Buffer,
                                              bufferSizeBytes);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteQword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONGLONG ValueData
    )
/*++

Routine Description:

    Write a REG_QWORD value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    ValueData - The data to write to the value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_QWORD,
                                              (UCHAR*)&ValueData,
                                              sizeof(ULONGLONG));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    )
/*++

Routine Description:

    Write a REG_SZ value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to write.
    Buffer - The data that is written to value.
    NumberOfCharacters - Number of WCHAR pointed to by Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);

    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_SZ,
                                              (UCHAR*)Buffer,
                                              bufferSizeBytes);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_RegistryPathDelete(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR Name
    )
/*++

Routine Description:

    Delete a registry key by path name.

Arguments:

    DmfModule - This Module's handle.
    Name - Path name of the key relative to handle.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    NTSTATUS ntStatus;
    HANDLE handle;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_HandleOpenByNameEx(Name,
                                           KEY_SET_VALUE,
                                           FALSE,
                                           &handle);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }
    ASSERT(handle != NULL);

    // Delete the key.
    //
    ntStatus = DMF_Registry_HandleDelete(DmfModule,
                                         handle);

    // Regardless of the above call, close the handle.
    //
    Registry_HandleClose(handle);
    handle = NULL;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
ScheduledTask_Result_Type
DMF_Registry_ScheduledTaskCallbackContainer(
    _In_ DMFMODULE DmfScheduledTask,
    _In_ VOID* ClientCallbackContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Call a number of callback functions that do work on the registry.

Parameters Description:

    DmfScheduledTask - The ScheduledTask DMF Module from which the callback is called.
    ClientCallbackContext - Client Context provided for this callback.
    PreviousState - Valid only for calls from D0Entry

Return Value:

    ScheduledTask_WorkResult_Success - Indicates the operations was successful.
    ScheduledTask_WorkResult_Fail - Indicates the operation was not successful. A retry will happen.

--*/
{
    NTSTATUS ntStatus;
    ScheduledTask_Result_Type returnValue;
    WDFDEVICE device;
    Registry_ContextScheduledTaskCallback* scheduledTaskCallbackContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_Registry_ScheduledTaskCallbackContainer");

    returnValue = ScheduledTask_WorkResult_FailButTryAgain;

    device = DMF_ParentDeviceGet(DmfScheduledTask);

    scheduledTaskCallbackContext = (Registry_ContextScheduledTaskCallback*)ClientCallbackContext;
    ASSERT(scheduledTaskCallbackContext != NULL);

    for (ULONG callbackIndex = 0; callbackIndex < scheduledTaskCallbackContext->NumberOfCallbacks; callbackIndex++)
    {
        // Create and open a Registry Module, do the registry work, close and destroy the Registry Module.
        //
        ntStatus = DMF_Registry_CallbackWork(device,
                                             scheduledTaskCallbackContext->Callbacks[callbackIndex]);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                        "DMF_Registry_CallbackWork callbackIndex=%d ntStatus=%!STATUS!",
                        callbackIndex,
                        ntStatus);
            goto Exit;
        }
    }

    // Work is done, no need to try again.
    //
    returnValue = ScheduledTask_WorkResult_Success;

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_SubKeysFromHandleEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    )
/*++

Routine Description:

    Given a registry handle, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    DmfModule - This Module's handle.
    Handle - An open registry key.
    ClientCallback - The enumeration function to call for each sub key.
    ClientCallbackContext -  The client context to pass into the enumeration function.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    returnValue = Registry_SubKeysFromHandleEnumerate(Handle,
                                                      ClientCallback,
                                                      ClientCallbackContext);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_SubKeysFromPathNameContainingStringEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR PathName,
    _In_ PWCHAR LookFor,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    )
/*++

Routine Description:

    Given a registry path name, enumerate all the sub keys and call an enumeration function for each of them
    which looks for a particular substring.

Arguments:

    DmfModule - This Module's handle.
    PathName - Path name of the registry key.
    LookFor - The substring to search for in the sub keys.
    RegistryEnumerationFunction - The function that does the comparison for all the sub keys.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    RegistryKeyEnumerationContextType context;
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // It is the substring that is searched for inside the enumerated sub keys.
   //
    context.FilterEnumeratorContext = LookFor;

    // It is the function that the client wants called for all the sub keys that contain
    // the substring to look for.
    //
    context.RegistryKeyEnumerationFunction = ClientCallback;
    context.ClientCallbackContext = ClientCallbackContext;

    // Enumerate all the sub keys and call the function that looks for the substring in each
    // of the enumerated sub keys.
    //
    returnValue = DMF_Registry_EnumerateKeysFromName(DmfModule,
                                                     PathName,
                                                     Registry_KeyEnumerationFilterStrstr,
                                                     &context);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

NTSTATUS
DMF_Registry_TreeWriteDeferred(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* RegistryTree,
    _In_ ULONG ItemCount
    )
/*++

Routine Description:

    Writes an array of registry trees to the registry in a deferred time. Keep retrying
    if STATUS_OBJECT_NAME_NOT_FOUND happens.

Arguments:

    DmfModule - This Module's handle.
    Tree - The array of registry trees.
    NumberOfTrees - The number of entries in the array.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_DeferredOperationAdd(DmfModule,
                                             RegistryTree,
                                             ItemCount,
                                             Registry_DeferredOperationWrite);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

NTSTATUS
DMF_Registry_TreeWriteEx(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* RegistryTree,
    _In_ ULONG ItemCount
    )
/*++

Routine Description:

    Writes an array of registry trees to the registry.

Arguments:

    DmfModule - This Module's handle.
    Tree - The array of registry trees.
    NumberOfTrees - The number of entries in the array.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_TreeWrite(DmfModule,
                                  RegistryTree,
                                  ItemCount);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueDelete(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName
    )
/*++

Routine Description:

    Delete a value from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_ValueActionAlways(Registry_ActionTypeDelete,
                                          DmfModule,
                                          Handle,
                                          ValueName,
                                          0,
                                          NULL,
                                          0,
                                          NULL);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Delete a value after calling a client comparison function to determine if that
    data should be deleted.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ComparisonCallback - Caller's comparison function.
    ComparisonCallbackContext - Caller's context sent to comparison. function.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // Value type is not needed for Delete.
   // ValueDataToCompare is optional...it will be passed to comparison function.
   //
    ntStatus = Registry_ValueActionIfNeeded(Registry_ActionTypeDelete,
                                            DmfModule,
                                            Handle,
                                            ValueName,
                                            0,
                                            ValueDataToCompare,
                                            ValueDataToCompareSize,
                                            ComparisonCallback,
                                            ComparisonCallbackContext,
                                            FALSE);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads any type of value from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The registry type of value to read.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(((Buffer != NULL) && (BufferSize > 0)) ||
        ((NULL == Buffer) && (0 == BufferSize) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // NOTE: Bytes read is optional. Clear in case of error.
   //
    if (BytesRead != NULL)
    {
        *BytesRead = 0;
    }

    // "Using uninitialized memory '*Buffer'.".
    //
    #pragma warning(suppress: 6001)
    ntStatus = Registry_ValueActionAlways(Registry_ActionTypeRead,
                                          DmfModule,
                                          Handle,
                                          ValueName,
                                          ValueType,
                                          Buffer,
                                          BufferSize,
                                          BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_BINARY from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(((Buffer != NULL) && (BufferSize > 0)) || 
           ((NULL == Buffer) && (0 == BufferSize) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_BINARY,
                                      (UCHAR*)Buffer,
                                      BufferSize,
                                      BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadDword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer
    )
/*++

Routine Description:

    Reads a REG_DWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_DWORD,
                                      (UCHAR*)Buffer,
                                      sizeof(ULONG),
                                      &bytesRead);
    // "Using 'bytesRead' from failed function call.
    //
    #pragma warning(suppress: 6102)
    ASSERT((NT_SUCCESS(ntStatus) && (sizeof(ULONG) == bytesRead)) || 
           (0 == bytesRead));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_DWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    Minimum - Caller's minimum expected value.
    Maximum - Caller's maximum expected value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueReadDword(DmfModule,
                                           Handle,
                                           ValueName,
                                           Buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (*Buffer < Minimum)
    {
        // Read value is too low.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    if (*Buffer > Maximum)
    {
        // Read value is too high.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_MULTI_SZ from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    NumberOfCharacters - Number of WCHAR in the array pointed to by Buffer.
    BytesRead - Number of bytes read and written into Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(((Buffer != NULL) && (NumberOfCharacters > 0)) || 
           ((NULL == Buffer) && (0 == NumberOfCharacters) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_MULTI_SZ,
                                      (UCHAR*)Buffer,
                                      bufferSizeBytes,
                                      BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadQword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ ULONGLONG* Buffer
    )
/*++

Routine Description:

    Reads a REG_QWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_QWORD,
                                      (UCHAR*)Buffer,
                                      sizeof(ULONGLONG),
                                      &bytesRead);
    // "Using 'bytesRead' from failed function call.
    //
    #pragma warning(suppress: 6102)
    ASSERT((NT_SUCCESS(ntStatus) && (sizeof(ULONGLONG) == bytesRead)) || 
           (0 == bytesRead));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_QWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    Minimum - Caller's minimum expected value.
    Maximum - Caller's maximum expected value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueReadQword(DmfModule,
                                           Handle,
                                           ValueName,
                                           Buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (*Buffer < Minimum)
    {
        // Read value is too low.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    if (*Buffer > Maximum)
    {
        // Read value is too high.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Reads a REG_SZ from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    NumberOfCharacters - Number of WCHAR in the array pointed to by Buffer.
    BytesRead - Number of bytes read and written into Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(((Buffer != NULL) && (NumberOfCharacters > 0)) ||
        ((NULL == Buffer) && (0 == NumberOfCharacters) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_SZ,
                                      (UCHAR*)Buffer,
                                      bufferSizeBytes,
                                      BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Writes any type of value to the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The registry type of value to read.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_ValueActionAlways(Registry_ActionTypeWrite,
                                          DmfModule,
                                          Handle,
                                          ValueName,
                                          ValueType,
                                          Buffer,
                                          BufferSize,
                                          NULL);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteBinary(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    )
/*++

Routine Description:

    Write a REG_BINARY from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueName - The name of the value that is written.
    Buffer - The string that is written.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_BINARY,
                                       (UCHAR*)Buffer,
                                       BufferSize);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteDword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueData
    )
/*++

Routine Description:

    Write a REG_DWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is written.
    ValueData - The data to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_DWORD,
                                       (UCHAR*)&ValueData,
                                       sizeof(ULONG));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Write the data for a value after calling a client comparison function to determine if that
    data should be written.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The Registry Type of the value.
    ValueDataToWrite - The data to write if the value is not set to one or it does not exist.
    ValueDataToWriteSize - The size of the buffer at ValueDataToWrite
    ComparisonCallback - Caller's comparison function.
    ComparisonCallbackContext - Caller's context sent to comparison. function.
    WriteIfNotFound - Indicates if the value should be written if it does not exist.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_ValueActionIfNeeded(Registry_ActionTypeWrite,
                                            DmfModule,
                                            Handle,
                                            ValueName,
                                            ValueType,
                                            ValueDataToWrite,
                                            ValueDataToWriteSize,
                                            ComparisonCallback,
                                            ComparisonCallbackContext,
                                            WriteIfNotFound);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteMultiString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    )
/*++

Routine Description:

    Write a REG_MULTI_SZ to the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is written.
    Buffer - The string that is written.
    NumberOfCharacters - Number of WCHAR pointed to by Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_MULTI_SZ,
                                       (UCHAR*)Buffer,
                                       bufferSizeBytes);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteQword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONGLONG ValueData
    )
/*++

Routine Description:

    Write a REG_QWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is written.
    ValueData - The data to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_QWORD,
                                       (UCHAR*)&ValueData,
                                       sizeof(ULONGLONG));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    )
/*++

Routine Description:

    Write a REG_SZ to the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is written.
    Buffer - The string that is written.
    NumberOfCharacters - Size in characters pointed to by Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(DmfModule != NULL);
    ASSERT(ValueName != NULL);
    ASSERT(*ValueName != L'\0');
    ASSERT(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_SZ,
                                       (UCHAR*)Buffer,
                                       bufferSizeBytes);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_Registry.c
//
