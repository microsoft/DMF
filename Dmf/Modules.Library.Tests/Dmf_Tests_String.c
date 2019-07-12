/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_String.c

Abstract:

    Functional tests for Dmf_String Module

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_String.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Thread that executes tests.
    //
    DMFMODULE DmfModuleThread;
    // The Module being tested.
    //
    DMFMODULE DmfModuleString;
} DMF_CONTEXT_Tests_String;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_String)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_String)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
static
VOID
Tests_String_CharacterConversions(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Performs unit tests on the Methods.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    CHAR* narrowStrings[] =
    {
        "",
        "a",
        "ab",
        "abcdedfghijklmnopqrstuvwxyz0123456789"
    };
    WCHAR* wideStrings[] =
    {
        L"",
        L"a",
        L"ab",
        L"abcdedfghijklmnopqrstuvwxyz0123456789"
    };
    DECLARE_CONST_UNICODE_STRING(u0, L"");
    DECLARE_CONST_UNICODE_STRING(u1, L"a");
    DECLARE_CONST_UNICODE_STRING(u2, L"ab");
    DECLARE_CONST_UNICODE_STRING(u3, L"abcdedfghijklmnopqrstuvwxyz0123456789");
    UNICODE_STRING unicodeStrings[] = { u0, u1, u2, u3 };
    #define STRING_LENGTH_SMALL     1
    #define STRING_LENGTH_BIG       64

    for (ULONG stringIndex = 0; stringIndex < ARRAYSIZE(narrowStrings); stringIndex++)
    {
        CHAR narrowBufferBig[STRING_LENGTH_BIG];
        CHAR narrowBufferSmall[STRING_LENGTH_SMALL];
        WCHAR wideBufferBig[STRING_LENGTH_BIG];
        WCHAR wideBufferSmall[STRING_LENGTH_SMALL];
        ANSI_STRING ansiString;
        UNICODE_STRING unicodeString;

        // Test DMF_String_WideStringCopyAsNarrow
        // --------------------------------------
        //

        // Expected Success.
        //
        ntStatus = DMF_String_WideStringCopyAsNarrow(DmfModule,
                                                     narrowBufferBig,
                                                     ARRAYSIZE(narrowBufferBig),
                                                     wideStrings[stringIndex]);
        ASSERT(NT_SUCCESS(ntStatus));
        ASSERT(strncmp(narrowBufferBig,
                       narrowStrings[stringIndex],
                       strlen(narrowStrings[stringIndex])) == 0);

        // Expected Failure.
        //
        ntStatus = DMF_String_WideStringCopyAsNarrow(DmfModule,
                                                     narrowBufferSmall,
                                                     ARRAYSIZE(narrowBufferSmall),
                                                     wideStrings[stringIndex]);
        ASSERT(! NT_SUCCESS(ntStatus) || 
               (wcslen(wideStrings[stringIndex]) == 0));

        // Test DMF_String_RtlUnicodeStringToAnsiString
        // --------------------------------------------
        //

        // Expected Success.
        //
        RtlZeroMemory(narrowBufferBig,
                      sizeof(narrowBufferBig));
        RtlInitAnsiString(&ansiString,
                          narrowBufferBig);
        ansiString.MaximumLength = sizeof(narrowBufferBig);
        RtlInitUnicodeString(&unicodeString,
                             wideStrings[stringIndex]);
        ntStatus = DMF_String_RtlUnicodeStringToAnsiString(DmfModule,
                                                           &ansiString,
                                                           &unicodeString);
        ASSERT(NT_SUCCESS(ntStatus));
        ASSERT(strncmp(ansiString.Buffer,
                       narrowStrings[stringIndex],
                       strlen(narrowStrings[stringIndex])) == 0);

        // Expected Success.
        //
        RtlZeroMemory(narrowBufferBig,
                      sizeof(narrowBufferBig));
        RtlInitAnsiString(&ansiString,
                          narrowBufferBig);
        ansiString.MaximumLength = sizeof(narrowBufferBig);
        ntStatus = DMF_String_RtlUnicodeStringToAnsiString(DmfModule,
                                                           &ansiString,
                                                           &unicodeStrings[stringIndex]);
        ASSERT(NT_SUCCESS(ntStatus));
        ASSERT(strncmp(ansiString.Buffer,
                       narrowStrings[stringIndex],
                       strlen(narrowStrings[stringIndex])) == 0);

        // Expected Failure.
        //
        RtlZeroMemory(narrowBufferSmall,
                      sizeof(narrowBufferSmall));
        RtlInitAnsiString(&ansiString,
                          narrowBufferSmall);
        ansiString.MaximumLength = sizeof(narrowBufferSmall);
        RtlInitUnicodeString(&unicodeString,
                             wideStrings[stringIndex]);
        ntStatus = DMF_String_RtlUnicodeStringToAnsiString(DmfModule,
                                                           &ansiString,
                                                           &unicodeString);
        ASSERT(!NT_SUCCESS(ntStatus) || 
               (unicodeString.Length == 0));

        // Expected Failure.
        //
        RtlZeroMemory(narrowBufferSmall,
                      sizeof(narrowBufferSmall));
        RtlInitAnsiString(&ansiString,
                          narrowBufferSmall);
        ansiString.MaximumLength = sizeof(narrowBufferSmall);
        RtlInitUnicodeString(&unicodeString,
                             wideStrings[stringIndex]);
        ntStatus = DMF_String_RtlUnicodeStringToAnsiString(DmfModule,
                                                           &ansiString,
                                                           &unicodeStrings[stringIndex]);
        ASSERT(!NT_SUCCESS(ntStatus) || 
               (unicodeStrings[stringIndex].Length == 0));

        // Test DMF_String_RtlAnsiStringToUnicodeString
        // --------------------------------------------
        //

        // Expected Success.
        //
        RtlZeroMemory(wideBufferBig,
                      sizeof(wideBufferBig));
        RtlInitUnicodeString(&unicodeString,
                             wideBufferBig);
        unicodeString.MaximumLength = sizeof(wideBufferBig);
        RtlInitAnsiString(&ansiString,
                          narrowStrings[stringIndex]);
        ntStatus = DMF_String_RtlAnsiStringToUnicodeString(DmfModule,
                                                           &unicodeString,
                                                           &ansiString);
        ASSERT(NT_SUCCESS(ntStatus));
        ASSERT(wcsncmp(unicodeString.Buffer,
                       wideStrings[stringIndex],
                       wcslen(wideStrings[stringIndex])) == 0);

        // Expected Failure.
        //
        RtlZeroMemory(wideBufferSmall,
                      sizeof(wideBufferSmall));
        RtlInitUnicodeString(&unicodeString,
                             wideBufferSmall);
        unicodeString.MaximumLength = sizeof(wideBufferSmall);
        RtlInitAnsiString(&ansiString,
                          narrowStrings[stringIndex]);
        ntStatus = DMF_String_RtlAnsiStringToUnicodeString(DmfModule,
                                                           &unicodeString,
                                                           &ansiString);
        ASSERT(! NT_SUCCESS(ntStatus) ||
               (wcslen(wideStrings[stringIndex]) == 0));
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_String_TableLookups(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Performs unit tests on the Methods.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    LONG result;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    CHAR* emptyTable[] =
    {
        NULL,
    };
    CHAR* stringsTable0[] = 
    {
        "",
        "a",
        "ab",
        "abc",
        "abcd",
        "abcde",
        "abcdef",
        "abcdefg",
        "abcdefgh",
    };
    // No strings in Table1 should be in Table 0.
    //
    CHAR* stringsTable1[] = 
    {
        "s",
        "st",
        "stu",
        "stuv",
        "stuvw",
        "stuvwx",
        "stuvwxy",
        "stuvwxyz",
    };
    CHAR* stringsTable2[] = 
    {
        "qwer",         // 0
        "asdf",         // 1
        "zxcv",         // 2
        "t",            // 3
        "fg",           // 4
        "vb",           // 5
        "poiulkhj",     // 6
        "mnbvcxz",      // 7
        "",             // 8
    };
    CHAR* stringsTable3[] = 
    {
        "mnb",
        "v",
        "poiulkjhqwer",
        "poiulkhj",
         "",
        "zxcvzxcv",
        "_@"
    };
    LONG answersTable3[] = 
    {
        7,
        5,
        -1,
        6,
        8,
        -1,
        -1
    };
    LONG answersTable4[] = 
    {
        -1,
        -1,
        -1,
        6,
        8,
        -1,
        -1
    };
    // {C1308310-8B25-47DA-9083-3C0102DAE19B}
    GUID guid0 = { 0xc1308310, 0x8b25, 0x47da, { 0x90, 0x83, 0x3c, 0x1, 0x2, 0xda, 0xe1, 0x9b } };
    // {61ED94BD-6AD4-4B61-B9D1-7B18F14BF9F6}
    GUID guid1 =  { 0x61ed94bd, 0x6ad4, 0x4b61, { 0xb9, 0xd1, 0x7b, 0x18, 0xf1, 0x4b, 0xf9, 0xf6 } };
    // {C323BE51-6E7A-4643-B4DD-A0E8EFE7488C}
    GUID guid2 = { 0xc323be51, 0x6e7a, 0x4643, { 0xb4, 0xdd, 0xa0, 0xe8, 0xef, 0xe7, 0x48, 0x8c } };
    // {4A9D6030-6966-411B-81E7-CBE8061CB475}
    GUID guid3 = { 0x4a9d6030, 0x6966, 0x411b, { 0x81, 0xe7, 0xcb, 0xe8, 0x6, 0x1c, 0xb4, 0x75 } };
    // {744F2CF2-B514-4147-B050-FDF7DE8B1761}
    GUID guid4 = { 0x744f2cf2, 0xb514, 0x4147, { 0xb0, 0x50, 0xfd, 0xf7, 0xde, 0x8b, 0x17, 0x61 } };
    // {8A166351-B60E-45E9-AB49-FD5DBB71FB39}
    GUID guid5 = { 0x8a166351, 0xb60e, 0x45e9, { 0xab, 0x49, 0xfd, 0x5d, 0xbb, 0x71, 0xfb, 0x39 } };
    GUID guidsTable[] =
    {
        guid0,
        guid1,
        guid2,
        guid3,
        guid4
    };
    GUID guidsTableEmpty[] =
    {
        guid0,
    };
    CHAR* stringsTable5[] = 
    {
        "abc123",
    };
    CHAR* stringsTable6[] = 
    {
        "a",
        "ab",
        "abc",
        "abc1",
        "abc123",
        "abc123456",
    };

    // Look for strings in an empty table. None should be found.
    //
    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable0); stringIndex++)
    {
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           emptyTable,
                                                           0,
                                                           stringsTable0[stringIndex]);
        ASSERT(result == -1);

        result = DMF_String_FindInListExactChar(DmfModule,
                                                emptyTable,
                                                0,
                                                stringsTable0[stringIndex]);
        ASSERT(result == -1);
    }

    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable0); stringIndex++)
    {
        // Look for strings from a table that is the same as the table being searched.
        // They should all be found. (Left comparison).
        //
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           stringsTable0,
                                                           ARRAYSIZE(stringsTable0),
                                                           stringsTable0[stringIndex]);
        ASSERT(result == stringIndex);

        // Look for strings that are not present in the table being searched.
        // None should be found. (Left comparison).
        //
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           stringsTable1,
                                                           ARRAYSIZE(stringsTable1),
                                                           stringsTable0[stringIndex]);
        ASSERT(result == -1);
    }

    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable1); stringIndex++)
    {
        // Look for strings from a table that is the same as the table being searched.
        // They should all be found. (Left comparison).
        //
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           stringsTable1,
                                                           ARRAYSIZE(stringsTable1),
                                                           stringsTable1[stringIndex]);
        ASSERT(result == stringIndex);

        // Look for strings that are not present in the table being searched.
        // None should be found. (Left comparison).
        //
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           stringsTable0,
                                                           ARRAYSIZE(stringsTable0),
                                                           stringsTable1[stringIndex]);
        ASSERT(result == -1);
    }

    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable0); stringIndex++)
    {
        // Look for strings from a table that is the same as the table being searched.
        // They should all be found. (Full comparison).
        //
        result = DMF_String_FindInListExactChar(DmfModule,
                                                stringsTable0,
                                                ARRAYSIZE(stringsTable0),
                                                stringsTable0[stringIndex]);
        ASSERT(result == stringIndex);

        // Look for strings that are not present in the table being searched.
        // None should be found. (Full comparison).
        //
        result = DMF_String_FindInListExactChar(DmfModule,
                                                stringsTable1,
                                                ARRAYSIZE(stringsTable1),
                                                stringsTable0[stringIndex]);
        ASSERT(result == -1);
    }

    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable3); stringIndex++)
    {
        // Look for strings that should be found using left comparison.
        //
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           stringsTable2,
                                                           ARRAYSIZE(stringsTable2),
                                                           stringsTable3[stringIndex]);
        ASSERT(result == answersTable3[stringIndex]);
    }

    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable3); stringIndex++)
    {
        // Look for strings that should be found using exact comparison.
        //
        result = DMF_String_FindInListExactChar(DmfModule,
                                                stringsTable2,
                                                ARRAYSIZE(stringsTable2),
                                                stringsTable3[stringIndex]);
        ASSERT(result == answersTable4[stringIndex]);
    }

    // Verify that if left sides of EITHER string match, result is FOUND.
    //
    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable6); stringIndex++)
    {
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           stringsTable5,
                                                           ARRAYSIZE(stringsTable5),
                                                           stringsTable6[stringIndex]);
        // Only the last string should fail.
        //
        ASSERT((result >= 0) ||
               (stringIndex == 5));
    }

    // Verify that if left sides of either string match, result is FOUND.
    //
    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable5); stringIndex++)
    {
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           stringsTable6,
                                                           ARRAYSIZE(stringsTable6),
                                                           stringsTable5[stringIndex]);
        // String only matches the last two records.
        //
        ASSERT((result == -1) || (result == 4) || (result == 5));
    }

    // Verify that if left sides of either string don't match, result is not FOUND.
    //
    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable5); stringIndex++)
    {
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           stringsTable1,
                                                           ARRAYSIZE(stringsTable1),
                                                           stringsTable5[stringIndex]);
        ASSERT(result == -1);
    }

    // Verify that if left sides of either string don't match, result is not FOUND.
    //
    for (LONG stringIndex = 0; stringIndex < ARRAYSIZE(stringsTable1); stringIndex++)
    {
        result = DMF_String_FindInListLookForLeftMatchChar(DmfModule,
                                                           stringsTable5,
                                                           ARRAYSIZE(stringsTable5),
                                                           stringsTable1[stringIndex]);
        ASSERT(result == -1);
    }

    // Search a table of GUIDs for all its own entries.
    // They should all be found.
    //
    for (LONG guidIndex = 0; guidIndex < ARRAYSIZE(guidsTable); guidIndex++)
    {
        result = DMF_String_FindInListExactGuid(DmfModule,
                                                guidsTable,
                                                ARRAYSIZE(guidsTable),
                                                &guidsTable[guidIndex]);
        ASSERT(result == guidIndex);
    }

    // Search a table of GUIDs for a GUID that should not be found.
    //
    result = DMF_String_FindInListExactGuid(DmfModule,
                                            guidsTable,
                                            ARRAYSIZE(guidsTable),
                                            &guid5);
    ASSERT(-1 == result);

    // Search an empty table of GUIDs.
    //
    result = DMF_String_FindInListExactGuid(DmfModule,
                                            guidsTableEmpty,
                                            0,
                                            &guid5);
    ASSERT(-1 == result);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_String_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_String* moduleContext;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Run the character conversion tests.
    //
    Tests_String_CharacterConversions(moduleContext->DmfModuleString);

    // Run the table look up tests.
    //
    Tests_String_TableLookups(moduleContext->DmfModuleString);

    // Repeat the test, until stop is signaled or the function stopped because the
    // driver is stopping.
    //
    if (! DMF_Thread_IsStopPending(DmfModuleThread))
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
Tests_String_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_String.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Tests_String* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    // Start the thread.
    //
    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread);

    // Tell the thread it has work to do.
    //
    DMF_Thread_WorkReady(moduleContext->DmfModuleThread);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_String_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Close an instance of a DMF Module of type Test_String.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_String* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Thread_Stop(moduleContext->DmfModuleThread);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_String_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_String* moduleContext;
    DMF_CONFIG_Thread moduleConfigThread;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // String
    // ------
    //
    DMF_String_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleString);

    // Thread
    // ------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_String_WorkThread;
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleThread);

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
DMF_Tests_String_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_String.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_String;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_String;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_String);
    dmfCallbacksDmf_Tests_String.ChildModulesAdd = DMF_Tests_String_ChildModulesAdd;
    dmfCallbacksDmf_Tests_String.DeviceOpen = Tests_String_Open;
    dmfCallbacksDmf_Tests_String.DeviceClose = Tests_String_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_String,
                                            Tests_String,
                                            DMF_CONTEXT_Tests_String,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Tests_String.CallbacksDmf = &dmfCallbacksDmf_Tests_String;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_String,
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

// eof: Dmf_Tests_String.c
//
