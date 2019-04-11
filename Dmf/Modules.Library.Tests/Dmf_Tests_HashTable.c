/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_HashTable.c

Abstract:

    Functional tests for Dmf_HashTable Module

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_HashTable.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define KEY_SIZE                    (32)
#define BUFFER_SIZE                 (32)
#define BUFFER_COUNT_PREALLOCATED   (16)
#define BUFFER_COUNT_MAXIMUM        (24)
#define THREAD_COUNT                (4)

// It is a table of data that is automatically generated. This data is
// then written to the hash table. Then, this table is used to find 
// entries in the hash table.
//
typedef struct
{
    UCHAR Key[KEY_SIZE];
    ULONG KeySize;
    UCHAR Buffer[BUFFER_SIZE];
    ULONG BufferSize;
} HashTable_DataRecord;

typedef enum _TEST_ACTION
{
    TEST_ACTION_READSUCCESS,
    TEST_ACTION_READFAIL,
    TEST_ACTION_ENUMERATE,
    TEST_ACTION_COUNT,
    TEST_ACTION_MINIUM      = TEST_ACTION_READSUCCESS,
    TEST_ACTION_MAXIMUM     = TEST_ACTION_ENUMERATE
} TEST_ACTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Automatically generated data that is used for tests.
    //
    HashTable_DataRecord DataRecords[BUFFER_COUNT_MAXIMUM];
    // HashTable Module to test using default hash function.
    //
    DMFMODULE DmfModuleHashTableDefault;
    // HashTable Module to test using custom hash function.
    //
    DMFMODULE DmfModuleHashTableCustom; 
    // Work threads that perform actions on the HashTable Module.
    //
    DMFMODULE DmfModuleThread[THREAD_COUNT];
} DMF_CONTEXT_Tests_HashTable;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_HashTable)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_HashTable)

// Memory Pool Tag.
//
#define MemoryTag 'TaHT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(EVT_DMF_HashTable_HashCalculate)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ULONG_PTR
HashTable_HashCalculate(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength)
{
    ULONG characterIndex;
    ULONG_PTR hashResult;

    UNREFERENCED_PARAMETER(DmfModule);

    hashResult = 0;
    for (characterIndex = 0; characterIndex < KeyLength; characterIndex++)
    {
        hashResult += Key[characterIndex];
    }

    return hashResult;
}

static
VOID
Tests_HashTable_DataGenerate(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_HashTable* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Generate the table with random data.
    //
    for (ULONG recordIndex = 0; recordIndex < BUFFER_COUNT_MAXIMUM; recordIndex++)
    {
        HashTable_DataRecord* dataRecord = &moduleContext->DataRecords[recordIndex];

        dataRecord->KeySize = TestsUtility_GenerateRandomNumber(1,
                                                                KEY_SIZE);
        dataRecord->BufferSize = TestsUtility_GenerateRandomNumber(1,
                                                                   BUFFER_SIZE);

        TestsUtility_FillWithSequentialData(dataRecord->Key,
                                            dataRecord->KeySize);
       
        TestsUtility_FillWithSequentialData(dataRecord->Buffer,
                                            dataRecord->BufferSize);
    }
}

static
VOID
Tests_HashTable_Populate(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleHashTable
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Tests_HashTable* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Populate the hash table.
    //
    for (ULONG recordIndex = 0; recordIndex < BUFFER_COUNT_MAXIMUM; recordIndex++)
    {
        HashTable_DataRecord* dataRecord = &moduleContext->DataRecords[recordIndex];

        ntStatus = DMF_HashTable_Write(DmfModuleHashTable,
                                       dataRecord->Key,
                                       dataRecord->KeySize,
                                       dataRecord->Buffer,
                                       dataRecord->BufferSize);
    }
}

INT
Tests_HashTable_DataRecordsSearch(
    _In_ HashTable_DataRecord* DataRecords,
    _In_ UCHAR* Key,
    _In_ ULONG KeySize
    )
{
    INT returnValue;
    HashTable_DataRecord* dataRecord;

    returnValue = -1;

    for (ULONG recordIndex = 0; recordIndex < BUFFER_COUNT_MAXIMUM; recordIndex++)
    {
        dataRecord = &DataRecords[recordIndex];

        if (dataRecord->KeySize == KeySize)
        {
            if (RtlCompareMemory(dataRecord->Key,
                                 Key,
                                 KeySize) == KeySize)
            {
                returnValue = recordIndex;
                break;
            }
        }
    }

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
HashTable_Find(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _Inout_updates_to_(*ValueLength, *ValueLength) UCHAR* Value,
    _Inout_ ULONG* ValueLength
    )
{
    DMFMODULE DmfModuleParent;
    DMF_CONTEXT_Tests_HashTable* moduleContext;

    DmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModuleParent);

    INT foundRecordIndex = Tests_HashTable_DataRecordsSearch(moduleContext->DataRecords,
                                                             Key,
                                                             KeyLength);
    ASSERT(foundRecordIndex >= 0);

    if ((*ValueLength == moduleContext->DataRecords[foundRecordIndex].BufferSize) &&
        RtlCompareMemory(Value,
                         moduleContext->DataRecords[foundRecordIndex].Buffer,
                         *ValueLength) == *ValueLength)
    {
        // Success.
        //
    }
    else
    {
        // Data does not match.
        //
        ASSERT(FALSE);
    }
}

#pragma code_seg("PAGE")
static
void
Tests_HashTable_ThreadAction_ReadSuccess(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_HashTable* moduleContext;
    NTSTATUS ntStatus;
    HashTable_DataRecord* dataRecord;
    UCHAR valueBuffer[BUFFER_SIZE];
    ULONG valueSize = sizeof(valueBuffer);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Generate a random index and look for the corresponding record in both hash tables.
    //
    ULONG recordIndex = TestsUtility_GenerateRandomNumber(0,
                                                          BUFFER_COUNT_MAXIMUM - 1);

    dataRecord = &moduleContext->DataRecords[recordIndex];

    ntStatus = DMF_HashTable_Read(moduleContext->DmfModuleHashTableDefault,
                                  dataRecord->Key,
                                  dataRecord->KeySize,
                                  valueBuffer,
                                  valueSize,
                                  &valueSize);
    ASSERT(NT_SUCCESS(ntStatus));
    ASSERT(valueSize == dataRecord->BufferSize);
    ASSERT(RtlCompareMemory(valueBuffer,
                            dataRecord->Buffer,
                            valueSize));

    valueSize = sizeof(valueBuffer);
    ntStatus = DMF_HashTable_Read(moduleContext->DmfModuleHashTableCustom,
                                  dataRecord->Key,
                                  dataRecord->KeySize,
                                  valueBuffer,
                                  valueSize,
                                  &valueSize);
    ASSERT(NT_SUCCESS(ntStatus));
    ASSERT(valueSize == dataRecord->BufferSize);
    ASSERT(RtlCompareMemory(valueBuffer,
                            dataRecord->Buffer,
                            valueSize));

    ntStatus = DMF_HashTable_Find(moduleContext->DmfModuleHashTableDefault,
                                  dataRecord->Key,
                                  dataRecord->KeySize,
                                  HashTable_Find);
    ASSERT(NT_SUCCESS(ntStatus));
    ASSERT(valueSize == dataRecord->BufferSize);
    ASSERT(RtlCompareMemory(valueBuffer,
                            dataRecord->Buffer,
                            valueSize));

    ntStatus = DMF_HashTable_Find(moduleContext->DmfModuleHashTableCustom,
                                  dataRecord->Key,
                                  dataRecord->KeySize,
                                  HashTable_Find);
    ASSERT(NT_SUCCESS(ntStatus));
    ASSERT(valueSize == dataRecord->BufferSize);
    ASSERT(RtlCompareMemory(valueBuffer,
                            dataRecord->Buffer,
                            valueSize));
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_HashTable_ThreadAction_ReadFail(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_HashTable* moduleContext;
    NTSTATUS ntStatus;
    UCHAR keyNotFound[BUFFER_SIZE];
    ULONG keyNotFoundSize;
    UCHAR valueBuffer[BUFFER_SIZE];
    ULONG valueSize = sizeof(valueBuffer);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

TryAgain:

    keyNotFoundSize = TestsUtility_GenerateRandomNumber(0,
                                                        BUFFER_SIZE);

    TestsUtility_FillWithSequentialData(keyNotFound,
                                        keyNotFoundSize);

    // Make sure this key is not found in the hash table.
    //
    INT foundRecordIndex = Tests_HashTable_DataRecordsSearch(moduleContext->DataRecords,
                                                             keyNotFound,
                                                             keyNotFoundSize);
    if (foundRecordIndex >= 0)
    {
        // Found. Generate another one and try again.
        //
        goto TryAgain;
    }

    valueSize = sizeof(valueBuffer);
    ntStatus = DMF_HashTable_Read(moduleContext->DmfModuleHashTableDefault,
                                  keyNotFound,
                                  keyNotFoundSize,
                                  valueBuffer,
                                  valueSize,
                                  &valueSize);
    ASSERT(! NT_SUCCESS(ntStatus));

    valueSize = sizeof(valueBuffer);
    ntStatus = DMF_HashTable_Read(moduleContext->DmfModuleHashTableCustom,
                                  keyNotFound,
                                  keyNotFoundSize,
                                  valueBuffer,
                                  valueSize,
                                  &valueSize);
    ASSERT(! NT_SUCCESS(ntStatus));
}
#pragma code_seg()

_Function_class_(EVT_DMF_HashTable_Enumerate)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
HashTable_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _In_reads_(ValueLength) UCHAR* Value,
    _In_ ULONG ValueLength,
    _In_ VOID* CallbackContext
    )
{
    DMFMODULE DmfModuleParent;
    DMF_CONTEXT_Tests_HashTable* moduleContext;

    DmfModuleParent = DMF_ParentModuleGet(DmfModule);
    if (CallbackContext != DmfModuleParent)
    {
        ASSERT(FALSE);
    }

    moduleContext = DMF_CONTEXT_GET(DmfModuleParent);

    INT foundRecordIndex = Tests_HashTable_DataRecordsSearch(moduleContext->DataRecords,
                                                             Key,
                                                             KeyLength);
    if (foundRecordIndex < 0)
    {
        // Not found. Error.
        //
        ASSERT(FALSE);
    }
    else
    {
        if ((ValueLength == moduleContext->DataRecords[foundRecordIndex].BufferSize) &&
            RtlCompareMemory(Value,
                             moduleContext->DataRecords[foundRecordIndex].Buffer,
                             ValueLength) == ValueLength)
        {
            // Success.
            //
        }
        else
        {
            // Not found. Error.
            //
            ASSERT(FALSE);
        }
    }

    return TRUE;
}

#pragma code_seg("PAGE")
static
void
Tests_HashTable_ThreadAction_Enumerate(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_HashTable* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_HashTable_Enumerate(moduleContext->DmfModuleHashTableDefault,
                            HashTable_Enumerate,
                            DmfModule);

    DMF_HashTable_Enumerate(moduleContext->DmfModuleHashTableCustom,
                            HashTable_Enumerate,
                            DmfModule);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_HashTable_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_HashTable* moduleContext;
    TEST_ACTION testAction;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);
    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_READSUCCESS:
            Tests_HashTable_ThreadAction_ReadSuccess(dmfModule);
            break;
        case TEST_ACTION_READFAIL:
            Tests_HashTable_ThreadAction_ReadFail(dmfModule);
            break;
        case TEST_ACTION_ENUMERATE:
            Tests_HashTable_ThreadAction_Enumerate(dmfModule);
            break;
        default:
            ASSERT(FALSE);
            break;
    }

    // Repeat the test, until stop is signaled.
    //
    if (!DMF_Thread_IsStopPending(DmfModuleThread))
    {
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
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
Tests_HashTable_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_HashTable.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_HashTable* moduleContext;
    NTSTATUS ntStatus;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // Generate random data used for the test.
    //
    Tests_HashTable_DataGenerate(DmfModule);

    // Write known entries into the hash table. These will be read and enumerated.
    // This tests the Write API.
    //
    Tests_HashTable_Populate(DmfModule,
                             moduleContext->DmfModuleHashTableDefault);
    Tests_HashTable_Populate(DmfModule,
                             moduleContext->DmfModuleHashTableCustom);

    // Create threads that read with expected success, read with expected failure
    // and enumerate.
    //
    for (index = 0; index < THREAD_COUNT; index++)
    {
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread[index]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThread[index]);
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
Tests_HashTable_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_HashTable.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_HashTable* moduleContext;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_Stop(moduleContext->DmfModuleThread[index]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_HashTable_ChildModulesAdd(
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
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONTEXT_Tests_HashTable* moduleContext;
    DMF_CONFIG_HashTable moduleConfigHashTable;
    DMF_CONFIG_Thread moduleConfigThread;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // HashTable (Default hash function)
    // ---------------------------------
    //
    DMF_CONFIG_HashTable_AND_ATTRIBUTES_INIT(&moduleConfigHashTable,
                                              &moduleAttributes);
    moduleAttributes.ClientModuleInstanceName = "HastTable.Default";
    moduleConfigHashTable.MaximumTableSize = BUFFER_COUNT_MAXIMUM;
    moduleConfigHashTable.MaximumValueLength = BUFFER_SIZE;
    moduleConfigHashTable.MaximumKeyLength = KEY_SIZE;
    moduleConfigHashTable.EvtHashTableHashCalculate = NULL;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleHashTableDefault);

    // HashTable (Custom hash function)
    // ---------------------------------
    //
    DMF_CONFIG_HashTable_AND_ATTRIBUTES_INIT(&moduleConfigHashTable,
                                              &moduleAttributes);
    moduleAttributes.ClientModuleInstanceName = "HastTable.Custom";
    moduleConfigHashTable.MaximumTableSize = BUFFER_COUNT_MAXIMUM;
    moduleConfigHashTable.MaximumValueLength = BUFFER_SIZE;
    moduleConfigHashTable.MaximumKeyLength = KEY_SIZE;
    moduleConfigHashTable.EvtHashTableHashCalculate = HashTable_HashCalculate;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleHashTableCustom);

    // Thread
    // ------
    //
    for (ULONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_HashTable_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThread[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Tests_HashTable;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Tests_HashTable;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Tests_HashTable_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_HashTable.

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

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Tests_HashTable);
    DmfCallbacksDmf_Tests_HashTable.ChildModulesAdd = DMF_Tests_HashTable_ChildModulesAdd;
    DmfCallbacksDmf_Tests_HashTable.DeviceOpen = Tests_HashTable_Open;
    DmfCallbacksDmf_Tests_HashTable.DeviceClose = Tests_HashTable_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Tests_HashTable,
                                            Tests_HashTable,
                                            DMF_CONTEXT_Tests_HashTable,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_Tests_HashTable.CallbacksDmf = &DmfCallbacksDmf_Tests_HashTable;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Tests_HashTable,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_Tests_HashTable.c
//
