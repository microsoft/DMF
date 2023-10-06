/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_BranchTrack.c

Abstract:

    BranchTrack Module, to collect and analyze run-time code coverage statistics for selected branches of code.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Core.h"
#include "DmfModules.Core.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_BranchTrack.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// A type used as a key for a hash table
//
typedef struct
{
    // Length of source file name string. The string itself is stored in RawData buffer.
    //
    ULONG FileNameLength;
    // Source file line number.
    //
    ULONG Line;
    // Length branch check point name string. The string itself is stored in RawData buffer.
    //
    ULONG BranchNameLength;
    // Length of Hint Name string. The string itself is stored in RawData buffer.
    //
    ULONG HintNameLength;
    // A callback to query status of this check point.
    //
    EVT_DMF_BranchTrack_StatusQuery* CallbackStatusQuery;
    // Client context associated with the check point.
    //
    ULONG_PTR Context;
    // Data buffer to store string data. Strings stored in the following order: <File name><zero byte><Branch name><zero byte>.
    //
    UCHAR RawData[ANYSIZE_ARRAY];
} HASH_TABLE_KEY;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_BranchTrack
{
    // Length of HashTable key buffer in bytes.
    //
    ULONG TableKeyBufferLength;
    // HashTable Module handle.
    //
    DMFMODULE DmfObjectHashTable;
    // BufferPool Module handle. We use it to avoid temporary key buffers allocation in Module Methods.
    //
    DMFMODULE DmfObjectBufferPool;
} DMF_CONTEXT_BranchTrack;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(BranchTrack)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(BranchTrack)

// Memory Pool Tag.
//
#define MemoryTag 'oMTB'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define BRANCHTRACK_NUMBER_OF_STRINGS_IN_RAWDATA    3

#define BRANCHTRACK_FILENAME_OFFSET(TableKey)       (0)
#define BRANCHTRACK_BRANCHNAME_OFFSET(TableKey)     (TableKey->FileNameLength + 1)
#define BRANCHTRACK_HINTNAME_OFFSET(TableKey)       (TableKey->FileNameLength + 1 + TableKey->BranchNameLength + 1)

#define BRANCHTRACK_MAXIMUM_HINT_NAME_LENGTH    32

// Default settings good for most drivers.
// (Client can override.)
//
#define BRANCHTRACK_DEFAULT_MAXIMUM_BRANCH_NAME_LENGTH      300

// The number of buffers preallocated by BufferPool. 
// Should roughly match the maximum number of concurrent threads calling into our Module Methods.
// In case there are more concurrent threads at some point than the number of buffers we specified - 
// there will be only some performance hit, cause additional buffers will need to be temporarily 
// allocated from lookaside list.
//
#define BRANCHTRACK_NUMBER_OF_BUFFERS           16

// Helper structure to use as a context during hash table enumeration to calculate output buffer size.
//
typedef struct _DETAILS_SIZE_CONTEXT
{
    ULONG SizeToAllocate;
} DETAILS_SIZE_CONTEXT;

// Helper structure to use as a context during hash table enumeration to populate output buffer.
//
typedef struct _DETAILS_DATA_CONTEXT
{
    BRANCHTRACK_REQUEST_OUTPUT_DATA* OutputData;
    BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS* PreviousEntry;
    ULONG ResponseLengthAllocated;
} DETAILS_DATA_CONTEXT;

_IRQL_requires_max_(DISPATCH_LEVEL)
static
inline
CHAR*
BranchTrack_FileNameBufferGet(
    _In_ HASH_TABLE_KEY* TableKey
    )
/*++

Routine Description:

    Returns a pointer to the first byte of FileName buffer in specified TableKey.

Arguments:

    TableKey - Pointer to a hash table key.

Return Value:

    Pointer to FileName buffer of the given table key.

--*/
{
    ULONG fileNameOffset;

    fileNameOffset = BRANCHTRACK_FILENAME_OFFSET(TableKey);
    return (CHAR*)(&TableKey->RawData[fileNameOffset]);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
inline
CHAR*
BranchTrack_BranchNameBufferGet(
    _In_ HASH_TABLE_KEY* TableKey
    )
/*++

Routine Description:

    Returns a pointer to the first byte of BranchName buffer in specified TableKey.

Arguments:

    TableKey - Pointer to a hash table key.

Return Value:

    Pointer to BranchName buffer of the given table key.

--*/
{
    ULONG branchNameOffset;

    branchNameOffset = BRANCHTRACK_BRANCHNAME_OFFSET(TableKey);
    return (CHAR*)(&TableKey->RawData[branchNameOffset]);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
inline
CHAR*
BranchTrack_HintNameBufferGet(
    _In_ HASH_TABLE_KEY* TableKey
    )
/*++

Routine Description:

    Returns a pointer to the first byte of HintName buffer in specified TableKey.

Arguments:

    TableKey - Pointer to a hash table key.

Return Value:

    Pointer to the HintName buffer of the given table key.

--*/
{
    ULONG hintNameOffset;

    hintNameOffset = BRANCHTRACK_HINTNAME_OFFSET(TableKey);
    return (CHAR*)(&TableKey->RawData[hintNameOffset]);
}

_Function_class_(EVT_DMF_HashTable_Find)
_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
BranchTrack_EVT_DMF_HashTable_Find(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _Inout_updates_bytes_(sizeof(ULONGLONG)) UCHAR* Value,
    _Inout_ ULONG* ValueLength
    )
/*++

Routine Description:

    EVT_DMF_HashTable_Find callback to increment number of times a branch was executed.

Arguments:
    DmfModule - The Child Module from which this callback is called.
    Key - Pointer to Key buffer of the hash table.
    KeyLength - Length of Key buffer.
    Value - Pointer to Value buffer of the hash table.
    ValueLength - Length of Value buffer of the hash table.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Key);
    UNREFERENCED_PARAMETER(KeyLength);

    if (*ValueLength == 0)
    {
        *ValueLength = sizeof(ULONGLONG);
        *(ULONGLONG*)Value = 0;
    }

    *(ULONGLONG*)Value = *(ULONGLONG*)Value + 1;
}

_Function_class_(EVT_DMF_HashTable_Find)
_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
BranchTrack_HashTable_CallbackEntryCreate(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _Out_writes_bytes_(sizeof(ULONGLONG)) UCHAR* Value,
    _Inout_ ULONG* ValueLength
    )
/*++

Routine Description:

    EVT_DMF_HashTable_Find callback to create the initial entry.

Arguments:
    DmfModule - The Child Module from which this callback is called.
    Key - Pointer to Key buffer of the hash table.
    KeyLength - Length of Key buffer.
    Value - Pointer to Value buffer of the hash table.
    ValueLength - Length of Value buffer of the hash table.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Key);
    UNREFERENCED_PARAMETER(KeyLength);

    *ValueLength = sizeof(ULONGLONG);
    *(ULONGLONG*)Value = 0;
}

_Function_class_(EVT_DMF_HashTable_Enumerate)
_IRQL_requires_max_(DISPATCH_LEVEL)
static
BOOLEAN
BranchTrack_EVT_DMF_HashTable_Enumerate_Status(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _In_reads_(ValueLength) UCHAR* Value,
    _In_ ULONG ValueLength,
    _In_ VOID* CallbackContext
    )
/*++

Routine Description:

    EVT_DMF_HashTable_Enumerate callback to populate output data buffer with Status information.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Key - Pointer to Key buffer of the current hash table entry.
    KeyLength - Length of Key buffer.
    Value - Pointer to Value buffer of the current hash table entry.
    ValueLength - Length of Value buffer of the hash table.
    CallbackContext - Context parameter passed to enumeration function.

Return Value:

    Always TRUE, we don't want to break the enumeration process.

--*/
{
    HASH_TABLE_KEY* tableKey;
    ULONGLONG tableValue;
    BRANCHTRACK_REQUEST_OUTPUT_DATA_STATUS* statusData;
    CHAR* keyBufferBranchName;

    UNREFERENCED_PARAMETER(KeyLength);

    tableKey = (HASH_TABLE_KEY*)Key;
    DmfAssert(NULL != tableKey);

    statusData = (BRANCHTRACK_REQUEST_OUTPUT_DATA_STATUS*)CallbackContext;
    DmfAssert(NULL != statusData);

    ++statusData->BranchesTotal;

    if (0 == ValueLength)
    {
        tableValue = 0;
    }
    else
    {
        DmfAssert(sizeof(ULONGLONG) == ValueLength);
        tableValue = *(ULONGLONG*)Value;
    }

    keyBufferBranchName = BranchTrack_BranchNameBufferGet(tableKey);

    DmfAssert(NULL != tableKey->CallbackStatusQuery);
    if (tableKey->CallbackStatusQuery(DmfModule,
                                      keyBufferBranchName,
                                      tableKey->Context,
                                      tableValue))
    {
        ++statusData->BranchesPassed;
    }

    return TRUE;
}

_Function_class_(EVT_DMF_HashTable_Enumerate)
_IRQL_requires_max_(DISPATCH_LEVEL)
static
BOOLEAN
BranchTrack_EVT_DMF_HashTable_Enumerate_DetailsSize(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _In_reads_(ValueLength) UCHAR* Value,
    _In_ ULONG ValueLength,
    _In_ VOID* CallbackContext
    )
/*++

Routine Description:

    EVT_DMF_HashTable_Enumerate callback to calculate the size of output buffer for Details information.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Key - Pointer to Key buffer of the current hash table entry.
    KeyLength - Length of Key buffer.
    Value - Pointer to Value buffer of the current hash table entry.
    ValueLength - Length of Value buffer of the hash table.
    CallbackContext - Context parameter passed to enumeration function.

Return Value:

    Always TRUE, we don't want to break the enumeration process.

--*/
{
    HASH_TABLE_KEY* tableKey;
    DETAILS_SIZE_CONTEXT* detailsSizeContext;
    ULONG currentEntrySize;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(KeyLength);
    UNREFERENCED_PARAMETER(Value);
    UNREFERENCED_PARAMETER(ValueLength);

    tableKey = (HASH_TABLE_KEY*)Key;
    DmfAssert(NULL != tableKey);

    detailsSizeContext = (DETAILS_SIZE_CONTEXT*)CallbackContext;
    DmfAssert(NULL != detailsSizeContext);

    // Three strings plus three terminators.
    //
    currentEntrySize = FIELD_OFFSET(BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS,
                                    StringBuffer[(size_t)tableKey->FileNameLength +
                                                 (size_t)tableKey->BranchNameLength +
                                                 (size_t)tableKey->HintNameLength +
                                                 (sizeof(CHAR) * BRANCHTRACK_NUMBER_OF_STRINGS_IN_RAWDATA)]);
    detailsSizeContext->SizeToAllocate += currentEntrySize;

    return TRUE;
}

_Function_class_(EVT_DMF_HashTable_Enumerate)
_IRQL_requires_max_(DISPATCH_LEVEL)
static
BOOLEAN
BranchTrack_EVT_DMF_HashTable_Enumerate_DetailsData(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _In_reads_(ValueLength) UCHAR* Value,
    _In_ ULONG ValueLength,
    _In_ VOID* CallbackContext
    )
/*++

Routine Description:

    EVT_DMF_HashTable_Enumerate callback to populate output data buffer with Details information.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Key - Pointer to Key buffer of the current hash table entry.
    KeyLength - Length of Key buffer.
    Value - Pointer to Value buffer of the current hash table entry.
    ValueLength - Length of Value buffer of the hash table.
    CallbackContext - Context parameter passed to enumeration function.

Return Value:

    Always TRUE, we don't want to break the enumeration process.

--*/
{
    HASH_TABLE_KEY* tableKey;
    ULONGLONG tableValue;
    DETAILS_DATA_CONTEXT* detailsDataContext;
    BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS* currentEntry;
    ULONG currentEntrySize;
    CHAR* keyBufferFileName;
    CHAR* keyBufferBranchName;
    CHAR* keyBufferHintName;

    UNREFERENCED_PARAMETER(KeyLength);

    tableKey = (HASH_TABLE_KEY*)Key;
    DmfAssert(NULL != tableKey);

    if (0 == ValueLength)
    {
        tableValue = 0;
    }
    else
    {
        DmfAssert(sizeof(ULONGLONG) == ValueLength);
        tableValue = *(ULONGLONG*)Value;
    }

    detailsDataContext = (DETAILS_DATA_CONTEXT*)CallbackContext;
    DmfAssert(NULL != detailsDataContext);
    DmfAssert(NULL != detailsDataContext->OutputData);

    currentEntrySize = FIELD_OFFSET(BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS, 
                                    StringBuffer[(size_t)tableKey->FileNameLength + 
                                                 (size_t)tableKey->BranchNameLength + 
                                                 (size_t)tableKey->HintNameLength + 
                                                 (sizeof(CHAR) * BRANCHTRACK_NUMBER_OF_STRINGS_IN_RAWDATA)]);

    if (detailsDataContext->OutputData->ResponseLength + currentEntrySize > detailsDataContext->ResponseLengthAllocated)
    {
        // This should never happen, unless there is an error in this Module.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Insufficient output buffer size");
        DmfAssert(FALSE);
        goto Exit;
    }

    keyBufferFileName = BranchTrack_FileNameBufferGet(tableKey);
    keyBufferBranchName = BranchTrack_BranchNameBufferGet(tableKey);
    keyBufferHintName = BranchTrack_HintNameBufferGet(tableKey);

    currentEntry = (BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS*)&detailsDataContext->OutputData->Response.Details[detailsDataContext->OutputData->ResponseLength];

    currentEntry->NextEntryOffset = 0;

    ULONG fileNameOffset = BRANCHTRACK_FILENAME_OFFSET(tableKey);
    ULONG branchNameOffset = BRANCHTRACK_BRANCHNAME_OFFSET(tableKey);
    ULONG hintNameOffset = BRANCHTRACK_HINTNAME_OFFSET(tableKey);

    currentEntry->FileNameOffset = FIELD_OFFSET(BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS,
                                                StringBuffer[fileNameOffset]);
    currentEntry->LineNumber = tableKey->Line;

    currentEntry->BranchNameOffset = FIELD_OFFSET(BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS,
                                                  StringBuffer[branchNameOffset]);

    currentEntry->HintNameOffset = FIELD_OFFSET(BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS,
                                                StringBuffer[hintNameOffset]);

    DmfAssert(NULL != tableKey->CallbackStatusQuery);
    currentEntry->IsPassed = tableKey->CallbackStatusQuery(DmfModule,
                                                           keyBufferBranchName,
                                                           tableKey->Context,
                                                           tableValue);

    RtlCopyMemory(&currentEntry->StringBuffer[fileNameOffset],
                  keyBufferFileName,
                  tableKey->FileNameLength);

    RtlCopyMemory(&currentEntry->StringBuffer[branchNameOffset],
                  keyBufferBranchName,
                  tableKey->BranchNameLength);

    RtlCopyMemory(&currentEntry->StringBuffer[hintNameOffset],
                  keyBufferHintName,
                  tableKey->HintNameLength);

    // Output the current state of the counter.
    //
    currentEntry->CounterState = tableValue;

    // Output the expected state of the counter.
    //
    DmfAssert(ValueLength >= sizeof(ULONGLONG));
    currentEntry->ExpectedState = (ULONGLONG)tableKey->Context;

    if (NULL != detailsDataContext->PreviousEntry)
    {
        DmfAssert(currentEntry > detailsDataContext->PreviousEntry);
        detailsDataContext->PreviousEntry->NextEntryOffset = (ULONG)((ULONG_PTR)currentEntry - (ULONG_PTR)detailsDataContext->PreviousEntry);
    }

    detailsDataContext->OutputData->ResponseLength += currentEntrySize;
    detailsDataContext->PreviousEntry = currentEntry;

Exit:

    return TRUE;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
_Must_inspect_result_
NTSTATUS
BranchTrack_ConfigInitialize(
    _In_ DMFMODULE DmfModule,
    _Inout_ DMF_CONTEXT_BranchTrack* ModuleContext
    )
/*++

Routine Description:

    Initializes the Module Context.

Arguments:

    DmfModule - This Module's handle.
    ModuleContext - This Module's context to initialize.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    DMF_CONFIG_HashTable* moduleConfigHashTable;
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DmfAssert(NULL != DmfModule);
    DmfAssert(NULL != ModuleContext);

    moduleConfigHashTable = (DMF_CONFIG_HashTable*)DMF_ModuleConfigGet(ModuleContext->DmfObjectHashTable);
    DmfAssert(moduleConfigHashTable != NULL);

    ModuleContext->TableKeyBufferLength = moduleConfigHashTable->MaximumKeyLength;

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
BranchTrack_ConfigCleanup(
    _Inout_ DMF_CONTEXT_BranchTrack* ModuleContext
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

    DmfAssert(NULL != ModuleContext);

    ModuleContext->DmfObjectHashTable = NULL;
    ModuleContext->DmfObjectBufferPool = NULL;
}
#pragma code_seg()

static
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
BranchTrack_QueryInformation_Status(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ SIZE_T* BytesReturned
    )
{
/*++

Routine Description:

    BRANCHTRACK_REQUEST_TYPE_STATUS request type handler

Arguments:

    DmfModule - This Module's handle.
    Request - Handle to a framework request object.
    BytesReturned - Number of bytes updated in request output buffer.

Return Value:

    NTSTATUS

--*/
    NTSTATUS ntStatus;
    DMF_CONTEXT_BranchTrack* moduleContext;
    BRANCHTRACK_REQUEST_OUTPUT_DATA* outputData;
    SIZE_T clientDriverNameLength;
    SIZE_T bufferLengthRequired;
    DMF_CONFIG_BranchTrack* moduleConfig;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(NULL != BytesReturned);
    *BytesReturned = 0;

    clientDriverNameLength = strlen(moduleConfig->ClientName);
    DmfAssert(clientDriverNameLength > 0);
    DmfAssert(clientDriverNameLength < BRANCH_TRACK_CLIENT_NAME_MAXIMUM_LENGTH);

    bufferLengthRequired = FIELD_OFFSET(BRANCHTRACK_REQUEST_OUTPUT_DATA,
                                        Response.Status.ClientModuleName[clientDriverNameLength + 1]);

    // Get a pointer to the output buffer. Make sure it is big enough.
    //
    ntStatus = WdfRequestRetrieveOutputBuffer(Request,
                                              bufferLengthRequired,
                                              (VOID**)&outputData,
                                              NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestRetrieveOutputBuffer fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    *BytesReturned = bufferLengthRequired;

    RtlZeroMemory(outputData,
                  bufferLengthRequired);

    outputData->ResponseType = BRANCHTRACK_REQUEST_TYPE_STATUS;
    outputData->ResponseLength = FIELD_OFFSET(BRANCHTRACK_REQUEST_OUTPUT_DATA_STATUS,
                                              ClientModuleName[clientDriverNameLength + 1]);
    outputData->Response.Status.BranchesTotal = 0;
    outputData->Response.Status.BranchesPassed = 0;

    // Buffer is already zeroed. Only copy the characters.
    //
    RtlCopyMemory(outputData->Response.Status.ClientModuleName,
                  moduleConfig->ClientName,
                  clientDriverNameLength);

    DMF_ModuleLock(DmfModule);

    DMF_HashTable_Enumerate(moduleContext->DmfObjectHashTable,
                            BranchTrack_EVT_DMF_HashTable_Enumerate_Status,
                            &outputData->Response.Status);

    DMF_ModuleUnlock(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
BranchTrack_QueryInformation_Details(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ SIZE_T* BytesReturned
    )
{
/*++

Routine Description:

    BRANCHTRACK_REQUEST_TYPE_DETAILS request type handler

Arguments:

    DmfModule - This Module's handle.
    Request - Handle to a framework request object.
    BytesReturned - Number of bytes updated in request output buffer.

Return Value:

    NTSTATUS

--*/
    NTSTATUS ntStatus;
    DMF_CONTEXT_BranchTrack* moduleContext;
    DMF_CONFIG_BranchTrack* moduleConfig;
    BRANCHTRACK_REQUEST_OUTPUT_DATA* outputData;
    DETAILS_SIZE_CONTEXT detailsSizeContext;
    DETAILS_DATA_CONTEXT detailsDataContext;
    SIZE_T bufferLengthRequired;

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    DmfAssert(NULL != moduleConfig);

    DmfAssert(NULL != BytesReturned);
    *BytesReturned = 0;

    RtlZeroMemory(&detailsSizeContext,
                  sizeof(detailsSizeContext));
    RtlZeroMemory(&detailsDataContext,
                  sizeof(detailsDataContext));

    // Two separate calls are necessary to HashTable data. The first gets the size
    // needed, the second gets the actual data. Acquire BranchTrack lock so that 
    // the number of entries cannot change in between the two calls.
    //
    DMF_ModuleLock(DmfModule);

    DMF_HashTable_Enumerate(moduleContext->DmfObjectHashTable,
                            BranchTrack_EVT_DMF_HashTable_Enumerate_DetailsSize,
                            &detailsSizeContext);

    bufferLengthRequired = FIELD_OFFSET(BRANCHTRACK_REQUEST_OUTPUT_DATA,
                                        Response.Details[0]) + (size_t)detailsSizeContext.SizeToAllocate;

    // Get a pointer to the output buffer. Make sure it is big enough.
    //
    ntStatus = WdfRequestRetrieveOutputBuffer(Request,
                                              bufferLengthRequired,
                                              (VOID**)&outputData,
                                              NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestRetrieveOutputBuffer fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    *BytesReturned = bufferLengthRequired;

    RtlZeroMemory(outputData,
                  bufferLengthRequired);

    outputData->ResponseType = BRANCHTRACK_REQUEST_TYPE_DETAILS;
    outputData->ResponseLength = 0;

    detailsDataContext.OutputData = outputData;
    detailsDataContext.PreviousEntry = NULL;
    detailsDataContext.ResponseLengthAllocated = detailsSizeContext.SizeToAllocate;

    DMF_HashTable_Enumerate(moduleContext->DmfObjectHashTable,
                            BranchTrack_EVT_DMF_HashTable_Enumerate_DetailsData,
                            &detailsDataContext);

    DmfAssert(outputData->ResponseLength == detailsSizeContext.SizeToAllocate);

Exit:

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
BranchTrack_QueryInformation(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ SIZE_T* BytesReturned
    )
{
/*++

Routine Description:

    IOCTL_BRANCHTRACK_QUERY_INFORMATION request handler.

Arguments:

    DmfModule - This Module's handle.
    Request - Handle to a framework request object.
    BytesReturned - Number of bytes updated in request output buffer.

Return Value:

    NTSTATUS

--*/
    NTSTATUS ntStatus;
    BRANCHTRACK_REQUEST_INPUT_DATA* inputData;

    FuncEntry(DMF_TRACE);

    DmfAssert(NULL != BytesReturned);

    // For SAL.
    //
    *BytesReturned = 0;

    // Get a pointer to the output buffer. Make sure it is big enough.
    //
    ntStatus = WdfRequestRetrieveInputBuffer(Request,
                                             sizeof(BRANCHTRACK_REQUEST_INPUT_DATA),
                                             (VOID**)&inputData,
                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestRetrieveInputBuffer fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Request type: %d", inputData->Type);

    switch (inputData->Type)
    {
        case BRANCHTRACK_REQUEST_TYPE_STATUS:
        {
            ntStatus = BranchTrack_QueryInformation_Status(DmfModule,
                                                           Request,
                                                           BytesReturned);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "BranchTrack_QueryInformation_Status fails: ntStatus=%!STATUS!", ntStatus);
            }

            break;
        }

        case BRANCHTRACK_REQUEST_TYPE_DETAILS:
        {
            ntStatus = BranchTrack_QueryInformation_Details(DmfModule,
                                                            Request,
                                                            BytesReturned);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "BranchTrack_QueryInformation_Details fails: ntStatus=%!STATUS!", ntStatus);
            }

            break;
        }

        default:
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Unsupported type: %d", inputData->Type);
            ntStatus = STATUS_NOT_SUPPORTED;
            DmfAssert(FALSE);
            break;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
BranchTrack_CheckPointProcess(
    _In_ DMFMODULE DmfModule,
    _In_z_ CHAR* BranchName,
    _In_z_ CHAR* HintName,
    _In_z_ CHAR* FileName,
    _In_ ULONG Line,
    _In_ EVT_DMF_BranchTrack_StatusQuery* CallbackStatusQuery,
    _In_ ULONG_PTR Context,
    _In_ EVT_DMF_HashTable_Find* CallbackFind
    )
/*++

Routine Description:

    Adds a custom branch checkpoint to the hash table, specifying a callback to query status.
    If checkpoint already exists there - it's count will be incremented.
    This function should not be used directly, use DMF_BRANCHTRACK_* macros instead.

Arguments:

    DmfModule - This Module's handle.
    BranchName - Name to associate with this branch checkpoint.
    HintName - Name of hint about condition for consumer.
    FileName - Name of a source file. (For possible future use.)
    Line - Source line number. (For possible future use.)
    CallbackStatusQuery - callback function to query check point status.
    Context - client's context to associate with this checkpoint.
    CallbackFind - The function that will perform the work (create/execute).

Return Value:

    None

    --*/
{
    DMF_CONFIG_BranchTrack* moduleConfig;
    DMF_CONTEXT_BranchTrack* moduleContext;
    HASH_TABLE_KEY* tableKeyBuffer;
    VOID* tableKeyBufferContext;
    ULONG fileNameLength;
    ULONG branchNameLength;
    ULONG hintNameLength;
    ULONG tableKeyLength;
    CHAR* keyBufferFileName;
    CHAR* keyBufferBranchName;
    CHAR* keyBufferHintName;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(FileName);
    UNREFERENCED_PARAMETER(Line);

    // Compiler warns about this for some reason.
    //
    UNREFERENCED_PARAMETER(CallbackFind);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    fileNameLength = (ULONG)strlen(FileName);
    if (fileNameLength > moduleConfig->MaximumFileNameLength)
    {
        fileNameLength = moduleConfig->MaximumFileNameLength;
    }

    branchNameLength = (ULONG)strlen(BranchName);
    DmfAssert(branchNameLength <= moduleConfig->MaximumBranchNameLength);
    if (branchNameLength > moduleConfig->MaximumBranchNameLength)
    {
        branchNameLength = moduleConfig->MaximumBranchNameLength;
    }

    hintNameLength = (ULONG)strlen(HintName);
    DmfAssert(hintNameLength <= BRANCHTRACK_MAXIMUM_HINT_NAME_LENGTH);
    if (hintNameLength > BRANCHTRACK_MAXIMUM_HINT_NAME_LENGTH)
    {
        hintNameLength = BRANCHTRACK_MAXIMUM_HINT_NAME_LENGTH;
    }

    tableKeyLength = FIELD_OFFSET(HASH_TABLE_KEY, 
                                  RawData[(size_t)fileNameLength + 
                                          (size_t)branchNameLength + 
                                          (size_t)hintNameLength + 
                                          (BRANCHTRACK_NUMBER_OF_STRINGS_IN_RAWDATA * sizeof(CHAR))]);
    tableKeyLength = (tableKeyLength + MAX_NATURAL_ALIGNMENT - 1) & ~(MAX_NATURAL_ALIGNMENT - 1);
    DmfAssert(tableKeyLength <= moduleContext->TableKeyBufferLength);

    tableKeyBuffer = NULL;
    tableKeyBufferContext = NULL;

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfObjectBufferPool,
                                  (VOID**)&tableKeyBuffer,
                                  &tableKeyBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferPool_Get fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Populate tableKeyBuffer and execute HashTable process entry function using this buffer as a key.
    //

    DmfAssert(NULL != tableKeyBuffer);

    RtlZeroMemory(tableKeyBuffer,
                  moduleContext->TableKeyBufferLength);

    tableKeyBuffer->FileNameLength = 0;
    tableKeyBuffer->Line = 0;
    tableKeyBuffer->BranchNameLength = branchNameLength;
    tableKeyBuffer->HintNameLength = hintNameLength;
    tableKeyBuffer->CallbackStatusQuery = CallbackStatusQuery;
    tableKeyBuffer->Context = Context;

    keyBufferFileName = BranchTrack_FileNameBufferGet(tableKeyBuffer);
    RtlCopyMemory(keyBufferFileName,
                  FileName,
                  fileNameLength);

    keyBufferBranchName = BranchTrack_BranchNameBufferGet(tableKeyBuffer);
    RtlCopyMemory(keyBufferBranchName,
                  BranchName,
                  branchNameLength);

    keyBufferHintName = BranchTrack_HintNameBufferGet(tableKeyBuffer);
    RtlCopyMemory(keyBufferHintName,
                  HintName,
                  hintNameLength);

    // Synchronize with calls to query data from HashTable.
    //
    DMF_ModuleLock(DmfModule);
    ntStatus = DMF_HashTable_Find(moduleContext->DmfObjectHashTable,
                                  (UCHAR*)tableKeyBuffer,
                                  tableKeyLength,
                                  CallbackFind);
    DMF_ModuleUnlock(DmfModule);

    DmfAssert(NT_SUCCESS(ntStatus));

    DMF_BufferPool_Put(moduleContext->DmfObjectBufferPool,
                       (VOID**)tableKeyBuffer);

Exit:

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_BranchTrack_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BranchTrack* moduleContext;
    DMF_CONFIG_BranchTrack* moduleConfig;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_HashTable moduleConfigHashTable;
    DMF_CONFIG_BufferPool moduleConfigBufferPool;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // HashTable
    // ---------
    //
    DMF_CONFIG_HashTable_AND_ATTRIBUTES_INIT(&moduleConfigHashTable,
                                             &moduleAttributes);
    // Calculate the size of HASH_TABLE_KEY structure and make sure it's properly aligned.
    // Increase every string parameter length by one, to allocate space for zero-endings.
    //
    moduleConfigHashTable.MaximumKeyLength = FIELD_OFFSET(HASH_TABLE_KEY,
                                                          RawData[(size_t)moduleConfig->MaximumFileNameLength +
                                                                  (size_t)moduleConfig->MaximumBranchNameLength +
                                                                  BRANCHTRACK_MAXIMUM_HINT_NAME_LENGTH +
                                                                  (BRANCHTRACK_NUMBER_OF_STRINGS_IN_RAWDATA * sizeof(CHAR))]);
    moduleConfigHashTable.MaximumKeyLength = (moduleConfigHashTable.MaximumKeyLength + MAX_NATURAL_ALIGNMENT - 1) & ~(MAX_NATURAL_ALIGNMENT - 1);
    moduleConfigHashTable.MaximumValueLength = sizeof(ULONGLONG);
    moduleConfigHashTable.MaximumTableSize = moduleConfig->MaximumBranches;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfObjectHashTable);

    // BufferPool
    // ----------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = BRANCHTRACK_NUMBER_OF_BUFFERS;
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = moduleConfigHashTable.MaximumKeyLength;
    moduleConfigBufferPool.Mode.SourceSettings.BufferContextSize = 0;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfObjectBufferPool);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

_Function_class_(DMF_ModuleDeviceIoControl)
static
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_BranchTrack_ModuleDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    DmfModule - This Module's handle.
    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.
    OutputBufferLength - Length of the request's output buffer,
                         if an output buffer is available.
    InputBufferLength - Length of the request's input buffer,
                        if an input buffer is available.
    IoControlCode - The driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    TRUE if this routine handled the request.

--*/
{
    BOOLEAN handled;
    DMF_CONTEXT_BranchTrack* moduleContext;
    SIZE_T bytesReturned;
    BOOLEAN requestHasNotBeenCompletedOrIsHeld;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    handled = FALSE;
    bytesReturned = 0;
    requestHasNotBeenCompletedOrIsHeld = FALSE;
    ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    switch (IoControlCode)
    {
        case IOCTL_BRANCHTRACK_QUERY_INFORMATION:
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "IOCTL_BRANCHTRACK_QUERY_INFORMATION received.");

            // Always indicate handled, regardless of error.
            //
            handled = TRUE;
            
            ntStatus = BranchTrack_QueryInformation(DmfModule,
                                                    Request,
                                                    &bytesReturned);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,  "BranchTrack_QueryInformation fails: ntStatus=%!STATUS!", ntStatus);
                break;
            }
            
            break;
        }
        default:
        {
            // Don't complete the request. It belongs to another Module.
            //
            requestHasNotBeenCompletedOrIsHeld = TRUE;
            DmfAssert(! handled);
            break;
        }
    }

    if (! requestHasNotBeenCompletedOrIsHeld)
    {
        // Only complete the request (1) it is handled by this Module, (2) has not been completed above and 
        // (3) is not enqueued above.
        // 
        WdfRequestCompleteWithInformation(Request,
                                          ntStatus,
                                          bytesReturned);
    }

    FuncExitVoid(DMF_TRACE);

    return handled;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_BranchTrack_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Create an instance of this DMF Module.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_BranchTrack* moduleContext;
    DMF_CONFIG_BranchTrack* moduleConfig;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = BranchTrack_ConfigInitialize(DmfModule,
                                            moduleContext);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    device = DMF_ParentDeviceGet(DmfModule);

    // Initialize the Client's table.
    //
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    // NOTE: It can be NULL if Client Driver does not use BranchTrack.
    //
    if (moduleConfig->BranchesInitialize != NULL)
    {
        // This context is the BranchTrack DMF Module.
        //
        moduleConfig->BranchesInitialize(DmfModule);
    }

    if (NULL == moduleConfig->SymbolicLinkName)
    {
        // Register a device interface so applications can find and open this device.
        //   
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Create Device Interface");
        ntStatus = WdfDeviceCreateDeviceInterface(device,
                                                  (LPGUID)&GUID_DEVINTERFACE_BranchTrack,
                                                  NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceCreateDeviceInterface fails, ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }
    else
    {
        UNICODE_STRING symbolicLinkName;

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Create Symbolic Link");

        RtlInitUnicodeString(&symbolicLinkName,
                             moduleConfig->SymbolicLinkName);

        // Create a symbolic link for the control object so that user-mode can open
        // the device. Since this is a filter driver and this is a Control Object,
        // we must use a Symbolic Link and cannot use a DeviceInterface. This allows
        // requests to come directly to this object without interference from the 
        // stack that is filtered.
        //
        // NOTE: There is a race condition when the stack is torn down and recreated.
        //       The destruction of symbolic link does not happen instantaneously and 
        //       it is possible for the new stack to start getting created before
        //       old symbolic link has been destroyed. In that case, when a new symbolic
        //       link of same name is created a STATUS_OBJECT_NAME_COLLISION will occur
        //       causing this function to fail...and the filtered device to not be added.
        //       It is a non-recoverable error and the only fix is a reboot.
        //       To work around that, if STATUS_OBJECT_NAME_COLLISION_HAPPENS, then this
        //       function tries again up to 4 times waiting 1 second between each time.
        //       Based on testing, a single retry is all that is necessary.
        //       A good way to exercise this path is do an ERASE followed by a RESET 
        //       quickly, or two resets quickly one after the other.
        //

        ULONG attempts = 0;
        const ULONG maximumAttempts = 4;
        const ULONG waitPeriodMs = 1000;

    TryAgain:

        ntStatus = WdfDeviceCreateSymbolicLink(device,
                                               &symbolicLinkName);
        if (! NT_SUCCESS(ntStatus))
        {
            if (STATUS_OBJECT_NAME_COLLISION == ntStatus)
            {
                // Try again...
                //
                TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "WdfDeviceCreateSymbolicLink ntStatus=%!STATUS! attempts=%d", ntStatus, attempts);
                if (attempts == maximumAttempts)
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Give up on WdfDeviceCreateSymbolicLink. ntStatus=%!STATUS!", ntStatus);
                    goto Exit;
                }

                // Wait one second between each attempt.
                //
                TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Waiting %d ms...", waitPeriodMs);
                DMF_Utility_DelayMilliseconds(waitPeriodMs);

                attempts++;

                TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Try WdfDeviceCreateSymbolicLink again...");
                goto TryAgain;
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceCreateSymbolicLink ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }
    }

Exit:

    if (! NT_SUCCESS(ntStatus))
    {
        BranchTrack_ConfigCleanup(moduleContext);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_BranchTrack_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Close an instance of this DMF Module.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BranchTrack* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    BranchTrack_ConfigCleanup(moduleContext);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BranchTrack_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of this DMF Module.

Arguments:

    Device - Client Driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_BranchTrack;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_BranchTrack;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_BranchTrack;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_BranchTrack);
    dmfCallbacksDmf_BranchTrack.DeviceOpen = DMF_BranchTrack_Open;
    dmfCallbacksDmf_BranchTrack.DeviceClose = DMF_BranchTrack_Close;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_BranchTrack);
    dmfCallbacksDmf_BranchTrack.ChildModulesAdd = DMF_BranchTrack_ChildModulesAdd;
    dmfCallbacksWdf_BranchTrack.ModuleDeviceIoControl = DMF_BranchTrack_ModuleDeviceIoControl;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_BranchTrack,
                                            BranchTrack,
                                            DMF_CONTEXT_BranchTrack,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_BranchTrack.CallbacksDmf = &dmfCallbacksDmf_BranchTrack;
    dmfModuleDescriptor_BranchTrack.CallbacksWdf = &dmfCallbacksWdf_BranchTrack;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_BranchTrack,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BranchTrack_CheckPointExecute(
    _In_opt_ DMFMODULE DmfModule,
    _In_z_ CHAR* BranchName,
    _In_z_ CHAR* HintName,
    _In_z_ CHAR* FileName,
    _In_ ULONG Line,
    _In_ EVT_DMF_BranchTrack_StatusQuery* CallbackStatusQuery,
    _In_ ULONG_PTR Context,
    _In_ BOOLEAN Condition
    )
/*++

Routine Description:

    Adds a custom branch checkpoint to the hash table, specifying a callback to query status.
    If checkpoint already exists there - it's count will be incremented.
    This function should not be used directly, use DMF_BRANCHTRACK_* macros instead.

Arguments:

    DmfModule - This Module's handle.
    BranchName - Name to associate with this branch checkpoint.
    HintName - Name of hint about condition for consumer.
    FileName - Name of a source file.
    Line - Source line number.
    CallbackStatusQuery - callback function to query check point status.
    Context - client's context to associate with this checkpoint.
    Condition - Zero means, do not add the branch. Non-Zero means add the branch.

Return Value:

    None

    --*/
{
    // NOTE: BranchTrack is an exception to the rule in that NULL DMFMODULE may be passed in.
    //       This occurs to support dynamic enable/disable of branch track. If the pointer is NULL
    //       then the function call exits immediately. (This is only allowed for Module Method.)
    //       Even logging is not executed by design.
    //
    if (NULL == DmfModule)
    {
        // NOP.
        //
        goto Exit;
    }

    FuncEntry(DMF_TRACE);

    // NOTE: If the Client Driver tries to use a Module that uses BranchTrack but does not use 
    // DMF_ModuleBranchTrack_DmfModuleInitializationTableCreate() the code will fail. There are currently
    // no drivers that use Modules that use BranchTrack but do not use the proper API. Eventually, all drivers will
    // use the DMF_ModuleBranchTrack_DmfModuleInitializationTableCreate() and the legacy API will be removed.
    //

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BranchTrack);

    if (Condition)
    {
        BranchTrack_CheckPointProcess(DmfModule,
                                      BranchName,
                                      HintName,
                                      FileName,
                                      Line,
                                      CallbackStatusQuery,
                                      Context,
                                      BranchTrack_EVT_DMF_HashTable_Find);
    }

    FuncExitVoid(DMF_TRACE);

Exit:
    ;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BranchTrack_CheckPointCreate(
    _In_opt_ DMFMODULE DmfModule,
    _In_z_ CHAR* BranchName,
    _In_z_ CHAR* HintName,
    _In_z_ CHAR* FileName,
    _In_ ULONG Line,
    _In_ EVT_DMF_BranchTrack_StatusQuery* CallbackStatusQuery,
    _In_ ULONG_PTR Context,
    _In_ BOOLEAN Condition
    )
/*++

Routine Description:

    Adds a custom branch checkpoint to the hash table, specifying a callback to query status.
    If checkpoint already exists there - it's count will be incremented.
    This function should not be used directly, use DMF_BRANCHTRACK_* macros instead.

Arguments:

    DmfModule - This Module's handle.
    BranchName - Name to associate with this branch checkpoint.
    HintName - Name of hint about condition for consumer.
    FileName - Name of a source file.
    Line - Source line number.
    CallbackStatusQuery - callback function to query check point status.
    Context - client's context to associate with this checkpoint.
    Condition - Zero means, do not add the branch. Non-Zero means add the branch.

Return Value:

    None

    --*/
{
    // NOTE: BranchTrack is an exception to the rule in that NULL DMFMODULE may be passed in.
    //       This occurs to support dynamic enable/disable of branch track. If the pointer is NULL
    //       then the function call exits immediately. (This is only allowed for Module Method.)
    //       Even logging is not executed by design.
    //
    if (NULL == DmfModule)
    {
        // NOP.
        //
        goto Exit;
    }

    FuncEntry(DMF_TRACE);

    // Open handler will call a callback in the Client that will call this function.
    // It is expected and correct.
    //
    DMFMODULE_VALIDATE_IN_METHOD_OPENING_OK(DmfModule,
                                            BranchTrack);

    if (Condition)
    {
        BranchTrack_CheckPointProcess(DmfModule,
                                      BranchName,
                                      HintName,
                                      FileName,
                                      Line,
                                      CallbackStatusQuery,
                                      Context,
                                      BranchTrack_HashTable_CallbackEntryCreate);
    }

    FuncExitVoid(DMF_TRACE);

Exit:
    ;
}

// Helper functions that are defined by this Module that are callbacks for processing BranchTrack
// records. The Client may also define their own callbacks in their own code.
// NOTE: These are not Module Methods because no DMF Module is passed.
//

_Function_class_(EVT_DMF_BranchTrack_StatusQuery)
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_BranchTrack_Helper_BranchStatusQuery_Count(
    _In_ DMFMODULE DmfModule,
    _In_z_ CHAR* BranchName,
    _In_ ULONG_PTR BranchContext,
    _In_ ULONGLONG Count
    )
/*++

Routine Description:

    Predefined callback to query check point status to check execution
    a given number of times.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    BranchName - Name of this branch.
    BranchContext - Context parameter passed during this branch checkpoint declaration.
    Count - Number of times the branch was executed.

Return Value:

    TRUE if branch is passed, otherwise FALSE.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(BranchName);

    return (Count == (ULONGLONG)BranchContext);
}

_Function_class_(EVT_DMF_BranchTrack_StatusQuery)
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_BranchTrack_Helper_BranchStatusQuery_MoreThan(
    _In_ DMFMODULE DmfModule,
    _In_z_ CHAR* BranchName,
    _In_ ULONG_PTR BranchContext,
    _In_ ULONGLONG Count
    )
/*++

Routine Description:

    Predefined callback to query check point status to check execution more than
    a given number of times.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    BranchName - Name of this branch.
    BranchContext - Context parameter passed during this branch checkpoint declaration.
    Count - Number of times the branch was executed.

Return Value:

    TRUE if branch is passed, otherwise FALSE.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(BranchName);

    return  (Count > (ULONGLONG)BranchContext);
}

_Function_class_(EVT_DMF_BranchTrack_StatusQuery)
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_BranchTrack_Helper_BranchStatusQuery_LessThan(
    _In_ DMFMODULE DmfModule,
    _In_z_ CHAR* BranchName,
    _In_ ULONG_PTR BranchContext,
    _In_ ULONGLONG Count
    )
/*++

Routine Description:

    Predefined callback to query check point status to check execution at less than
    a given number of times.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    BranchName - Name of this branch.
    BranchContext - Context parameter passed during this branch checkpoint declaration.
    Count - Number of times the branch was executed.

Return Value:

    TRUE if branch is passed, otherwise FALSE.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(BranchName);

    return  (Count < (ULONGLONG)BranchContext);
}

_Function_class_(EVT_DMF_BranchTrack_StatusQuery)
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast(
    _In_ DMFMODULE DmfModule,
    _In_z_ CHAR* BranchName,
    _In_ ULONG_PTR BranchContext,
    _In_ ULONGLONG Count
    )
/*++

Routine Description:

    Predefined callback to query check point status to check execution at least
    a given number of times.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    BranchName - Name of this branch.
    BranchContext - Context parameter passed during this branch checkpoint declaration.
    Count - Number of times the branch was executed.

Return Value:

    TRUE if branch is passed, otherwise FALSE.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(BranchName);

    return  (Count >= (ULONGLONG)BranchContext);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_BranchTrack_CONFIG_INIT(
    _Out_ DMF_CONFIG_BranchTrack* ModuleConfig,
    _In_z_ CHAR* ClientName
    )
/*++

Routine Description:

    Initialize this Module's Module Config. The values are generally acceptable for most drivers.
    If the Client wishes to, Client may override.
    Release builds always initialize to zero to disable BranchTrack.

Arguments:

    ModuleConfig - This Module's Module Config to initialize.

Return Value:

    TRUE if branch is passed, otherwise FALSE.

--*/
{
    RtlZeroMemory(ModuleConfig,
                  sizeof(DMF_CONFIG_BranchTrack));
    size_t bytesToCopy;
    size_t clientNameLengthBytes;

    clientNameLengthBytes = strlen(ClientName);
    if (clientNameLengthBytes <= BRANCH_TRACK_CLIENT_NAME_MAXIMUM_LENGTH)
    {
        bytesToCopy = clientNameLengthBytes;
    }
    else
    {
        // Name is longer than expected.
        //
        DmfAssert(FALSE);
        bytesToCopy = BRANCH_TRACK_CLIENT_NAME_MAXIMUM_LENGTH;
    }
    // This is the name to use in BranchTrackReader.exe.
    //
    RtlCopyMemory(ModuleConfig->ClientName,
                  ClientName,
                  bytesToCopy);

    ModuleConfig->MaximumBranches = BRANCHTRACK_DEFAULT_MAXIMUM_BRANCHES;
    ModuleConfig->MaximumBranchNameLength = BRANCHTRACK_DEFAULT_MAXIMUM_BRANCH_NAME_LENGTH;
}

// eof: Dmf_BranchTrack.c
//
