/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_String.c

Abstract:

    Support for general string related operations.

    NOTE: Method Name format is: "DMF_String_[Function][Char|Wchar|Unicode]{Insensitive}

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_String.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This Module has no Context.
//
DMF_MODULE_DECLARE_NO_CONTEXT(String)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(String)

// Memory Pool Tag.
//
#define MemoryTag 'irtS'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

LONG
String_FindInListExactCharCallback(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR* StringInList,
    _In_ CHAR* LookFor
    )
/*++

Routine Description:

    Perform exact case sensitive comparison between string in list and the given string.

Arguments:

    DmfModule - This Module's handle.
    StringInList - The string in the list.
    LookFor - The given string.

Return Value:

    0 - Match
    -1 - String in list comes before the given string.
    1 - String in list comes after the given string.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    return (strcmp(StringInList,
                   LookFor));
}

LONG
String_FindInListLeftListMatchCharCallback(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR* StringInList,
    _In_ CHAR* LookFor
    )
/*++

Routine Description:

    Perform LEFT case sensitive comparison between string in list and the given string.
    Comparison is made between the full string in list and left of the given string.

Arguments:

    DmfModule - This Module's handle.
    StringInList - The string in the list.
    LookFor - The given string.

Return Value:

    0 - Match
    -1 - String in list comes before the given string.
    1 - String in list comes after the given string.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    return strncmp(StringInList,
                   LookFor,
                   strlen(StringInList));
}

LONG
String_FindInListLeftLookForMatchCharCallback(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR* StringInList,
    _In_ CHAR* LookFor
    )
/*++

Routine Description:

    Perform LEFT case sensitive comparison between string in list and the given string.
    Comparison is made between the full given string and left of the string in the list.

Arguments:

    DmfModule - This Module's handle.
    StringInList - The string in the list.
    LookFor - The given string.

Return Value:

    0 - Match
    -1 - String in list comes before the given string.
    1 - String in list comes after the given string.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    return strncmp(StringInList,
                   LookFor,
                   strlen(LookFor));
}


#if defined(DMF_USER_MODE)

static
CHAR* 
String_WideStringToMultiString(
    _In_ WCHAR* WideString
    )
{
    ULONG numberOfBytes;
    PCHAR multiString;

    multiString = NULL;

    // Get the length of the converted string
    //
    numberOfBytes = WideCharToMultiByte(CP_ACP,
                                        0,
                                        WideString,
                                        -1,
                                        NULL,
                                        0,
                                        NULL,
                                        NULL);
    if (0 == numberOfBytes)
    {
        goto Exit;
    }

    // Allocate space to hold the converted string
    //
    multiString = (PCHAR)malloc(numberOfBytes);
    if (NULL == multiString)
    {
        goto Exit;
    }

    ZeroMemory(multiString, 
                numberOfBytes);

    // Convert the string
    //
    numberOfBytes = WideCharToMultiByte(CP_ACP,
                                        0,
                                        WideString,
                                        -1,
                                        multiString,
                                        numberOfBytes,
                                        NULL,
                                        NULL);
    if (0 == numberOfBytes)
    {
        free(multiString);
        multiString = NULL;
        goto Exit;
    }

Exit:

    return multiString;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_String;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_String_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type String.

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

    DMF_MODULE_DESCRIPTOR_INIT(DmfModuleDescriptor_String,
                               String,
                               DMF_MODULE_OPTIONS_DISPATCH,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_String,
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

LONG
DMF_String_FindInListChar(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR** StringList,
    _In_ ULONG NumberOfStringsInStringList,
    _In_ CHAR* LookFor,
    _In_ EVT_DMF_String_CompareCharCallback ComparisonCallback
    )
/*++

Routine Description:

    Given a list of strings, find a given string using a caller specific callback function for
    comparison between the strings in list and given to string.

Arguments:

    DmfModule - This Module's handle.
    StringList - List of strings to search.
    NumberOfStringsInStringList - Number of strings in StringList.
    LookFor - String to look for.
    ComparisonCallback - String comparison callback function for Client specific comparisons.

Return Value:

    -1 - LookFor is not found in StringList.
     non-zero: Index of string in StringList that matches LookFor.

--*/
{
    LONG returnValue;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_String);

    ASSERT(StringList != NULL);

    // -1 indicates "not found int list".
    //
    returnValue = -1;

    for (ULONG stringIndex = 0; stringIndex < NumberOfStringsInStringList; stringIndex++)
    {
        ASSERT(StringList[stringIndex] != NULL);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Compare StringList[%u]=[%s] with [%s]", 
                    stringIndex,
                    StringList[stringIndex],
                    LookFor);
        if (ComparisonCallback(DmfModule,
                               StringList[stringIndex],
                               LookFor) == 0)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Compare StringList[%u]=[%s] with [%s]: Match", 
                        stringIndex,
                        StringList[stringIndex],
                        LookFor);
            returnValue = stringIndex;
            break;
        }
    }

    return returnValue;
}

LONG
DMF_String_FindInListExactChar(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR** StringList,
    _In_ ULONG NumberOfStringsInStringList,
    _In_ CHAR* LookFor
    )
/*++

Routine Description:

    Given a list of strings, find a given string using an exact match.

Arguments:

    DmfModule - This Module's handle.
    StringList - List of strings to search.
    NumberOfStringsInStringList - Number of strings in StringList.
    LookFor - String to look for.

Return Value:

    -1 - LookFor is not found in StringList.
     non-zero: Index of string in StringList that matches LookFor.

--*/
{
    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_String);

    return DMF_String_FindInListChar(DmfModule,
                                     StringList,
                                     NumberOfStringsInStringList,
                                     LookFor,
                                     String_FindInListExactCharCallback);
}

LONG
DMF_String_FindInListLookForLeftMatchChar(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR** StringList,
    _In_ ULONG NumberOfStringsInStringList,
    _In_ CHAR* LookFor
    )
/*++

Routine Description:

    Given a list of strings, find a given string by matching the beginning of the given string
    with a string in the list.

Arguments:

    DmfModule - This Module's handle.
    StringList - List of strings to search.
    NumberOfStringsInStringList - Number of strings in StringList.
    LookFor - String to look for.

Return Value:

    -1 - LookFor is not found in StringList.
     non-zero: Index of string in StringList that matches LookFor.

--*/
{
    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_String);

    return DMF_String_FindInListChar(DmfModule,
                                     StringList,
                                     NumberOfStringsInStringList,
                                     LookFor,
                                     String_FindInListLeftLookForMatchCharCallback);
}

NTSTATUS
DMF_String_WideStringCopyAsAnsi(
    _Out_writes_(BufferSize) CHAR* AnsiString,
    _In_z_ WCHAR* WideString,
    _In_ ULONG BufferSize
    )
/*++

Routine Description:

    Copy a Wide String as an Ansi String.

Arguments:

    AnsiString - Target Ansi String.
    WideString - Source Wide String.
    BufferSize - Total size of output buffer.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(BufferSize);

    FuncEntry(DMF_TRACE);

    ASSERT(AnsiString != NULL);
    ASSERT(WideString != NULL);

    *AnsiString = '\0';

#if !defined(DMF_USER_MODE)
    ANSI_STRING ansiString;
    UNICODE_STRING unicodeString;

    RtlInitAnsiString(&ansiString,
                      AnsiString);
    ansiString.MaximumLength = (USHORT)BufferSize;

    RtlInitUnicodeString(&unicodeString,
                         WideString);
    ASSERT(unicodeString.Length <= BufferSize);

    // Since this funtion does not allocate memory it will always succeed.
    //
    ntStatus = RtlUnicodeStringToAnsiString(&ansiString,
                                            &unicodeString,
                                            FALSE);
    ASSERT(NT_SUCCESS(ntStatus));
#else
    char* string;

    string = String_WideStringToMultiString(WideString);
    if (NULL == string)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "String_WideStringToMultiString");
        goto Exit;
    }

    strcpy_s(AnsiString, 
             BufferSize,
             string);
    free(string);

    ntStatus = STATUS_SUCCESS;

Exit:
    ;

#endif

    // AnsiString has the converted string.
    //
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_String.c
//
