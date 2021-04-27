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

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_String.tmh"
#endif

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

_Function_class_(EVT_DMF_String_CompareCharCallback)
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

_Function_class_(EVT_DMF_String_CompareCharCallback)
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
    LONG returnValue;
    size_t comparisonLength;

    UNREFERENCED_PARAMETER(DmfModule);

    // Always check full length of given string.
    //
    comparisonLength = strlen(LookFor);

    if (0 == comparisonLength)
    {
        // Special case because strncmp always returns 0 in this case. It happens if
        // either string is "".
        //
        if (*StringInList)
        {
            // Given string is smaller.
            //
            returnValue = -1;
        }
        else
        {
            // They are both equal.
            //
            returnValue = 0;
        }
        goto Exit;
    }

    returnValue = strncmp(StringInList,
                          LookFor,
                          comparisonLength);

Exit:

    return returnValue;
}

#if defined(DMF_USER_MODE)

static
WCHAR* 
String_MultiStringToWideString(
    _In_ CHAR* NarrowString
    )
/*++

Routine Description:

    Allocate a buffer for a Wide string and copy into it a converted version of a given
    Narrow string. Caller must free the returned buffer.

Arguments:

    NarrowString - The given Narrow string to convert.

Return Value:

    The allocated buffer containing the Wide version of the given Narrow string.
    NULL if a buffer could not be allocated.

--*/
{
    ULONG numberOfCharacters;
    WCHAR* wideString;

    wideString = NULL;

    // Get the length of the converted string
    //
    numberOfCharacters = MultiByteToWideChar(CP_ACP,
                                             0,
                                             NarrowString,
                                             -1,
                                             NULL,
                                             0);
    if (0 == numberOfCharacters)
    {
        goto Exit;
    }

    // Allocate space to hold the converted string
    //
    size_t sizeOfBufer = numberOfCharacters * sizeof(WCHAR);
    wideString = (WCHAR*)malloc(sizeOfBufer);
    if (NULL == wideString)
    {
        goto Exit;
    }

    ZeroMemory(wideString, 
               sizeOfBufer);

    // Convert the string
    //
    numberOfCharacters = MultiByteToWideChar(CP_ACP,
                                             0,
                                             NarrowString,
                                             -1,
                                             wideString,
                                             numberOfCharacters);
    if (0 == numberOfCharacters)
    {
        free(wideString);
        wideString = NULL;
        goto Exit;
    }

Exit:

    return wideString;
}

static
CHAR* 
String_WideStringToMultiString(
    _In_ WCHAR* WideString
    )
{
    ULONG numberOfBytes;
    CHAR* multiString;

    multiString = NULL;

    // Get the length of the converted string
    // NOTE: This function returns the number of bytes needed to hold
    //       the result (not the number of characters).
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
    multiString = (CHAR*)malloc(numberOfBytes);
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

#pragma code_seg("PAGE")
static
NTSTATUS
String_NarrowStringCopyAsUnicode(
    _Out_ UNICODE_STRING* UnicodeString,
    _In_z_ CHAR* NarrowString
    )
/*++

Routine Description:

    Copy a Narrow string as a Unicode string.

Arguments:

    UnicodeString - Target Unicode string.
    NarrowString - Source Narrow string.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(UnicodeString != NULL);
    DmfAssert(NarrowString != NULL);

    WCHAR* wideString;

    // Create a converted string.
    //
    wideString = String_MultiStringToWideString(NarrowString);
    if (NULL == wideString)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "String_MultiStringToWideString");
        goto Exit;
    }

    // Get length of converted string.
    //
    size_t stringLength = wcslen(wideString);

    // Check to make sure that destinations ANSI string's buffer is big enough for 
    // the string and zero terminator.
    //
    if (stringLength + sizeof(WCHAR) > UnicodeString->MaximumLength)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    // Copy the converted string to the destination buffer.
    //
    wcscpy_s(UnicodeString->Buffer,
             UnicodeString->MaximumLength / sizeof(WCHAR),
             wideString);

    // Zero-terminate the destination string.
    //
    UnicodeString->Buffer[stringLength] = L'\0';

    // Update the length of the new Unicode string.
    //
    UnicodeString->Length = (USHORT)(stringLength * sizeof(WCHAR));

    ntStatus = STATUS_SUCCESS;

Exit:

    // Free temporary converted string if it could be allocated.
    //
    if (wideString != NULL)
    {
        free(wideString);
    }

    // AnsiString has the converted string.
    //
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
static
NTSTATUS
String_WideStringCopyAsAnsi(
    _Out_ ANSI_STRING* AnsiString,
    _In_z_ WCHAR* WideString
    )
/*++

Routine Description:

    Copy a Wide String as an Ansi String.

Arguments:

    AnsiString - Target Ansi String.
    WideString - Source Wide String.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(AnsiString != NULL);
    DmfAssert(WideString != NULL);

    char* multibyteString;

    // Create a converted string.
    //
    multibyteString = String_WideStringToMultiString(WideString);
    if (NULL == multibyteString)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "String_WideStringToMultiString");
        goto Exit;
    }

    // Get length of converted string.
    //
    size_t stringLength = strlen(multibyteString);

    // Check to make sure that destinations ANSI string's buffer is big enough for 
    // the string and zero terminator.
    //
    if (stringLength + sizeof(CHAR) > AnsiString->MaximumLength)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    // Copy the converted string to the destination buffer.
    //
    strncpy_s(AnsiString->Buffer,
              AnsiString->MaximumLength,
              multibyteString,
              stringLength);
    // Zero-terminate the destination string.
    //
    AnsiString->Buffer[stringLength] = '\0';

    // Update the length of new Ansi string.
    //
    AnsiString->Length = (USHORT)(stringLength * sizeof(CHAR));

    ntStatus = STATUS_SUCCESS;

Exit:

    // Free temporary converted string if it could be allocated.
    //
    if (multibyteString != NULL)
    {
        free(multibyteString);
    }

    // AnsiString has the converted string.
    //
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#endif

typedef struct
{
    WCHAR* LastString;
} String_MultiSzFindLastContext;

_Function_class_(EVT_DMF_String_MutilSzCallback)
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
String_MultiSzFindLastCallback(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* String,
    _In_ VOID* CallbackContext
    )
/*++

Routine Description:

    Performs an assignment operation to the context using the given multi string.

    NOTE: Alternative implementations of the callback could include a specific index demand and would need to have a context that contains a counter.

Arguments:

    DmfModule - This Module's handle.
    String - The given MULTI_SZ string.
    CallbackContext - The given context.

Return Value:

    TRUE - The callback only assigns the given MULTI_SZ string to the given context.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    String_MultiSzFindLastContext* callbackContext;

    callbackContext = (String_MultiSzFindLastContext*)CallbackContext;
    callbackContext->LastString = String;

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_String;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_MODULE_DESCRIPTOR_INIT(dmfModuleDescriptor_String,
                               String,
                               DMF_MODULE_OPTIONS_DISPATCH,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_String,
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

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    DmfAssert(StringList != NULL);

    // -1 indicates "not found int list".
    //
    returnValue = -1;

    for (ULONG stringIndex = 0; stringIndex < NumberOfStringsInStringList; stringIndex++)
    {
        DmfAssert(StringList[stringIndex] != NULL);
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Compare StringList[%u]=[%s] with [%s]", 
                    stringIndex,
                    StringList[stringIndex],
                    LookFor);
        if (ComparisonCallback(DmfModule,
                               StringList[stringIndex],
                               LookFor) == 0)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Compare StringList[%u]=[%s] with [%s]: Match", 
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
    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    return DMF_String_FindInListChar(DmfModule,
                                     StringList,
                                     NumberOfStringsInStringList,
                                     LookFor,
                                     String_FindInListExactCharCallback);
}

LONG
DMF_String_FindInListExactGuid(
    _In_ DMFMODULE DmfModule,
    _In_ GUID* GuidList,
    _In_ ULONG NumberOfGuidsInGuidList,
    _In_ GUID* LookFor
    )
/*++

Routine Description:

    Given a list of GUIDs, find the index of a given GUID.

Arguments:

    DmfModule - This Module's handle.
    GuidList - List of strings to search.
    NumberOfGuidsInGuidList - Number of GUIDs in GuidList.
    LookFor - GUID to look for.

Return Value:

    -1 - LookFor is not found in GuidList.
    non-negative: Index of GUID in GuidList that matches LookFor.

--*/
{
    LONG returnValue;

    UNREFERENCED_PARAMETER(DmfModule);
    DmfAssert(GuidList != NULL);

    // -1 indicates "not found int list".
    //
    returnValue = -1;

    for (ULONG guidIndex = 0; guidIndex < NumberOfGuidsInGuidList; guidIndex++)
    {
        if (DMF_Utility_IsEqualGUID((LPGUID)&GuidList[guidIndex],
                                    (LPGUID)LookFor))
        {
            returnValue = guidIndex;
            break;
        }
    }

    return returnValue;
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
    with a string in the list. if the either string matches the left side of the other string,
    the result is match.

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
    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    return DMF_String_FindInListChar(DmfModule,
                                     StringList,
                                     NumberOfStringsInStringList,
                                     LookFor,
                                     String_FindInListLeftLookForMatchCharCallback);
}

#pragma code_seg("PAGE")
NTSTATUS
DMF_String_MultiSzEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_z_ WCHAR* MultiSzWideString,
    _In_ EVT_DMF_String_MultiSzCallback Callback,
    _In_ VOID* CallbackContext
    )
/*++

Routine Description:

    Calls a given enumeration callback for every string found in the given MULTI_SZ string.

Arguments:

    DmfModule - This Module's handle.
    MultiSzWideString - The given MULTI_SZ string.
    Callback - The given enumeration callback.
    CallbackContext - The context passed to the enumeration callback.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    BOOLEAN continueEnumeration;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    // Set an offset pointer to the front of the incoming multi string.
    // 
    PWSTR stringOffset = MultiSzWideString;

    // NOTE: Support zero length strings within MULTI_SZ strings such as L"\0Last\0\0".
    //
    BOOLEAN searching = TRUE;
    while (searching)
    {
        if (*stringOffset == L'\0' &&
            *(stringOffset + 1) == L'\0')
        {
            // The end of MULTI_SZ string has been reached.
            //
            break;
        }

        continueEnumeration = Callback(DmfModule,
                                       stringOffset,
                                       CallbackContext);
        if (!continueEnumeration)
        {
            break;
        }

        // Run to the end of the current string terminator.
        // 
        while (*stringOffset != L'\0') 
        {
            stringOffset++;
        }

        // Check if the next character is another null
        // 
        stringOffset++;
        if (*stringOffset == L'\0')
        {
            break;
        }
    }

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
WCHAR*
DMF_String_MultiSzFindLast(
    _In_ DMFMODULE DmfModule,
    _In_z_ WCHAR* MultiSzWideString
    )
/*++

Routine Description:

    Returns the last found string in the given MULTI_SZ string.

Arguments:

    DmfModule - This Module's handle.
    MultiSzWideString - The given MULTI_SZ string.

Return Value:

    WCHAR*

--*/
{
    NTSTATUS ntStatus;
    String_MultiSzFindLastContext context;
 
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    RtlZeroMemory(&context,
                  sizeof(context));
    
    ntStatus = DMF_String_MultiSzEnumerate(DmfModule, 
                                           MultiSzWideString,
                                           String_MultiSzFindLastCallback, 
                                           &context);

    if (!NT_SUCCESS(ntStatus))
    {
        // Last string is already NULL.
        //
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return context.LastString;
}
#pragma code_seg()

#pragma code_seg("PAGE")
NTSTATUS
DMF_String_RtlAnsiStringToUnicodeString(
    _In_ DMFMODULE DmfModule,
    _Out_ PUNICODE_STRING DestinationString,
    _In_ PCANSI_STRING SourceString
    )
/*++

Routine Description:

    Copy an Ansi string as a Unicode string.

Arguments:

    DmfModule - This Module's handle.
    DestinationString - Destination Unicode string.
    SourceString - Source Ansi string.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    DmfAssert(DestinationString != NULL);
    DmfAssert(SourceString != NULL);

#if defined(DMF_KERNEL_MODE)
    // Kernel-mode directly supports the conversion.
    //
    ntStatus = RtlAnsiStringToUnicodeString(DestinationString,
                                            SourceString,
                                            FALSE);
#else
    // User-mode drivers do not support this API directly (even using GetProceAddress()).
    // So, use the Win32 functions to do that work.
    //

    // Unicode string may not be zero terminated so create a copy of it zero terminated.
    //
    CHAR* zeroTerminatedNarrowString;
    WDFMEMORY zeroTerminatedNarrowStringMemory;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               SourceString->Length + sizeof(WCHAR),
                               &zeroTerminatedNarrowStringMemory,
                               (VOID**)&zeroTerminatedNarrowString);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Create the zero terminated source string.
    //
    size_t numberOfElementsTotal = (SourceString->Length + sizeof(CHAR)) / sizeof(CHAR);
    size_t numberOfElementsToCopy = numberOfElementsTotal - 1;
    strncpy_s(zeroTerminatedNarrowString,
              numberOfElementsTotal,
              SourceString->Buffer,
              numberOfElementsToCopy);
    zeroTerminatedNarrowString[numberOfElementsToCopy] = L'\0';

    // Perform the conversion and write to destination buffer.
    //
    ntStatus = String_NarrowStringCopyAsUnicode(DestinationString,
                                                zeroTerminatedNarrowString);
    
    // Free the temporary buffer.
    //
    WdfObjectDelete(zeroTerminatedNarrowStringMemory);

Exit:
    ;
#endif // defined(DMF_KERNEL_MODE)

    // AnsiString has the converted string.
    //
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
NTSTATUS
DMF_String_RtlUnicodeStringToAnsiString(
    _In_ DMFMODULE DmfModule,
    _Out_ PANSI_STRING DestinationString,
    _In_ PCUNICODE_STRING SourceString
    )
/*++

Routine Description:

    Copy a Unicode string as an Ansi string.

Arguments:

    DmfModule - This Module's handle.
    DestinationString - Destination Ansi string.
    SourceString - Source Unicode string.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    DmfAssert(DestinationString != NULL);
    DmfAssert(SourceString != NULL);

#if defined(DMF_KERNEL_MODE)
    // Kernel-mode directly supports the conversion.
    //
    ntStatus = RtlUnicodeStringToAnsiString(DestinationString,
                                            SourceString,
                                            FALSE);
#else
    // User-mode drivers do not support this API directly (even using GetProceAddress()).
    // So, use the Win32 functions to do that work.
    //

    // Unicode string may not be zero terminated so create a copy of it zero terminated.
    //
    WCHAR* zeroTerminatedWideString;
    WDFMEMORY zeroTerminatedWideStringMemory;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               SourceString->Length + sizeof(WCHAR),
                               &zeroTerminatedWideStringMemory,
                               (VOID**)&zeroTerminatedWideString);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Create the zero terminated source string.
    //
    size_t numberOfElementsTotal = (SourceString->Length + sizeof(WCHAR)) / sizeof(WCHAR);
    size_t numberOfElementsToCopy = numberOfElementsTotal - 1;
    wcsncpy_s(zeroTerminatedWideString,
              numberOfElementsTotal,
              SourceString->Buffer,
              numberOfElementsToCopy);
    zeroTerminatedWideString[numberOfElementsToCopy] = L'\0';

    // Perform the conversion and write to destination buffer.
    //
    ntStatus = String_WideStringCopyAsAnsi(DestinationString,
                                           zeroTerminatedWideString);
    
    // Free the temporary buffer.
    //
    WdfObjectDelete(zeroTerminatedWideStringMemory);

Exit:
    ;
#endif // defined(DMF_KERNEL_MODE)

    // AnsiString has the converted string.
    //
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
NTSTATUS
DMF_String_WideStringCopyAsNarrow(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(BufferSize) CHAR* NarrowString,
    _In_ ULONG BufferSize,
    _In_z_ WCHAR* WideString
    )
/*++

Routine Description:

    Copy a Wide String as an Narrow String.

Arguments:

    DmfModule - This Module's handle.
    NarrowString - Target Narrow string.
    BufferSize - Size of buffer pointed to by NarrowString.
    WideString - Source Wide String.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ANSI_STRING ansiString;
    UNICODE_STRING unicodeString;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 String);

    DmfAssert(NarrowString != NULL);
    DmfAssert(WideString != NULL);

    if (BufferSize < sizeof(CHAR))
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    *NarrowString = '\0';

    RtlZeroMemory(&ansiString,
                  sizeof(ansiString));
    ansiString.Buffer = NarrowString;
    ansiString.Length = (USHORT)strlen(NarrowString);
    ansiString.MaximumLength = (USHORT)BufferSize;

    RtlInitUnicodeString(&unicodeString,
                         WideString);
    if (unicodeString.Length / sizeof(WCHAR) > BufferSize)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    ntStatus = DMF_String_RtlUnicodeStringToAnsiString(DmfModule,
                                                       &ansiString,
                                                       &unicodeString);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_String.c
//
